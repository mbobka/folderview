#ifndef FOLDERVIEW_VIEWER_PLAN_H
#define FOLDERVIEW_VIEWER_PLAN_H

#include <stddef.h>
#include <stdint.h>

/*
 * Чистые расчёты вьювера без WinAPI, CRT и аллокаций: целевой размер
 * декодирования, масштаб DCT для JPEG, необходимость передекодирования,
 * бюджет и порядок вытеснения кэша кадров. Проверяются нативными
 * C-регрессиями под UBSan (tests/ViewerPlanTests.c) до Windows-сборки.
 */

#define FV_PLAN_MIB ((uint64_t)1024 * 1024)
/* Бюджет кэша кадров: 1/8 физической памяти в пределах [256 МиБ; 2 ГиБ]. */
#define FV_PLAN_CACHE_BUDGET_MIN (256 * FV_PLAN_MIB)
#define FV_PLAN_CACHE_BUDGET_MAX (2048 * FV_PLAN_MIB)
/* Объём памяти неизвестен — консервативный бюджет. */
#define FV_PLAN_CACHE_BUDGET_UNKNOWN (512 * FV_PLAN_MIB)
/* Доля бюджета под D2D-битмапы (видеопамять): половина, но не больше 512 МиБ. */
#define FV_PLAN_BITMAP_BUDGET_MAX (512 * FV_PLAN_MIB)

/*
 * Вписывает source_width x source_height в рамку box_width x box_height с
 * сохранением пропорций и никогда не увеличивает. Нулевая сторона рамки — без
 * ограничения по ней. max_dimension (0 — без ограничения) — предел каждой
 * стороны результата (максимальный размер D2D-битмапа). Результат не меньше
 * 1x1; при нулевом исходном размере — 0x0.
 */
void FvPlan_FitTarget(
    uint32_t source_width,
    uint32_t source_height,
    uint32_t box_width,
    uint32_t box_height,
    uint32_t max_dimension,
    uint32_t* target_width,
    uint32_t* target_height);

/*
 * Наибольший делитель DCT-масштаба JPEG из {8, 4, 2}, при котором размер
 * ceil(source / d) по обеим сторонам не меньше целевого; иначе 1 (полное
 * разрешение). Масштаб DCT заметно дешевле полного декодирования.
 */
uint32_t FvPlan_ChooseDctDenominator(
    uint32_t source_width,
    uint32_t source_height,
    uint32_t target_width,
    uint32_t target_height);

/*
 * Можно ли взять нативный DCT-размер вместо точного целевого (без
 * IWICBitmapScaler): не меньше цели и больше неё не более чем на
 * slack_percent процентов по каждой стороне.
 */
int FvPlan_AcceptDctSize(
    uint32_t dct_width,
    uint32_t dct_height,
    uint32_t target_width,
    uint32_t target_height,
    uint32_t slack_percent);

/* ceil(value / denominator), denominator > 0. */
uint32_t FvPlan_DivideCeil(uint32_t value, uint32_t denominator);

/*
 * Нужно ли передекодировать кадр, декодированный в decoded_width x
 * decoded_height, чтобы показать его в display_width x display_height
 * пикселей экрана (размер исходного кадра, умноженный на масштаб показа,
 * округлённый вверх). TRUE (1), если декодированный кадр превышает
 * max_dimension (0 — без ограничения) или меньше нужного более чем на 1 px
 * по какой-либо стороне. Нужный размер — FvPlan_FitTarget(source, display,
 * max_dimension): больше исходного разрешения передекодирование не просит.
 */
int FvPlan_NeedsRedecode(
    uint32_t source_width,
    uint32_t source_height,
    uint32_t decoded_width,
    uint32_t decoded_height,
    uint32_t display_width,
    uint32_t display_height,
    uint32_t max_dimension);

/*
 * Размер декодирования кадра под рамку: FvPlan_FitTarget, но если цель
 * больше половины исходного кадра по какой-либо стороне (масштаб DCT её не
 * достигает), уменьшение стоило бы IWICBitmapScaler поверх полного
 * декодирования — дороже, чем сэкономит (замер: +10–25 мс на 16,7 Мп).
 * Тогда возвращается полное разрешение, если оно в пределе max_dimension;
 * остаток уменьшения (меньше двукратного) делает D2D при отрисовке.
 */
void FvPlan_DecodeTarget(
    uint32_t source_width,
    uint32_t source_height,
    uint32_t box_width,
    uint32_t box_height,
    uint32_t max_dimension,
    uint32_t* target_width,
    uint32_t* target_height);

/*
 * Бюджет кэша кадров в байтах (пиксели + D2D-битмапы) по объёму физической
 * памяти: 1/8, но в пределах [FV_PLAN_CACHE_BUDGET_MIN; _MAX]; 0 —
 * FV_PLAN_CACHE_BUDGET_UNKNOWN.
 */
uint64_t FvPlan_CacheBudget(uint64_t physical_memory_bytes);

/* Часть бюджета под D2D-битмапы: половина, не больше FV_PLAN_BITMAP_BUDGET_MAX. */
uint64_t FvPlan_BitmapBudget(uint64_t cache_budget);

/*
 * Выбирает кадры для вытеснения. bytes[i] — сколько байт держит кадр i
 * (0 — пусто). Кадры перебираются от самых дальних от current к ближним; на
 * равном расстоянии первым идёт кадр позади текущего (листают обычно вперёд).
 * Вытесняется любой непустой кадр дальше radius, а затем — пока сумма
 * превышает budget. Текущий кадр не вытесняется никогда. Индексы пишутся в
 * evicted (не больше evicted_capacity), возвращается их число; *remaining —
 * итоговая сумма байт. Переполнение суммы насыщается и считается «над
 * бюджетом» (вытесняется всё, кроме текущего кадра).
 */
size_t FvPlan_SelectEvictions(
    const uint64_t* bytes,
    size_t count,
    size_t current,
    size_t radius,
    uint64_t budget,
    size_t* evicted,
    size_t evicted_capacity,
    uint64_t* remaining);

/*
 * Фоновая загрузка кадров: задания рабочего потока, их приоритеты,
 * окно prefetch и отбраковка устаревших результатов.
 */

/* Причина задания; меньшее значение — выше приоритет. */
#define FV_PLAN_REASON_CURRENT 0
#define FV_PLAN_REASON_UPGRADE 1
#define FV_PLAN_REASON_PREFETCH 2

/* Окно prefetch по умолчанию: вперёд по направлению листания и назад. */
#define FV_PLAN_PREFETCH_AHEAD 3U
#define FV_PLAN_PREFETCH_BEHIND 1U
/* Предел окна (настройка FOLDERVIEW_PREFETCH ограничивается им). */
#define FV_PLAN_PREFETCH_MAX 8U

/*
 * Ключ задания: одно и то же задание — тот же кадр того же поколения списка с
 * той же рамкой и пределом стороны. Причина в ключ не входит: prefetch,
 * ставший текущим кадром, не перезапускается, а повышается в приоритете.
 */
typedef struct FvPlanJobKey {
    uint64_t generation;
    size_t index;
    uint32_t box_width;
    uint32_t box_height;
    uint32_t max_dimension;
    int reason;
} FvPlanJobKey;

int FvPlan_JobKeyEqual(const FvPlanJobKey* left, const FvPlanJobKey* right);

/* Индекс первого ключа, равного key (FvPlan_JobKeyEqual), иначе count. */
size_t FvPlan_FindJobKey(const FvPlanJobKey* keys, size_t count, const FvPlanJobKey* key);

/*
 * Порядок prefetch вокруг current: сначала до ahead кадров по направлению
 * листания (direction < 0 — назад, иначе вперёд) по возрастанию расстояния,
 * затем до behind кадров в обратную сторону. Не выходит за [0; count) — то
 * есть за границу ZIP или папки. Пишет не больше capacity
 * индексов, возвращает их число.
 */
size_t FvPlan_PrefetchOrder(
    size_t count,
    size_t current,
    int direction,
    size_t ahead,
    size_t behind,
    size_t* order,
    size_t capacity);

/*
 * Сколько первых кандидатов prefetch можно декодировать, не раскачивая кэш.
 * bytes[i] — байты кадра i в кэше (0 — пусто); на выходе к нему прибавлены
 * оценки estimate принятых незакэшированных кандидатов. Кандидат
 * принимается, если с ним FvPlan_SelectEvictions (тот же radius и budget)
 * не вытесняет ни один кадр в пределах окна (расстояние от current не
 * больше самого дальнего принятого кандидата) — в том числе сам кандидат и
 * текущий кадр. Первый неподходящий кандидат (или дальше radius) обрывает
 * перебор. evicted_scratch — рабочий массив на count элементов.
 *
 * known_bytes (может быть NULL) — фактический размер кадра i в кэше после
 * прежнего декода под ту же рамку (0 — неизвестен): он берётся вместо
 * estimate. Оценка по текущему кадру ошибается, если кадры архива разного
 * размера (обложка 100x100 и соседний кадр 12000x12000), и тогда принятый
 * кандидат после декода сразу вытеснялся бы и заказывался снова.
 */
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
    size_t* evicted_scratch);

/*
 * Условия, при которых кадр prefetch был вытеснен по бюджету сразу после
 * декода. Такой кадр не заказывается снова, пока условия не изменятся:
 * поколение списка, текущий кадр, окно prefetch (направление, вперёд,
 * назад), рамка декода и предел стороны, бюджет кэша.
 */
typedef struct FvPlanPrefetchContext {
    uint64_t generation;
    size_t current;
    int direction;
    size_t ahead;
    size_t behind;
    uint32_t box_width;
    uint32_t box_height;
    uint32_t max_dimension;
    uint64_t budget;
} FvPlanPrefetchContext;

/* Равны ли условия (направление сравнивается по знаку: <0 — назад, иначе вперёд). */
int FvPlan_PrefetchContextEqual(
    const FvPlanPrefetchContext* left,
    const FvPlanPrefetchContext* right);

/*
 * Подавлен ли повторный заказ кадра: suppressed — кадр помечен как
 * вытесненный сразу после prefetch при условиях recorded; now — текущие
 * условия. Подавление действует, только пока условия те же.
 */
int FvPlan_PrefetchSuppressed(
    int suppressed,
    const FvPlanPrefetchContext* recorded,
    const FvPlanPrefetchContext* now);

/* Что UI-поток делает с результатом задания. */
#define FV_PLAN_RESULT_ACCEPT 0
/* Другое поколение списка (сменился ZIP или папка) или индекс вне списка. */
#define FV_PLAN_RESULT_STALE 1
/* В кэше уже кадр не хуже: результат не нужен. */
#define FV_PLAN_RESULT_REDUNDANT 2
/* Задание не выполнено (ошибка или отмена). */
#define FV_PLAN_RESULT_FAILED 3

/*
 * Решение по результату задания. cached_* — размер кадра в кэше (0 —
 * пусто), result_* — размер результата. Непустой кэш заменяется, если
 * результат больше хотя бы по одной стороне (передекод при зуме) или если
 * кадр в кэше больше max_dimension (0 — без предела), а результат в него
 * укладывается (render target с меньшим пределом битмапа).
 */
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
    uint32_t max_dimension);

#endif
