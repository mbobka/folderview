#include "ZipArchive.h"

#include <zlib.h>

#define ZIP_STATE_COOKIE 0x5A495031UL

#define ZIP_LOCAL_FILE_HEADER_SIGNATURE 0x04034B50UL
#define ZIP_CENTRAL_DIRECTORY_SIGNATURE 0x02014B50UL
#define ZIP_END_OF_CENTRAL_DIRECTORY_SIGNATURE 0x06054B50UL
#define ZIP64_END_OF_CENTRAL_DIRECTORY_SIGNATURE 0x06064B50UL
#define ZIP64_END_OF_CENTRAL_DIRECTORY_LOCATOR_SIGNATURE 0x07064B50UL

#define ZIP_END_OF_CENTRAL_DIRECTORY_SIZE 22U
#define ZIP64_END_OF_CENTRAL_DIRECTORY_SIZE 56U
#define ZIP64_END_OF_CENTRAL_DIRECTORY_LOCATOR_SIZE 20U
#define ZIP_CENTRAL_DIRECTORY_HEADER_SIZE 46U
#define ZIP_LOCAL_FILE_HEADER_SIZE 30U
#define ZIP_MAX_COMMENT_SIZE 65535U
#define ZIP_IO_CHUNK_SIZE (64U * 1024U)

#define ZIP64_EXTRA_FIELD_TAG 0x0001U
#define ZIP_UNICODE_PATH_EXTRA_FIELD_TAG 0x7075U

#define ZIP_METHOD_STORED 0U
#define ZIP_METHOD_DEFLATE 8U

#define ZIP_FLAG_ENCRYPTED 0x0001U
#define ZIP_FLAG_UTF8 0x0800U

static WORD zip_read_u16(const BYTE* data) {
    return (WORD)((WORD)data[0] | ((WORD)data[1] << 8));
}

static DWORD zip_read_u32(const BYTE* data) {
    return (DWORD)data[0] |
        ((DWORD)data[1] << 8) |
        ((DWORD)data[2] << 16) |
        ((DWORD)data[3] << 24);
}

static ULONGLONG zip_read_u64(const BYTE* data) {
    return (ULONGLONG)zip_read_u32(data) |
        ((ULONGLONG)zip_read_u32(data + 4) << 32);
}

static BOOL zip_add_u64(ULONGLONG left, ULONGLONG right, ULONGLONG* result) {
    if (left > ~(ULONGLONG)0 - right) {
        return FALSE;
    }
    *result = left + right;
    return TRUE;
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

static BOOL zip_read_exact_at(
    ZipArchive* archive,
    ULONGLONG offset,
    void* destination,
    SIZE_T size) {
    LARGE_INTEGER position;
    BYTE* output = (BYTE*)destination;
    ULONGLONG end_offset;

    if (archive == NULL || destination == NULL ||
        !zip_add_u64(offset, (ULONGLONG)size, &end_offset) ||
        end_offset > archive->file_size) {
        SetLastError(ERROR_HANDLE_EOF);
        return FALSE;
    }

    if (size == 0) {
        return TRUE;
    }

    position.QuadPart = (LONGLONG)offset;
    if (!SetFilePointerEx(archive->file, position, NULL, FILE_BEGIN)) {
        return FALSE;
    }

    while (size != 0) {
        DWORD requested = size > ZIP_IO_CHUNK_SIZE
            ? ZIP_IO_CHUNK_SIZE
            : (DWORD)size;
        DWORD received = 0;

        if (!ReadFile(archive->file, output, requested, &received, NULL)) {
            return FALSE;
        }
        if (received != requested) {
            SetLastError(ERROR_HANDLE_EOF);
            return FALSE;
        }

        output += received;
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

            if (name_size == 0 || name_size > 0xFFFFU) {
                return FALSE;
            }
            *utf8_name = extra + field_offset + 5;
            *utf8_name_size = (WORD)name_size;
            return TRUE;
        }

        position = field_offset + field_size;
    }

    return position == extra_size;
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

    *is_directory = FALSE;
    if (name_size == 0) {
        SetLastError(ERROR_BAD_FORMAT);
        return NULL;
    }

    if ((flags & ZIP_FLAG_UTF8) != 0) {
        code_page = CP_UTF8;
        conversion_flags = MB_ERR_INVALID_CHARS;
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
        }
    }

    wide_count = MultiByteToWideChar(
        code_page,
        conversion_flags,
        (LPCCH)bytes,
        (int)byte_count,
        NULL,
        0);
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

static BOOL zip_parse_zip64_extra(
    const BYTE* extra,
    WORD extra_size,
    DWORD compressed_size32,
    DWORD uncompressed_size32,
    DWORD local_offset32,
    WORD disk_start16,
    ULONGLONG* compressed_size,
    ULONGLONG* uncompressed_size,
    ULONGLONG* local_offset,
    DWORD* disk_start) {
    SIZE_T position = 0;
    BOOL needs_zip64 = compressed_size32 == 0xFFFFFFFFUL ||
        uncompressed_size32 == 0xFFFFFFFFUL ||
        local_offset32 == 0xFFFFFFFFUL ||
        disk_start16 == 0xFFFFU;

    *compressed_size = compressed_size32;
    *uncompressed_size = uncompressed_size32;
    *local_offset = local_offset32;
    *disk_start = disk_start16;

    if (!needs_zip64) {
        return TRUE;
    }

    while (position + 4 <= extra_size) {
        WORD tag = zip_read_u16(extra + position);
        WORD field_size = zip_read_u16(extra + position + 2);
        SIZE_T field_offset = position + 4;
        SIZE_T field_end;
        SIZE_T cursor;

        if ((SIZE_T)field_size > (SIZE_T)extra_size - field_offset) {
            return zip_bad_format();
        }
        field_end = field_offset + field_size;

        if (tag != ZIP64_EXTRA_FIELD_TAG) {
            position = field_end;
            continue;
        }

        cursor = field_offset;
        if (uncompressed_size32 == 0xFFFFFFFFUL) {
            if (field_end - cursor < 8) {
                return zip_bad_format();
            }
            *uncompressed_size = zip_read_u64(extra + cursor);
            cursor += 8;
        }
        if (compressed_size32 == 0xFFFFFFFFUL) {
            if (field_end - cursor < 8) {
                return zip_bad_format();
            }
            *compressed_size = zip_read_u64(extra + cursor);
            cursor += 8;
        }
        if (local_offset32 == 0xFFFFFFFFUL) {
            if (field_end - cursor < 8) {
                return zip_bad_format();
            }
            *local_offset = zip_read_u64(extra + cursor);
            cursor += 8;
        }
        if (disk_start16 == 0xFFFFU) {
            if (field_end - cursor < 4) {
                return zip_bad_format();
            }
            *disk_start = zip_read_u32(extra + cursor);
        }
        return TRUE;
    }

    return zip_bad_format();
}

static BOOL zip_grow_entries(ZipArchive* archive) {
    SIZE_T new_capacity;
    SIZE_T bytes;
    ZipEntry* new_entries;

    if (archive->entry_count < archive->entry_capacity) {
        return TRUE;
    }

    new_capacity = archive->entry_capacity == 0 ? 16 : archive->entry_capacity * 2;
    if (new_capacity < archive->entry_capacity ||
        new_capacity > ((SIZE_T)-1) / sizeof(ZipEntry)) {
        SetLastError(ERROR_OUTOFMEMORY);
        return FALSE;
    }
    bytes = new_capacity * sizeof(ZipEntry);

    if (archive->entries == NULL) {
        new_entries = (ZipEntry*)HeapAlloc(GetProcessHeap(), 0, bytes);
    }
    else {
        new_entries = (ZipEntry*)HeapReAlloc(
            GetProcessHeap(),
            0,
            archive->entries,
            bytes);
    }
    if (new_entries == NULL) {
        SetLastError(ERROR_OUTOFMEMORY);
        return FALSE;
    }

    archive->entries = new_entries;
    archive->entry_capacity = new_capacity;
    return TRUE;
}

static BOOL zip_append_entry(ZipArchive* archive, const ZipEntry* entry) {
    if (!zip_grow_entries(archive)) {
        return FALSE;
    }
    archive->entries[archive->entry_count] = *entry;
    ++archive->entry_count;
    return TRUE;
}

static BOOL zip_locate_central_directory(
    ZipArchive* archive,
    ULONGLONG* total_entries,
    ULONGLONG* directory_offset,
    ULONGLONG* directory_size) {
    SIZE_T tail_size;
    ULONGLONG tail_offset;
    BYTE* tail;
    SIZE_T position;
    SIZE_T eocd_position = (SIZE_T)-1;
    ULONGLONG eocd_offset;
    WORD disk_number;
    WORD central_disk;
    WORD entries_on_disk16;
    WORD total_entries16;
    DWORD directory_size32;
    DWORD directory_offset32;
    BOOL needs_zip64;
    BOOL has_zip64_locator = FALSE;
    BYTE locator[ZIP64_END_OF_CENTRAL_DIRECTORY_LOCATOR_SIZE];
    ULONGLONG directory_end;
    ULONGLONG directory_limit;

    if (archive->file_size < ZIP_END_OF_CENTRAL_DIRECTORY_SIZE) {
        return zip_bad_format();
    }

    tail_size = archive->file_size >
            (ULONGLONG)(ZIP_END_OF_CENTRAL_DIRECTORY_SIZE + ZIP_MAX_COMMENT_SIZE)
        ? ZIP_END_OF_CENTRAL_DIRECTORY_SIZE + ZIP_MAX_COMMENT_SIZE
        : (SIZE_T)archive->file_size;
    tail_offset = archive->file_size - tail_size;
    tail = (BYTE*)zip_heap_alloc(tail_size);
    if (tail == NULL) {
        SetLastError(ERROR_OUTOFMEMORY);
        return FALSE;
    }
    if (!zip_read_exact_at(archive, tail_offset, tail, tail_size)) {
        zip_heap_free(tail);
        return FALSE;
    }

    position = tail_size - ZIP_END_OF_CENTRAL_DIRECTORY_SIZE;
    for (;;) {
        if (zip_read_u32(tail + position) ==
                ZIP_END_OF_CENTRAL_DIRECTORY_SIGNATURE &&
            (SIZE_T)zip_read_u16(tail + position + 20) ==
                tail_size - position - ZIP_END_OF_CENTRAL_DIRECTORY_SIZE) {
            eocd_position = position;
            break;
        }
        if (position == 0) {
            break;
        }
        --position;
    }
    if (eocd_position == (SIZE_T)-1) {
        zip_heap_free(tail);
        return zip_bad_format();
    }

    eocd_offset = tail_offset + eocd_position;
    disk_number = zip_read_u16(tail + eocd_position + 4);
    central_disk = zip_read_u16(tail + eocd_position + 6);
    entries_on_disk16 = zip_read_u16(tail + eocd_position + 8);
    total_entries16 = zip_read_u16(tail + eocd_position + 10);
    directory_size32 = zip_read_u32(tail + eocd_position + 12);
    directory_offset32 = zip_read_u32(tail + eocd_position + 16);

    if (disk_number != 0 || central_disk != 0) {
        zip_heap_free(tail);
        SetLastError(ERROR_NOT_SUPPORTED);
        return FALSE;
    }

    needs_zip64 = entries_on_disk16 == 0xFFFFU ||
        total_entries16 == 0xFFFFU ||
        directory_size32 == 0xFFFFFFFFUL ||
        directory_offset32 == 0xFFFFFFFFUL;

    if (eocd_offset >= ZIP64_END_OF_CENTRAL_DIRECTORY_LOCATOR_SIZE &&
        zip_read_exact_at(
            archive,
            eocd_offset - ZIP64_END_OF_CENTRAL_DIRECTORY_LOCATOR_SIZE,
            locator,
            sizeof(locator)) &&
        zip_read_u32(locator) ==
            ZIP64_END_OF_CENTRAL_DIRECTORY_LOCATOR_SIGNATURE) {
        has_zip64_locator = TRUE;
    }

    if (has_zip64_locator) {
        BYTE eocd64[ZIP64_END_OF_CENTRAL_DIRECTORY_SIZE];
        ULONGLONG eocd64_offset = zip_read_u64(locator + 8);
        ULONGLONG eocd64_record_size;
        ULONGLONG eocd64_end;

        if (zip_read_u32(locator + 4) != 0 || zip_read_u32(locator + 16) != 1) {
            zip_heap_free(tail);
            SetLastError(ERROR_NOT_SUPPORTED);
            return FALSE;
        }
        if (!zip_read_exact_at(
                archive,
                eocd64_offset,
                eocd64,
                sizeof(eocd64))) {
            zip_heap_free(tail);
            return FALSE;
        }
        if (zip_read_u32(eocd64) !=
                ZIP64_END_OF_CENTRAL_DIRECTORY_SIGNATURE ||
            zip_read_u64(eocd64 + 4) < 44) {
            zip_heap_free(tail);
            return zip_bad_format();
        }

        eocd64_record_size = zip_read_u64(eocd64 + 4);
        if (!zip_add_u64(eocd64_offset, 12, &eocd64_end) ||
            !zip_add_u64(eocd64_end, eocd64_record_size, &eocd64_end) ||
            eocd64_end > eocd_offset -
                ZIP64_END_OF_CENTRAL_DIRECTORY_LOCATOR_SIZE) {
            zip_heap_free(tail);
            return zip_bad_format();
        }
        if (zip_read_u32(eocd64 + 16) != 0 ||
            zip_read_u32(eocd64 + 20) != 0 ||
            zip_read_u64(eocd64 + 24) != zip_read_u64(eocd64 + 32)) {
            zip_heap_free(tail);
            SetLastError(ERROR_NOT_SUPPORTED);
            return FALSE;
        }

        *total_entries = zip_read_u64(eocd64 + 32);
        *directory_size = zip_read_u64(eocd64 + 40);
        *directory_offset = zip_read_u64(eocd64 + 48);
        directory_limit = eocd64_offset;
    }
    else {
        if (needs_zip64) {
            zip_heap_free(tail);
            return zip_bad_format();
        }
        if (entries_on_disk16 != total_entries16) {
            zip_heap_free(tail);
            SetLastError(ERROR_NOT_SUPPORTED);
            return FALSE;
        }

        *total_entries = total_entries16;
        *directory_size = directory_size32;
        *directory_offset = directory_offset32;
        directory_limit = eocd_offset;
    }

    zip_heap_free(tail);

    if (!zip_add_u64(*directory_offset, *directory_size, &directory_end) ||
        directory_end > directory_limit ||
        *total_entries > *directory_size / ZIP_CENTRAL_DIRECTORY_HEADER_SIZE) {
        return zip_bad_format();
    }
    return TRUE;
}

static BOOL zip_read_central_directory(ZipArchive* archive) {
    ULONGLONG total_entries;
    ULONGLONG directory_offset;
    ULONGLONG directory_size;
    ULONGLONG directory_end;
    ULONGLONG cursor;
    ULONGLONG index;

    if (!zip_locate_central_directory(
            archive,
            &total_entries,
            &directory_offset,
            &directory_size)) {
        return FALSE;
    }

    archive->central_directory_offset = directory_offset;
    archive->central_directory_size = directory_size;
    directory_end = directory_offset + directory_size;
    cursor = directory_offset;

    for (index = 0; index < total_entries; ++index) {
        BYTE header[ZIP_CENTRAL_DIRECTORY_HEADER_SIZE];
        WORD flags;
        WORD method;
        WORD name_size;
        WORD extra_size;
        WORD comment_size;
        WORD disk_start16;
        DWORD compressed_size32;
        DWORD uncompressed_size32;
        DWORD local_offset32;
        ULONGLONG record_size;
        ULONGLONG record_end;
        BYTE* name = NULL;
        BYTE* extra = NULL;
        WCHAR* wide_name = NULL;
        BOOL is_directory = FALSE;
        ULONGLONG compressed_size;
        ULONGLONG uncompressed_size;
        ULONGLONG local_offset;
        DWORD disk_start;
        ZipEntry entry;

        if (directory_end - cursor < ZIP_CENTRAL_DIRECTORY_HEADER_SIZE ||
            !zip_read_exact_at(archive, cursor, header, sizeof(header)) ||
            zip_read_u32(header) != ZIP_CENTRAL_DIRECTORY_SIGNATURE) {
            return zip_bad_format();
        }

        flags = zip_read_u16(header + 8);
        method = zip_read_u16(header + 10);
        compressed_size32 = zip_read_u32(header + 20);
        uncompressed_size32 = zip_read_u32(header + 24);
        name_size = zip_read_u16(header + 28);
        extra_size = zip_read_u16(header + 30);
        comment_size = zip_read_u16(header + 32);
        disk_start16 = zip_read_u16(header + 34);
        local_offset32 = zip_read_u32(header + 42);

        record_size = ZIP_CENTRAL_DIRECTORY_HEADER_SIZE;
        if (!zip_add_u64(record_size, name_size, &record_size) ||
            !zip_add_u64(record_size, extra_size, &record_size) ||
            !zip_add_u64(record_size, comment_size, &record_size) ||
            !zip_add_u64(cursor, record_size, &record_end) ||
            record_end > directory_end ||
            name_size == 0) {
            return zip_bad_format();
        }

        name = (BYTE*)zip_heap_alloc(name_size);
        if (name == NULL) {
            SetLastError(ERROR_OUTOFMEMORY);
            return FALSE;
        }
        if (!zip_read_exact_at(
                archive,
                cursor + ZIP_CENTRAL_DIRECTORY_HEADER_SIZE,
                name,
                name_size)) {
            zip_heap_free(name);
            return FALSE;
        }

        if (extra_size != 0) {
            extra = (BYTE*)zip_heap_alloc(extra_size);
            if (extra == NULL) {
                zip_heap_free(name);
                SetLastError(ERROR_OUTOFMEMORY);
                return FALSE;
            }
            if (!zip_read_exact_at(
                    archive,
                    cursor + ZIP_CENTRAL_DIRECTORY_HEADER_SIZE + name_size,
                    extra,
                    extra_size)) {
                zip_heap_free(extra);
                zip_heap_free(name);
                return FALSE;
            }
        }

        if (!zip_parse_zip64_extra(
                extra,
                extra_size,
                compressed_size32,
                uncompressed_size32,
                local_offset32,
                disk_start16,
                &compressed_size,
                &uncompressed_size,
                &local_offset,
                &disk_start)) {
            zip_heap_free(extra);
            zip_heap_free(name);
            return FALSE;
        }
        if (disk_start != 0) {
            zip_heap_free(extra);
            zip_heap_free(name);
            SetLastError(ERROR_NOT_SUPPORTED);
            return FALSE;
        }

        wide_name = zip_decode_name(
            name,
            name_size,
            flags,
            extra,
            extra_size,
            &is_directory);
        zip_heap_free(extra);
        zip_heap_free(name);
        if (wide_name == NULL) {
            return FALSE;
        }

        if (!is_directory) {
            entry.name = wide_name;
            entry.compressed_size = compressed_size;
            entry.uncompressed_size = uncompressed_size;
            entry.local_header_offset = local_offset;
            entry.crc32 = zip_read_u32(header + 16);
            entry.compression_method = method;
            entry.flags = flags;
            entry.is_directory = FALSE;

            if (!zip_append_entry(archive, &entry)) {
                zip_heap_free(wide_name);
                return FALSE;
            }
        }
        else {
            zip_heap_free(wide_name);
        }

        cursor = record_end;
    }

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

    if (archive->file != NULL && archive->file != INVALID_HANDLE_VALUE) {
        CloseHandle(archive->file);
    }
    ZipArchive_Init(archive);
}

BOOL ZipArchive_Open(ZipArchive* archive, LPCWSTR path) {
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
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_RANDOM_ACCESS,
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

SIZE_T ZipArchive_EntryCount(const ZipArchive* archive) {
    return ZipArchive_IsOpen(archive) ? archive->entry_count : 0;
}

const ZipEntry* ZipArchive_EntryAt(const ZipArchive* archive, SIZE_T index) {
    if (!ZipArchive_IsOpen(archive) || index >= archive->entry_count) {
        return NULL;
    }
    return archive->entries + index;
}

const ZipEntry* ZipArchive_FindEntry(const ZipArchive* archive, LPCWSTR name) {
    SIZE_T index;

    if (!ZipArchive_IsOpen(archive) || name == NULL) {
        return NULL;
    }
    for (index = 0; index < archive->entry_count; ++index) {
        if (lstrcmpW(archive->entries[index].name, name) == 0) {
            return archive->entries + index;
        }
    }
    return NULL;
}

LPCWSTR ZipEntry_Name(const ZipEntry* entry) {
    return entry != NULL ? entry->name : NULL;
}

static BOOL zip_name_has_extension(
    LPCWSTR name,
    const WCHAR* const* extensions,
    SIZE_T extension_count) {
    LPCWSTR cursor = name;
    LPCWSTR extension = NULL;
    SIZE_T index;

    while (*cursor != L'\0') {
        if (*cursor == L'.') {
            extension = cursor;
        }
        else if (*cursor == L'\\') {
            extension = NULL;
        }
        ++cursor;
    }
    if (extension == NULL) {
        return FALSE;
    }

    for (index = 0; index < extension_count; ++index) {
        if (extensions[index] != NULL &&
            lstrcmpiW(extension, extensions[index]) == 0) {
            return TRUE;
        }
    }
    return FALSE;
}

static int zip_compare_entries(const ZipEntry* left, const ZipEntry* right) {
    int result = CompareStringOrdinal(
        left->name,
        -1,
        right->name,
        -1,
        TRUE);

    if (result == CSTR_LESS_THAN) {
        return -1;
    }
    if (result == CSTR_GREATER_THAN) {
        return 1;
    }
    return lstrcmpW(left->name, right->name);
}

static void zip_sort_entry_list(const ZipEntry** entries, SIZE_T count) {
    SIZE_T start;
    SIZE_T end;

    if (count < 2) {
        return;
    }

    /* Heap sort не требует ни CRT, ни дополнительной памяти. */
    for (start = count / 2; start != 0; --start) {
        SIZE_T root = start - 1;

        while (root <= (count - 2) / 2) {
            SIZE_T child = root * 2 + 1;
            const ZipEntry* value;

            if (child + 1 < count &&
                zip_compare_entries(entries[child], entries[child + 1]) < 0) {
                ++child;
            }
            if (zip_compare_entries(entries[root], entries[child]) >= 0) {
                break;
            }
            value = entries[root];
            entries[root] = entries[child];
            entries[child] = value;
            root = child;
        }
    }

    for (end = count; end > 1; --end) {
        SIZE_T root = 0;
        SIZE_T heap_count = end - 1;
        const ZipEntry* value = entries[0];

        entries[0] = entries[heap_count];
        entries[heap_count] = value;

        while (heap_count > 1 && root <= (heap_count - 2) / 2) {
            SIZE_T child = root * 2 + 1;

            if (child + 1 < heap_count &&
                zip_compare_entries(entries[child], entries[child + 1]) < 0) {
                ++child;
            }
            if (zip_compare_entries(entries[root], entries[child]) >= 0) {
                break;
            }
            value = entries[root];
            entries[root] = entries[child];
            entries[child] = value;
            root = child;
        }
    }
}

BOOL ZipArchive_GetImageEntries(
    const ZipArchive* archive,
    const WCHAR* const* extensions,
    SIZE_T extension_count,
    const ZipEntry*** out_entries,
    SIZE_T* out_count) {
    SIZE_T count = 0;
    SIZE_T index;
    const ZipEntry** result;

    if (out_entries == NULL || out_count == NULL ||
        (extension_count != 0 && extensions == NULL)) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    *out_entries = NULL;
    *out_count = 0;

    if (!ZipArchive_IsOpen(archive)) {
        SetLastError(ERROR_INVALID_HANDLE);
        return FALSE;
    }

    for (index = 0; index < archive->entry_count; ++index) {
        if (zip_name_has_extension(
                archive->entries[index].name,
                extensions,
                extension_count)) {
            ++count;
        }
    }
    if (count == 0) {
        return TRUE;
    }
    if (count > ((SIZE_T)-1) / sizeof(*result)) {
        SetLastError(ERROR_OUTOFMEMORY);
        return FALSE;
    }

    result = (const ZipEntry**)zip_heap_alloc(count * sizeof(*result));
    if (result == NULL) {
        SetLastError(ERROR_OUTOFMEMORY);
        return FALSE;
    }
    count = 0;
    for (index = 0; index < archive->entry_count; ++index) {
        if (zip_name_has_extension(
                archive->entries[index].name,
                extensions,
                extension_count)) {
            result[count++] = archive->entries + index;
        }
    }

    zip_sort_entry_list(result, count);
    *out_entries = result;
    *out_count = count;
    return TRUE;
}

void ZipArchive_FreeEntryList(const ZipEntry** entries) {
    zip_heap_free((void*)entries);
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

static BOOL zip_entry_data_offset(
    ZipArchive* archive,
    const ZipEntry* entry,
    ULONGLONG* data_offset) {
    BYTE header[ZIP_LOCAL_FILE_HEADER_SIZE];
    ULONGLONG offset;
    ULONGLONG end_offset;

    if (!zip_read_exact_at(
            archive,
            entry->local_header_offset,
            header,
            sizeof(header)) ||
        zip_read_u32(header) != ZIP_LOCAL_FILE_HEADER_SIGNATURE ||
        zip_read_u16(header + 8) != entry->compression_method ||
        (zip_read_u16(header + 6) & ZIP_FLAG_ENCRYPTED) != 0) {
        return zip_bad_format();
    }

    if (!zip_add_u64(
            entry->local_header_offset,
            ZIP_LOCAL_FILE_HEADER_SIZE,
            &offset) ||
        !zip_add_u64(offset, zip_read_u16(header + 26), &offset) ||
        !zip_add_u64(offset, zip_read_u16(header + 28), &offset) ||
        !zip_add_u64(offset, entry->compressed_size, &end_offset) ||
        end_offset > archive->file_size) {
        return zip_bad_format();
    }

    *data_offset = offset;
    return TRUE;
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

static BOOL zip_inflate_entry(
    ZipArchive* archive,
    ULONGLONG data_offset,
    const ZipEntry* entry,
    BYTE* output) {
    BYTE* input;
    BYTE overflow_byte;
    z_stream stream;
    ULONGLONG input_loaded = 0;
    ULONGLONG input_consumed = 0;
    ULONGLONG output_produced = 0;
    BOOL using_overflow_byte = FALSE;
    int status;
    BOOL success = FALSE;

    input = (BYTE*)zip_heap_alloc(ZIP_IO_CHUNK_SIZE);
    if (input == NULL) {
        SetLastError(ERROR_OUTOFMEMORY);
        return FALSE;
    }

    ZeroMemory(&stream, sizeof(stream));
    stream.zalloc = zip_zalloc;
    stream.zfree = zip_zfree;
    stream.opaque = GetProcessHeap();
    status = inflateInit2(&stream, -MAX_WBITS);
    if (status != Z_OK) {
        zip_heap_free(input);
        SetLastError(status == Z_MEM_ERROR ? ERROR_OUTOFMEMORY : ERROR_BAD_FORMAT);
        return FALSE;
    }

    for (;;) {
        uInt input_before;
        uInt output_before;
        ULONGLONG input_remaining;
        ULONGLONG output_remaining;

        if (stream.avail_in == 0 && input_loaded < entry->compressed_size) {
            DWORD chunk;

            input_remaining = entry->compressed_size - input_loaded;
            chunk = input_remaining > ZIP_IO_CHUNK_SIZE
                ? ZIP_IO_CHUNK_SIZE
                : (DWORD)input_remaining;
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
    zip_heap_free(input);
    return success;
}

BOOL ZipArchive_ExtractEntry(
    ZipArchive* archive,
    const ZipEntry* entry,
    ULONGLONG max_output_bytes,
    BYTE** data,
    SIZE_T* size) {
    ULONGLONG data_offset;
    BYTE* output = NULL;
    DWORD error;

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
        entry->uncompressed_size > (ULONGLONG)((SIZE_T)-1)) {
        SetLastError(ERROR_FILE_TOO_LARGE);
        return FALSE;
    }
    if (!zip_entry_data_offset(archive, entry, &data_offset)) {
        return FALSE;
    }

    if (entry->uncompressed_size != 0) {
        output = (BYTE*)zip_heap_alloc((SIZE_T)entry->uncompressed_size);
        if (output == NULL) {
            SetLastError(ERROR_OUTOFMEMORY);
            return FALSE;
        }
    }

    if (entry->compression_method == ZIP_METHOD_STORED) {
        if (entry->compressed_size != entry->uncompressed_size ||
            (entry->uncompressed_size != 0 &&
                !zip_read_exact_at(
                    archive,
                    data_offset,
                    output,
                    (SIZE_T)entry->uncompressed_size))) {
            error = GetLastError();
            zip_heap_free(output);
            SetLastError(error == ERROR_SUCCESS ? ERROR_BAD_FORMAT : error);
            return FALSE;
        }
    }
    else if (!zip_inflate_entry(archive, data_offset, entry, output)) {
        error = GetLastError();
        zip_heap_free(output);
        SetLastError(error);
        return FALSE;
    }

    if (zip_crc32_bytes(output, (SIZE_T)entry->uncompressed_size) != entry->crc32) {
        zip_heap_free(output);
        SetLastError(ERROR_CRC);
        return FALSE;
    }

    *data = output;
    *size = (SIZE_T)entry->uncompressed_size;
    return TRUE;
}

BOOL ZipArchive_ExtractByName(
    ZipArchive* archive,
    LPCWSTR name,
    ULONGLONG max_output_bytes,
    BYTE** data,
    SIZE_T* size) {
    const ZipEntry* entry = ZipArchive_FindEntry(archive, name);

    if (entry == NULL) {
        if (data != NULL) {
            *data = NULL;
        }
        if (size != NULL) {
            *size = 0;
        }
        SetLastError(ERROR_FILE_NOT_FOUND);
        return FALSE;
    }
    return ZipArchive_ExtractEntry(
        archive,
        entry,
        max_output_bytes,
        data,
        size);
}

void ZipArchive_FreeBuffer(void* data) {
    zip_heap_free(data);
}
