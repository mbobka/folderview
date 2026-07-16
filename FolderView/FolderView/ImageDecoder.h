#pragma once

#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

// Декодированное изображение в 32-битном premultiplied BGRA.
// Память pixels принадлежит структуре и выделяется из кучи процесса.
typedef struct DecodedImage {
    BYTE* pixels;
    SIZE_T pixels_size;
    UINT width;
    UINT height;
    UINT stride;
    BOOL has_alpha;
} DecodedImage;

// Инициализирует COM на текущем потоке и создаёт WIC-фабрику.
// Initialize и Shutdown должны вызываться на одном и том же потоке.
HRESULT ImageDecoder_Initialize(void);

// Освобождает WIC-фабрику и балансирует инициализацию COM.
void ImageDecoder_Shutdown(void);

// Декодирует первый кадр файла через WIC.
// Перед первым вызовом out_image должен быть обнулён. При успехе ранее
// принадлежавший out_image буфер освобождается и заменяется новым.
HRESULT ImageDecoder_DecodeFile(LPCWSTR path, DecodedImage* out_image);

// Декодирует первый кадр из памяти через WIC.
// Размер ограничен UINT_MAX из-за контракта SHCreateMemStream.
HRESULT ImageDecoder_DecodeMemory(
    const BYTE* data,
    SIZE_T data_size,
    DecodedImage* out_image);

// Проверяет наличие непустого согласованного BGRA-буфера.
BOOL DecodedImage_IsValid(const DecodedImage* image);

// Освобождает pixels через HeapFree и обнуляет все поля структуры.
void DecodedImage_Free(DecodedImage* image);

#ifdef __cplusplus
}
#endif
