#pragma once

#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * ZIP-архив читается только через WinAPI и хранит всю свою память в process heap.
 * Структуру ZipArchive перед первым использованием необходимо инициализировать
 * через ZipArchive_Init. Объект не является потокобезопасным.
 */

#define ZIP_DEFAULT_MAX_EXTRACTED_SIZE (512ULL * 1024ULL * 1024ULL)

typedef struct ZipEntry {
    WCHAR* name;
    ULONGLONG compressed_size;
    ULONGLONG uncompressed_size;
    ULONGLONG local_header_offset;
    WORD compression_method;
    WORD flags;
    /* Длина исходного имени в байтах: из неё планируется чтение local header. */
    WORD name_size;
} ZipEntry;

/* Накопительные счётчики файлового I/O: каждый ReadFile и прочитанные байты. */
typedef struct ZipIoStats {
    ULONGLONG read_calls;
    ULONGLONG read_bytes;
} ZipIoStats;

/*
 * Разбивка открытия архива по фазам (тики QueryPerformanceCounter):
 * tail — чтение хвоста с EOCD; directory — поиск/проверка EOCD и разбор
 * central directory (включая дочитывания).
 */
typedef struct ZipOpenStats {
    ZipIoStats tail_io;
    ZipIoStats directory_io;
    ULONGLONG tail_ticks;
    ULONGLONG directory_ticks;
} ZipOpenStats;

typedef struct ZipArchive {
    HANDLE file;
    WCHAR* path;
    ZipEntry* entries;
    SIZE_T entry_count;
    SIZE_T entry_capacity;
    ULONGLONG file_size;
    ULONGLONG central_directory_offset;
    ULONGLONG central_directory_size;
    /* Время изменения на момент открытия: проверка, что файл не подменён. */
    FILETIME last_write_time;
    BOOL has_last_write_time;
    /*
     * Переиспользуемый буфер сжатого входа deflate: без него каждая запись
     * получала бы свежие страницы (VirtualAlloc + page faults) размером с вход.
     */
    BYTE* input_buffer;
    SIZE_T input_capacity;
    ZipIoStats io;
    ZipOpenStats open_stats;
    DWORD state_cookie;
} ZipArchive;

/* Жизненный цикл архива. */
void ZipArchive_Init(ZipArchive* archive);
BOOL ZipArchive_Open(ZipArchive* archive, LPCWSTR path);
/*
 * Как ZipArchive_Open, но с явными флагами CreateFileW (FILE_FLAG_*),
 * которые добавляются к FILE_ATTRIBUTE_NORMAL. ZipArchive_DefaultOpenFlags —
 * флаги, которые использует ZipArchive_Open. Нужен для A/B-замеров флагов.
 */
BOOL ZipArchive_OpenEx(ZipArchive* archive, LPCWSTR path, DWORD file_flags);
DWORD ZipArchive_DefaultOpenFlags(void);
void ZipArchive_Close(ZipArchive* archive);
BOOL ZipArchive_IsOpen(const ZipArchive* archive);
LPCWSTR ZipArchive_Path(const ZipArchive* archive);
/*
 * TRUE, если размер и время изменения файла по открытому хэндлу совпадают с
 * зафиксированными при открытии: разобранный central directory можно
 * переиспользовать вместо повторного открытия того же архива.
 */
BOOL ZipArchive_IsUnchanged(const ZipArchive* archive);

/* Записи принадлежат архиву и становятся недействительными после Close/Open. */
SIZE_T ZipArchive_EntryCount(const ZipArchive* archive);
const ZipEntry* ZipArchive_EntryAt(const ZipArchive* archive, SIZE_T index);
LPCWSTR ZipEntry_Name(const ZipEntry* entry);

/*
 * Извлекает entry в process heap. При max_output_bytes == 0 применяется
 * ZIP_DEFAULT_MAX_EXTRACTED_SIZE. Для пустого файла data == NULL допустим.
 * Запись читается одним ReadFile от local header (заголовок, имя, запас под
 * extra и данные); deflate распаковывается из памяти, вход больше 64 МиБ —
 * кусками по 8 МиБ. data может указывать внутрь блока кучи, поэтому буфер
 * освобождается только через ZipArchive_FreeBuffer.
 */
BOOL ZipArchive_ExtractEntry(
    ZipArchive* archive,
    const ZipEntry* entry,
    ULONGLONG max_output_bytes,
    BYTE** data,
    SIZE_T* size);
void ZipArchive_FreeBuffer(void* data);

/*
 * Чтение записей из другого потока (фоновый декод вьювера). Поток
 * получает собственный дубликат хэндла открытого архива
 * (ZipArchive_DuplicateFile): он ссылается на тот же file object, поэтому
 * не стоит сетевого открытия, живёт независимо от ZipArchive_Close и сохраняет
 * гарантию FILE_SHARE_READ (файл не меняется, пока открыт хотя бы один
 * хэндл). Закрывается вызывающим (CloseHandle).
 */
BOOL ZipArchive_DuplicateFile(const ZipArchive* archive, HANDLE* file);

/* Переиспользуемый буфер входа deflate и счётчики I/O одного потока. */
typedef struct ZipReadScratch {
    BYTE* input_buffer;
    SIZE_T input_capacity;
    ZipIoStats io;
} ZipReadScratch;

void ZipReadScratch_Init(ZipReadScratch* scratch);
void ZipReadScratch_Free(ZipReadScratch* scratch);

/*
 * Как ZipArchive_ExtractEntry, но по хэндлу file размера file_size и копии
 * записи (имя не используется; entry->name может быть NULL). Принадлежность
 * записи архиву не проверяется: границы данных сверяются с file_size при
 * разборе local header. Буфер освобождается через ZipArchive_FreeBuffer.
 */
BOOL ZipArchive_ExtractEntryFromFile(
    HANDLE file,
    ULONGLONG file_size,
    const ZipEntry* entry,
    ULONGLONG max_output_bytes,
    ZipReadScratch* scratch,
    BYTE** data,
    SIZE_T* size);

/* Счётчики I/O с момента открытия и разбивка фаз открытия. */
void ZipArchive_GetIoStats(const ZipArchive* archive, ZipIoStats* stats);
void ZipArchive_GetOpenStats(const ZipArchive* archive, ZipOpenStats* stats);

#ifdef __cplusplus
}
#endif
