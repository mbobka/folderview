/*
 * Нативные регрессии платформонезависимого разбора ZIP (ZipFormat.c).
 * Архивы собираются в памяти; цикл дочитывания окна повторяет ZipArchive.c:
 * начальный хвост фиксированного размера, затем расширение влево до
 * required_offset при ZIP_FORMAT_NEED_MORE.
 */
#include "ZipFormat.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TAIL_WINDOW (128U * 1024U)
#define SLACK 1024U

#define EXPECT(condition) \
    do { \
        if (!(condition)) { \
            fprintf(stderr, "ZipFormatTests: failure at line %d: %s\n", __LINE__, #condition); \
            return 1; \
        } \
    } while (0)

typedef struct Buffer {
    uint8_t* data;
    size_t size;
    size_t capacity;
} Buffer;

static void buffer_reserve(Buffer* buffer, size_t extra) {
    if (buffer->size + extra <= buffer->capacity) {
        return;
    }
    while (buffer->size + extra > buffer->capacity) {
        buffer->capacity = buffer->capacity == 0 ? 4096 : buffer->capacity * 2;
    }
    buffer->data = (uint8_t*)realloc(buffer->data, buffer->capacity);
    if (buffer->data == NULL) {
        abort();
    }
}

static void put_bytes(Buffer* buffer, const void* data, size_t size) {
    if (size == 0) {
        return;
    }
    buffer_reserve(buffer, size);
    memcpy(buffer->data + buffer->size, data, size);
    buffer->size += size;
}

static void put_fill(Buffer* buffer, uint8_t value, size_t size) {
    if (size == 0) {
        return;
    }
    buffer_reserve(buffer, size);
    memset(buffer->data + buffer->size, value, size);
    buffer->size += size;
}

static void put_u16(Buffer* buffer, uint16_t value) {
    uint8_t bytes[2] = {(uint8_t)value, (uint8_t)(value >> 8)};
    put_bytes(buffer, bytes, sizeof(bytes));
}

static void put_u32(Buffer* buffer, uint32_t value) {
    put_u16(buffer, (uint16_t)value);
    put_u16(buffer, (uint16_t)(value >> 16));
}

static void put_u64(Buffer* buffer, uint64_t value) {
    put_u32(buffer, (uint32_t)value);
    put_u32(buffer, (uint32_t)(value >> 32));
}

static void write_u16_at(Buffer* buffer, size_t offset, uint16_t value) {
    buffer->data[offset] = (uint8_t)value;
    buffer->data[offset + 1] = (uint8_t)(value >> 8);
}

static void write_u32_at(Buffer* buffer, size_t offset, uint32_t value) {
    write_u16_at(buffer, offset, (uint16_t)value);
    write_u16_at(buffer, offset + 2, (uint16_t)(value >> 16));
}

static void buffer_free(Buffer* buffer) {
    free(buffer->data);
    buffer->data = NULL;
    buffer->size = 0;
    buffer->capacity = 0;
}

/* --- Построитель архива --------------------------------------------------- */

typedef struct Builder {
    Buffer file;
    Buffer directory;
    uint64_t entries;
} Builder;

typedef struct EntryOptions {
    uint16_t local_extra_size;
    int zip64_central;
    uint16_t disk_start;
    uint16_t central_comment_size;
} EntryOptions;

static const EntryOptions DEFAULT_ENTRY = {0, 0, 0, 0};

/* Stored-запись: local header + имя + extra-заполнитель + данные, плюс запись CD. */
static uint64_t add_entry(
    Builder* builder,
    const char* name,
    uint32_t data_size,
    const EntryOptions* options) {
    uint64_t local_offset = builder->file.size;
    uint16_t name_size = (uint16_t)strlen(name);
    Buffer* cd = &builder->directory;

    put_u32(&builder->file, 0x04034B50U);
    put_u16(&builder->file, 20);
    put_u16(&builder->file, 0);
    put_u16(&builder->file, 0);
    put_u16(&builder->file, 0);
    put_u16(&builder->file, 0);
    put_u32(&builder->file, 0);
    put_u32(&builder->file, data_size);
    put_u32(&builder->file, data_size);
    put_u16(&builder->file, name_size);
    put_u16(&builder->file, options->local_extra_size);
    put_bytes(&builder->file, name, name_size);
    if (options->local_extra_size >= 4) {
        /* Одно неизвестное TLV-поле на весь extra. */
        put_u16(&builder->file, 0x9999U);
        put_u16(&builder->file, (uint16_t)(options->local_extra_size - 4));
        put_fill(&builder->file, 0xEE, options->local_extra_size - 4U);
    } else {
        put_fill(&builder->file, 0, options->local_extra_size);
    }
    put_fill(&builder->file, (uint8_t)('A' + builder->entries % 26), data_size);

    put_u32(cd, 0x02014B50U);
    put_u16(cd, 20);
    put_u16(cd, 20);
    put_u16(cd, 0);
    put_u16(cd, 0);
    put_u16(cd, 0);
    put_u16(cd, 0);
    put_u32(cd, 0);
    put_u32(cd, options->zip64_central ? 0xFFFFFFFFU : data_size);
    put_u32(cd, options->zip64_central ? 0xFFFFFFFFU : data_size);
    put_u16(cd, name_size);
    put_u16(cd, options->zip64_central ? 28 : 0);
    put_u16(cd, options->central_comment_size);
    put_u16(cd, options->disk_start);
    put_u16(cd, 0);
    put_u32(cd, 0);
    put_u32(cd, options->zip64_central ? 0xFFFFFFFFU : (uint32_t)local_offset);
    put_bytes(cd, name, name_size);
    if (options->zip64_central) {
        put_u16(cd, 0x0001U);
        put_u16(cd, 24);
        put_u64(cd, data_size);
        put_u64(cd, data_size);
        put_u64(cd, local_offset);
    }
    put_fill(cd, 'k', options->central_comment_size);

    ++builder->entries;
    return local_offset;
}

typedef struct FinishOptions {
    int zip64_eocd;
    uint16_t comment_size;
    int digital_signature;
} FinishOptions;

static const FinishOptions DEFAULT_FINISH = {0, 0, 0};

/* Дописывает central directory, (ZIP64 EOCD + locator) и EOCD; возвращает смещение CD. */
static uint64_t finish(Builder* builder, const FinishOptions* options) {
    uint64_t directory_offset = builder->file.size;
    uint64_t directory_size;
    Buffer* file = &builder->file;

    if (options->digital_signature) {
        put_u32(&builder->directory, 0x05054B50U);
        put_u16(&builder->directory, 5);
        put_bytes(&builder->directory, "sig!!", 5);
    }
    directory_size = builder->directory.size;
    put_bytes(file, builder->directory.data, builder->directory.size);

    if (options->zip64_eocd) {
        uint64_t eocd64_offset = file->size;

        put_u32(file, 0x06064B50U);
        put_u64(file, 44);
        put_u16(file, 45);
        put_u16(file, 45);
        put_u32(file, 0);
        put_u32(file, 0);
        put_u64(file, builder->entries);
        put_u64(file, builder->entries);
        put_u64(file, directory_size);
        put_u64(file, directory_offset);

        put_u32(file, 0x07064B50U);
        put_u32(file, 0);
        put_u64(file, eocd64_offset);
        put_u32(file, 1);
    }

    put_u32(file, 0x06054B50U);
    put_u16(file, 0);
    put_u16(file, 0);
    put_u16(file, options->zip64_eocd ? 0xFFFFU : (uint16_t)builder->entries);
    put_u16(file, options->zip64_eocd ? 0xFFFFU : (uint16_t)builder->entries);
    put_u32(file, options->zip64_eocd ? 0xFFFFFFFFU : (uint32_t)directory_size);
    put_u32(file, options->zip64_eocd ? 0xFFFFFFFFU : (uint32_t)directory_offset);
    put_u16(file, options->comment_size);
    put_fill(file, 'c', options->comment_size);
    return directory_offset;
}

static void builder_free(Builder* builder) {
    buffer_free(&builder->file);
    buffer_free(&builder->directory);
    builder->entries = 0;
}

/* --- Цикл чтения, как в ZipArchive.c -------------------------------------- */

typedef struct LocateResult {
    ZipFormatStatus status;
    ZipFormatDirectory directory;
    int expansions;
    uint64_t first_required;
    uint64_t window_offset;
} LocateResult;

typedef struct CallbackState {
    int calls;
    int accept;
} CallbackState;

static int counting_callback(
    void* context,
    const ZipFormatWindow* window,
    const ZipFormatDirectory* directory) {
    CallbackState* state = (CallbackState*)context;

    (void)window;
    (void)directory;
    ++state->calls;
    return state->accept;
}

static LocateResult locate_with(
    const uint8_t* data,
    size_t size,
    size_t initial_window,
    ZipFormatValidateFn validate,
    void* context) {
    LocateResult result;
    uint64_t window_offset = size > initial_window ? size - initial_window : 0;

    memset(&result, 0, sizeof(result));
    for (;;) {
        /* Окно копируется в буфер точного размера: данные левее недоступны. */
        size_t window_size = (size_t)(size - window_offset);
        uint8_t* copy = (uint8_t*)malloc(window_size == 0 ? 1 : window_size);
        ZipFormatWindow window;
        uint64_t required = UINT64_MAX;

        if (copy == NULL) {
            abort();
        }
        memcpy(copy, data + window_offset, window_size);
        window.data = copy;
        window.offset = window_offset;
        window.size = window_size;
        window.file_size = size;
        result.status = ZipFormat_LocateCentralDirectory(
            &window,
            validate,
            context,
            &result.directory,
            &required);
        free(copy);
        if (result.status != ZIP_FORMAT_NEED_MORE) {
            break;
        }
        if (required >= window_offset || result.expansions > 8) {
            /* Окно обязано расти, иначе цикл не завершится. */
            result.status = (ZipFormatStatus)99;
            break;
        }
        if (result.expansions == 0) {
            result.first_required = required;
        }
        window_offset = required;
        ++result.expansions;
    }
    result.window_offset = window_offset;
    return result;
}

static LocateResult locate(const Buffer* file, size_t initial_window) {
    return locate_with(file->data, file->size, initial_window, NULL, NULL);
}

/* Проходит CD найденного каталога и сверяет число записей и local headers. */
static int walk_directory(
    const Buffer* file,
    const ZipFormatDirectory* directory,
    uint64_t* entries_seen) {
    uint64_t cursor = 0;
    uint64_t index;

    for (index = 0; index < directory->total_entries; ++index) {
        ZipFormatCentralEntry entry;
        uint64_t read_size;
        uint64_t data_offset;

        EXPECT(ZipFormat_ReadCentralEntry(
            file->data + directory->offset + cursor,
            directory->size - cursor,
            &entry) == ZIP_FORMAT_OK);
        read_size = ZipFormat_EntryReadSize(
            file->size,
            entry.local_header_offset,
            entry.name_size,
            entry.compressed_size,
            SLACK);
        EXPECT(read_size >= ZIP_FORMAT_LOCAL_HEADER_SIZE);
        EXPECT(ZipFormat_ParseLocalHeader(
            file->data + entry.local_header_offset,
            (size_t)read_size,
            entry.method,
            entry.local_header_offset,
            entry.compressed_size,
            file->size,
            &data_offset) == ZIP_FORMAT_OK);
        EXPECT(data_offset == ZIP_FORMAT_LOCAL_HEADER_SIZE + entry.name_size);
        cursor += entry.record_size;
    }
    *entries_seen = index;
    return 0;
}

/* --- Тесты ----------------------------------------------------------------- */

static int test_archive_smaller_than_window(void) {
    Builder builder = {0};
    LocateResult result;
    uint64_t seen = 0;
    uint64_t directory_offset;

    add_entry(&builder, "2020-01-01_0001.jpg", 1000, &DEFAULT_ENTRY);
    add_entry(&builder, "2020-01-01_0002.jpg", 2000, &DEFAULT_ENTRY);
    add_entry(&builder, "model_info.txt", 10, &DEFAULT_ENTRY);
    directory_offset = finish(&builder, &DEFAULT_FINISH);

    result = locate(&builder.file, TAIL_WINDOW);
    EXPECT(result.status == ZIP_FORMAT_OK);
    EXPECT(result.expansions == 0);
    EXPECT(result.directory.total_entries == 3);
    EXPECT(result.directory.offset == directory_offset);
    EXPECT(walk_directory(&builder.file, &result.directory, &seen) == 0);
    EXPECT(seen == 3);
    builder_free(&builder);
    return 0;
}

static int test_empty_archive(void) {
    Builder builder = {0};
    LocateResult result;

    finish(&builder, &DEFAULT_FINISH);
    EXPECT(builder.file.size == ZIP_FORMAT_EOCD_SIZE);
    result = locate(&builder.file, TAIL_WINDOW);
    EXPECT(result.status == ZIP_FORMAT_OK);
    EXPECT(result.directory.total_entries == 0);
    EXPECT(result.directory.size == 0);
    builder_free(&builder);
    return 0;
}

static int test_directory_larger_than_window_needs_one_extra_read(void) {
    Builder builder = {0};
    LocateResult result;
    uint64_t seen = 0;
    uint64_t directory_offset;
    char name[128];
    int index;

    for (index = 0; index < 2000; ++index) {
        snprintf(name, sizeof(name),
            "very-long-directory-name-for-central-directory/%05d-padding.jpg", index);
        add_entry(&builder, name, 64, &DEFAULT_ENTRY);
    }
    directory_offset = finish(&builder, &DEFAULT_FINISH);
    EXPECT(builder.directory.size > TAIL_WINDOW);

    result = locate(&builder.file, TAIL_WINDOW);
    EXPECT(result.status == ZIP_FORMAT_OK);
    EXPECT(result.expansions == 1);
    EXPECT(result.first_required == directory_offset);
    EXPECT(result.directory.total_entries == 2000);
    EXPECT(walk_directory(&builder.file, &result.directory, &seen) == 0);
    EXPECT(seen == 2000);
    builder_free(&builder);
    return 0;
}

static int test_zip64_eocd_and_extra(void) {
    Builder builder = {0};
    EntryOptions zip64_entry = DEFAULT_ENTRY;
    FinishOptions zip64_finish = DEFAULT_FINISH;
    LocateResult result;
    ZipFormatCentralEntry entry;
    uint64_t local_offsets[3];
    uint64_t directory_offset;
    uint64_t cursor = 0;
    int index;

    zip64_entry.zip64_central = 1;
    zip64_finish.zip64_eocd = 1;
    local_offsets[0] = add_entry(&builder, "a.jpg", 100, &zip64_entry);
    local_offsets[1] = add_entry(&builder, "b.jpg", 200, &DEFAULT_ENTRY);
    local_offsets[2] = add_entry(&builder, "c.jpg", 300, &zip64_entry);
    directory_offset = finish(&builder, &zip64_finish);

    result = locate(&builder.file, TAIL_WINDOW);
    EXPECT(result.status == ZIP_FORMAT_OK);
    EXPECT(result.directory.total_entries == 3);
    EXPECT(result.directory.offset == directory_offset);
    for (index = 0; index < 3; ++index) {
        EXPECT(ZipFormat_ReadCentralEntry(
            builder.file.data + directory_offset + cursor,
            result.directory.size - cursor,
            &entry) == ZIP_FORMAT_OK);
        EXPECT(entry.local_header_offset == local_offsets[index]);
        EXPECT(entry.compressed_size == (uint64_t)(index + 1) * 100U);
        EXPECT(entry.uncompressed_size == (uint64_t)(index + 1) * 100U);
        cursor += entry.record_size;
    }
    builder_free(&builder);
    return 0;
}

static int test_zip64_directory_larger_than_window(void) {
    Builder builder = {0};
    FinishOptions zip64_finish = DEFAULT_FINISH;
    LocateResult result;
    uint64_t directory_offset;
    char name[96];
    int index;

    zip64_finish.zip64_eocd = 1;
    for (index = 0; index < 3000; ++index) {
        snprintf(name, sizeof(name), "meta/padding-padding-padding/%06d.txt", index);
        add_entry(&builder, name, 1, &DEFAULT_ENTRY);
    }
    directory_offset = finish(&builder, &zip64_finish);
    EXPECT(builder.directory.size > TAIL_WINDOW);

    result = locate(&builder.file, TAIL_WINDOW);
    EXPECT(result.status == ZIP_FORMAT_OK);
    EXPECT(result.expansions == 1);
    EXPECT(result.first_required == directory_offset);
    EXPECT(result.directory.total_entries == 3000);
    builder_free(&builder);
    return 0;
}

static int test_maximum_comment(void) {
    Builder builder = {0};
    FinishOptions finish_options = DEFAULT_FINISH;
    LocateResult result;
    uint64_t directory_offset;

    finish_options.comment_size = 65535;
    add_entry(&builder, "photo.jpg", 5000, &DEFAULT_ENTRY);
    directory_offset = finish(&builder, &finish_options);

    /* Хвост 128 КиБ вмещает EOCD с максимальным комментарием и весь CD. */
    result = locate(&builder.file, TAIL_WINDOW);
    EXPECT(result.status == ZIP_FORMAT_OK);
    EXPECT(result.expansions == 0);
    EXPECT(result.directory.offset == directory_offset);

    /* Окно ровно под поиск EOCD: CD левее, нужно ровно одно дочитывание. */
    result = locate(&builder.file, ZIP_FORMAT_EOCD_SEARCH_SIZE);
    EXPECT(result.status == ZIP_FORMAT_OK);
    EXPECT(result.expansions == 1);
    EXPECT(result.first_required == directory_offset);
    builder_free(&builder);
    return 0;
}

static int test_comment_longer_than_maximum_is_not_found(void) {
    Builder builder = {0};
    LocateResult result;

    add_entry(&builder, "photo.jpg", 10, &DEFAULT_ENTRY);
    finish(&builder, &DEFAULT_FINISH);
    /* Мусор после EOCD сверх максимального комментария: EOCD вне зоны поиска. */
    put_fill(&builder.file, 'z', 65536);
    result = locate(&builder.file, TAIL_WINDOW);
    EXPECT(result.status == ZIP_FORMAT_BAD_FORMAT);
    builder_free(&builder);
    return 0;
}

static int test_fake_eocd_inside_comment(void) {
    Builder builder = {0};
    FinishOptions finish_options = DEFAULT_FINISH;
    LocateResult result;
    uint64_t directory_offset;
    size_t comment_start;

    finish_options.comment_size = 64;
    add_entry(&builder, "photo.jpg", 10, &DEFAULT_ENTRY);
    directory_offset = finish(&builder, &finish_options);
    comment_start = builder.file.size - 64;
    /* Поддельная EOCD-сигнатура с невозможными значениями внутри комментария. */
    write_u32_at(&builder.file, comment_start + 8, 0x06054B50U);
    write_u16_at(&builder.file, comment_start + 8 + 10, 7);

    result = locate(&builder.file, TAIL_WINDOW);
    EXPECT(result.status == ZIP_FORMAT_OK);
    EXPECT(result.directory.offset == directory_offset);
    EXPECT(result.directory.total_entries == 1);
    builder_free(&builder);
    return 0;
}

static int test_digital_signature_after_directory(void) {
    Builder builder = {0};
    FinishOptions finish_options = DEFAULT_FINISH;
    LocateResult result;

    finish_options.digital_signature = 1;
    add_entry(&builder, "photo.jpg", 10, &DEFAULT_ENTRY);
    finish(&builder, &finish_options);
    result = locate(&builder.file, TAIL_WINDOW);
    EXPECT(result.status == ZIP_FORMAT_OK);
    EXPECT(result.directory.total_entries == 1);
    builder_free(&builder);
    return 0;
}

static int test_validate_callback(void) {
    Builder builder = {0};
    CallbackState state = {0, 1};
    LocateResult result;

    add_entry(&builder, "photo.jpg", 10, &DEFAULT_ENTRY);
    finish(&builder, &DEFAULT_FINISH);

    result = locate_with(builder.file.data, builder.file.size, TAIL_WINDOW, counting_callback, &state);
    EXPECT(result.status == ZIP_FORMAT_OK);
    EXPECT(state.calls == 1);

    state.calls = 0;
    state.accept = 0;
    result = locate_with(builder.file.data, builder.file.size, TAIL_WINDOW, counting_callback, &state);
    EXPECT(result.status == ZIP_FORMAT_BAD_FORMAT);
    EXPECT(state.calls >= 1);
    builder_free(&builder);
    return 0;
}

static int test_corrupt_and_truncated_structures(void) {
    Builder builder = {0};
    LocateResult result;
    uint64_t directory_offset;
    Buffer copy = {0};
    uint8_t tiny[10] = {0x50, 0x4B, 0x05, 0x06};

    add_entry(&builder, "a.jpg", 100, &DEFAULT_ENTRY);
    add_entry(&builder, "b.jpg", 100, &DEFAULT_ENTRY);
    directory_offset = finish(&builder, &DEFAULT_FINISH);

    /* Файл короче EOCD. */
    result = locate_with(tiny, sizeof(tiny), TAIL_WINDOW, NULL, NULL);
    EXPECT(result.status == ZIP_FORMAT_BAD_FORMAT);

    /* Усечённый архив: EOCD обрезан. */
    result = locate_with(builder.file.data, builder.file.size - 10, TAIL_WINDOW, NULL, NULL);
    EXPECT(result.status == ZIP_FORMAT_BAD_FORMAT);

    /* Усечённый архив: отрезано начало вместе с частью CD. */
    result = locate_with(
        builder.file.data + directory_offset + 10,
        builder.file.size - directory_offset - 10,
        TAIL_WINDOW,
        NULL,
        NULL);
    EXPECT(result.status == ZIP_FORMAT_BAD_FORMAT);

    /* Испорченная сигнатура второй записи CD. */
    put_bytes(&copy, builder.file.data, builder.file.size);
    write_u32_at(&copy, (size_t)directory_offset + 46 + 5, 0x12345678U);
    result = locate(&copy, TAIL_WINDOW);
    EXPECT(result.status == ZIP_FORMAT_BAD_FORMAT);
    buffer_free(&copy);

    /* Длина имени первой записи выходит за конец CD. */
    put_bytes(&copy, builder.file.data, builder.file.size);
    write_u16_at(&copy, (size_t)directory_offset + 28, 60000);
    result = locate(&copy, TAIL_WINDOW);
    EXPECT(result.status == ZIP_FORMAT_BAD_FORMAT);
    buffer_free(&copy);

    /* Пустое имя записи. */
    put_bytes(&copy, builder.file.data, builder.file.size);
    write_u16_at(&copy, (size_t)directory_offset + 28, 0);
    result = locate(&copy, TAIL_WINDOW);
    EXPECT(result.status == ZIP_FORMAT_BAD_FORMAT);
    buffer_free(&copy);

    /* EOCD обещает записей больше, чем вмещает размер CD. */
    put_bytes(&copy, builder.file.data, builder.file.size);
    write_u16_at(&copy, copy.size - 22 + 8, 500);
    write_u16_at(&copy, copy.size - 22 + 10, 500);
    result = locate(&copy, TAIL_WINDOW);
    EXPECT(result.status == ZIP_FORMAT_BAD_FORMAT);
    buffer_free(&copy);

    /* CD «заходит» за EOCD. */
    put_bytes(&copy, builder.file.data, builder.file.size);
    write_u32_at(&copy, copy.size - 22 + 16, (uint32_t)directory_offset + 1);
    result = locate(&copy, TAIL_WINDOW);
    EXPECT(result.status == ZIP_FORMAT_BAD_FORMAT);
    buffer_free(&copy);

    /* Многотомный архив (номер диска EOCD). */
    put_bytes(&copy, builder.file.data, builder.file.size);
    write_u16_at(&copy, copy.size - 22 + 4, 1);
    result = locate(&copy, TAIL_WINDOW);
    EXPECT(result.status == ZIP_FORMAT_BAD_FORMAT);
    buffer_free(&copy);

    builder_free(&builder);
    return 0;
}

static int test_zip64_broken_structures(void) {
    Builder builder = {0};
    FinishOptions zip64_finish = DEFAULT_FINISH;
    EntryOptions zip64_entry = DEFAULT_ENTRY;
    LocateResult result;
    Buffer copy = {0};
    size_t locator;
    size_t eocd64;
    uint64_t directory_offset;

    zip64_finish.zip64_eocd = 1;
    zip64_entry.zip64_central = 1;
    add_entry(&builder, "a.jpg", 100, &zip64_entry);
    directory_offset = finish(&builder, &zip64_finish);
    locator = builder.file.size - 22 - 20;
    eocd64 = locator - 56;

    /* Locator указывает за конец файла. */
    put_bytes(&copy, builder.file.data, builder.file.size);
    write_u32_at(&copy, locator + 8, 0x7FFFFFF0U);
    result = locate(&copy, TAIL_WINDOW);
    EXPECT(result.status == ZIP_FORMAT_BAD_FORMAT);
    buffer_free(&copy);

    /* Неверная сигнатура ZIP64 EOCD. */
    put_bytes(&copy, builder.file.data, builder.file.size);
    write_u32_at(&copy, eocd64, 0x01020304U);
    result = locate(&copy, TAIL_WINDOW);
    EXPECT(result.status == ZIP_FORMAT_BAD_FORMAT);
    buffer_free(&copy);

    /* Несколько дисков в locator. */
    put_bytes(&copy, builder.file.data, builder.file.size);
    write_u32_at(&copy, locator + 16, 2);
    result = locate(&copy, TAIL_WINDOW);
    EXPECT(result.status == ZIP_FORMAT_BAD_FORMAT);
    buffer_free(&copy);

    /* ZIP64 extra короче обязательных полей (0xFFFFFFFF без значения). */
    put_bytes(&copy, builder.file.data, builder.file.size);
    write_u16_at(&copy, (size_t)directory_offset + 46 + 5 + 2, 8);
    result = locate(&copy, TAIL_WINDOW);
    EXPECT(result.status == ZIP_FORMAT_BAD_FORMAT);
    buffer_free(&copy);

    builder_free(&builder);
    return 0;
}

static int test_entry_on_other_disk_is_unsupported(void) {
    Builder builder = {0};
    EntryOptions other_disk = DEFAULT_ENTRY;
    ZipFormatCentralEntry entry;
    LocateResult result;
    uint64_t directory_offset;

    other_disk.disk_start = 1;
    add_entry(&builder, "a.jpg", 100, &other_disk);
    directory_offset = finish(&builder, &DEFAULT_FINISH);
    EXPECT(ZipFormat_ReadCentralEntry(
        builder.file.data + directory_offset,
        builder.directory.size,
        &entry) == ZIP_FORMAT_UNSUPPORTED);
    result = locate(&builder.file, TAIL_WINDOW);
    EXPECT(result.status == ZIP_FORMAT_BAD_FORMAT);
    builder_free(&builder);
    return 0;
}

static int test_local_extra_longer_than_slack(void) {
    Builder builder = {0};
    EntryOptions long_extra = DEFAULT_ENTRY;
    uint64_t local_offset;
    uint64_t read_size;
    uint64_t data_offset;

    long_extra.local_extra_size = 4000;
    local_offset = add_entry(&builder, "photo.jpg", 5000, &long_extra);
    add_entry(&builder, "next.jpg", 5000, &DEFAULT_ENTRY);
    finish(&builder, &DEFAULT_FINISH);

    read_size = ZipFormat_EntryReadSize(builder.file.size, local_offset, 9, 5000, SLACK);
    EXPECT(read_size == 30U + 9U + SLACK + 5000U);
    EXPECT(ZipFormat_ParseLocalHeader(
        builder.file.data + local_offset,
        (size_t)read_size,
        0,
        local_offset,
        5000,
        builder.file.size,
        &data_offset) == ZIP_FORMAT_OK);
    /* Данные не поместились в первое чтение: нужно ровно одно дочитывание. */
    EXPECT(data_offset == 30U + 9U + 4000U);
    EXPECT(data_offset + 5000U > read_size);
    builder_free(&builder);
    return 0;
}

static int test_entry_read_size_limits(void) {
    /* Запрос не выходит за конец файла. */
    EXPECT(ZipFormat_EntryReadSize(1000, 900, 10, 5000, SLACK) == 100);
    /* Local header не помещается. */
    EXPECT(ZipFormat_EntryReadSize(1000, 990, 10, 5, SLACK) == 0);
    EXPECT(ZipFormat_EntryReadSize(1000, 2000, 10, 5, SLACK) == 0);
    /* Переполнение суммы насыщается до конца файла. */
    EXPECT(ZipFormat_EntryReadSize(UINT64_MAX, 0, 10, UINT64_MAX - 5, SLACK) == UINT64_MAX);
    EXPECT(ZipFormat_EntryReadSize(1000000, 0, 10, 100, SLACK) == 30U + 10U + SLACK + 100U);
    return 0;
}

static int test_local_header_checks(void) {
    Builder builder = {0};
    Buffer copy = {0};
    uint64_t local_offset;
    uint64_t data_offset;

    local_offset = add_entry(&builder, "photo.jpg", 100, &DEFAULT_ENTRY);
    finish(&builder, &DEFAULT_FINISH);

    /* Слишком короткий буфер. */
    EXPECT(ZipFormat_ParseLocalHeader(builder.file.data, 29, 0, 0, 100, builder.file.size, &data_offset) ==
        ZIP_FORMAT_BAD_FORMAT);
    /* Метод не совпадает с central directory. */
    EXPECT(ZipFormat_ParseLocalHeader(builder.file.data, 64, 8, 0, 100, builder.file.size, &data_offset) ==
        ZIP_FORMAT_BAD_FORMAT);
    /* Данные за концом файла. */
    EXPECT(ZipFormat_ParseLocalHeader(builder.file.data, 64, 0, 0, builder.file.size, builder.file.size, &data_offset) ==
        ZIP_FORMAT_BAD_FORMAT);
    /* Переполнение смещения. */
    EXPECT(ZipFormat_ParseLocalHeader(builder.file.data, 64, 0, UINT64_MAX - 10, 100, UINT64_MAX, &data_offset) ==
        ZIP_FORMAT_BAD_FORMAT);

    /* Шифрованная запись. */
    put_bytes(&copy, builder.file.data, builder.file.size);
    write_u16_at(&copy, (size_t)local_offset + 6, 1);
    EXPECT(ZipFormat_ParseLocalHeader(copy.data, 64, 0, 0, 100, copy.size, &data_offset) ==
        ZIP_FORMAT_BAD_FORMAT);
    buffer_free(&copy);

    /* Неверная сигнатура. */
    put_bytes(&copy, builder.file.data, builder.file.size);
    write_u32_at(&copy, (size_t)local_offset, 0x02014B50U);
    EXPECT(ZipFormat_ParseLocalHeader(copy.data, 64, 0, 0, 100, copy.size, &data_offset) ==
        ZIP_FORMAT_BAD_FORMAT);
    buffer_free(&copy);

    EXPECT(ZipFormat_ParseLocalHeader(builder.file.data, 64, 0, 0, 100, builder.file.size, &data_offset) ==
        ZIP_FORMAT_OK);
    EXPECT(data_offset == 30U + 9U);
    builder_free(&builder);
    return 0;
}

typedef struct TestCase {
    const char* name;
    int (*run)(void);
} TestCase;

int main(void) {
    static const TestCase tests[] = {
        {"archive_smaller_than_window", test_archive_smaller_than_window},
        {"empty_archive", test_empty_archive},
        {"directory_larger_than_window_needs_one_extra_read", test_directory_larger_than_window_needs_one_extra_read},
        {"zip64_eocd_and_extra", test_zip64_eocd_and_extra},
        {"zip64_directory_larger_than_window", test_zip64_directory_larger_than_window},
        {"maximum_comment", test_maximum_comment},
        {"comment_longer_than_maximum_is_not_found", test_comment_longer_than_maximum_is_not_found},
        {"fake_eocd_inside_comment", test_fake_eocd_inside_comment},
        {"digital_signature_after_directory", test_digital_signature_after_directory},
        {"validate_callback", test_validate_callback},
        {"corrupt_and_truncated_structures", test_corrupt_and_truncated_structures},
        {"zip64_broken_structures", test_zip64_broken_structures},
        {"entry_on_other_disk_is_unsupported", test_entry_on_other_disk_is_unsupported},
        {"local_extra_longer_than_slack", test_local_extra_longer_than_slack},
        {"entry_read_size_limits", test_entry_read_size_limits},
        {"local_header_checks", test_local_header_checks},
    };
    size_t index;
    size_t count = sizeof(tests) / sizeof(tests[0]);

    /* Без буферизации: при аварийном останове (UBSan trap) виден последний тест. */
    setvbuf(stdout, NULL, _IONBF, 0);
    for (index = 0; index < count; ++index) {
        if (tests[index].run() != 0) {
            fprintf(stderr, "ZipFormatTests: FAILED %s\n", tests[index].name);
            return 1;
        }
        printf("ZipFormatTests: ok - %s\n", tests[index].name);
    }
    printf("ZipFormatTests: %zu tests OK\n", count);
    return 0;
}
