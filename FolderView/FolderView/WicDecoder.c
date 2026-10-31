#define CINTERFACE
#define COBJMACROS

#include "ImageDecoder.h"
#include "ViewerPlan.h"

#include <objbase.h>
#include <objidl.h>
#include <wincodec.h>

// Общий контекст функций без явного контекста (UI-поток вьювера).
static ImageDecoderContext g_default_context;

static IWICImagingFactory* context_factory(const ImageDecoderContext* context)
{
    return context != NULL ? (IWICImagingFactory*)context->factory : NULL;
}

static HRESULT decode_wic_decoder(
    IWICImagingFactory* factory,
    IWICBitmapDecoder* decoder,
    const ImageDecodeOptions* options,
    DecodedImage* out_image,
    ImageDecodeStats* stats);

static ULONGLONG decoder_now(void)
{
    LARGE_INTEGER now;

    QueryPerformanceCounter(&now);
    return (ULONGLONG)now.QuadPart;
}

static BOOL frame_supports_alpha(IWICImagingFactory* factory, IWICBitmapFrameDecode* frame)
{
    WICPixelFormatGUID pixel_format;
    IWICComponentInfo* component_info = NULL;
    IWICPixelFormatInfo2* pixel_format_info = NULL;
    IWICPalette* palette = NULL;
    BOOL supports_alpha = FALSE;
    HRESULT hr;

    hr = IWICBitmapFrameDecode_GetPixelFormat(frame, &pixel_format);
    if (FAILED(hr)) {
        return FALSE;
    }

    hr = IWICImagingFactory_CreateComponentInfo(
        factory,
        &pixel_format,
        &component_info);
    if (FAILED(hr)) {
        return FALSE;
    }

    hr = IWICComponentInfo_QueryInterface(
        component_info,
        &IID_IWICPixelFormatInfo2,
        (void**)&pixel_format_info);
    if (SUCCEEDED(hr)) {
        if (FAILED(IWICPixelFormatInfo2_SupportsTransparency(
                pixel_format_info,
                &supports_alpha))) {
            supports_alpha = FALSE;
        }
        IWICPixelFormatInfo2_Release(pixel_format_info);
    }

    IWICComponentInfo_Release(component_info);

    // У индексированных форматов прозрачность хранится в палитре, а не
    // в описании формата пикселя.
    if (!supports_alpha &&
        SUCCEEDED(IWICImagingFactory_CreatePalette(factory, &palette))) {
        if (SUCCEEDED(IWICBitmapFrameDecode_CopyPalette(frame, palette)) &&
            FAILED(IWICPalette_HasAlpha(palette, &supports_alpha))) {
            supports_alpha = FALSE;
        }
        IWICPalette_Release(palette);
    }

    return supports_alpha;
}

HRESULT ImageDecoderContext_Create(ImageDecoderContext* context, DWORD coinit)
{
    HRESULT hr;
    IWICImagingFactory* factory = NULL;

    if (context == NULL) {
        return E_INVALIDARG;
    }
    if (context->factory != NULL) {
        return S_FALSE;
    }
    context->com_owned = FALSE;

    hr = CoInitializeEx(NULL, coinit);
    if (SUCCEEDED(hr)) {
        // Даже S_FALSE увеличивает счётчик COM и требует CoUninitialize.
        context->com_owned = TRUE;
    }
    else if (hr != RPC_E_CHANGED_MODE) {
        return hr;
    }

    hr = CoCreateInstance(
        &CLSID_WICImagingFactory,
        NULL,
        CLSCTX_INPROC_SERVER,
        &IID_IWICImagingFactory,
        (void**)&factory);
    if (FAILED(hr)) {
        if (context->com_owned) {
            CoUninitialize();
            context->com_owned = FALSE;
        }
        return hr;
    }

    context->factory = factory;
    return S_OK;
}

void ImageDecoderContext_Destroy(ImageDecoderContext* context)
{
    if (context == NULL) {
        return;
    }
    if (context->factory != NULL) {
        IWICImagingFactory_Release((IWICImagingFactory*)context->factory);
        context->factory = NULL;
    }

    if (context->com_owned) {
        CoUninitialize();
        context->com_owned = FALSE;
    }
}

HRESULT ImageDecoder_Initialize(void)
{
    return ImageDecoderContext_Create(&g_default_context, COINIT_APARTMENTTHREADED);
}

void ImageDecoder_Shutdown(void)
{
    ImageDecoderContext_Destroy(&g_default_context);
}

BOOL DecodedImage_IsValid(const DecodedImage* image)
{
    if (image == NULL || image->pixels == NULL) {
        return FALSE;
    }
    if (image->width == 0 || image->height == 0 || image->stride == 0) {
        return FALSE;
    }
    if (image->width > ((UINT)~(UINT)0) / 4U ||
        image->stride < image->width * 4U) {
        return FALSE;
    }
    if ((SIZE_T)image->height > (SIZE_T)~(SIZE_T)0 / (SIZE_T)image->stride) {
        return FALSE;
    }

    return image->pixels_size >= (SIZE_T)image->stride * (SIZE_T)image->height;
}

void DecodedImage_Free(DecodedImage* image)
{
    if (image == NULL) {
        return;
    }

    if (image->pixels != NULL) {
        HeapFree(GetProcessHeap(), 0, image->pixels);
    }

    image->pixels = NULL;
    image->pixels_size = 0;
    image->width = 0;
    image->height = 0;
    image->stride = 0;
    image->has_alpha = FALSE;
    image->source_width = 0;
    image->source_height = 0;
}

HRESULT ImageDecoder_DecodeFile(LPCWSTR path, DecodedImage* out_image)
{
    return ImageDecoder_DecodeFileEx(path, NULL, out_image, NULL);
}

HRESULT ImageDecoder_DecodeMemory(
    const BYTE* data,
    SIZE_T data_size,
    DecodedImage* out_image)
{
    return ImageDecoder_DecodeMemoryEx(data, data_size, NULL, out_image, NULL);
}

HRESULT ImageDecoder_DecodeFileEx(
    LPCWSTR path,
    const ImageDecodeOptions* options,
    DecodedImage* out_image,
    ImageDecodeStats* stats)
{
    return ImageDecoder_DecodeFileWith(&g_default_context, path, options, out_image, stats);
}

HRESULT ImageDecoder_DecodeMemoryEx(
    const BYTE* data,
    SIZE_T data_size,
    const ImageDecodeOptions* options,
    DecodedImage* out_image,
    ImageDecodeStats* stats)
{
    return ImageDecoder_DecodeMemoryWith(
        &g_default_context,
        data,
        data_size,
        options,
        out_image,
        stats);
}

HRESULT ImageDecoder_DecodeFileWith(
    const ImageDecoderContext* context,
    LPCWSTR path,
    const ImageDecodeOptions* options,
    DecodedImage* out_image,
    ImageDecodeStats* stats)
{
    IWICImagingFactory* factory = context_factory(context);
    IWICBitmapDecoder* decoder = NULL;
    ULONGLONG started = decoder_now();
    HRESULT hr;

    if (stats != NULL) {
        ZeroMemory(stats, sizeof(*stats));
    }
    if (path == NULL || path[0] == L'\0' || out_image == NULL) {
        return E_INVALIDARG;
    }
    if (factory == NULL) {
        return CO_E_NOTINITIALIZED;
    }

    hr = IWICImagingFactory_CreateDecoderFromFilename(
        factory,
        path,
        NULL,
        GENERIC_READ,
        WICDecodeMetadataCacheOnDemand,
        &decoder);
    if (FAILED(hr)) {
        return hr;
    }
    if (stats != NULL) {
        stats->open_ticks = decoder_now() - started;
    }

    hr = decode_wic_decoder(factory, decoder, options, out_image, stats);
    IWICBitmapDecoder_Release(decoder);
    return hr;
}

HRESULT ImageDecoder_DecodeMemoryWith(
    const ImageDecoderContext* context,
    const BYTE* data,
    SIZE_T data_size,
    const ImageDecodeOptions* options,
    DecodedImage* out_image,
    ImageDecodeStats* stats)
{
    IWICImagingFactory* factory = context_factory(context);
    IWICStream* stream = NULL;
    IWICBitmapDecoder* decoder = NULL;
    ULONGLONG started = decoder_now();
    HRESULT hr;

    if (stats != NULL) {
        ZeroMemory(stats, sizeof(*stats));
    }
    if (data == NULL || data_size == 0 || out_image == NULL) {
        return E_INVALIDARG;
    }
    if (data_size > (SIZE_T)(DWORD)~(DWORD)0) {
        return HRESULT_FROM_WIN32(ERROR_FILE_TOO_LARGE);
    }
    if (factory == NULL) {
        return CO_E_NOTINITIALIZED;
    }

    // IWICStream читает прямо из буфера вызывающего (SHCreateMemStream копировал
    // весь файл). Буфер живёт до возврата: декодер и поток освобождаются здесь.
    hr = IWICImagingFactory_CreateStream(factory, &stream);
    if (FAILED(hr)) {
        return hr;
    }
    hr = IWICStream_InitializeFromMemory(stream, (BYTE*)data, (DWORD)data_size);
    if (SUCCEEDED(hr)) {
        hr = IWICImagingFactory_CreateDecoderFromStream(
            factory,
            (IStream*)stream,
            NULL,
            WICDecodeMetadataCacheOnDemand,
            &decoder);
    }
    if (SUCCEEDED(hr)) {
        if (stats != NULL) {
            stats->open_ticks = decoder_now() - started;
        }
        hr = decode_wic_decoder(factory, decoder, options, out_image, stats);
        IWICBitmapDecoder_Release(decoder);
    }

    IWICStream_Release(stream);
    return hr;
}

// Размер, до которого декодер кадра умеет масштабировать нативно
// (IWICBitmapSourceTransform; у JPEG — DCT 1/2, 1/4, 1/8), не меньше цели и
// меньше исходного. FALSE — масштаба нет или он бесполезен.
static BOOL query_dct_size(
    IWICBitmapFrameDecode* frame,
    UINT source_width,
    UINT source_height,
    UINT target_width,
    UINT target_height,
    UINT* out_width,
    UINT* out_height,
    UINT* out_denominator)
{
    IWICBitmapSourceTransform* transform = NULL;
    UINT denominator;
    UINT width;
    UINT height;
    HRESULT hr;

    denominator = FvPlan_ChooseDctDenominator(
        source_width,
        source_height,
        target_width,
        target_height);
    if (denominator <= 1) {
        return FALSE;
    }
    hr = IWICBitmapFrameDecode_QueryInterface(
        frame,
        &IID_IWICBitmapSourceTransform,
        (void**)&transform);
    if (FAILED(hr)) {
        return FALSE;
    }

    width = FvPlan_DivideCeil(source_width, denominator);
    height = FvPlan_DivideCeil(source_height, denominator);
    hr = IWICBitmapSourceTransform_GetClosestSize(transform, &width, &height);
    IWICBitmapSourceTransform_Release(transform);
    if (FAILED(hr) ||
        width < target_width || height < target_height ||
        width == 0 || height == 0 ||
        (width >= source_width && height >= source_height)) {
        return FALSE;
    }

    *out_width = width;
    *out_height = height;
    *out_denominator = denominator;
    return TRUE;
}

static HRESULT wrap_scaler(
    IWICImagingFactory* factory,
    IWICBitmapSource* source,
    UINT width,
    UINT height,
    ImageDecoderInterpolation interpolation,
    IWICBitmapSource** result)
{
    IWICBitmapScaler* scaler = NULL;
    HRESULT hr;

    *result = NULL;
    hr = IWICImagingFactory_CreateBitmapScaler(factory, &scaler);
    if (FAILED(hr)) {
        return hr;
    }
    hr = IWICBitmapScaler_Initialize(
        scaler,
        source,
        width,
        height,
        (WICBitmapInterpolationMode)interpolation);
    if (FAILED(hr)) {
        IWICBitmapScaler_Release(scaler);
        return hr;
    }
    *result = (IWICBitmapSource*)scaler;
    return S_OK;
}

static HRESULT wrap_converter(
    IWICImagingFactory* factory,
    IWICBitmapSource* source,
    IWICBitmapSource** result)
{
    IWICFormatConverter* converter = NULL;
    union {
        ULONGLONG bits;
        double value;
    } alpha_threshold;
    HRESULT hr;

    *result = NULL;
    alpha_threshold.bits = 0;
    hr = IWICImagingFactory_CreateFormatConverter(factory, &converter);
    if (FAILED(hr)) {
        return hr;
    }
    hr = IWICFormatConverter_Initialize(
        converter,
        source,
        &GUID_WICPixelFormat32bppPBGRA,
        WICBitmapDitherTypeNone,
        NULL,
        alpha_threshold.value,
        WICBitmapPaletteTypeCustom);
    if (FAILED(hr)) {
        IWICFormatConverter_Release(converter);
        return hr;
    }
    *result = (IWICBitmapSource*)converter;
    return S_OK;
}

// Цепочка source -> [scaler] -> конвертер PBGRA -> CopyPixels в pixels.
// С альфой конвертер идёт до scaler: масштабировать нужно premultiplied
// пиксели, иначе на краях прозрачности появляется ореол.
static HRESULT copy_scaled_pbgra(
    IWICImagingFactory* factory,
    IWICBitmapSource* source,
    UINT target_width,
    UINT target_height,
    BOOL has_alpha,
    ImageDecoderInterpolation interpolation,
    UINT stride,
    UINT pixels_size,
    BYTE* pixels,
    BOOL* scaled)
{
    IWICBitmapSource* first = NULL;
    IWICBitmapSource* second = NULL;
    UINT width = 0;
    UINT height = 0;
    BOOL needs_scale;
    HRESULT hr;

    *scaled = FALSE;
    hr = IWICBitmapSource_GetSize(source, &width, &height);
    if (FAILED(hr)) {
        return hr;
    }
    needs_scale = width != target_width || height != target_height;

    if (!needs_scale) {
        hr = wrap_converter(factory, source, &first);
    } else if (has_alpha) {
        hr = wrap_converter(factory, source, &first);
        if (SUCCEEDED(hr)) {
            hr = wrap_scaler(factory, first, target_width, target_height, interpolation, &second);
        }
    } else {
        hr = wrap_scaler(factory, source, target_width, target_height, interpolation, &first);
        if (SUCCEEDED(hr)) {
            hr = wrap_converter(factory, first, &second);
        }
    }

    if (SUCCEEDED(hr)) {
        IWICBitmapSource* last = second != NULL ? second : first;

        hr = IWICBitmapSource_CopyPixels(last, NULL, stride, pixels_size, pixels);
        *scaled = needs_scale;
    }

    if (second != NULL) {
        IWICBitmapSource_Release(second);
    }
    if (first != NULL) {
        IWICBitmapSource_Release(first);
    }
    return hr;
}

static HRESULT decode_wic_decoder(
    IWICImagingFactory* factory,
    IWICBitmapDecoder* decoder,
    const ImageDecodeOptions* options,
    DecodedImage* out_image,
    ImageDecodeStats* stats)
{
    static const ImageDecodeOptions full_resolution = {0};
    IWICBitmapFrameDecode* frame = NULL;
    IWICBitmapSource* dct_source = NULL;
    IWICBitmapSource* source;
    BYTE* pixels = NULL;
    UINT source_width;
    UINT source_height;
    UINT32 target_width;
    UINT32 target_height;
    UINT stride;
    SIZE_T pixels_size;
    ULONGLONG required_size;
    ULONGLONG started;
    ImageDecoderInterpolation interpolation;
    UINT dct_width = 0;
    UINT dct_height = 0;
    UINT dct_denominator = 1;
    BOOL has_alpha;
    BOOL scaled = FALSE;
    HRESULT hr;

    if (decoder == NULL || out_image == NULL) {
        return E_INVALIDARG;
    }
    if (options == NULL) {
        options = &full_resolution;
    }
    interpolation = options->interpolation;
    if ((int)interpolation < (int)IMAGE_DECODER_INTERPOLATION_NEAREST ||
        (int)interpolation > (int)IMAGE_DECODER_INTERPOLATION_HIGH_QUALITY_CUBIC) {
        interpolation = IMAGE_DECODER_DEFAULT_INTERPOLATION;
    }

    started = decoder_now();
    hr = IWICBitmapDecoder_GetFrame(decoder, 0, &frame);
    if (FAILED(hr)) {
        return hr;
    }

    hr = IWICBitmapFrameDecode_GetSize(frame, &source_width, &source_height);
    if (FAILED(hr)) {
        IWICBitmapFrameDecode_Release(frame);
        return hr;
    }
    if (source_width == 0 || source_height == 0) {
        IWICBitmapFrameDecode_Release(frame);
        return WINCODEC_ERR_BADIMAGE;
    }
    if (stats != NULL) {
        stats->open_ticks += decoder_now() - started;
        stats->source_width = source_width;
        stats->source_height = source_height;
        stats->dct_denominator = 1;
    }

    // Цель — вписать в рамку; уменьшение меньше двукратного (DCT его не
    // достигает) не делается — см. FvPlan_DecodeTarget.
    FvPlan_DecodeTarget(
        source_width,
        source_height,
        options->box_width,
        options->box_height,
        options->max_dimension,
        &target_width,
        &target_height);
    has_alpha = frame_supports_alpha(factory, frame);

    // Масштаб DCT только для непрозрачных кадров: JPEG альфы не имеет, а
    // для остальных форматов декодер преобразование не поддерживает.
    if ((target_width != source_width || target_height != source_height) &&
        !has_alpha &&
        !options->disable_source_transform &&
        query_dct_size(
            frame,
            source_width,
            source_height,
            target_width,
            target_height,
            &dct_width,
            &dct_height,
            &dct_denominator)) {
        // DCT-размер чуть больше цели — берём его без scaler: остаток
        // уменьшения дёшево и без потерь качества делает D2D при отрисовке.
        if (FvPlan_AcceptDctSize(
                dct_width,
                dct_height,
                target_width,
                target_height,
                options->dct_slack_percent) &&
            (options->max_dimension == 0 ||
             (dct_width <= options->max_dimension && dct_height <= options->max_dimension))) {
            target_width = dct_width;
            target_height = dct_height;
        }
    }

    if (target_width > ((UINT)~(UINT)0) / 4U) {
        IWICBitmapFrameDecode_Release(frame);
        return HRESULT_FROM_WIN32(ERROR_ARITHMETIC_OVERFLOW);
    }
    stride = target_width * 4U;
    required_size = (ULONGLONG)stride * (ULONGLONG)target_height;
    if (required_size > (ULONGLONG)(UINT)~(UINT)0 ||
        required_size > (ULONGLONG)(SIZE_T)~(SIZE_T)0) {
        IWICBitmapFrameDecode_Release(frame);
        return HRESULT_FROM_WIN32(ERROR_FILE_TOO_LARGE);
    }
    pixels_size = (SIZE_T)required_size;

    pixels = (BYTE*)HeapAlloc(GetProcessHeap(), 0, pixels_size);
    if (pixels == NULL) {
        IWICBitmapFrameDecode_Release(frame);
        return E_OUTOFMEMORY;
    }

    started = decoder_now();
    source = (IWICBitmapSource*)frame;
    if (dct_denominator > 1) {
        /*
         * Запрос ровно нативного DCT-размера у IWICBitmapScaler WIC
         * выполняет через IWICBitmapSourceTransform декодера — потоково, без
         * промежуточного буфера кадра (замер: на 1,5–3,5 мс быстрее явного
         * IWICBitmapSourceTransform::CopyPixels в промежуточный IWICBitmap).
         * Интерполяция здесь не участвует.
         */
        if (SUCCEEDED(wrap_scaler(
                factory,
                source,
                dct_width,
                dct_height,
                IMAGE_DECODER_INTERPOLATION_NEAREST,
                &dct_source))) {
            source = dct_source;
            if (stats != NULL) {
                stats->transform_width = dct_width;
                stats->transform_height = dct_height;
                stats->dct_denominator = dct_denominator;
            }
        }
    }

    hr = copy_scaled_pbgra(
        factory,
        source,
        target_width,
        target_height,
        has_alpha,
        interpolation,
        stride,
        (UINT)pixels_size,
        pixels,
        &scaled);
    if (FAILED(hr) && dct_source != NULL) {
        // DCT-путь не сработал — повторяем от исходного кадра.
        IWICBitmapSource_Release(dct_source);
        dct_source = NULL;
        if (stats != NULL) {
            stats->transform_width = 0;
            stats->transform_height = 0;
            stats->dct_denominator = 1;
        }
        hr = copy_scaled_pbgra(
            factory,
            (IWICBitmapSource*)frame,
            target_width,
            target_height,
            has_alpha,
            interpolation,
            stride,
            (UINT)pixels_size,
            pixels,
            &scaled);
    }
    if (stats != NULL) {
        stats->pixels_ticks = decoder_now() - started;
        stats->scaled = scaled;
    }

    if (dct_source != NULL) {
        IWICBitmapSource_Release(dct_source);
    }
    IWICBitmapFrameDecode_Release(frame);

    if (FAILED(hr)) {
        HeapFree(GetProcessHeap(), 0, pixels);
        return hr;
    }

    DecodedImage_Free(out_image);
    out_image->pixels = pixels;
    out_image->pixels_size = pixels_size;
    out_image->width = target_width;
    out_image->height = target_height;
    out_image->stride = stride;
    out_image->has_alpha = has_alpha;
    out_image->source_width = source_width;
    out_image->source_height = source_height;
    return S_OK;
}
