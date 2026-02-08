#pragma once

#include <memory>
#include <vector>
#include <string>
#include <cstdint>
#include <windows.h>

// Декодированное изображение в формате BGRA (совместимо с D2D)
struct DecodedImage {
    std::vector<uint8_t> pixels;
    int width = 0;
    int height = 0;
    int stride = 0;
    int bitsPerPixel = 32;
    bool hasAlpha = false;  // true для PNG/GIF с прозрачностью

    bool IsValid() const { return !pixels.empty() && width > 0 && height > 0; }
};

// Абстрактный декодер изображений
class IImageDecoder {
public:
    virtual ~IImageDecoder() = default;
    
    // Проверяет, может ли декодер обработать файл по расширению
    virtual bool CanDecode(const wchar_t* path) const = 0;
    
    // Декодирует изображение
    virtual std::unique_ptr<DecodedImage> Decode(const wchar_t* path) = 0;
    
    // Проверяет доступность декодера (библиотека загружена, GPU есть и т.д.)
    virtual bool IsAvailable() const = 0;
    
    // Имя декодера для отладки
    virtual const wchar_t* Name() const = 0;
};

// Цепочка декодеров с автоматическим fallback
class ImageDecoderChain {
    std::vector<std::unique_ptr<IImageDecoder>> decoders_;
    
    ImageDecoderChain() = default;
public:
    static ImageDecoderChain& Instance();
    
    void AddDecoder(std::unique_ptr<IImageDecoder> decoder);
    
    // Декодирует файл, пробуя декодеры по приоритету
    std::unique_ptr<DecodedImage> Decode(const wchar_t* path);
    
    // Инициализирует цепочку декодеров
    static void Initialize();
    
    // Освобождает ресурсы
    static void Shutdown();
};

// Фабричные функции для создания декодеров
std::unique_ptr<IImageDecoder> CreateNvJpegDecoder();
std::unique_ptr<IImageDecoder> CreateIppJpegDecoder();
std::unique_ptr<IImageDecoder> CreateWicDecoder();  // Для всех остальных форматов
