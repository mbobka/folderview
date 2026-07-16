#define CINTERFACE
#define COBJMACROS

#include "ImageDecoder.h"

#include <objbase.h>
#include <objidl.h>
#include <shlwapi.h>
#include <wincodec.h>

static IWICImagingFactory* g_wic_factory;
static BOOL g_com_reference_owned;

static HRESULT decode_wic_decoder(
    IWICBitmapDecoder* decoder,
    DecodedImage* out_image);

static BOOL frame_supports_alpha(IWICBitmapFrameDecode* frame)
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
        g_wic_factory,
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
        SUCCEEDED(IWICImagingFactory_CreatePalette(g_wic_factory, &palette))) {
        if (SUCCEEDED(IWICBitmapFrameDecode_CopyPalette(frame, palette)) &&
            FAILED(IWICPalette_HasAlpha(palette, &supports_alpha))) {
            supports_alpha = FALSE;
        }
        IWICPalette_Release(palette);
    }

    return supports_alpha;
}

HRESULT ImageDecoder_Initialize(void)
{
    HRESULT hr;
    IWICImagingFactory* factory = NULL;

    if (g_wic_factory != NULL) {
        return S_FALSE;
    }

    hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if (SUCCEEDED(hr)) {
        // Даже S_FALSE увеличивает счётчик COM и требует CoUninitialize.
        g_com_reference_owned = TRUE;
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
        if (g_com_reference_owned) {
            CoUninitialize();
            g_com_reference_owned = FALSE;
        }
        return hr;
    }

    g_wic_factory = factory;
    return S_OK;
}

void ImageDecoder_Shutdown(void)
{
    if (g_wic_factory != NULL) {
        IWICImagingFactory_Release(g_wic_factory);
        g_wic_factory = NULL;
    }

    if (g_com_reference_owned) {
        CoUninitialize();
        g_com_reference_owned = FALSE;
    }
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
}

HRESULT ImageDecoder_DecodeFile(LPCWSTR path, DecodedImage* out_image)
{
    IWICBitmapDecoder* decoder = NULL;
    HRESULT hr;

    if (path == NULL || path[0] == L'\0' || out_image == NULL) {
        return E_INVALIDARG;
    }
    if (g_wic_factory == NULL) {
        return CO_E_NOTINITIALIZED;
    }

    hr = IWICImagingFactory_CreateDecoderFromFilename(
        g_wic_factory,
        path,
        NULL,
        GENERIC_READ,
        WICDecodeMetadataCacheOnDemand,
        &decoder);
    if (FAILED(hr)) {
        return hr;
    }

    hr = decode_wic_decoder(decoder, out_image);
    IWICBitmapDecoder_Release(decoder);
    return hr;
}

HRESULT ImageDecoder_DecodeMemory(
    const BYTE* data,
    SIZE_T data_size,
    DecodedImage* out_image)
{
    IStream* stream = NULL;
    IWICBitmapDecoder* decoder = NULL;
    HRESULT hr;

    if (data == NULL || data_size == 0 || out_image == NULL) {
        return E_INVALIDARG;
    }
    if (data_size > (SIZE_T)(UINT)~(UINT)0) {
        return HRESULT_FROM_WIN32(ERROR_FILE_TOO_LARGE);
    }
    if (g_wic_factory == NULL) {
        return CO_E_NOTINITIALIZED;
    }

    stream = SHCreateMemStream(data, (UINT)data_size);
    if (stream == NULL) {
        return E_OUTOFMEMORY;
    }

    hr = IWICImagingFactory_CreateDecoderFromStream(
        g_wic_factory,
        stream,
        NULL,
        WICDecodeMetadataCacheOnDemand,
        &decoder);
    if (SUCCEEDED(hr)) {
        hr = decode_wic_decoder(decoder, out_image);
        IWICBitmapDecoder_Release(decoder);
    }

    IStream_Release(stream);
    return hr;
}

static HRESULT decode_wic_decoder(
    IWICBitmapDecoder* decoder,
    DecodedImage* out_image)
{
    IWICBitmapFrameDecode* frame = NULL;
    IWICFormatConverter* converter = NULL;
    BYTE* pixels = NULL;
    UINT width;
    UINT height;
    UINT stride;
    SIZE_T pixels_size;
    ULONGLONG required_size;
    union {
        ULONGLONG bits;
        double value;
    } alpha_threshold;
    BOOL has_alpha;
    HRESULT hr;

    if (decoder == NULL || out_image == NULL) {
        return E_INVALIDARG;
    }

    hr = IWICBitmapDecoder_GetFrame(decoder, 0, &frame);
    if (FAILED(hr)) {
        return hr;
    }

    hr = IWICBitmapFrameDecode_GetSize(frame, &width, &height);
    if (FAILED(hr)) {
        IWICBitmapFrameDecode_Release(frame);
        return hr;
    }
    if (width == 0 || height == 0) {
        IWICBitmapFrameDecode_Release(frame);
        return WINCODEC_ERR_BADIMAGE;
    }
    if (width > ((UINT)~(UINT)0) / 4U) {
        IWICBitmapFrameDecode_Release(frame);
        return HRESULT_FROM_WIN32(ERROR_ARITHMETIC_OVERFLOW);
    }

    stride = width * 4U;
    required_size = (ULONGLONG)stride * (ULONGLONG)height;
    if (required_size > (ULONGLONG)(UINT)~(UINT)0 ||
        required_size > (ULONGLONG)(SIZE_T)~(SIZE_T)0) {
        IWICBitmapFrameDecode_Release(frame);
        return HRESULT_FROM_WIN32(ERROR_FILE_TOO_LARGE);
    }
    pixels_size = (SIZE_T)required_size;
    has_alpha = frame_supports_alpha(frame);
    alpha_threshold.bits = 0;

    hr = IWICImagingFactory_CreateFormatConverter(
        g_wic_factory,
        &converter);
    if (FAILED(hr)) {
        IWICBitmapFrameDecode_Release(frame);
        return hr;
    }

    hr = IWICFormatConverter_Initialize(
        converter,
        (IWICBitmapSource*)frame,
        &GUID_WICPixelFormat32bppPBGRA,
        WICBitmapDitherTypeNone,
        NULL,
        alpha_threshold.value,
        WICBitmapPaletteTypeCustom);
    if (FAILED(hr)) {
        IWICFormatConverter_Release(converter);
        IWICBitmapFrameDecode_Release(frame);
        return hr;
    }

    pixels = (BYTE*)HeapAlloc(GetProcessHeap(), 0, pixels_size);
    if (pixels == NULL) {
        IWICFormatConverter_Release(converter);
        IWICBitmapFrameDecode_Release(frame);
        return E_OUTOFMEMORY;
    }

    hr = IWICFormatConverter_CopyPixels(
        converter,
        NULL,
        stride,
        (UINT)pixels_size,
        pixels);

    IWICFormatConverter_Release(converter);
    IWICBitmapFrameDecode_Release(frame);

    if (FAILED(hr)) {
        HeapFree(GetProcessHeap(), 0, pixels);
        return hr;
    }

    DecodedImage_Free(out_image);
    out_image->pixels = pixels;
    out_image->pixels_size = pixels_size;
    out_image->width = width;
    out_image->height = height;
    out_image->stride = stride;
    out_image->has_alpha = has_alpha;
    return S_OK;
}
