#include "ImageDecoder.h"

// Singleton instance
static std::unique_ptr<ImageDecoderChain> g_instance;

ImageDecoderChain& ImageDecoderChain::Instance() {
    if (!g_instance) {
        g_instance = std::unique_ptr<ImageDecoderChain>(new ImageDecoderChain());
    }
    return *g_instance;
}

void ImageDecoderChain::AddDecoder(std::unique_ptr<IImageDecoder> decoder) {
    if (decoder && decoder->IsAvailable()) {
        wchar_t msg[256];
        swprintf_s(msg, L"[ImageDecoder] Added decoder: %s\n", decoder->Name());
        OutputDebugStringW(msg);
        decoders_.push_back(std::move(decoder));
    }
}

std::unique_ptr<DecodedImage> ImageDecoderChain::Decode(const wchar_t* path) {
    // Пробуем каждый декодер по порядку приоритета
    for (auto& decoder : decoders_) {
        if (decoder->CanDecode(path)) {
            auto decoded = decoder->Decode(path);
            if (decoded && decoded->IsValid()) {
                wchar_t msg[512];
                swprintf_s(msg, L"[ImageDecoder] Decoded with %s: %s\n", 
                    decoder->Name(), path);
                OutputDebugStringW(msg);
                return decoded;
            }
        }
    }
    
    OutputDebugStringW(L"[ImageDecoder] All decoders failed!\n");
    return nullptr;
}

std::unique_ptr<DecodedImage> ImageDecoderChain::DecodeFromMemory(const uint8_t* data, size_t size, const wchar_t* hint) {
    // Пробуем каждый декодер по порядку приоритета
    for (auto& decoder : decoders_) {
        if (decoder->CanDecode(hint)) {
            auto decoded = decoder->DecodeFromMemory(data, size, hint);
            if (decoded && decoded->IsValid()) {
                wchar_t msg[512];
                swprintf_s(msg, L"[ImageDecoder] Decoded from memory with %s: %s\n", 
                    decoder->Name(), hint);
                OutputDebugStringW(msg);
                return decoded;
            }
        }
    }
    
    OutputDebugStringW(L"[ImageDecoder] All decoders failed for memory decode!\n");
    return nullptr;
}

void ImageDecoderChain::Initialize() {
    auto& chain = Instance();
    
    // Приоритет: nvJPEG (GPU) > WIC (fallback)
    
    // 1. NVIDIA nvJPEG - самый быстрый на NVIDIA GPU
    if (auto decoder = CreateNvJpegDecoder()) {
        chain.AddDecoder(std::move(decoder));
    }
    
    // 2. WIC - универсальный fallback для всех форматов
    if (auto decoder = CreateWicDecoder()) {
        chain.AddDecoder(std::move(decoder));
    }
    
    wchar_t msg[256];
    swprintf_s(msg, L"[ImageDecoder] Initialized with %zu decoders\n", 
        chain.decoders_.size());
    OutputDebugStringW(msg);
}

void ImageDecoderChain::Shutdown() {
    if (g_instance) {
        g_instance->decoders_.clear();
        g_instance.reset();
    }
}
