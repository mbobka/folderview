#include "ViewerPlan.h"

#define FV_PLAN_UNBOUNDED 0xFFFFFFFFU

static uint32_t fv_plan_min_u32(uint32_t left, uint32_t right) {
    return left < right ? left : right;
}

/* Округление value * numerator / denominator к ближайшему, без переполнения. */
static uint32_t fv_plan_scale_round(uint32_t value, uint32_t numerator, uint32_t denominator) {
    uint64_t product = (uint64_t)value * (uint64_t)numerator;
    uint64_t result = (product + (uint64_t)denominator / 2U) / (uint64_t)denominator;

    if (result == 0) {
        return 1;
    }
    return result > (uint64_t)FV_PLAN_UNBOUNDED ? FV_PLAN_UNBOUNDED : (uint32_t)result;
}

void FvPlan_FitTarget(
    uint32_t source_width,
    uint32_t source_height,
    uint32_t box_width,
    uint32_t box_height,
    uint32_t max_dimension,
    uint32_t* target_width,
    uint32_t* target_height) {
    uint32_t limit_width;
    uint32_t limit_height;

    if (source_width == 0 || source_height == 0) {
        *target_width = 0;
        *target_height = 0;
        return;
    }

    limit_width = box_width == 0 ? FV_PLAN_UNBOUNDED : box_width;
    limit_height = box_height == 0 ? FV_PLAN_UNBOUNDED : box_height;
    if (max_dimension != 0) {
        limit_width = fv_plan_min_u32(limit_width, max_dimension);
        limit_height = fv_plan_min_u32(limit_height, max_dimension);
    }
    /* Никогда не увеличиваем. */
    limit_width = fv_plan_min_u32(limit_width, source_width);
    limit_height = fv_plan_min_u32(limit_height, source_height);

    if (limit_width == source_width && limit_height == source_height) {
        *target_width = source_width;
        *target_height = source_height;
        return;
    }

    /*
     * Сравнение source_w / source_h >= limit_w / limit_h без деления: если
     * ширина ограничивает сильнее, по ней и масштабируем, высота округляется.
     */
    if ((uint64_t)source_width * (uint64_t)limit_height >=
        (uint64_t)source_height * (uint64_t)limit_width) {
        *target_width = limit_width;
        *target_height = fv_plan_min_u32(
            fv_plan_scale_round(source_height, limit_width, source_width),
            limit_height);
    } else {
        *target_height = limit_height;
        *target_width = fv_plan_min_u32(
            fv_plan_scale_round(source_width, limit_height, source_height),
            limit_width);
    }
}

int FvPlan_AcceptDctSize(
    uint32_t dct_width,
    uint32_t dct_height,
    uint32_t target_width,
    uint32_t target_height,
    uint32_t slack_percent) {
    if (dct_width < target_width || dct_height < target_height) {
        return 0;
    }
    /* (dct - target) * 100 <= target * slack: оба произведения < 2^64. */
    return (uint64_t)(dct_width - target_width) * 100U <=
            (uint64_t)target_width * (uint64_t)slack_percent &&
        (uint64_t)(dct_height - target_height) * 100U <=
            (uint64_t)target_height * (uint64_t)slack_percent;
}

uint32_t FvPlan_DivideCeil(uint32_t value, uint32_t denominator) {
    if (denominator == 0) {
        return value;
    }
    return value / denominator + (value % denominator != 0 ? 1U : 0U);
}

uint32_t FvPlan_ChooseDctDenominator(
    uint32_t source_width,
    uint32_t source_height,
    uint32_t target_width,
    uint32_t target_height) {
    static const uint32_t denominators[] = {8U, 4U, 2U};
    size_t index;

    if (source_width == 0 || source_height == 0) {
        return 1;
    }
    for (index = 0; index < sizeof(denominators) / sizeof(denominators[0]); ++index) {
        uint32_t denominator = denominators[index];

        if (FvPlan_DivideCeil(source_width, denominator) >= target_width &&
            FvPlan_DivideCeil(source_height, denominator) >= target_height) {
            return denominator;
        }
    }
    return 1;
}

int FvPlan_NeedsRedecode(
    uint32_t source_width,
    uint32_t source_height,
    uint32_t decoded_width,
    uint32_t decoded_height,
    uint32_t display_width,
    uint32_t display_height,
    uint32_t max_dimension) {
    uint32_t wanted_width;
    uint32_t wanted_height;

    if (source_width == 0 || source_height == 0) {
        return 0;
    }
    if (decoded_width == 0 || decoded_height == 0) {
        return 1;
    }
    if (max_dimension != 0 &&
        (decoded_width > max_dimension || decoded_height > max_dimension)) {
        return 1;
    }
    /* Нулевой размер показа (свёрнутое окно) не повод декодировать полный кадр. */
    if (display_width == 0 || display_height == 0) {
        return 0;
    }

    FvPlan_FitTarget(
        source_width,
        source_height,
        display_width,
        display_height,
        max_dimension,
        &wanted_width,
        &wanted_height);
    /* Допуск 1 px: округления DCT-масштаба и вписывания расходятся на пиксель. */
    return (uint64_t)decoded_width + 1U < (uint64_t)wanted_width ||
        (uint64_t)decoded_height + 1U < (uint64_t)wanted_height;
}

void FvPlan_DecodeTarget(
    uint32_t source_width,
    uint32_t source_height,
    uint32_t box_width,
    uint32_t box_height,
    uint32_t max_dimension,
    uint32_t* target_width,
    uint32_t* target_height) {
    FvPlan_FitTarget(
        source_width,
        source_height,
        box_width,
        box_height,
        max_dimension,
        target_width,
        target_height);
    if (*target_width == source_width && *target_height == source_height) {
        return;
    }
    if (FvPlan_ChooseDctDenominator(
            source_width,
            source_height,
            *target_width,
            *target_height) > 1) {
        return;
    }
    if (max_dimension == 0 ||
        (source_width <= max_dimension && source_height <= max_dimension)) {
        *target_width = source_width;
        *target_height = source_height;
    }
}

uint64_t FvPlan_CacheBudget(uint64_t physical_memory_bytes) {
    uint64_t budget;

    if (physical_memory_bytes == 0) {
        return FV_PLAN_CACHE_BUDGET_UNKNOWN;
    }
    budget = physical_memory_bytes / 8U;
    if (budget < FV_PLAN_CACHE_BUDGET_MIN) {
        return FV_PLAN_CACHE_BUDGET_MIN;
    }
    if (budget > FV_PLAN_CACHE_BUDGET_MAX) {
        return FV_PLAN_CACHE_BUDGET_MAX;
    }
    return budget;
}

uint64_t FvPlan_BitmapBudget(uint64_t cache_budget) {
    uint64_t budget = cache_budget / 2U;

    return budget > FV_PLAN_BITMAP_BUDGET_MAX ? FV_PLAN_BITMAP_BUDGET_MAX : budget;
}

size_t FvPlan_SelectEvictions(
    const uint64_t* bytes,
    size_t count,
    size_t current,
    size_t radius,
    uint64_t budget,
    size_t* evicted,
    size_t evicted_capacity,
    uint64_t* remaining) {
    uint64_t total = 0;
    size_t evicted_count = 0;
    size_t max_distance;
    size_t distance;
    size_t index;

    for (index = 0; index < count; ++index) {
        uint64_t next = total + bytes[index];

        /* Насыщение вместо переполнения: такая сумма в любом случае над бюджетом. */
        total = next < total ? ~(uint64_t)0 : next;
    }
    if (count == 0 || current >= count) {
        if (remaining != NULL) {
            *remaining = total;
        }
        return 0;
    }

    max_distance = current > count - 1U - current ? current : count - 1U - current;
    for (distance = max_distance; distance > 0; --distance) {
        int side;

        for (side = 0; side < 2; ++side) {
            size_t candidate;

            if (side == 0) {
                if (distance > current) {
                    continue;
                }
                candidate = current - distance;
            } else {
                if (distance > count - 1U - current) {
                    continue;
                }
                candidate = current + distance;
            }
            if (bytes[candidate] == 0 || (distance <= radius && total <= budget)) {
                continue;
            }
            if (evicted_count >= evicted_capacity) {
                if (remaining != NULL) {
                    *remaining = total;
                }
                return evicted_count;
            }
            evicted[evicted_count++] = candidate;
            /* Насыщенная сумма точно неизвестна: остаётся «над бюджетом». */
            if (total != ~(uint64_t)0) {
                total = total >= bytes[candidate] ? total - bytes[candidate] : 0;
            }
        }
    }

    if (remaining != NULL) {
        *remaining = total;
    }
    return evicted_count;
}

int FvPlan_JobKeyEqual(const FvPlanJobKey* left, const FvPlanJobKey* right) {
    return left->generation == right->generation &&
        left->index == right->index &&
        left->box_width == right->box_width &&
        left->box_height == right->box_height &&
        left->max_dimension == right->max_dimension;
}

size_t FvPlan_FindJobKey(const FvPlanJobKey* keys, size_t count, const FvPlanJobKey* key) {
    size_t index;

    for (index = 0; index < count; ++index) {
        if (FvPlan_JobKeyEqual(&keys[index], key)) {
            return index;
        }
    }
    return count;
}

size_t FvPlan_PrefetchOrder(
    size_t count,
    size_t current,
    int direction,
    size_t ahead,
    size_t behind,
    size_t* order,
    size_t capacity) {
    size_t written = 0;
    size_t distance;
    size_t before = current;
    size_t after;

    if (current >= count) {
        return 0;
    }
    after = count - 1U - current;
    /* Кадров позади/впереди меньше окна — окно обрезается границей списка. */
    for (distance = 1; distance <= ahead && written < capacity; ++distance) {
        if (direction < 0) {
            if (distance > before) {
                break;
            }
            order[written++] = current - distance;
        } else {
            if (distance > after) {
                break;
            }
            order[written++] = current + distance;
        }
    }
    for (distance = 1; distance <= behind && written < capacity; ++distance) {
        if (direction < 0) {
            if (distance > after) {
                break;
            }
            order[written++] = current + distance;
        } else {
            if (distance > before) {
                break;
            }
            order[written++] = current - distance;
        }
    }
    return written;
}

static size_t fv_plan_distance(size_t left, size_t right) {
    return left > right ? left - right : right - left;
}

size_t FvPlan_FitPrefetch(
    uint64_t* bytes,
    size_t count,
    size_t current,
    size_t radius,
    uint64_t budget,
    const size_t* candidates,
    size_t candidate_count,
    uint64_t estimate,
    const uint64_t* known_bytes,
    size_t* evicted_scratch) {
    size_t accepted;
    size_t window = 0;

    if (current >= count) {
        return 0;
    }
    for (accepted = 0; accepted < candidate_count; ++accepted) {
        size_t candidate = candidates[accepted];
        size_t distance;
        size_t candidate_window;
        size_t evicted_count;
        size_t index;
        int rejected = 0;

        if (candidate >= count || candidate == current) {
            break;
        }
        distance = fv_plan_distance(candidate, current);
        if (distance > radius) {
            break;
        }
        candidate_window = distance > window ? distance : window;
        if (bytes[candidate] != 0) {
            /* Уже в кэше: расширяет окно, памяти не добавляет. */
            window = candidate_window;
            continue;
        }

        /* Известный по прежнему декоду размер точнее оценки по текущему кадру. */
        {
            uint64_t expected = known_bytes != NULL && known_bytes[candidate] != 0
                ? known_bytes[candidate]
                : estimate;

            /* Оценка 0 (пустой кадр) не отличалась бы от «не в кэше». */
            bytes[candidate] = expected != 0 ? expected : 1U;
        }
        evicted_count = FvPlan_SelectEvictions(
            bytes,
            count,
            current,
            radius,
            budget,
            evicted_scratch,
            count,
            NULL);
        for (index = 0; index < evicted_count; ++index) {
            if (fv_plan_distance(evicted_scratch[index], current) <= candidate_window) {
                rejected = 1;
                break;
            }
        }
        if (rejected) {
            bytes[candidate] = 0;
            break;
        }
        window = candidate_window;
    }
    return accepted;
}

int FvPlan_PrefetchContextEqual(
    const FvPlanPrefetchContext* left,
    const FvPlanPrefetchContext* right) {
    return left->generation == right->generation &&
        left->current == right->current &&
        (left->direction < 0) == (right->direction < 0) &&
        left->ahead == right->ahead &&
        left->behind == right->behind &&
        left->box_width == right->box_width &&
        left->box_height == right->box_height &&
        left->max_dimension == right->max_dimension &&
        left->budget == right->budget;
}

int FvPlan_PrefetchSuppressed(
    int suppressed,
    const FvPlanPrefetchContext* recorded,
    const FvPlanPrefetchContext* now) {
    return suppressed && FvPlan_PrefetchContextEqual(recorded, now);
}

int FvPlan_ResultDisposition(
    uint64_t job_generation,
    uint64_t current_generation,
    size_t index,
    size_t count,
    int succeeded,
    uint32_t cached_width,
    uint32_t cached_height,
    uint32_t result_width,
    uint32_t result_height,
    uint32_t max_dimension) {
    if (job_generation != current_generation || index >= count) {
        return FV_PLAN_RESULT_STALE;
    }
    if (!succeeded || result_width == 0 || result_height == 0) {
        return FV_PLAN_RESULT_FAILED;
    }
    if (cached_width == 0 || cached_height == 0) {
        return FV_PLAN_RESULT_ACCEPT;
    }
    if (max_dimension != 0 &&
        (cached_width > max_dimension || cached_height > max_dimension)) {
        return result_width <= max_dimension && result_height <= max_dimension
            ? FV_PLAN_RESULT_ACCEPT
            : FV_PLAN_RESULT_REDUNDANT;
    }
    if (max_dimension != 0 &&
        (result_width > max_dimension || result_height > max_dimension)) {
        return FV_PLAN_RESULT_REDUNDANT;
    }
    return result_width > cached_width || result_height > cached_height
        ? FV_PLAN_RESULT_ACCEPT
        : FV_PLAN_RESULT_REDUNDANT;
}
