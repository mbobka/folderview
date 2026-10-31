#include "ZipFormat.h"

#define ZIP_FORMAT_LOCAL_SIGNATURE 0x04034B50UL
#define ZIP_FORMAT_CENTRAL_SIGNATURE 0x02014B50UL
#define ZIP_FORMAT_DIGITAL_SIGNATURE 0x05054B50UL
#define ZIP_FORMAT_EOCD_SIGNATURE 0x06054B50UL
#define ZIP_FORMAT_ZIP64_EOCD_SIGNATURE 0x06064B50UL
#define ZIP_FORMAT_ZIP64_LOCATOR_SIGNATURE 0x07064B50UL
#define ZIP_FORMAT_ZIP64_EXTRA_TAG 0x0001U
#define ZIP_FORMAT_FLAG_ENCRYPTED 0x0001U

/* Результат доступа к диапазону файла в окне. */
#define ZF_RANGE_OUTSIDE_FILE 0
#define ZF_RANGE_AVAILABLE 1
#define ZF_RANGE_NEED_MORE (-1)

static uint16_t zf_u16(const uint8_t* data) {
    return (uint16_t)((uint16_t)data[0] | ((uint16_t)data[1] << 8));
}

static uint32_t zf_u32(const uint8_t* data) {
    return (uint32_t)data[0] |
        ((uint32_t)data[1] << 8) |
        ((uint32_t)data[2] << 16) |
        ((uint32_t)data[3] << 24);
}

static uint64_t zf_u64(const uint8_t* data) {
    return (uint64_t)zf_u32(data) | ((uint64_t)zf_u32(data + 4) << 32);
}

static int zf_add(uint64_t left, uint64_t right, uint64_t* result) {
    if (left > UINT64_MAX - right) {
        return 0;
    }
    *result = left + right;
    return 1;
}

/*
 * Диапазон [offset, offset + size) файла: вне файла — кандидат отвергается
 * (как неудачное чтение за EOF в исходном коде), левее окна — нужно дочитать.
 */
static int zf_range(
    const ZipFormatWindow* window,
    uint64_t offset,
    uint64_t size,
    const uint8_t** data,
    uint64_t* required_offset) {
    uint64_t end;

    if (!zf_add(offset, size, &end) || end > window->file_size) {
        return ZF_RANGE_OUTSIDE_FILE;
    }
    if (offset < window->offset) {
        *required_offset = offset;
        return ZF_RANGE_NEED_MORE;
    }
    *data = window->data + (size_t)(offset - window->offset);
    return ZF_RANGE_AVAILABLE;
}

static int zf_parse_zip64_extra(
    const uint8_t* extra,
    uint16_t extra_size,
    uint32_t compressed_size32,
    uint32_t uncompressed_size32,
    uint32_t local_offset32,
    uint16_t disk_start16,
    uint64_t* compressed_size,
    uint64_t* uncompressed_size,
    uint64_t* local_offset,
    uint32_t* disk_start) {
    size_t position = 0;
    int needs_zip64 = compressed_size32 == 0xFFFFFFFFUL ||
        uncompressed_size32 == 0xFFFFFFFFUL ||
        local_offset32 == 0xFFFFFFFFUL ||
        disk_start16 == 0xFFFFU;

    *compressed_size = compressed_size32;
    *uncompressed_size = uncompressed_size32;
    *local_offset = local_offset32;
    *disk_start = disk_start16;

    if (!needs_zip64) {
        return 1;
    }

    while (position + 4 <= (size_t)extra_size) {
        uint16_t tag = zf_u16(extra + position);
        uint16_t field_size = zf_u16(extra + position + 2);
        size_t field_offset = position + 4;
        size_t field_end;
        size_t cursor;

        if ((size_t)field_size > (size_t)extra_size - field_offset) {
            return 0;
        }
        field_end = field_offset + field_size;

        if (tag != ZIP_FORMAT_ZIP64_EXTRA_TAG) {
            position = field_end;
            continue;
        }

        cursor = field_offset;
        if (uncompressed_size32 == 0xFFFFFFFFUL) {
            if (field_end - cursor < 8) {
                return 0;
            }
            *uncompressed_size = zf_u64(extra + cursor);
            cursor += 8;
        }
        if (compressed_size32 == 0xFFFFFFFFUL) {
            if (field_end - cursor < 8) {
                return 0;
            }
            *compressed_size = zf_u64(extra + cursor);
            cursor += 8;
        }
        if (local_offset32 == 0xFFFFFFFFUL) {
            if (field_end - cursor < 8) {
                return 0;
            }
            *local_offset = zf_u64(extra + cursor);
            cursor += 8;
        }
        if (disk_start16 == 0xFFFFU) {
            if (field_end - cursor < 4) {
                return 0;
            }
            *disk_start = zf_u32(extra + cursor);
        }
        return 1;
    }

    return 0;
}

ZipFormatStatus ZipFormat_ReadCentralEntry(
    const uint8_t* record,
    uint64_t available,
    ZipFormatCentralEntry* entry) {
    uint16_t name_size;
    uint16_t extra_size;
    uint16_t comment_size;
    uint64_t record_size;
    uint32_t disk_start;

    if (record == NULL || entry == NULL ||
        available < ZIP_FORMAT_CENTRAL_HEADER_SIZE ||
        zf_u32(record) != ZIP_FORMAT_CENTRAL_SIGNATURE) {
        return ZIP_FORMAT_BAD_FORMAT;
    }

    name_size = zf_u16(record + 28);
    extra_size = zf_u16(record + 30);
    comment_size = zf_u16(record + 32);
    /* Сумма трёх uint16 плюс 46 не переполняет uint64. */
    record_size = (uint64_t)ZIP_FORMAT_CENTRAL_HEADER_SIZE +
        name_size + extra_size + comment_size;
    if (record_size > available || name_size == 0) {
        return ZIP_FORMAT_BAD_FORMAT;
    }

    entry->flags = zf_u16(record + 8);
    entry->method = zf_u16(record + 10);
    entry->name = record + ZIP_FORMAT_CENTRAL_HEADER_SIZE;
    entry->name_size = name_size;
    entry->extra = record + ZIP_FORMAT_CENTRAL_HEADER_SIZE + name_size;
    entry->extra_size = extra_size;
    entry->record_size = record_size;

    if (!zf_parse_zip64_extra(
            entry->extra,
            extra_size,
            zf_u32(record + 20),
            zf_u32(record + 24),
            zf_u32(record + 42),
            zf_u16(record + 34),
            &entry->compressed_size,
            &entry->uncompressed_size,
            &entry->local_header_offset,
            &disk_start)) {
        return ZIP_FORMAT_BAD_FORMAT;
    }
    if (disk_start != 0) {
        return ZIP_FORMAT_UNSUPPORTED;
    }
    return ZIP_FORMAT_OK;
}

/*
 * Структурная проверка всего central directory кандидата: каждая запись
 * разбирается, после записей допустимы только хвост короче 4 байт или
 * стандартная цифровая подпись каталога.
 */
static int zf_validate_layout(
    const ZipFormatWindow* window,
    const ZipFormatDirectory* directory,
    uint64_t* required_offset) {
    const uint8_t* data = NULL;
    uint64_t cursor = 0;
    uint64_t index;
    uint64_t remaining;
    int range;

    if (directory->size == 0) {
        /* total_entries <= size / 46 уже гарантирует 0 записей. */
        return 1;
    }
    range = zf_range(window, directory->offset, directory->size, &data, required_offset);
    if (range != ZF_RANGE_AVAILABLE) {
        return range;
    }

    for (index = 0; index < directory->total_entries; ++index) {
        ZipFormatCentralEntry entry;

        if (ZipFormat_ReadCentralEntry(
                data + (size_t)cursor,
                directory->size - cursor,
                &entry) != ZIP_FORMAT_OK) {
            return 0;
        }
        cursor += entry.record_size;
    }

    remaining = directory->size - cursor;
    if (remaining < 4) {
        return 1;
    }
    if (remaining >= 6 &&
        zf_u32(data + (size_t)cursor) == ZIP_FORMAT_DIGITAL_SIGNATURE) {
        uint64_t signature_end = 6U + (uint64_t)zf_u16(data + (size_t)cursor + 4);

        if (signature_end <= remaining && remaining - signature_end < 4) {
            return 1;
        }
    }
    return 0;
}

/*
 * Оценка кандидата EOCD по смещению eocd_offset. Возвращает -1 для
 * отвергнутого кандидата, иначе оценку; при нехватке окна выставляет
 * *need_more. Полная проверка выполняется, только если оценка выше best.
 */
static int zf_eocd_candidate(
    const ZipFormatWindow* window,
    uint64_t eocd_offset,
    int best_score,
    ZipFormatValidateFn validate,
    void* context,
    ZipFormatDirectory* candidate,
    uint64_t* required_offset,
    int* need_more) {
    const uint8_t* eocd = NULL;
    uint64_t trailing_size;
    uint64_t directory_end;
    uint16_t comment_size;
    uint16_t entries_on_disk;
    uint16_t total_entries;
    uint32_t directory_size;
    uint32_t directory_offset;
    int needs_zip64;
    int score = 0;
    int range;

    range = zf_range(window, eocd_offset, ZIP_FORMAT_EOCD_SIZE, &eocd, required_offset);
    if (range == ZF_RANGE_NEED_MORE) {
        *need_more = 1;
        return -1;
    }
    if (range != ZF_RANGE_AVAILABLE ||
        zf_u32(eocd) != ZIP_FORMAT_EOCD_SIGNATURE) {
        return -1;
    }

    trailing_size = window->file_size - eocd_offset - ZIP_FORMAT_EOCD_SIZE;
    comment_size = zf_u16(eocd + 20);
    if ((uint64_t)comment_size > trailing_size ||
        zf_u16(eocd + 4) != 0 ||
        zf_u16(eocd + 6) != 0) {
        return -1;
    }

    entries_on_disk = zf_u16(eocd + 8);
    total_entries = zf_u16(eocd + 10);
    directory_size = zf_u32(eocd + 12);
    directory_offset = zf_u32(eocd + 16);
    needs_zip64 = entries_on_disk == 0xFFFFU ||
        total_entries == 0xFFFFU ||
        directory_size == 0xFFFFFFFFUL ||
        directory_offset == 0xFFFFFFFFUL;

    if (needs_zip64) {
        const uint8_t* locator = NULL;
        const uint8_t* eocd64 = NULL;
        uint64_t locator_offset;
        uint64_t eocd64_offset;
        uint64_t eocd64_end;
        uint64_t total_entries64;
        uint64_t directory_size64;
        uint64_t directory_offset64;

        if (eocd_offset < ZIP_FORMAT_ZIP64_LOCATOR_SIZE) {
            return -1;
        }
        locator_offset = eocd_offset - ZIP_FORMAT_ZIP64_LOCATOR_SIZE;
        range = zf_range(
            window,
            locator_offset,
            ZIP_FORMAT_ZIP64_LOCATOR_SIZE,
            &locator,
            required_offset);
        if (range == ZF_RANGE_NEED_MORE) {
            *need_more = 1;
            return -1;
        }
        if (range != ZF_RANGE_AVAILABLE ||
            zf_u32(locator) != ZIP_FORMAT_ZIP64_LOCATOR_SIGNATURE ||
            zf_u32(locator + 4) != 0 ||
            zf_u32(locator + 16) != 1) {
            return -1;
        }

        eocd64_offset = zf_u64(locator + 8);
        range = zf_range(
            window,
            eocd64_offset,
            ZIP_FORMAT_ZIP64_EOCD_SIZE,
            &eocd64,
            required_offset);
        if (range == ZF_RANGE_NEED_MORE) {
            *need_more = 1;
            return -1;
        }
        if (range != ZF_RANGE_AVAILABLE ||
            zf_u32(eocd64) != ZIP_FORMAT_ZIP64_EOCD_SIGNATURE ||
            zf_u64(eocd64 + 4) < 44) {
            return -1;
        }
        if (!zf_add(eocd64_offset, 12, &eocd64_end) ||
            !zf_add(eocd64_end, zf_u64(eocd64 + 4), &eocd64_end) ||
            eocd64_end > locator_offset ||
            zf_u32(eocd64 + 16) != 0 ||
            zf_u32(eocd64 + 20) != 0 ||
            zf_u64(eocd64 + 24) != zf_u64(eocd64 + 32)) {
            return -1;
        }

        total_entries64 = zf_u64(eocd64 + 32);
        directory_size64 = zf_u64(eocd64 + 40);
        directory_offset64 = zf_u64(eocd64 + 48);
        if (!zf_add(directory_offset64, directory_size64, &directory_end) ||
            directory_end > eocd64_offset ||
            total_entries64 > directory_size64 / ZIP_FORMAT_CENTRAL_HEADER_SIZE) {
            return -1;
        }
        if (directory_end == eocd64_offset) {
            score += 2;
        }
        candidate->total_entries = total_entries64;
        candidate->size = directory_size64;
        candidate->offset = directory_offset64;
    }
    else {
        if (entries_on_disk != total_entries ||
            !zf_add(directory_offset, directory_size, &directory_end) ||
            directory_end > eocd_offset ||
            (uint64_t)total_entries >
                (uint64_t)directory_size / ZIP_FORMAT_CENTRAL_HEADER_SIZE) {
            return -1;
        }
        if (directory_end == eocd_offset) {
            score += 2;
        }
        candidate->total_entries = total_entries;
        candidate->size = directory_size;
        candidate->offset = directory_offset;
    }

    if ((uint64_t)comment_size == trailing_size) {
        ++score;
    }
    if (score <= best_score) {
        return score;
    }

    range = zf_validate_layout(window, candidate, required_offset);
    if (range == ZF_RANGE_NEED_MORE) {
        *need_more = 1;
        return -1;
    }
    if (range != 1) {
        return -1;
    }
    if (validate != NULL && !validate(context, window, candidate)) {
        return -1;
    }
    return score;
}

ZipFormatStatus ZipFormat_LocateCentralDirectory(
    const ZipFormatWindow* window,
    ZipFormatValidateFn validate,
    void* context,
    ZipFormatDirectory* directory,
    uint64_t* required_offset) {
    uint64_t position;
    uint64_t search_start;
    uint64_t window_end;
    int best_score = -1;

    if (window == NULL || directory == NULL || required_offset == NULL ||
        (window->data == NULL && window->size != 0) ||
        !zf_add(window->offset, window->size, &window_end) ||
        window_end != window->file_size) {
        return ZIP_FORMAT_BAD_FORMAT;
    }
    if (window->file_size < ZIP_FORMAT_EOCD_SIZE) {
        return ZIP_FORMAT_BAD_FORMAT;
    }

    search_start = window->file_size > ZIP_FORMAT_EOCD_SEARCH_SIZE
        ? window->file_size - ZIP_FORMAT_EOCD_SEARCH_SIZE
        : 0;
    position = window->file_size - ZIP_FORMAT_EOCD_SIZE;
    for (;;) {
        ZipFormatDirectory candidate;
        int need_more = 0;
        int score = zf_eocd_candidate(
            window,
            position,
            best_score,
            validate,
            context,
            &candidate,
            required_offset,
            &need_more);

        if (need_more) {
            return ZIP_FORMAT_NEED_MORE;
        }
        /* Оценка выше лучшей возможна только после полной проверки. */
        if (score > best_score) {
            best_score = score;
            *directory = candidate;
        }
        if (position == search_start) {
            break;
        }
        --position;
    }

    return best_score >= 0 ? ZIP_FORMAT_OK : ZIP_FORMAT_BAD_FORMAT;
}

uint64_t ZipFormat_EntryReadSize(
    uint64_t file_size,
    uint64_t local_header_offset,
    uint16_t name_size,
    uint64_t data_size,
    uint64_t slack) {
    uint64_t available;
    uint64_t wanted = (uint64_t)ZIP_FORMAT_LOCAL_HEADER_SIZE + name_size;

    if (local_header_offset > file_size ||
        file_size - local_header_offset < ZIP_FORMAT_LOCAL_HEADER_SIZE) {
        return 0;
    }
    available = file_size - local_header_offset;
    /* Насыщающее сложение: при переполнении всё равно ограничит конец файла. */
    if (!zf_add(wanted, slack, &wanted) || !zf_add(wanted, data_size, &wanted)) {
        wanted = UINT64_MAX;
    }
    return wanted < available ? wanted : available;
}

ZipFormatStatus ZipFormat_ParseLocalHeader(
    const uint8_t* header,
    size_t available,
    uint16_t expected_method,
    uint64_t local_header_offset,
    uint64_t compressed_size,
    uint64_t file_size,
    uint64_t* data_offset) {
    uint64_t header_size;
    uint64_t data_start;
    uint64_t data_end;

    if (header == NULL || data_offset == NULL ||
        available < ZIP_FORMAT_LOCAL_HEADER_SIZE ||
        zf_u32(header) != ZIP_FORMAT_LOCAL_SIGNATURE ||
        zf_u16(header + 8) != expected_method ||
        (zf_u16(header + 6) & ZIP_FORMAT_FLAG_ENCRYPTED) != 0) {
        return ZIP_FORMAT_BAD_FORMAT;
    }

    header_size = (uint64_t)ZIP_FORMAT_LOCAL_HEADER_SIZE +
        zf_u16(header + 26) + zf_u16(header + 28);
    if (!zf_add(local_header_offset, header_size, &data_start) ||
        !zf_add(data_start, compressed_size, &data_end) ||
        data_end > file_size) {
        return ZIP_FORMAT_BAD_FORMAT;
    }

    *data_offset = header_size;
    return ZIP_FORMAT_OK;
}
