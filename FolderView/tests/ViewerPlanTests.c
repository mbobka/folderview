/*
 * Нативные регрессии чистых расчётов вьювера (ViewerPlan.c): целевой размер
 * декодирования под экран, масштаб DCT, передекодирование при зуме,
 * бюджет кэша и порядок вытеснения кадров, фоновая загрузка (ключи заданий,
 * окно prefetch, бюджет prefetch, отбраковка устаревших результатов).
 */
#include "ViewerPlan.h"

#include <stdio.h>

#define EXPECT(condition) \
    do { \
        if (!(condition)) { \
            fprintf(stderr, "ViewerPlanTests: failure at line %d: %s\n", __LINE__, #condition); \
            return 1; \
        } \
    } while (0)

#define MIB ((uint64_t)1024 * 1024)
#define GIB ((uint64_t)1024 * 1024 * 1024)

static int test_fit_portrait_into_landscape_monitor(void) {
    uint32_t width = 0;
    uint32_t height = 0;

    /* Типичный кадр 3334x5000 под монитор 2560x1440: ограничивает высота. */
    FvPlan_FitTarget(3334, 5000, 2560, 1440, 16384, &width, &height);
    EXPECT(height == 1440);
    EXPECT(width == 960);

    /* Под 3840x2160. */
    FvPlan_FitTarget(3334, 5000, 3840, 2160, 16384, &width, &height);
    EXPECT(height == 2160);
    EXPECT(width == 1440);
    return 0;
}

static int test_fit_landscape(void) {
    uint32_t width = 0;
    uint32_t height = 0;

    /* 6000/4000 = 1,5 < 1920/1080: ограничивает высота. */
    FvPlan_FitTarget(6000, 4000, 1920, 1080, 0, &width, &height);
    EXPECT(width == 1620 && height == 1080);

    /* 6000/2000 = 3 > 1920/1080: ограничивает ширина. */
    FvPlan_FitTarget(6000, 2000, 1920, 1080, 0, &width, &height);
    EXPECT(width == 1920 && height == 640);
    return 0;
}

static int test_fit_never_upscales(void) {
    uint32_t width = 0;
    uint32_t height = 0;

    FvPlan_FitTarget(800, 600, 2560, 1440, 16384, &width, &height);
    EXPECT(width == 800 && height == 600);

    /* Нулевая рамка — без ограничения: полное разрешение. */
    FvPlan_FitTarget(3334, 5000, 0, 0, 0, &width, &height);
    EXPECT(width == 3334 && height == 5000);
    return 0;
}

static int test_fit_respects_max_bitmap_size(void) {
    uint32_t width = 0;
    uint32_t height = 0;

    /* Полное разрешение больше D2D-предела 16384 — вписываем в предел. */
    FvPlan_FitTarget(40000, 10000, 0, 0, 16384, &width, &height);
    EXPECT(width == 16384);
    EXPECT(height == 4096);

    FvPlan_FitTarget(10000, 40000, 0, 0, 16384, &width, &height);
    EXPECT(height == 16384);
    EXPECT(width == 4096);

    /* Рамка больше предела: предел всё равно действует. */
    FvPlan_FitTarget(20000, 20000, 30000, 30000, 8192, &width, &height);
    EXPECT(width == 8192 && height == 8192);
    return 0;
}

static int test_fit_extreme_aspect_never_zero(void) {
    uint32_t width = 0;
    uint32_t height = 0;

    FvPlan_FitTarget(100000, 1, 100, 100, 0, &width, &height);
    EXPECT(width == 100);
    EXPECT(height == 1);

    FvPlan_FitTarget(0, 10, 100, 100, 0, &width, &height);
    EXPECT(width == 0 && height == 0);

    FvPlan_FitTarget(0xFFFFFFFFU, 0xFFFFFFFFU, 0, 0, 0, &width, &height);
    EXPECT(width == 0xFFFFFFFFU && height == 0xFFFFFFFFU);

    FvPlan_FitTarget(0xFFFFFFFFU, 3, 0xFFFFFFFEU, 0, 0, &width, &height);
    EXPECT(width == 0xFFFFFFFEU);
    EXPECT(height == 3);
    return 0;
}

static int test_fit_box_with_one_side(void) {
    uint32_t width = 0;
    uint32_t height = 0;

    FvPlan_FitTarget(4000, 3000, 1000, 0, 0, &width, &height);
    EXPECT(width == 1000 && height == 750);
    FvPlan_FitTarget(4000, 3000, 0, 300, 0, &width, &height);
    EXPECT(width == 400 && height == 300);
    return 0;
}

static int test_dct_denominator(void) {
    /* 3334x5000 -> 960x1440: 1/2 = 1667x2500 подходит, 1/4 = 834x1250 мало. */
    EXPECT(FvPlan_ChooseDctDenominator(3334, 5000, 960, 1440) == 2);
    /* -> 1440x2160: 1/2 = 1667x2500 подходит. */
    EXPECT(FvPlan_ChooseDctDenominator(3334, 5000, 1440, 2160) == 2);
    /* Малая цель: 1/8 = 417x625 >= 400x600. */
    EXPECT(FvPlan_ChooseDctDenominator(3334, 5000, 400, 600) == 8);
    /* 1/4: 834x1250 >= 800x1200, 1/8 мало. */
    EXPECT(FvPlan_ChooseDctDenominator(3334, 5000, 800, 1200) == 4);
    /* Цель больше половины — только полное разрешение. */
    EXPECT(FvPlan_ChooseDctDenominator(3334, 5000, 2000, 3000) == 1);
    EXPECT(FvPlan_ChooseDctDenominator(3334, 5000, 3334, 5000) == 1);
    /* Округление вверх: 1001/2 = 501 >= 501. */
    EXPECT(FvPlan_ChooseDctDenominator(1001, 1001, 501, 501) == 2);
    EXPECT(FvPlan_ChooseDctDenominator(1000, 1000, 501, 501) == 1);
    EXPECT(FvPlan_ChooseDctDenominator(0, 1000, 1, 1) == 1);
    EXPECT(FvPlan_DivideCeil(0xFFFFFFFFU, 8) == 0x20000000U);
    EXPECT(FvPlan_DivideCeil(16, 8) == 2);
    EXPECT(FvPlan_DivideCeil(17, 8) == 3);
    EXPECT(FvPlan_DivideCeil(5, 0) == 5);
    return 0;
}

static int test_accept_dct_size(void) {
    /* 1/2 от 3334x5000 = 1667x2500 под 4K-цель 1440x2160: +15,8% — берём. */
    EXPECT(FvPlan_AcceptDctSize(1667, 2500, 1440, 2160, 25));
    /* Под 1440p-цель 960x1440: +74% — нужен scaler. */
    EXPECT(!FvPlan_AcceptDctSize(1667, 2500, 960, 1440, 25));
    /* Ровно граница допуска. */
    EXPECT(FvPlan_AcceptDctSize(125, 125, 100, 100, 25));
    EXPECT(!FvPlan_AcceptDctSize(126, 125, 100, 100, 25));
    /* Меньше цели — никогда. */
    EXPECT(!FvPlan_AcceptDctSize(99, 100, 100, 100, 25));
    /* Без допуска — только точное совпадение. */
    EXPECT(FvPlan_AcceptDctSize(100, 100, 100, 100, 0));
    EXPECT(!FvPlan_AcceptDctSize(101, 100, 100, 100, 0));
    /* Большие значения без переполнения. */
    EXPECT(FvPlan_AcceptDctSize(0xFFFFFFFFU, 0xFFFFFFFFU, 0xFFFFFFFFU, 0xFFFFFFFFU, 0xFFFFFFFFU));
    return 0;
}

static int test_needs_redecode(void) {
    /* Кадр под экран 960x1440, показ «вписать» 918x1377 — хватает. */
    EXPECT(!FvPlan_NeedsRedecode(3334, 5000, 960, 1440, 918, 1377, 16384));
    /* Ровно размер декода и +1 px допуска — хватает. */
    EXPECT(!FvPlan_NeedsRedecode(3334, 5000, 960, 1440, 961, 1441, 16384));
    /* Зум x2: показ 1836x2754 — нужен передекод. */
    EXPECT(FvPlan_NeedsRedecode(3334, 5000, 960, 1440, 1836, 2754, 16384));
    /* 1:1 — нужно полное разрешение. */
    EXPECT(FvPlan_NeedsRedecode(3334, 5000, 960, 1440, 3334, 5000, 16384));
    /* Уже полное разрешение: дальнейший зум не требует декода. */
    EXPECT(!FvPlan_NeedsRedecode(3334, 5000, 3334, 5000, 13336, 20000, 16384));
    /* Полное разрешение больше D2D-предела — передекодировать вниз. */
    EXPECT(FvPlan_NeedsRedecode(40000, 10000, 40000, 10000, 1000, 250, 16384));
    /* Уже вписан в предел, зум сверх предела — передекод не поможет. */
    EXPECT(!FvPlan_NeedsRedecode(40000, 10000, 16384, 4096, 80000, 20000, 16384));
    /* Свёрнутое окно (нулевой показ) — не передекодируем. */
    EXPECT(!FvPlan_NeedsRedecode(3334, 5000, 960, 1440, 0, 0, 16384));
    /* Пустой декод — нужен. */
    EXPECT(FvPlan_NeedsRedecode(3334, 5000, 0, 0, 100, 100, 16384));
    return 0;
}

static int test_decode_target(void) {
    uint32_t width = 0;
    uint32_t height = 0;

    /* Портрет под 1440p: DCT 1/2 достигает цели — уменьшаем. */
    FvPlan_DecodeTarget(3334, 5000, 2560, 1440, 16384, &width, &height);
    EXPECT(width == 960 && height == 1440);
    /* Пейзаж 5000x3334 под 4K: цель 3239x2160 больше половины — полный кадр. */
    FvPlan_DecodeTarget(5000, 3334, 3840, 2160, 16384, &width, &height);
    EXPECT(width == 5000 && height == 3334);
    /* Тот же пейзаж под 1440p: 2160x1440, DCT 1/2 = 2500x1667 — уменьшаем. */
    FvPlan_DecodeTarget(5000, 3334, 2560, 1440, 16384, &width, &height);
    EXPECT(width == 2160 && height == 1440);
    /* Зум x2 от «вписать»: показ 1836x2754 больше половины — полный кадр. */
    FvPlan_DecodeTarget(3334, 5000, 1836, 2754, 16384, &width, &height);
    EXPECT(width == 3334 && height == 5000);
    /* Ровно половина — DCT 1/2 ещё достигает цели. */
    FvPlan_DecodeTarget(4000, 6000, 2000, 3000, 0, &width, &height);
    EXPECT(width == 2000 && height == 3000);
    /* Полный кадр больше предела D2D — вписываем в предел даже без DCT. */
    FvPlan_DecodeTarget(20000, 10000, 15000, 7500, 16384, &width, &height);
    EXPECT(width == 15000 && height == 7500);
    FvPlan_DecodeTarget(20000, 10000, 0, 0, 16384, &width, &height);
    EXPECT(width == 16384 && height == 8192);
    /* Меньше рамки — без изменений. */
    FvPlan_DecodeTarget(800, 600, 2560, 1440, 16384, &width, &height);
    EXPECT(width == 800 && height == 600);
    return 0;
}

static int test_cache_budget(void) {
    EXPECT(FvPlan_CacheBudget(0) == 512 * MIB);
    EXPECT(FvPlan_CacheBudget(1 * GIB) == 256 * MIB);
    EXPECT(FvPlan_CacheBudget(4 * GIB) == 512 * MIB);
    EXPECT(FvPlan_CacheBudget(8 * GIB) == 1 * GIB);
    EXPECT(FvPlan_CacheBudget(16 * GIB) == 2 * GIB);
    EXPECT(FvPlan_CacheBudget(64 * GIB) == 2 * GIB);
    EXPECT(FvPlan_CacheBudget(~(uint64_t)0) == 2 * GIB);
    EXPECT(FvPlan_BitmapBudget(256 * MIB) == 128 * MIB);
    EXPECT(FvPlan_BitmapBudget(1 * GIB) == 512 * MIB);
    EXPECT(FvPlan_BitmapBudget(2 * GIB) == 512 * MIB);
    return 0;
}

static int test_evictions_by_radius(void) {
    uint64_t bytes[10] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
    size_t evicted[10];
    uint64_t remaining = 0;
    size_t count;

    /* Текущий 5, радиус 2: за радиусом 0,1,2 и 8,9; бюджет не ограничивает. */
    count = FvPlan_SelectEvictions(bytes, 10, 5, 2, 1000, evicted, 10, &remaining);
    EXPECT(count == 5);
    /* От дальних к ближним; на равном расстоянии сначала позади. */
    EXPECT(evicted[0] == 0);
    EXPECT(evicted[1] == 1);
    EXPECT(evicted[2] == 9);
    EXPECT(evicted[3] == 2);
    EXPECT(evicted[4] == 8);
    EXPECT(remaining == 5);
    return 0;
}

static int test_evictions_by_budget_farthest_first(void) {
    uint64_t bytes[7] = {10, 10, 10, 10, 10, 10, 10};
    size_t evicted[7];
    uint64_t remaining = 0;
    size_t count;

    /* Текущий 3, сумма 70, бюджет 35: вытесняем 0, 6, 1, 5 -> 30. */
    count = FvPlan_SelectEvictions(bytes, 7, 3, 1000, 35, evicted, 7, &remaining);
    EXPECT(count == 4);
    EXPECT(evicted[0] == 0);
    EXPECT(evicted[1] == 6);
    EXPECT(evicted[2] == 1);
    EXPECT(evicted[3] == 5);
    EXPECT(remaining == 30);
    return 0;
}

static int test_evictions_never_current(void) {
    uint64_t bytes[3] = {0, 500, 0};
    size_t evicted[3];
    uint64_t remaining = 0;
    size_t count;

    /* Текущий кадр сам по себе больше бюджета — всё равно остаётся. */
    count = FvPlan_SelectEvictions(bytes, 3, 1, 0, 100, evicted, 3, &remaining);
    EXPECT(count == 0);
    EXPECT(remaining == 500);

    {
        uint64_t many[4] = {100, 100, 100, 100};

        count = FvPlan_SelectEvictions(many, 4, 0, 1000, 0, evicted, 3, &remaining);
        EXPECT(count == 3);
        EXPECT(evicted[0] == 3 && evicted[1] == 2 && evicted[2] == 1);
        EXPECT(remaining == 100);
    }
    return 0;
}

static int test_evictions_skip_empty_and_edges(void) {
    uint64_t bytes[6] = {0, 40, 0, 0, 0, 40};
    size_t evicted[6];
    uint64_t remaining = 0;
    size_t count;

    /* Текущий последний: кандидаты только позади. */
    count = FvPlan_SelectEvictions(bytes, 6, 5, 1000, 0, evicted, 6, &remaining);
    EXPECT(count == 1);
    EXPECT(evicted[0] == 1);
    EXPECT(remaining == 40);

    /* Бюджет не превышен и радиус велик — ничего. */
    count = FvPlan_SelectEvictions(bytes, 6, 2, 1000, 80, evicted, 6, &remaining);
    EXPECT(count == 0);
    EXPECT(remaining == 80);

    /* Пустой кэш и некорректный current. */
    EXPECT(FvPlan_SelectEvictions(bytes, 0, 0, 0, 0, evicted, 6, &remaining) == 0);
    EXPECT(FvPlan_SelectEvictions(bytes, 6, 6, 0, 0, evicted, 6, &remaining) == 0);

    /* Ограниченная ёмкость выхода. */
    {
        uint64_t many[5] = {1, 1, 1, 1, 1};

        count = FvPlan_SelectEvictions(many, 5, 2, 0, 0, evicted, 2, &remaining);
        EXPECT(count == 2);
        EXPECT(evicted[0] == 0 && evicted[1] == 4);
        EXPECT(remaining == 3);
    }
    return 0;
}

static int test_evictions_saturating_total(void) {
    uint64_t bytes[3] = {~(uint64_t)0, 1, ~(uint64_t)0};
    size_t evicted[3];
    uint64_t remaining = 0;
    size_t count;

    /* Переполненная сумма насыщается и остаётся «над бюджетом» до конца. */
    count = FvPlan_SelectEvictions(bytes, 3, 1, 1000, 10, evicted, 3, &remaining);
    EXPECT(count == 2);
    EXPECT(evicted[0] == 0 && evicted[1] == 2);
    EXPECT(remaining == ~(uint64_t)0);
    return 0;
}

static FvPlanJobKey make_key(uint64_t generation, size_t index, uint32_t box, int reason) {
    FvPlanJobKey key;

    key.generation = generation;
    key.index = index;
    key.box_width = box;
    key.box_height = box;
    key.max_dimension = 16384;
    key.reason = reason;
    return key;
}

static int test_job_key_match(void) {
    FvPlanJobKey keys[3];
    FvPlanJobKey probe;

    keys[0] = make_key(1, 5, 1440, FV_PLAN_REASON_CURRENT);
    keys[1] = make_key(1, 6, 1440, FV_PLAN_REASON_PREFETCH);
    keys[2] = make_key(1, 5, 5000, FV_PLAN_REASON_UPGRADE);

    /* Причина не входит в ключ: prefetch того же кадра — то же задание. */
    probe = make_key(1, 6, 1440, FV_PLAN_REASON_CURRENT);
    EXPECT(FvPlan_FindJobKey(keys, 3, &probe) == 1);
    /* Другая рамка (передекод при зуме) — другое задание. */
    probe = make_key(1, 5, 5000, FV_PLAN_REASON_CURRENT);
    EXPECT(FvPlan_FindJobKey(keys, 3, &probe) == 2);
    /* Другое поколение списка — нет совпадения. */
    probe = make_key(2, 5, 1440, FV_PLAN_REASON_CURRENT);
    EXPECT(FvPlan_FindJobKey(keys, 3, &probe) == 3);
    /* Другой предел стороны (новый render target) — другое задание. */
    probe = keys[0];
    probe.max_dimension = 8192;
    EXPECT(!FvPlan_JobKeyEqual(&probe, &keys[0]));
    EXPECT(FvPlan_FindJobKey(keys, 0, &probe) == 0);
    return 0;
}

static int test_prefetch_order_forward_and_backward(void) {
    size_t order[16];
    size_t count;

    /* Вперёд: +1, +2, +3, затем -1. */
    count = FvPlan_PrefetchOrder(100, 10, 1, 3, 1, order, 16);
    EXPECT(count == 4);
    EXPECT(order[0] == 11 && order[1] == 12 && order[2] == 13 && order[3] == 9);

    /* Назад: -1, -2, -3, затем +1. */
    count = FvPlan_PrefetchOrder(100, 10, -1, 3, 1, order, 16);
    EXPECT(count == 4);
    EXPECT(order[0] == 9 && order[1] == 8 && order[2] == 7 && order[3] == 11);

    /* Направление 0 (первый показ) — как вперёд. */
    count = FvPlan_PrefetchOrder(100, 0, 0, 2, 1, order, 16);
    EXPECT(count == 2);
    EXPECT(order[0] == 1 && order[1] == 2);
    return 0;
}

static int test_prefetch_order_respects_bounds(void) {
    size_t order[16];
    size_t count;

    /* Конец списка (граница ZIP или папки): вперёд нечего, назад 1. */
    count = FvPlan_PrefetchOrder(10, 9, 1, 3, 1, order, 16);
    EXPECT(count == 1);
    EXPECT(order[0] == 8);
    /* Предпоследний: вперёд только 9. */
    count = FvPlan_PrefetchOrder(10, 8, 1, 3, 2, order, 16);
    EXPECT(count == 3);
    EXPECT(order[0] == 9 && order[1] == 7 && order[2] == 6);
    /* Начало списка при листании назад. */
    count = FvPlan_PrefetchOrder(10, 0, -1, 3, 1, order, 16);
    EXPECT(count == 1);
    EXPECT(order[0] == 1);
    /* Один кадр, пустой список, current вне списка. */
    EXPECT(FvPlan_PrefetchOrder(1, 0, 1, 3, 1, order, 16) == 0);
    EXPECT(FvPlan_PrefetchOrder(0, 0, 1, 3, 1, order, 16) == 0);
    EXPECT(FvPlan_PrefetchOrder(5, 5, 1, 3, 1, order, 16) == 0);
    /* Ограниченная ёмкость. */
    count = FvPlan_PrefetchOrder(100, 50, 1, 3, 1, order, 2);
    EXPECT(count == 2);
    EXPECT(order[0] == 51 && order[1] == 52);
    /* Пустое окно. */
    EXPECT(FvPlan_PrefetchOrder(100, 50, 1, 0, 0, order, 16) == 0);
    /* Крайние индексы без переполнения. */
    count = FvPlan_PrefetchOrder(~(size_t)0, ~(size_t)0 - 1U, 1, 3, 1, order, 16);
    EXPECT(count == 1);
    EXPECT(order[0] == ~(size_t)0 - 2U);
    return 0;
}

static int test_fit_prefetch_within_budget(void) {
    uint64_t bytes[20] = {0};
    size_t scratch[20];
    size_t candidates[4] = {11, 12, 13, 9};
    size_t accepted;

    /* Текущий 10 (10 байт), кадр ~10 байт, бюджет 100: всё окно помещается. */
    bytes[10] = 10;
    accepted = FvPlan_FitPrefetch(bytes, 20, 10, 50, 100, candidates, 4, 10, NULL, scratch);
    EXPECT(accepted == 4);
    EXPECT(bytes[11] == 10 && bytes[12] == 10 && bytes[13] == 10 && bytes[9] == 10);
    return 0;
}

static int test_fit_prefetch_stops_before_thrashing(void) {
    uint64_t bytes[20] = {0};
    size_t scratch[20];
    size_t candidates[4] = {11, 12, 13, 9};
    size_t accepted;

    /* Бюджет 35 на текущий 10 и кадры по 10: влезают только +1 и +2. */
    bytes[10] = 10;
    accepted = FvPlan_FitPrefetch(bytes, 20, 10, 50, 35, candidates, 4, 10, NULL, scratch);
    EXPECT(accepted == 2);
    EXPECT(bytes[11] == 10 && bytes[12] == 10);
    /* Отвергнутый кандидат не оставляет оценку в массиве. */
    EXPECT(bytes[13] == 0 && bytes[9] == 0);
    return 0;
}

static int test_fit_prefetch_evicts_only_far_frames(void) {
    uint64_t bytes[20] = {0};
    size_t scratch[20];
    size_t candidates[4] = {11, 12, 13, 9};
    size_t accepted;

    /* Дальние кадры 0 и 19 вытесняются ради окна — это не раскачка. */
    bytes[0] = 10;
    bytes[19] = 10;
    bytes[10] = 10;
    accepted = FvPlan_FitPrefetch(bytes, 20, 10, 50, 50, candidates, 4, 10, NULL, scratch);
    EXPECT(accepted == 4);

    /* Кадр внутри окна (-3, уже показанный) не вытесняется ради prefetch. */
    {
        uint64_t near_bytes[20] = {0};

        near_bytes[10] = 10;
        near_bytes[7] = 10;
        accepted = FvPlan_FitPrefetch(near_bytes, 20, 10, 50, 40, candidates, 4, 10, NULL, scratch);
        /*
         * +1, +2 (сумма 40) — да; с +3 сумма 50 и первым (позади, на том же
         * расстоянии 3) вытеснился бы кадр 7 — внутри окна, поэтому нет.
         */
        EXPECT(accepted == 2);
        EXPECT(near_bytes[13] == 0 && near_bytes[7] == 10);
    }
    return 0;
}

static int test_fit_prefetch_cached_and_limits(void) {
    uint64_t bytes[20] = {0};
    size_t scratch[20];
    size_t candidates[4] = {11, 12, 13, 9};
    size_t bad_candidates[2] = {10, 11};
    size_t accepted;

    /* Уже закэшированный кандидат принимается без добавления памяти. */
    bytes[10] = 10;
    bytes[11] = 10;
    accepted = FvPlan_FitPrefetch(bytes, 20, 10, 50, 20, candidates, 4, 10, NULL, scratch);
    EXPECT(accepted == 1);
    EXPECT(bytes[11] == 10 && bytes[12] == 0);

    /* Радиус кэша: кандидат дальше радиуса не берётся. */
    {
        uint64_t zero[20] = {0};

        zero[10] = 1;
        accepted = FvPlan_FitPrefetch(zero, 20, 10, 2, 1000, candidates, 4, 1, NULL, scratch);
        EXPECT(accepted == 2);
    }
    /* Текущий кадр в списке кандидатов и некорректный current. */
    EXPECT(FvPlan_FitPrefetch(bytes, 20, 10, 50, 1000, bad_candidates, 2, 1, NULL, scratch) == 0);
    EXPECT(FvPlan_FitPrefetch(bytes, 20, 20, 50, 1000, candidates, 4, 1, NULL, scratch) == 0);
    /* Сам текущий кадр больше бюджета: prefetch не начинается. */
    {
        uint64_t big[20] = {0};

        big[10] = 500;
        accepted = FvPlan_FitPrefetch(big, 20, 10, 50, 100, candidates, 4, 10, NULL, scratch);
        EXPECT(accepted == 0);
        EXPECT(big[11] == 0);
    }
    return 0;
}

/*
 * Сценарий бесконечного prefetch: неразвёрнутое окно (полное разрешение), бюджет
 * 512 МиБ, текущий кадр 100x100 (оценка ~80 КБ), сосед 12000x12000
 * (~549 МиБ). По оценке сосед принимается, после декода вытесняется — без
 * известного размера он заказывался бы снова и снова.
 */
static int test_fit_prefetch_mixed_sizes_no_loop(void) {
    const uint64_t budget = 512 * MIB;
    const uint64_t current_bytes = 100ULL * 100ULL * 4ULL;
    const uint64_t estimate = current_bytes * 2ULL;
    const uint64_t neighbour_bytes = 12000ULL * 12000ULL * 4ULL;
    uint64_t bytes[20] = {0};
    uint64_t known[20] = {0};
    size_t scratch[20];
    size_t candidates[1] = {11};
    size_t accepted;
    size_t evicted_count;

    /* Оценка по текущему кадру: кандидат принят (это и давало цикл). */
    bytes[10] = current_bytes;
    accepted = FvPlan_FitPrefetch(bytes, 20, 10, 50, budget, candidates, 1, estimate, NULL, scratch);
    EXPECT(accepted == 1);

    /* После декода trim (тот же отбор) вытесняет его по бюджету. */
    bytes[11] = neighbour_bytes;
    evicted_count = FvPlan_SelectEvictions(bytes, 20, 10, 50, budget, scratch, 20, NULL);
    EXPECT(evicted_count == 1 && scratch[0] == 11);

    /* Фактический размер известен — повторного заказа нет. */
    bytes[11] = 0;
    known[11] = neighbour_bytes;
    accepted = FvPlan_FitPrefetch(bytes, 20, 10, 50, budget, candidates, 1, estimate, known, scratch);
    EXPECT(accepted == 0);
    EXPECT(bytes[11] == 0);
    return 0;
}

static int test_fit_prefetch_known_smaller_than_estimate(void) {
    uint64_t bytes[20] = {0};
    uint64_t known[20] = {0};
    size_t scratch[20];
    size_t candidates[2] = {11, 12};
    size_t accepted;

    /* Текущий кадр большой (оценка 60), соседи маленькие (известно 5). */
    bytes[10] = 30;
    known[11] = 5;
    known[12] = 5;
    accepted = FvPlan_FitPrefetch(bytes, 20, 10, 50, 50, candidates, 2, 60, NULL, scratch);
    EXPECT(accepted == 0);
    accepted = FvPlan_FitPrefetch(bytes, 20, 10, 50, 50, candidates, 2, 60, known, scratch);
    EXPECT(accepted == 2);
    EXPECT(bytes[11] == 5 && bytes[12] == 5);
    return 0;
}

static int test_prefetch_suppression_context(void) {
    FvPlanPrefetchContext recorded;
    FvPlanPrefetchContext now;

    recorded.generation = 3;
    recorded.current = 10;
    recorded.direction = 1;
    recorded.ahead = 3;
    recorded.behind = 1;
    recorded.box_width = 0;
    recorded.box_height = 0;
    recorded.max_dimension = 16384;
    recorded.budget = 512 * MIB;

    /* Те же условия — кадр, вытесненный сразу после prefetch, не заказываем. */
    now = recorded;
    EXPECT(FvPlan_PrefetchSuppressed(1, &recorded, &now));
    EXPECT(!FvPlan_PrefetchSuppressed(0, &recorded, &now));
    /* Направление — по знаку. */
    now.direction = 2;
    EXPECT(FvPlan_PrefetchSuppressed(1, &recorded, &now));

    /* Любое изменение условий снимает подавление. */
    now = recorded; now.generation = 4;
    EXPECT(!FvPlan_PrefetchSuppressed(1, &recorded, &now));
    now = recorded; now.current = 11;
    EXPECT(!FvPlan_PrefetchSuppressed(1, &recorded, &now));
    now = recorded; now.direction = -1;
    EXPECT(!FvPlan_PrefetchSuppressed(1, &recorded, &now));
    now = recorded; now.ahead = 2;
    EXPECT(!FvPlan_PrefetchSuppressed(1, &recorded, &now));
    now = recorded; now.behind = 0;
    EXPECT(!FvPlan_PrefetchSuppressed(1, &recorded, &now));
    now = recorded; now.box_width = 3840; now.box_height = 2160;
    EXPECT(!FvPlan_PrefetchSuppressed(1, &recorded, &now));
    now = recorded; now.max_dimension = 8192;
    EXPECT(!FvPlan_PrefetchSuppressed(1, &recorded, &now));
    now = recorded; now.budget = 1024 * MIB;
    EXPECT(!FvPlan_PrefetchSuppressed(1, &recorded, &now));
    return 0;
}

static int test_result_disposition(void) {
    /* Пустой кэш, то же поколение — принимаем. */
    EXPECT(FvPlan_ResultDisposition(3, 3, 5, 10, 1, 0, 0, 960, 1440, 16384) == FV_PLAN_RESULT_ACCEPT);
    /* Поколение сменилось (другой ZIP) — устарел. */
    EXPECT(FvPlan_ResultDisposition(2, 3, 5, 10, 1, 0, 0, 960, 1440, 16384) == FV_PLAN_RESULT_STALE);
    /* Индекс вне нового списка — устарел. */
    EXPECT(FvPlan_ResultDisposition(3, 3, 10, 10, 1, 0, 0, 960, 1440, 16384) == FV_PLAN_RESULT_STALE);
    /* Ошибка/отмена. */
    EXPECT(FvPlan_ResultDisposition(3, 3, 5, 10, 0, 0, 0, 0, 0, 16384) == FV_PLAN_RESULT_FAILED);
    EXPECT(FvPlan_ResultDisposition(3, 3, 5, 10, 1, 0, 0, 0, 1440, 16384) == FV_PLAN_RESULT_FAILED);
    /* Передекод при зуме: больше кэша — заменяем. */
    EXPECT(FvPlan_ResultDisposition(3, 3, 5, 10, 1, 960, 1440, 3334, 5000, 16384) == FV_PLAN_RESULT_ACCEPT);
    /* Prefetch пришёл после передекода в полное разрешение — не нужен. */
    EXPECT(FvPlan_ResultDisposition(3, 3, 5, 10, 1, 3334, 5000, 960, 1440, 16384) == FV_PLAN_RESULT_REDUNDANT);
    /* Тот же размер — не нужен. */
    EXPECT(FvPlan_ResultDisposition(3, 3, 5, 10, 1, 960, 1440, 960, 1440, 16384) == FV_PLAN_RESULT_REDUNDANT);
    /* Кэш больше предела битмапа — уменьшенный результат нужен. */
    EXPECT(FvPlan_ResultDisposition(3, 3, 5, 10, 1, 20000, 10000, 8192, 4096, 8192) == FV_PLAN_RESULT_ACCEPT);
    /* ... но не тот, что сам больше предела. */
    EXPECT(FvPlan_ResultDisposition(3, 3, 5, 10, 1, 20000, 10000, 16384, 8192, 8192) == FV_PLAN_RESULT_REDUNDANT);
    /* Больше предела при кэше в пределе — не заменяем. */
    EXPECT(FvPlan_ResultDisposition(3, 3, 5, 10, 1, 4096, 2048, 16384, 8192, 8192) == FV_PLAN_RESULT_REDUNDANT);
    /* Без предела. */
    EXPECT(FvPlan_ResultDisposition(3, 3, 5, 10, 1, 4096, 2048, 16384, 8192, 0) == FV_PLAN_RESULT_ACCEPT);
    return 0;
}

typedef struct TestCase {
    const char* name;
    int (*run)(void);
} TestCase;

int main(void) {
    static const TestCase tests[] = {
        {"fit_portrait_into_landscape_monitor", test_fit_portrait_into_landscape_monitor},
        {"fit_landscape", test_fit_landscape},
        {"fit_never_upscales", test_fit_never_upscales},
        {"fit_respects_max_bitmap_size", test_fit_respects_max_bitmap_size},
        {"fit_extreme_aspect_never_zero", test_fit_extreme_aspect_never_zero},
        {"fit_box_with_one_side", test_fit_box_with_one_side},
        {"dct_denominator", test_dct_denominator},
        {"accept_dct_size", test_accept_dct_size},
        {"needs_redecode", test_needs_redecode},
        {"decode_target", test_decode_target},
        {"cache_budget", test_cache_budget},
        {"evictions_by_radius", test_evictions_by_radius},
        {"evictions_by_budget_farthest_first", test_evictions_by_budget_farthest_first},
        {"evictions_never_current", test_evictions_never_current},
        {"evictions_skip_empty_and_edges", test_evictions_skip_empty_and_edges},
        {"evictions_saturating_total", test_evictions_saturating_total},
        {"job_key_match", test_job_key_match},
        {"prefetch_order_forward_and_backward", test_prefetch_order_forward_and_backward},
        {"prefetch_order_respects_bounds", test_prefetch_order_respects_bounds},
        {"fit_prefetch_within_budget", test_fit_prefetch_within_budget},
        {"fit_prefetch_stops_before_thrashing", test_fit_prefetch_stops_before_thrashing},
        {"fit_prefetch_evicts_only_far_frames", test_fit_prefetch_evicts_only_far_frames},
        {"fit_prefetch_cached_and_limits", test_fit_prefetch_cached_and_limits},
        {"fit_prefetch_mixed_sizes_no_loop", test_fit_prefetch_mixed_sizes_no_loop},
        {"fit_prefetch_known_smaller_than_estimate", test_fit_prefetch_known_smaller_than_estimate},
        {"prefetch_suppression_context", test_prefetch_suppression_context},
        {"result_disposition", test_result_disposition},
    };
    size_t index;
    size_t count = sizeof(tests) / sizeof(tests[0]);

    /* Без буферизации: при аварийном останове (UBSan trap) виден последний тест. */
    setvbuf(stdout, NULL, _IONBF, 0);
    for (index = 0; index < count; ++index) {
        if (tests[index].run() != 0) {
            fprintf(stderr, "ViewerPlanTests: FAILED %s\n", tests[index].name);
            return 1;
        }
        printf("ViewerPlanTests: ok - %s\n", tests[index].name);
    }
    printf("ViewerPlanTests: %zu tests OK\n", count);
    return 0;
}
