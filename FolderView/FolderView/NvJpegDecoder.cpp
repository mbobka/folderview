#include "ImageDecoder.h"
#include <fstream>
#include <algorithm>

#ifdef HAS_NVJPEG
#include <nvjpeg.h>
#include <cuda_runtime.h>

class NvJpegDecoder : public IImageDecoder {
    nvjpegHandle_t handle_ = nullptr;
    nvjpegJpegState_t state_ = nullptr;
    cudaStream_t stream_ = nullptr;
    bool available_ = false;

public:
    NvJpegDecoder() {
        // Проверяем наличие CUDA GPU
        int deviceCount = 0;
        if (cudaGetDeviceCount(&deviceCount) != cudaSuccess || deviceCount == 0)
            return;

        // Инициализируем nvJPEG
        if (nvjpegCreateSimple(&handle_) != NVJPEG_STATUS_SUCCESS)
            return;

        if (nvjpegJpegStateCreate(handle_, &state_) != NVJPEG_STATUS_SUCCESS) {
            nvjpegDestroy(handle_);
            handle_ = nullptr;
            return;
        }

        if (cudaStreamCreate(&stream_) != cudaSuccess) {
            nvjpegJpegStateDestroy(state_);
            nvjpegDestroy(handle_);
            handle_ = nullptr;
            state_ = nullptr;
            return;
        }

        available_ = true;
    }

    ~NvJpegDecoder() override {
        if (state_) nvjpegJpegStateDestroy(state_);
        if (stream_) cudaStreamDestroy(stream_);
        if (handle_) nvjpegDestroy(handle_);
    }

    bool IsAvailable() const override { return available_; }
    const wchar_t* Name() const override { return L"nvJPEG (CUDA)"; }

    bool CanDecode(const wchar_t* path) const override {
        std::wstring p(path);
        auto dotPos = p.find_last_of(L'.');
        if (dotPos == std::wstring::npos) return false;
        
        auto ext = p.substr(dotPos);
        std::transform(ext.begin(), ext.end(), ext.begin(), ::towlower);
        return ext == L".jpg" || ext == L".jpeg";
    }

    std::unique_ptr<DecodedImage> Decode(const wchar_t* path) override {
        // Читаем файл в память
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file) return nullptr;

        auto fileSize = file.tellg();
        file.seekg(0);
        std::vector<unsigned char> jpegData(static_cast<size_t>(fileSize));
        file.read(reinterpret_cast<char*>(jpegData.data()), fileSize);
        file.close();

        return DecodeFromMemory(jpegData.data(), jpegData.size(), path);
    }

    std::unique_ptr<DecodedImage> DecodeFromMemory(const uint8_t* data, size_t size, const wchar_t* hint) override {
        if (!data || size == 0) return nullptr;

        // Получаем информацию об изображении
        int nComponents;
        nvjpegChromaSubsampling_t subsampling;
        int widths[NVJPEG_MAX_COMPONENT], heights[NVJPEG_MAX_COMPONENT];

        if (nvjpegGetImageInfo(handle_, data, size,
            &nComponents, &subsampling, widths, heights) != NVJPEG_STATUS_SUCCESS)
            return nullptr;

        int width = widths[0];
        int height = heights[0];

        // Выделяем память на GPU для BGR формата (3 байта на пиксель)
        nvjpegImage_t output;
        memset(&output, 0, sizeof(output));

        size_t bgrPitch = width * 3;  // BGRI = 3 bytes per pixel
        if (cudaMalloc(&output.channel[0], bgrPitch * height) != cudaSuccess)
            return nullptr;
        output.pitch[0] = static_cast<unsigned int>(bgrPitch);

        // Декодируем в BGRI формат
        auto status = nvjpegDecode(handle_, state_, data, size,
            NVJPEG_OUTPUT_BGRI, &output, stream_);

        if (status != NVJPEG_STATUS_SUCCESS) {
            cudaFree(output.channel[0]);
            return nullptr;
        }

        cudaStreamSynchronize(stream_);

        // Копируем BGR данные с GPU
        std::vector<unsigned char> bgrData(bgrPitch * height);
        cudaMemcpy(bgrData.data(), output.channel[0], bgrPitch * height, cudaMemcpyDeviceToHost);
        cudaFree(output.channel[0]);

        // Конвертируем BGR -> BGRA
        auto result = std::make_unique<DecodedImage>();
        result->width = width;
        result->height = height;
        result->stride = width * 4;
        result->pixels.resize(static_cast<size_t>(result->stride) * height);

        const unsigned char* src = bgrData.data();
        unsigned char* dst = result->pixels.data();
        size_t pixelCount = static_cast<size_t>(width) * height;
        
        for (size_t i = 0; i < pixelCount; i++) {
            dst[0] = src[0];  // B
            dst[1] = src[1];  // G
            dst[2] = src[2];  // R
            dst[3] = 255;     // A
            src += 3;
            dst += 4;
        }

        return result;
    }
};

std::unique_ptr<IImageDecoder> CreateNvJpegDecoder() {
    auto decoder = std::make_unique<NvJpegDecoder>();
    if (decoder->IsAvailable()) {
        OutputDebugStringW(L"[ImageDecoder] nvJPEG decoder initialized\n");
        return decoder;
    }
    OutputDebugStringW(L"[ImageDecoder] nvJPEG not available\n");
    return nullptr;
}

#else
#warning "nvJPEG support not compiled in. Define HAS_NVJPEG and link against nvJPEG library to enable."

std::unique_ptr<IImageDecoder> CreateNvJpegDecoder() {
    OutputDebugStringW(L"[ImageDecoder] nvJPEG not compiled in\n");
    return nullptr;
}

#endif // HAS_NVJPEG
