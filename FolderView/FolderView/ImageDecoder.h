#pragma once

#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

// Декодированное изображение в 32-битном premultiplied BGRA.
// Память pixels принадлежит структуре и выделяется из кучи процесса.
// width/height — размер буфера pixels; кадр может быть декодирован меньше
// исходного (под экран), исходный размер — source_width/source_height.
typedef struct DecodedImage {
    BYTE* pixels;
    SIZE_T pixels_size;
    UINT width;
    UINT height;
    UINT stride;
    BOOL has_alpha;
    UINT source_width;
    UINT source_height;
} DecodedImage;

// Режим интерполяции IWICBitmapScaler при уменьшении под целевой размер.
typedef enum ImageDecoderInterpolation {
    IMAGE_DECODER_INTERPOLATION_NEAREST = 0,
    IMAGE_DECODER_INTERPOLATION_LINEAR = 1,
    IMAGE_DECODER_INTERPOLATION_CUBIC = 2,
    IMAGE_DECODER_INTERPOLATION_FANT = 3,
    IMAGE_DECODER_INTERPOLATION_HIGH_QUALITY_CUBIC = 4
} ImageDecoderInterpolation;

// Режим по умолчанию (вьювер): выбран замером, см. UPSTREAM.md.
#define IMAGE_DECODER_DEFAULT_INTERPOLATION IMAGE_DECODER_INTERPOLATION_FANT

// Параметры декодирования. Нулевая структура — полное разрешение.
typedef struct ImageDecodeOptions {
    // Рамка, в которую вписывается кадр (без увеличения); 0 — без ограничения.
    UINT box_width;
    UINT box_height;
    // Предел каждой стороны результата (максимальный D2D-битмап); 0 — нет.
    UINT max_dimension;
    ImageDecoderInterpolation interpolation;
    // TRUE — не использовать IWICBitmapSourceTransform (только для A/B).
    BOOL disable_source_transform;
    // Допуск в процентах: если нативный DCT-размер больше цели не более чем
    // на столько, он берётся как есть, без IWICBitmapScaler (остаток
    // уменьшения делает D2D при отрисовке).
    UINT dct_slack_percent;
} ImageDecodeOptions;

// Допуск DCT-размера по умолчанию (вьювер): см. UPSTREAM.md.
#define IMAGE_DECODER_DEFAULT_DCT_SLACK_PERCENT 25U

// Подробности одного декодирования (для трассы и бенчмарка). Время — в
// тиках QueryPerformanceCounter.
typedef struct ImageDecodeStats {
    UINT source_width;
    UINT source_height;
    // Размер после масштаба DCT через IWICBitmapSourceTransform; 0 — не применялся.
    UINT transform_width;
    UINT transform_height;
    // Делитель масштаба DCT (1 — полное разрешение).
    UINT dct_denominator;
    // TRUE — применён IWICBitmapScaler.
    BOOL scaled;
    // Создание декодера и кадра.
    ULONGLONG open_ticks;
    // Декодирование пикселей (DCT, scaler, конвертация в PBGRA) в итоговый
    // буфер: WIC выполняет цепочку потоково, по частям её не разделить.
    ULONGLONG pixels_ticks;
} ImageDecodeStats;

// Инициализирует COM на текущем потоке и создаёт WIC-фабрику.
// Initialize и Shutdown должны вызываться на одном и том же потоке.
HRESULT ImageDecoder_Initialize(void);

// Освобождает WIC-фабрику и балансирует инициализацию COM.
void ImageDecoder_Shutdown(void);

// Декодирует первый кадр файла через WIC в полном разрешении.
// Перед первым вызовом out_image должен быть обнулён. При успехе ранее
// принадлежавший out_image буфер освобождается и заменяется новым.
HRESULT ImageDecoder_DecodeFile(LPCWSTR path, DecodedImage* out_image);

// Декодирует первый кадр из памяти через WIC в полном разрешении.
// Буфер не копируется (IWICStream поверх памяти); размер ограничен UINT_MAX.
HRESULT ImageDecoder_DecodeMemory(
    const BYTE* data,
    SIZE_T data_size,
    DecodedImage* out_image);

// Как DecodeFile/DecodeMemory, но с целевым размером: options == NULL —
// полное разрешение; stats может быть NULL. data должен жить до возврата.
HRESULT ImageDecoder_DecodeFileEx(
    LPCWSTR path,
    const ImageDecodeOptions* options,
    DecodedImage* out_image,
    ImageDecodeStats* stats);

HRESULT ImageDecoder_DecodeMemoryEx(
    const BYTE* data,
    SIZE_T data_size,
    const ImageDecodeOptions* options,
    DecodedImage* out_image,
    ImageDecodeStats* stats);

// Проверяет наличие непустого согласованного BGRA-буфера.
BOOL DecodedImage_IsValid(const DecodedImage* image);

// Освобождает pixels через HeapFree и обнуляет все поля структуры.
void DecodedImage_Free(DecodedImage* image);

#ifdef __cplusplus
}
#endif
