#include "ZipArchive.h"
#include "ZipFormat.h"

#include <zlib.h>

#define ZIP_STATE_COOKIE 0x5A495031UL

#define ZIP_UNICODE_PATH_EXTRA_FIELD_TAG 0x7075U

#define ZIP_METHOD_STORED 0U
#define ZIP_METHOD_DEFLATE 8U

#define ZIP_FLAG_ENCRYPTED 0x0001U
#define ZIP_FLAG_UTF8 0x0800U

/*
 * Стратегия I/O рассчитана на SMB, где каждый ReadFile — сетевой round trip:
 * - хвост архива читается одним запросом ZIP_TAIL_READ_SIZE (вмещает EOCD с
 *   максимальным комментарием и типичный central directory целиком), больший
 *   central directory дочитывается одним запросом недостающего диапазона;
 * - запись читается одним запросом от local header: заголовок + имя + запас
 *   под local extra + данные; при нехватке запаса — одно дочитывание.
 */
#define ZIP_TAIL_READ_SIZE (128U * 1024U)
#define ZIP_MAX_DIRECTORY_WINDOW (256ULL * 1024ULL * 1024ULL)
#define ZIP_LOCAL_HEADER_SLACK 1024U
/* До этого порога сжатый вход deflate читается целиком и распаковывается из памяти. */
#define ZIP_INFLATE_WHOLE_INPUT_LIMIT (64ULL * 1024ULL * 1024ULL)
/* Больший сжатый вход читается кусками этого размера. */
#define ZIP_INFLATE_CHUNK_SIZE (8U * 1024U * 1024U)
/* Предел одного ReadFile: длинные чтения делятся на такие части. */
#define ZIP_MAX_READ_CALL (64U * 1024U * 1024U)

/*
 * Буфер ExtractEntry может начинаться внутри блока кучи (данные stored-записи
 * читаются вместе с local header). Перед данными лежит метка с расстоянием
 * до начала блока; ZipArchive_FreeBuffer освобождает именно блок.
 */
#define ZIP_BUFFER_TAG_SIZE 16U
#define ZIP_BUFFER_TAG_MAGIC 0x4655425AUL

static WORD zip_read_u16(const BYTE* data) {
    return (WORD)((WORD)data[0] | ((WORD)data[1] << 8));
}

static DWORD zip_read_u32(const BYTE* data) {
    return (DWORD)data[0] |
        ((DWORD)data[1] << 8) |
        ((DWORD)data[2] << 16) |
        ((DWORD)data[3] << 24);
}

static void zip_write_u32(BYTE* data, DWORD value) {
    data[0] = (BYTE)value;
    data[1] = (BYTE)(value >> 8);
    data[2] = (BYTE)(value >> 16);
    data[3] = (BYTE)(value >> 24);
}

static void* zip_heap_alloc(SIZE_T size) {
    if (size == 0) {
        size = 1;
    }
    return HeapAlloc(GetProcessHeap(), 0, size);
}

static void zip_heap_free(void* memory) {
    if (memory != NULL) {
        HeapFree(GetProcessHeap(), 0, memory);
    }
}

static WCHAR* zip_duplicate_path(LPCWSTR path) {
    SIZE_T length;
    SIZE_T bytes;
    WCHAR* copy;

    if (path == NULL) {
        return NULL;
    }

    length = (SIZE_T)lstrlenW(path);
    if (length > ((SIZE_T)-1) / sizeof(WCHAR) - 1) {
        SetLastError(ERROR_ARITHMETIC_OVERFLOW);
        return NULL;
    }
    bytes = (length + 1) * sizeof(WCHAR);
    copy = (WCHAR*)zip_heap_alloc(bytes);
    if (copy == NULL) {
        SetLastError(ERROR_OUTOFMEMORY);
        return NULL;
    }
    CopyMemory(copy, path, bytes);
    return copy;
}

static ULONGLONG zip_now(void) {
    LARGE_INTEGER now;

    QueryPerformanceCounter(&now);
    return (ULONGLONG)now.QuadPart;
}

static void zip_io_delta(
    const ZipIoStats* before,
    const ZipIoStats* after,
    ZipIoStats* delta) {
    delta->read_calls = after->read_calls - before->read_calls;
    delta->read_bytes = after->read_bytes - before->read_bytes;
}

/*
 * Позиционное чтение: смещение задаётся через OVERLAPPED (для синхронного
 * хэндла ReadFile всё равно блокирующий), без отдельного SetFilePointerEx.
 */
static BOOL zip_read_exact_at(
    ZipArchive* archive,
    ULONGLONG offset,
    void* destination,
    SIZE_T size) {
    BYTE* output = (BYTE*)destination;

    if (archive == NULL || destination == NULL ||
        offset > archive->file_size ||
        (ULONGLONG)size > archive->file_size - offset) {
        SetLastError(ERROR_HANDLE_EOF);
        return FALSE;
    }

    while (size != 0) {
        OVERLAPPED overlapped;
        DWORD requested = size > ZIP_MAX_READ_CALL
            ? ZIP_MAX_READ_CALL
            : (DWORD)size;
        DWORD received = 0;

        ZeroMemory(&overlapped, sizeof(overlapped));
        overlapped.Offset = (DWORD)offset;
        overlapped.OffsetHigh = (DWORD)(offset >> 32);
        ++archive->io.read_calls;
        if (!ReadFile(archive->file, output, requested, &received, &overlapped)) {
            return FALSE;
        }
        archive->io.read_bytes += received;
        if (received != requested) {
            SetLastError(ERROR_HANDLE_EOF);
            return FALSE;
        }

        output += received;
        offset += received;
        size -= received;
    }

    return TRUE;
}

static BOOL zip_bad_format(void) {
    SetLastError(ERROR_BAD_FORMAT);
    return FALSE;
}

static DWORD zip_crc32_bytes(const BYTE* data, SIZE_T size) {
    uLong value = crc32(0L, Z_NULL, 0);

    while (size != 0) {
        uInt chunk = size > 0xFFFFFFFFU ? 0xFFFFFFFFU : (uInt)size;
        value = crc32(value, (const Bytef*)data, chunk);
        data += chunk;
        size -= chunk;
    }
    return (DWORD)value;
}

static BOOL zip_find_unicode_path(
    const BYTE* original_name,
    WORD original_name_size,
    const BYTE* extra,
    WORD extra_size,
    const BYTE** utf8_name,
    WORD* utf8_name_size) {
    SIZE_T position = 0;
    DWORD original_crc = zip_crc32_bytes(original_name, original_name_size);

    *utf8_name = NULL;
    *utf8_name_size = 0;

    while (position + 4 <= extra_size) {
        WORD tag = zip_read_u16(extra + position);
        WORD field_size = zip_read_u16(extra + position + 2);
        SIZE_T field_offset = position + 4;

        if ((SIZE_T)field_size > (SIZE_T)extra_size - field_offset) {
            return FALSE;
        }

        if (tag == ZIP_UNICODE_PATH_EXTRA_FIELD_TAG &&
            field_size >= 5 &&
            extra[field_offset] == 1 &&
            zip_read_u32(extra + field_offset + 1) == original_crc) {
            SIZE_T name_size = (SIZE_T)field_size - 5;

            if (name_size == 0) {
                position = field_offset + field_size;
                continue;
            }
            *utf8_name = extra + field_offset + 5;
            *utf8_name_size = (WORD)name_size;
            return TRUE;
        }

        position = field_offset + field_size;
    }

    /* Некоторые архиваторы оставляют в конце extra-поля до трёх байт padding. */
    return (SIZE_T)extra_size - position < 4;
}

static WCHAR* zip_decode_name(
    const BYTE* name,
    WORD name_size,
    WORD flags,
    const BYTE* extra,
    WORD extra_size,
    BOOL* is_directory) {
    const BYTE* bytes = name;
    WORD byte_count = name_size;
    UINT code_page = 437;
    DWORD conversion_flags = 0;
    const BYTE* unicode_name = NULL;
    WORD unicode_name_size = 0;
    int wide_count;
    WCHAR* result;
    SIZE_T index;
    BOOL can_fallback_to_legacy = FALSE;

    *is_directory = FALSE;
    if (name_size == 0) {
        SetLastError(ERROR_BAD_FORMAT);
        return NULL;
    }

    if ((flags & ZIP_FLAG_UTF8) != 0) {
        code_page = CP_UTF8;
        conversion_flags = MB_ERR_INVALID_CHARS;
        can_fallback_to_legacy = TRUE;
    }
    else if (extra_size != 0) {
        if (!zip_find_unicode_path(
                name,
                name_size,
                extra,
                extra_size,
                &unicode_name,
                &unicode_name_size)) {
            SetLastError(ERROR_BAD_FORMAT);
            return NULL;
        }
        if (unicode_name != NULL) {
            bytes = unicode_name;
            byte_count = unicode_name_size;
            code_page = CP_UTF8;
            conversion_flags = MB_ERR_INVALID_CHARS;
            can_fallback_to_legacy = TRUE;
        }
    }

retry_conversion:
    wide_count = MultiByteToWideChar(
        code_page,
        conversion_flags,
        (LPCCH)bytes,
        (int)byte_count,
        NULL,
        0);
    if (wide_count <= 0 && can_fallback_to_legacy) {
        /* Битый UTF-8 не должен делать весь архив нечитаемым. */
        bytes = name;
        byte_count = name_size;
        code_page = 437;
        conversion_flags = 0;
        can_fallback_to_legacy = FALSE;
        goto retry_conversion;
    }
    if (wide_count <= 0 ||
        (SIZE_T)wide_count > ((SIZE_T)-1) / sizeof(WCHAR) - 1) {
        if (GetLastError() == ERROR_SUCCESS) {
            SetLastError(ERROR_BAD_FORMAT);
        }
        return NULL;
    }

    result = (WCHAR*)zip_heap_alloc(((SIZE_T)wide_count + 1) * sizeof(WCHAR));
    if (result == NULL) {
        SetLastError(ERROR_OUTOFMEMORY);
        return NULL;
    }

    if (MultiByteToWideChar(
            code_page,
            conversion_flags,
            (LPCCH)bytes,
            (int)byte_count,
            result,
            wide_count) != wide_count) {
        DWORD error = GetLastError();
        zip_heap_free(result);
        SetLastError(error);
        return NULL;
    }
    result[wide_count] = L'\0';

    for (index = 0; index < (SIZE_T)wide_count; ++index) {
        if (result[index] == L'\0') {
            zip_heap_free(result);
            SetLastError(ERROR_BAD_FORMAT);
            return NULL;
        }
        if (result[index] == L'/') {
            result[index] = L'\\';
        }
    }
    *is_directory = result[wide_count - 1] == L'\\';
    return result;
}

/* --- Разбор central directory из окна в памяти --------------------------- */

typedef struct ZipEntryList {
    ZipEntry* entries;
    SIZE_T count;
    SIZE_T capacity;
} ZipEntryList;

static void zip_entry_list_free(ZipEntryList* list) {
    SIZE_T index;

    for (index = 0; index < list->count; ++index) {
        zip_heap_free(list->entries[index].name);
    }
    zip_heap_free(list->entries);
    list->entries = NULL;
    list->count = 0;
    list->capacity = 0;
}

static BOOL zip_entry_list_append(ZipEntryList* list, const ZipEntry* entry) {
    if (list->count == list->capacity) {
        SIZE_T new_capacity = list->capacity == 0 ? 16 : list->capacity * 2;
        SIZE_T bytes;
        ZipEntry* new_entries;

        if (new_capacity < list->capacity ||
            new_capacity > ((SIZE_T)-1) / sizeof(ZipEntry)) {
            SetLastError(ERROR_OUTOFMEMORY);
            return FALSE;
        }
        bytes = new_capacity * sizeof(ZipEntry);
        new_entries = list->entries == NULL
            ? (ZipEntry*)HeapAlloc(GetProcessHeap(), 0, bytes)
            : (ZipEntry*)HeapReAlloc(GetProcessHeap(), 0, list->entries, bytes);
        if (new_entries == NULL) {
            SetLastError(ERROR_OUTOFMEMORY);
            return FALSE;
        }
        list->entries = new_entries;
        list->capacity = new_capacity;
    }
    list->entries[list->count++] = *entry;
    return TRUE;
}

typedef struct ZipParseContext {
    /* Записи последнего принятого кандидата EOCD. */
    ZipEntryList result;
    BOOL has_result;
} ZipParseContext;

/*
 * Полный разбор central directory кандидата: структуру уже проверил
 * ZipFormat, здесь декодируются имена (ошибка декодирования отвергает
 * кандидата, как в исходном FolderView). Результат принятого кандидата
 * переиспользуется — второго прохода по файлу нет.
 */
static int zip_parse_candidate(
    void* context,
    const ZipFormatWindow* window,
    const ZipFormatDirectory* directory) {
    ZipParseContext* parse = (ZipParseContext*)context;
    ZipEntryList parsed = {0};
    const BYTE* data = NULL;
    ULONGLONG cursor = 0;
    ULONGLONG index;

    if (directory->size != 0) {
        data = window->data + (SIZE_T)(directory->offset - window->offset);
    }

    for (index = 0; index < directory->total_entries; ++index) {
        ZipFormatCentralEntry central;
        ZipEntry entry;
        BOOL is_directory;
        WCHAR* name;

        if (ZipFormat_ReadCentralEntry(
                data + (SIZE_T)cursor,
                directory->size - cursor,
                &central) != ZIP_FORMAT_OK) {
            goto reject;
        }
        name = zip_decode_name(
            central.name,
            central.name_size,
            central.flags,
            central.extra,
            central.extra_size,
            &is_directory);
        if (name == NULL) {
            goto reject;
        }
        cursor += central.record_size;
        if (is_directory) {
            zip_heap_free(name);
            continue;
        }

        entry.name = name;
        entry.compressed_size = central.compressed_size;
        entry.uncompressed_size = central.uncompressed_size;
        entry.local_header_offset = central.local_header_offset;
        entry.compression_method = central.method;
        entry.flags = central.flags;
        entry.name_size = central.name_size;
        if (!zip_entry_list_append(&parsed, &entry)) {
            zip_heap_free(name);
            goto reject;
        }
    }

    zip_entry_list_free(&parse->result);
    parse->result = parsed;
    parse->has_result = TRUE;
    return 1;

reject:
    zip_entry_list_free(&parsed);
    return 0;
}

/* Расширяет окно-суффикс влево до new_offset одним ReadFile недостающей части. */
static BOOL zip_extend_window(
    ZipArchive* archive,
    BYTE** window,
    ULONGLONG* window_offset,
    SIZE_T* window_size,
    ULONGLONG new_offset) {
    ULONGLONG new_size64 = archive->file_size - new_offset;
    SIZE_T prefix;
    BYTE* extended;

    if (new_offset >= *window_offset) {
        return zip_bad_format();
    }
    if (new_size64 > ZIP_MAX_DIRECTORY_WINDOW ||
        new_size64 > (ULONGLONG)((SIZE_T)-1)) {
        SetLastError(ERROR_NOT_SUPPORTED);
        return FALSE;
    }

    prefix = (SIZE_T)(*window_offset - new_offset);
    extended = (BYTE*)zip_heap_alloc((SIZE_T)new_size64);
    if (extended == NULL) {
        SetLastError(ERROR_OUTOFMEMORY);
        return FALSE;
    }
    if (!zip_read_exact_at(archive, new_offset, extended, prefix)) {
        DWORD error = GetLastError();
        zip_heap_free(extended);
        SetLastError(error);
        return FALSE;
    }
    CopyMemory(extended + prefix, *window, *window_size);
    zip_heap_free(*window);
    *window = extended;
    *window_offset = new_offset;
    *window_size = (SIZE_T)new_size64;
    return TRUE;
}

static BOOL zip_read_central_directory(ZipArchive* archive) {
    ULONGLONG started = zip_now();
    ULONGLONG tail_done;
    ZipIoStats io_started = archive->io;
    ZipIoStats io_tail;
    ZipParseContext parse;
    ZipFormatDirectory directory;
    ZipFormatStatus status;
    BYTE* window;
    SIZE_T window_size;
    ULONGLONG window_offset;
    DWORD error;

    if (archive->file_size < ZIP_FORMAT_EOCD_SIZE) {
        return zip_bad_format();
    }

    /* Фаза tail: один ReadFile хвоста архива. */
    window_size = archive->file_size > ZIP_TAIL_READ_SIZE
        ? ZIP_TAIL_READ_SIZE
        : (SIZE_T)archive->file_size;
    window_offset = archive->file_size - window_size;
    window = (BYTE*)zip_heap_alloc(window_size);
    if (window == NULL) {
        SetLastError(ERROR_OUTOFMEMORY);
        return FALSE;
    }
    if (!zip_read_exact_at(archive, window_offset, window, window_size)) {
        error = GetLastError();
        zip_heap_free(window);
        SetLastError(error);
        return FALSE;
    }
    tail_done = zip_now();
    io_tail = archive->io;
    archive->open_stats.tail_ticks = tail_done - started;
    zip_io_delta(&io_started, &io_tail, &archive->open_stats.tail_io);

    /* Фаза directory: поиск EOCD и разбор из памяти, дочитывания — по запросу. */
    parse.result.entries = NULL;
    parse.result.count = 0;
    parse.result.capacity = 0;
    parse.has_result = FALSE;
    for (;;) {
        ZipFormatWindow format_window;
        ULONGLONG required_offset = 0;

        format_window.data = window;
        format_window.offset = window_offset;
        format_window.size = window_size;
        format_window.file_size = archive->file_size;

        /* Каждая попытка начинает поиск заново: прежний результат не нужен. */
        zip_entry_list_free(&parse.result);
        parse.has_result = FALSE;
        status = ZipFormat_LocateCentralDirectory(
            &format_window,
            zip_parse_candidate,
            &parse,
            &directory,
            &required_offset);
        if (status != ZIP_FORMAT_NEED_MORE) {
            break;
        }
        if (!zip_extend_window(
                archive,
                &window,
                &window_offset,
                &window_size,
                required_offset)) {
            error = GetLastError();
            zip_heap_free(window);
            SetLastError(error);
            return FALSE;
        }
    }
    zip_heap_free(window);

    if (status != ZIP_FORMAT_OK || !parse.has_result) {
        zip_entry_list_free(&parse.result);
        SetLastError(status == ZIP_FORMAT_UNSUPPORTED
            ? ERROR_NOT_SUPPORTED
            : ERROR_BAD_FORMAT);
        return FALSE;
    }

    archive->entries = parse.result.entries;
    archive->entry_count = parse.result.count;
    archive->entry_capacity = parse.result.capacity;
    archive->central_directory_offset = directory.offset;
    archive->central_directory_size = directory.size;

    archive->open_stats.directory_ticks = zip_now() - tail_done;
    zip_io_delta(&io_tail, &archive->io, &archive->open_stats.directory_io);
    return TRUE;
}

void ZipArchive_Init(ZipArchive* archive) {
    if (archive == NULL) {
        return;
    }

    archive->file = INVALID_HANDLE_VALUE;
    archive->path = NULL;
    archive->entries = NULL;
    archive->entry_count = 0;
    archive->entry_capacity = 0;
    archive->file_size = 0;
    archive->central_directory_offset = 0;
    archive->central_directory_size = 0;
    archive->last_write_time.dwLowDateTime = 0;
    archive->last_write_time.dwHighDateTime = 0;
    archive->has_last_write_time = FALSE;
    archive->input_buffer = NULL;
    archive->input_capacity = 0;
    ZeroMemory(&archive->io, sizeof(archive->io));
    ZeroMemory(&archive->open_stats, sizeof(archive->open_stats));
    archive->state_cookie = ZIP_STATE_COOKIE;
}

void ZipArchive_Close(ZipArchive* archive) {
    SIZE_T index;

    if (archive == NULL || archive->state_cookie != ZIP_STATE_COOKIE) {
        return;
    }

    for (index = 0; index < archive->entry_count; ++index) {
        zip_heap_free(archive->entries[index].name);
    }
    zip_heap_free(archive->entries);
    zip_heap_free(archive->path);
    zip_heap_free(archive->input_buffer);

    if (archive->file != NULL && archive->file != INVALID_HANDLE_VALUE) {
        CloseHandle(archive->file);
    }
    ZipArchive_Init(archive);
}

/*
 * Без FILE_FLAG_RANDOM_ACCESS: этот флаг отключает read-ahead кэш-менеджера
 * (на SMB замер: 64 КиБ-чтения 1,4 МиБ/с против 6,9 МиБ/с без флага).
 * Крупные одиночные чтения от него больше не зависят, а последовательный
 * просмотр альбома выигрывает от штатного read-ahead.
 */
DWORD ZipArchive_DefaultOpenFlags(void) {
    return 0;
}

BOOL ZipArchive_Open(ZipArchive* archive, LPCWSTR path) {
    return ZipArchive_OpenEx(archive, path, ZipArchive_DefaultOpenFlags());
}

BOOL ZipArchive_OpenEx(ZipArchive* archive, LPCWSTR path, DWORD file_flags) {
    LARGE_INTEGER file_size;
    DWORD error;

    if (archive == NULL || path == NULL || *path == L'\0') {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    if (archive->state_cookie != ZIP_STATE_COOKIE) {
        ZipArchive_Init(archive);
    }
    else {
        ZipArchive_Close(archive);
    }

    archive->file = CreateFileW(
        path,
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL | file_flags,
        NULL);
    if (archive->file == INVALID_HANDLE_VALUE) {
        return FALSE;
    }
    if (!GetFileSizeEx(archive->file, &file_size) || file_size.QuadPart < 0) {
        error = GetLastError();
        ZipArchive_Close(archive);
        SetLastError(error);
        return FALSE;
    }
    archive->file_size = (ULONGLONG)file_size.QuadPart;
    /* Без времени изменения архив просто не переиспользуется. */
    archive->has_last_write_time =
        GetFileTime(archive->file, NULL, NULL, &archive->last_write_time);

    archive->path = zip_duplicate_path(path);
    if (archive->path == NULL || !zip_read_central_directory(archive)) {
        error = GetLastError();
        ZipArchive_Close(archive);
        SetLastError(error);
        return FALSE;
    }

    return TRUE;
}

BOOL ZipArchive_IsOpen(const ZipArchive* archive) {
    return archive != NULL &&
        archive->state_cookie == ZIP_STATE_COOKIE &&
        archive->file != NULL &&
        archive->file != INVALID_HANDLE_VALUE;
}

LPCWSTR ZipArchive_Path(const ZipArchive* archive) {
    return ZipArchive_IsOpen(archive) ? archive->path : NULL;
}

BOOL ZipArchive_IsUnchanged(const ZipArchive* archive) {
    LARGE_INTEGER file_size;
    FILETIME last_write_time;

    if (!ZipArchive_IsOpen(archive) || !archive->has_last_write_time) {
        return FALSE;
    }
    /*
     * Хэндл открыт с FILE_SHARE_READ, поэтому запись в файл другими
     * процессами невозможна; сверка — страховка в духе проверки поколения.
     */
    if (!GetFileSizeEx(archive->file, &file_size) ||
        file_size.QuadPart < 0 ||
        (ULONGLONG)file_size.QuadPart != archive->file_size ||
        !GetFileTime(archive->file, NULL, NULL, &last_write_time)) {
        return FALSE;
    }
    return last_write_time.dwLowDateTime == archive->last_write_time.dwLowDateTime &&
        last_write_time.dwHighDateTime == archive->last_write_time.dwHighDateTime;
}

SIZE_T ZipArchive_EntryCount(const ZipArchive* archive) {
    return ZipArchive_IsOpen(archive) ? archive->entry_count : 0;
}

const ZipEntry* ZipArchive_EntryAt(const ZipArchive* archive, SIZE_T index) {
    if (!ZipArchive_IsOpen(archive) || index >= archive->entry_count) {
        return NULL;
    }
    return archive->entries + index;
}

LPCWSTR ZipEntry_Name(const ZipEntry* entry) {
    return entry != NULL ? entry->name : NULL;
}

static BOOL zip_entry_belongs_to_archive(
    const ZipArchive* archive,
    const ZipEntry* entry) {
    SIZE_T index;

    for (index = 0; index < archive->entry_count; ++index) {
        if (archive->entries + index == entry) {
            return TRUE;
        }
    }
    return FALSE;
}

static voidpf zip_zalloc(voidpf opaque, uInt items, uInt size) {
    SIZE_T byte_count;

    if (size != 0 && (SIZE_T)items > ((SIZE_T)-1) / (SIZE_T)size) {
        return NULL;
    }
    byte_count = (SIZE_T)items * (SIZE_T)size;
    if (byte_count == 0) {
        byte_count = 1;
    }
    return HeapAlloc((HANDLE)opaque, 0, byte_count);
}

static void zip_zfree(voidpf opaque, voidpf address) {
    if (address != NULL) {
        HeapFree((HANDLE)opaque, 0, address);
    }
}

/*
 * Распаковка deflate. Первая порция сжатого входа уже в памяти (прочитана
 * вместе с local header); если вход длиннее порога, остальное читается
 * кусками ZIP_INFLATE_CHUNK_SIZE в тот же буфер.
 */
static BOOL zip_inflate_entry(
    ZipArchive* archive,
    const ZipEntry* entry,
    ULONGLONG data_offset,
    BYTE* input,
    SIZE_T input_capacity,
    const BYTE* first_input,
    SIZE_T first_input_size,
    BYTE* output) {
    BYTE overflow_byte;
    z_stream stream;
    ULONGLONG input_loaded = first_input_size;
    ULONGLONG input_consumed = 0;
    ULONGLONG output_produced = 0;
    BOOL using_overflow_byte = FALSE;
    int status;
    BOOL success = FALSE;

    ZeroMemory(&stream, sizeof(stream));
    stream.zalloc = zip_zalloc;
    stream.zfree = zip_zfree;
    stream.opaque = GetProcessHeap();
    status = inflateInit2(&stream, -MAX_WBITS);
    if (status != Z_OK) {
        SetLastError(status == Z_MEM_ERROR ? ERROR_OUTOFMEMORY : ERROR_BAD_FORMAT);
        return FALSE;
    }
    stream.next_in = (Bytef*)first_input;
    stream.avail_in = (uInt)first_input_size;

    for (;;) {
        uInt input_before;
        uInt output_before;
        ULONGLONG input_remaining;
        ULONGLONG output_remaining;

        if (stream.avail_in == 0 && input_loaded < entry->compressed_size) {
            DWORD chunk;

            input_remaining = entry->compressed_size - input_loaded;
            chunk = input_remaining > ZIP_INFLATE_CHUNK_SIZE
                ? ZIP_INFLATE_CHUNK_SIZE
                : (DWORD)input_remaining;
            if ((SIZE_T)chunk > input_capacity) {
                chunk = (DWORD)input_capacity;
            }
            if (!zip_read_exact_at(
                    archive,
                    data_offset + input_loaded,
                    input,
                    chunk)) {
                goto cleanup;
            }
            input_loaded += chunk;
            stream.next_in = input;
            stream.avail_in = chunk;
        }

        if (stream.avail_out == 0) {
            output_remaining = entry->uncompressed_size - output_produced;
            if (output_remaining != 0) {
                uInt chunk = output_remaining > 0xFFFFFFFFU
                    ? 0xFFFFFFFFU
                    : (uInt)output_remaining;
                stream.next_out = output + (SIZE_T)output_produced;
                stream.avail_out = chunk;
                using_overflow_byte = FALSE;
            }
            else {
                stream.next_out = &overflow_byte;
                stream.avail_out = 1;
                using_overflow_byte = TRUE;
            }
        }

        input_before = stream.avail_in;
        output_before = stream.avail_out;
        status = inflate(&stream, Z_NO_FLUSH);
        input_consumed += input_before - stream.avail_in;

        if (using_overflow_byte) {
            if (output_before != stream.avail_out) {
                SetLastError(ERROR_BAD_FORMAT);
                goto cleanup;
            }
        }
        else {
            output_produced += output_before - stream.avail_out;
        }

        if (status == Z_STREAM_END) {
            break;
        }
        if (status != Z_OK) {
            SetLastError(status == Z_MEM_ERROR
                ? ERROR_OUTOFMEMORY
                : ERROR_BAD_FORMAT);
            goto cleanup;
        }
        if (stream.avail_in == 0 &&
            input_loaded == entry->compressed_size &&
            output_before == stream.avail_out) {
            SetLastError(ERROR_BAD_FORMAT);
            goto cleanup;
        }
    }

    if (input_consumed != entry->compressed_size ||
        output_produced != entry->uncompressed_size) {
        SetLastError(ERROR_BAD_FORMAT);
        goto cleanup;
    }
    success = TRUE;

cleanup:
    inflateEnd(&stream);
    return success;
}

/* Метка перед данными: magic, расстояние до начала блока кучи, контроль. */
static BYTE* zip_tag_buffer(BYTE* block, SIZE_T data_offset) {
    BYTE* data = block + data_offset;
    BYTE* tag = data - ZIP_BUFFER_TAG_SIZE;

    zip_write_u32(tag, ZIP_BUFFER_TAG_MAGIC);
    zip_write_u32(tag + 4, (DWORD)data_offset);
    zip_write_u32(tag + 8, ~(DWORD)ZIP_BUFFER_TAG_MAGIC);
    zip_write_u32(tag + 12, (DWORD)data_offset ^ (DWORD)ZIP_BUFFER_TAG_MAGIC);
    return data;
}

/*
 * Первое чтение записи: local header + имя + запас + data_bytes данных одним
 * ReadFile. Если local extra длиннее запаса и первые data_bytes данных не
 * поместились, блок увеличивается и недостающее дочитывается одним запросом.
 * data_bytes не больше compressed_size.
 *
 * use_input_buffer: блок — переиспользуемый archive->input_buffer (вход
 * deflate, владеет архив, вызывающий не освобождает); иначе — новый блок
 * кучи, которым владеет вызывающий (данные stored-записи).
 */
static BOOL zip_read_entry_head(
    ZipArchive* archive,
    const ZipEntry* entry,
    ULONGLONG data_bytes,
    BOOL use_input_buffer,
    BYTE** block,
    SIZE_T* block_size,
    SIZE_T* data_offset) {
    ULONGLONG read_size = ZipFormat_EntryReadSize(
        archive->file_size,
        entry->local_header_offset,
        entry->name_size,
        data_bytes,
        ZIP_LOCAL_HEADER_SLACK);
    ULONGLONG header_size;
    ULONGLONG needed;
    BYTE* buffer;
    DWORD error;

    *block = NULL;
    *block_size = 0;
    *data_offset = 0;
    if (read_size < ZIP_FORMAT_LOCAL_HEADER_SIZE) {
        return zip_bad_format();
    }
    if (read_size > (ULONGLONG)((SIZE_T)-1)) {
        SetLastError(ERROR_FILE_TOO_LARGE);
        return FALSE;
    }

    if (use_input_buffer) {
        if (archive->input_capacity < (SIZE_T)read_size) {
            /* Содержимое прежнего буфера не нужно: выделяем заново. */
            zip_heap_free(archive->input_buffer);
            archive->input_buffer = NULL;
            archive->input_capacity = 0;
            archive->input_buffer = (BYTE*)zip_heap_alloc((SIZE_T)read_size);
            if (archive->input_buffer == NULL) {
                SetLastError(ERROR_OUTOFMEMORY);
                return FALSE;
            }
            archive->input_capacity = (SIZE_T)read_size;
        }
        buffer = archive->input_buffer;
    } else {
        buffer = (BYTE*)zip_heap_alloc((SIZE_T)read_size);
        if (buffer == NULL) {
            SetLastError(ERROR_OUTOFMEMORY);
            return FALSE;
        }
    }
    if (!zip_read_exact_at(
            archive,
            entry->local_header_offset,
            buffer,
            (SIZE_T)read_size)) {
        goto failure;
    }
    if (ZipFormat_ParseLocalHeader(
            buffer,
            (SIZE_T)read_size,
            entry->compression_method,
            entry->local_header_offset,
            entry->compressed_size,
            archive->file_size,
            &header_size) != ZIP_FORMAT_OK) {
        SetLastError(ERROR_BAD_FORMAT);
        goto failure;
    }

    /* Границы данных в файле проверены ParseLocalHeader: сумма не переполняется. */
    needed = header_size + data_bytes;
    if (needed > read_size) {
        BYTE* grown;

        if (needed > (ULONGLONG)((SIZE_T)-1)) {
            SetLastError(ERROR_FILE_TOO_LARGE);
            goto failure;
        }
        if (use_input_buffer && archive->input_capacity >= (SIZE_T)needed) {
            grown = buffer;
        } else {
            grown = (BYTE*)HeapReAlloc(GetProcessHeap(), 0, buffer, (SIZE_T)needed);
            if (grown == NULL) {
                SetLastError(ERROR_OUTOFMEMORY);
                goto failure;
            }
            if (use_input_buffer) {
                archive->input_buffer = grown;
                archive->input_capacity = (SIZE_T)needed;
            }
        }
        buffer = grown;
        if (!zip_read_exact_at(
                archive,
                entry->local_header_offset + read_size,
                buffer + (SIZE_T)read_size,
                (SIZE_T)(needed - read_size))) {
            goto failure;
        }
        read_size = needed;
    }

    *block = buffer;
    *block_size = (SIZE_T)read_size;
    *data_offset = (SIZE_T)header_size;
    return TRUE;

failure:
    error = GetLastError();
    if (!use_input_buffer) {
        zip_heap_free(buffer);
    }
    SetLastError(error == ERROR_SUCCESS ? ERROR_BAD_FORMAT : error);
    return FALSE;
}

static BOOL zip_extract_entry(
    ZipArchive* archive,
    const ZipEntry* entry,
    ULONGLONG max_output_bytes,
    BYTE** data,
    SIZE_T* size);

BOOL ZipArchive_ExtractEntry(
    ZipArchive* archive,
    const ZipEntry* entry,
    ULONGLONG max_output_bytes,
    BYTE** data,
    SIZE_T* size) {
    if (data == NULL || size == NULL) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    *data = NULL;
    *size = 0;

    if (!ZipArchive_IsOpen(archive) || entry == NULL ||
        !zip_entry_belongs_to_archive(archive, entry)) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    return zip_extract_entry(archive, entry, max_output_bytes, data, size);
}

BOOL ZipArchive_DuplicateFile(const ZipArchive* archive, HANDLE* file) {
    HANDLE process = GetCurrentProcess();

    if (file == NULL) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    *file = INVALID_HANDLE_VALUE;
    if (!ZipArchive_IsOpen(archive)) {
        SetLastError(ERROR_INVALID_HANDLE);
        return FALSE;
    }
    if (!DuplicateHandle(process, archive->file, process, file, 0, FALSE, DUPLICATE_SAME_ACCESS)) {
        *file = INVALID_HANDLE_VALUE;
        return FALSE;
    }
    return TRUE;
}

void ZipReadScratch_Init(ZipReadScratch* scratch) {
    if (scratch == NULL) {
        return;
    }
    scratch->input_buffer = NULL;
    scratch->input_capacity = 0;
    ZeroMemory(&scratch->io, sizeof(scratch->io));
}

void ZipReadScratch_Free(ZipReadScratch* scratch) {
    if (scratch == NULL) {
        return;
    }
    zip_heap_free(scratch->input_buffer);
    ZipReadScratch_Init(scratch);
}

BOOL ZipArchive_ExtractEntryFromFile(
    HANDLE file,
    ULONGLONG file_size,
    const ZipEntry* entry,
    ULONGLONG max_output_bytes,
    ZipReadScratch* scratch,
    BYTE** data,
    SIZE_T* size) {
    ZipArchive reader;
    BOOL extracted;
    DWORD error;

    if (data == NULL || size == NULL) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    *data = NULL;
    *size = 0;
    if (file == NULL || file == INVALID_HANDLE_VALUE || entry == NULL || scratch == NULL) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    /*
     * Временный «архив» без записей поверх чужого хэндла: только для
     * позиционного чтения. Close для него не вызывается — хэндлом и буфером
     * владеют вызывающий и scratch.
     */
    ZipArchive_Init(&reader);
    reader.file = file;
    reader.file_size = file_size;
    reader.input_buffer = scratch->input_buffer;
    reader.input_capacity = scratch->input_capacity;
    reader.io = scratch->io;
    extracted = zip_extract_entry(&reader, entry, max_output_bytes, data, size);
    error = GetLastError();
    scratch->input_buffer = reader.input_buffer;
    scratch->input_capacity = reader.input_capacity;
    scratch->io = reader.io;
    SetLastError(error);
    return extracted;
}

static BOOL zip_extract_entry(
    ZipArchive* archive,
    const ZipEntry* entry,
    ULONGLONG max_output_bytes,
    BYTE** data,
    SIZE_T* size) {
    BYTE* block = NULL;
    SIZE_T block_size = 0;
    SIZE_T data_offset = 0;
    DWORD error;

    if ((entry->flags & ZIP_FLAG_ENCRYPTED) != 0 ||
        (entry->compression_method != ZIP_METHOD_STORED &&
            entry->compression_method != ZIP_METHOD_DEFLATE)) {
        SetLastError(ERROR_NOT_SUPPORTED);
        return FALSE;
    }

    if (max_output_bytes == 0) {
        max_output_bytes = ZIP_DEFAULT_MAX_EXTRACTED_SIZE;
    }
    if (entry->uncompressed_size > max_output_bytes ||
        entry->uncompressed_size > (ULONGLONG)((SIZE_T)-1) - ZIP_BUFFER_TAG_SIZE) {
        SetLastError(ERROR_FILE_TOO_LARGE);
        return FALSE;
    }

    if (entry->compression_method == ZIP_METHOD_STORED) {
        if (entry->compressed_size != entry->uncompressed_size) {
            return zip_bad_format();
        }
        /* Заголовок и данные — одним чтением в итоговый блок. */
        if (!zip_read_entry_head(
                archive,
                entry,
                entry->compressed_size,
                FALSE,
                &block,
                &block_size,
                &data_offset)) {
            return FALSE;
        }
        if (entry->uncompressed_size == 0) {
            zip_heap_free(block);
            return TRUE;
        }
        /* data_offset >= 30: метка помещается в уже разобранный local header. */
        *data = zip_tag_buffer(block, data_offset);
        *size = (SIZE_T)entry->uncompressed_size;
        return TRUE;
    }
    else {
        /*
         * Вход до порога — целиком одним чтением; больше — первая порция
         * ZIP_INFLATE_CHUNK_SIZE, и этот же блок служит буфером следующих.
         */
        ULONGLONG head_data = entry->compressed_size <= ZIP_INFLATE_WHOLE_INPUT_LIMIT
            ? entry->compressed_size
            : ZIP_INFLATE_CHUNK_SIZE;
        ULONGLONG first_size;
        BYTE* output_block = NULL;
        BYTE* output = NULL;

        /* Вход читается в переиспользуемый буфер архива: его не освобождаем. */
        if (!zip_read_entry_head(
                archive,
                entry,
                head_data,
                TRUE,
                &block,
                &block_size,
                &data_offset)) {
            return FALSE;
        }
        first_size = (ULONGLONG)(block_size - data_offset);
        if (first_size > entry->compressed_size) {
            first_size = entry->compressed_size;
        }

        if (entry->uncompressed_size != 0) {
            output_block = (BYTE*)zip_heap_alloc(
                (SIZE_T)entry->uncompressed_size + ZIP_BUFFER_TAG_SIZE);
            if (output_block == NULL) {
                SetLastError(ERROR_OUTOFMEMORY);
                return FALSE;
            }
            output = output_block + ZIP_BUFFER_TAG_SIZE;
        }

        if (!zip_inflate_entry(
                archive,
                entry,
                entry->local_header_offset + data_offset,
                block,
                block_size,
                block + data_offset,
                (SIZE_T)first_size,
                output)) {
            error = GetLastError();
            zip_heap_free(output_block);
            SetLastError(error == ERROR_SUCCESS ? ERROR_BAD_FORMAT : error);
            return FALSE;
        }

        if (output_block != NULL) {
            *data = zip_tag_buffer(output_block, ZIP_BUFFER_TAG_SIZE);
            *size = (SIZE_T)entry->uncompressed_size;
        }
        return TRUE;
    }
}

void ZipArchive_FreeBuffer(void* data) {
    BYTE* bytes = (BYTE*)data;
    const BYTE* tag;
    DWORD data_offset;

    if (bytes == NULL) {
        return;
    }
    tag = bytes - ZIP_BUFFER_TAG_SIZE;
    data_offset = zip_read_u32(tag + 4);
    if (zip_read_u32(tag) == ZIP_BUFFER_TAG_MAGIC &&
        zip_read_u32(tag + 8) == ~(DWORD)ZIP_BUFFER_TAG_MAGIC &&
        zip_read_u32(tag + 12) == (data_offset ^ (DWORD)ZIP_BUFFER_TAG_MAGIC) &&
        data_offset >= ZIP_BUFFER_TAG_SIZE) {
        zip_heap_free(bytes - data_offset);
        return;
    }
    /* Буфер без метки (не из ExtractEntry) освобождается как есть. */
    zip_heap_free(data);
}

void ZipArchive_GetIoStats(const ZipArchive* archive, ZipIoStats* stats) {
    if (stats == NULL) {
        return;
    }
    if (archive == NULL || archive->state_cookie != ZIP_STATE_COOKIE) {
        ZeroMemory(stats, sizeof(*stats));
        return;
    }
    *stats = archive->io;
}

void ZipArchive_GetOpenStats(const ZipArchive* archive, ZipOpenStats* stats) {
    if (stats == NULL) {
        return;
    }
    if (archive == NULL || archive->state_cookie != ZIP_STATE_COOKIE) {
        ZeroMemory(stats, sizeof(*stats));
        return;
    }
    *stats = archive->open_stats;
}
