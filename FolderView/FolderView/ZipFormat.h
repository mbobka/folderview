#ifndef FOLDERVIEW_ZIP_FORMAT_H
#define FOLDERVIEW_ZIP_FORMAT_H

#include <stddef.h>
#include <stdint.h>

/*
 * Платформонезависимый разбор структур ZIP из памяти: EOCD/ZIP64 EOCD,
 * записи central directory, local file header. Без WinAPI, CRT и аллокаций —
 * проверяется нативными C-регрессиями (tests/ZipFormatTests.c) до
 * Windows-сборки. Все размеры и смещения проверяются на переполнение.
 */

#define ZIP_FORMAT_EOCD_SIZE 22U
#define ZIP_FORMAT_MAX_COMMENT_SIZE 65535U
/* EOCD ищется только в последних EOCD + максимальный комментарий байтах. */
#define ZIP_FORMAT_EOCD_SEARCH_SIZE (ZIP_FORMAT_EOCD_SIZE + ZIP_FORMAT_MAX_COMMENT_SIZE)
#define ZIP_FORMAT_ZIP64_LOCATOR_SIZE 20U
#define ZIP_FORMAT_ZIP64_EOCD_SIZE 56U
#define ZIP_FORMAT_CENTRAL_HEADER_SIZE 46U
#define ZIP_FORMAT_LOCAL_HEADER_SIZE 30U

typedef enum ZipFormatStatus {
    ZIP_FORMAT_OK = 0,
    ZIP_FORMAT_BAD_FORMAT = 1,
    ZIP_FORMAT_UNSUPPORTED = 2,
    /* Нужны байты файла левее окна: см. required_offset. */
    ZIP_FORMAT_NEED_MORE = 3
} ZipFormatStatus;

/*
 * Окно — суффикс файла в памяти: data[0] — байт файла по смещению offset,
 * offset + size == file_size. Окно растёт только влево (дочитыванием).
 */
typedef struct ZipFormatWindow {
    const uint8_t* data;
    uint64_t offset;
    size_t size;
    uint64_t file_size;
} ZipFormatWindow;

typedef struct ZipFormatDirectory {
    uint64_t total_entries;
    uint64_t offset;
    uint64_t size;
} ZipFormatDirectory;

typedef struct ZipFormatCentralEntry {
    uint16_t flags;
    uint16_t method;
    uint64_t compressed_size;
    uint64_t uncompressed_size;
    uint64_t local_header_offset;
    const uint8_t* name;
    uint16_t name_size;
    const uint8_t* extra;
    uint16_t extra_size;
    /* Полная длина записи: заголовок + имя + extra + комментарий. */
    uint64_t record_size;
} ZipFormatCentralEntry;

/*
 * Дополнительная проверка кандидата EOCD вызывающим кодом (например,
 * декодирование имён). Вызывается только после успешной структурной
 * проверки, когда весь central directory кандидата уже лежит в окне.
 * Возвращает ненулевое значение, если кандидат принят.
 */
typedef int (*ZipFormatValidateFn)(
    void* context,
    const ZipFormatWindow* window,
    const ZipFormatDirectory* directory);

/*
 * Разбирает одну запись central directory, начинающуюся в record; available —
 * сколько байт осталось до конца central directory. Учитывает ZIP64 extra.
 * UNSUPPORTED — запись на другом диске (многотомные архивы).
 */
ZipFormatStatus ZipFormat_ReadCentralEntry(
    const uint8_t* record,
    uint64_t available,
    ZipFormatCentralEntry* entry);

/*
 * Ищет лучший EOCD-кандидат (как исходный FolderView: приоритет точной
 * границы central directory и точной длины комментария), проверяет
 * ZIP64 locator/EOCD и полностью проходит central directory кандидата по
 * окну. При NEED_MORE в *required_offset — наименьшее смещение файла,
 * которое нужно добавить к окну; затем вызов повторяется с расширенным окном.
 */
ZipFormatStatus ZipFormat_LocateCentralDirectory(
    const ZipFormatWindow* window,
    ZipFormatValidateFn validate,
    void* context,
    ZipFormatDirectory* directory,
    uint64_t* required_offset);

/*
 * Сколько байт читать одним запросом от local header записи: заголовок +
 * имя (по central directory) + slack под local extra + data_size, но не
 * дальше конца файла. 0 — local header не помещается в файл.
 */
uint64_t ZipFormat_EntryReadSize(
    uint64_t file_size,
    uint64_t local_header_offset,
    uint16_t name_size,
    uint64_t data_size,
    uint64_t slack);

/*
 * Проверяет local file header (сигнатура, метод как в central directory,
 * отсутствие шифрования) и границы данных записи в файле. В *data_offset —
 * смещение данных относительно начала local header.
 */
ZipFormatStatus ZipFormat_ParseLocalHeader(
    const uint8_t* header,
    size_t available,
    uint16_t expected_method,
    uint64_t local_header_offset,
    uint64_t compressed_size,
    uint64_t file_size,
    uint64_t* data_offset);

#endif
