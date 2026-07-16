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
    DWORD crc32;
    WORD compression_method;
    WORD flags;
    BOOL is_directory;
} ZipEntry;

typedef struct ZipArchive {
    HANDLE file;
    WCHAR* path;
    ZipEntry* entries;
    SIZE_T entry_count;
    SIZE_T entry_capacity;
    ULONGLONG file_size;
    ULONGLONG central_directory_offset;
    ULONGLONG central_directory_size;
    DWORD state_cookie;
} ZipArchive;

/* Жизненный цикл архива. */
void ZipArchive_Init(ZipArchive* archive);
BOOL ZipArchive_Open(ZipArchive* archive, LPCWSTR path);
void ZipArchive_Close(ZipArchive* archive);
BOOL ZipArchive_IsOpen(const ZipArchive* archive);
LPCWSTR ZipArchive_Path(const ZipArchive* archive);

/* Записи принадлежат архиву и становятся недействительными после Close/Open. */
SIZE_T ZipArchive_EntryCount(const ZipArchive* archive);
const ZipEntry* ZipArchive_EntryAt(const ZipArchive* archive, SIZE_T index);
const ZipEntry* ZipArchive_FindEntry(const ZipArchive* archive, LPCWSTR name);
LPCWSTR ZipEntry_Name(const ZipEntry* entry);

/*
 * Возвращает отсортированный массив указателей на изображения. Сам массив надо
 * освободить через ZipArchive_FreeEntryList, записи остаются собственностью архива.
 */
BOOL ZipArchive_GetImageEntries(
    const ZipArchive* archive,
    const WCHAR* const* extensions,
    SIZE_T extension_count,
    const ZipEntry*** out_entries,
    SIZE_T* out_count);
void ZipArchive_FreeEntryList(const ZipEntry** entries);

/*
 * Извлекает entry в process heap. При max_output_bytes == 0 применяется
 * ZIP_DEFAULT_MAX_EXTRACTED_SIZE. Для пустого файла data == NULL допустим.
 * Полученный буфер освобождается через ZipArchive_FreeBuffer.
 */
BOOL ZipArchive_ExtractEntry(
    ZipArchive* archive,
    const ZipEntry* entry,
    ULONGLONG max_output_bytes,
    BYTE** data,
    SIZE_T* size);
BOOL ZipArchive_ExtractByName(
    ZipArchive* archive,
    LPCWSTR name,
    ULONGLONG max_output_bytes,
    BYTE** data,
    SIZE_T* size);
void ZipArchive_FreeBuffer(void* data);

#ifdef __cplusplus
}
#endif
