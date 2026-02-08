#include "ImageDecoder.h"
#include <wincodec.h>
#include <wrl/client.h>
#include <algorithm>
#include <cctype>
#include <shlwapi.h>
#pragma comment(lib, "shlwapi.lib")

using Microsoft::WRL::ComPtr;

// WIC декодер - универсальный fallback для всех форматов
class WicDecoder : public IImageDecoder {
    ComPtr<IWICImagingFactory> factory_;
    bool available_ = false;

public:
    WicDecoder() {
        HRESULT hr = CoCreateInstance(
            CLSID_WICImagingFactory,
            nullptr,
            CLSCTX_INPROC_SERVER,
            IID_PPV_ARGS(&factory_));
        
        available_ = SUCCEEDED(hr);
        
        if (available_) {
            OutputDebugStringW(L"[ImageDecoder] WIC decoder initialized\n");
        }
    }

    bool IsAvailable() const override { return available_; }
    const wchar_t* Name() const override { return L"Windows Imaging Component"; }

    bool CanDecode(const wchar_t* path) const override {
        // WIC поддерживает множество форматов
        std::wstring p(path);
        auto dotPos = p.find_last_of(L'.');
        if (dotPos == std::wstring::npos) return false;
        
        auto ext = p.substr(dotPos);
        std::transform(ext.begin(), ext.end(), ext.begin(), ::towlower);
        
        // WIC поддерживает: JPEG, PNG, BMP, GIF, TIFF, ICO, WDP, DDS
        return ext == L".jpg" || ext == L".jpeg" ||
               ext == L".png" || ext == L".bmp" ||
               ext == L".gif" || ext == L".tiff" || ext == L".tif" ||
               ext == L".ico" || ext == L".wdp" || ext == L".dds" ||
               ext == L".webp" || ext == L".heic" || ext == L".heif";
    }

    std::unique_ptr<DecodedImage> Decode(const wchar_t* path) override {
        if (!factory_) return nullptr;

        ComPtr<IWICBitmapDecoder> decoder;
        HRESULT hr = factory_->CreateDecoderFromFilename(
            path,
            nullptr,
            GENERIC_READ,
            WICDecodeMetadataCacheOnDemand,
            &decoder);
        
        if (FAILED(hr)) return nullptr;

        return DecodeFromWicDecoder(decoder.Get());
    }

    std::unique_ptr<DecodedImage> DecodeFromMemory(const uint8_t* data, size_t size, const wchar_t* hint) override {
        if (!factory_ || !data || size == 0) return nullptr;

        // Создаём IStream из данных в памяти
        ComPtr<IStream> stream;
        stream.Attach(SHCreateMemStream(data, static_cast<UINT>(size)));
        if (!stream) return nullptr;

        ComPtr<IWICBitmapDecoder> decoder;
        HRESULT hr = factory_->CreateDecoderFromStream(
            stream.Get(),
            nullptr,
            WICDecodeMetadataCacheOnDemand,
            &decoder);
        
        if (FAILED(hr)) return nullptr;

        return DecodeFromWicDecoder(decoder.Get());
    }

private:
    std::unique_ptr<DecodedImage> DecodeFromWicDecoder(IWICBitmapDecoder* decoder) {
        if (!decoder) return nullptr;

        ComPtr<IWICBitmapFrameDecode> frame;
        HRESULT hr = decoder->GetFrame(0, &frame);
        if (FAILED(hr)) return nullptr;

        UINT width, height;
        hr = frame->GetSize(&width, &height);
        if (FAILED(hr)) return nullptr;

        // Конвертируем в BGRA
        ComPtr<IWICFormatConverter> converter;
        hr = factory_->CreateFormatConverter(&converter);
        if (FAILED(hr)) return nullptr;

        hr = converter->Initialize(
            frame.Get(),
            GUID_WICPixelFormat32bppBGRA,
            WICBitmapDitherTypeNone,
            nullptr,
            0.0,
            WICBitmapPaletteTypeCustom);
        if (FAILED(hr)) return nullptr;

        auto result = std::make_unique<DecodedImage>();
        result->width = static_cast<int>(width);
        result->height = static_cast<int>(height);
        result->stride = static_cast<int>(width * 4);
        result->pixels.resize(static_cast<size_t>(result->stride) * height);

        hr = converter->CopyPixels(
            nullptr,
            result->stride,
            static_cast<UINT>(result->pixels.size()),
            result->pixels.data());
        
        if (FAILED(hr)) return nullptr;

        return result;
    }
};

std::unique_ptr<IImageDecoder> CreateWicDecoder() {
    auto decoder = std::make_unique<WicDecoder>();
    if (decoder->IsAvailable()) {
        return decoder;
    }
    OutputDebugStringW(L"[ImageDecoder] WIC not available\n");
    return nullptr;
}
