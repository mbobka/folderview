#pragma once

#include <windows.h>
#include <vector>
#include <string>
#include <memory>
#include <cstdint>
#include <fstream>

// Структура для описания файла в ZIP-архиве
struct ZipEntry {
    std::wstring name;
    uint64_t compressedSize;
    uint64_t uncompressedSize;
    uint64_t localHeaderOffset;
    uint16_t compressionMethod;
    bool isDirectory;
};

// Чтение ZIP-архива без распаковки на диск
class ZipArchive {
public:
    ZipArchive();
    ~ZipArchive();

    // Открыть ZIP-файл
    bool Open(const std::wstring& path);
    
    // Закрыть архив
    void Close();

    // Проверка, открыт ли архив
    bool IsOpen() const { return isOpen_; }

    // Получить список файлов
    const std::vector<ZipEntry>& GetEntries() const { return entries_; }

    // Извлечь файл в память
    bool ExtractToMemory(const ZipEntry& entry, std::vector<uint8_t>& outData);

    // Путь к архиву
    const std::wstring& GetPath() const { return archivePath_; }

private:
    bool ReadCentralDirectory();
    bool Inflate(const uint8_t* compressedData, uint64_t compressedSize,
                 uint8_t* uncompressedData, uint64_t uncompressedSize);

    std::wstring archivePath_;
    std::vector<ZipEntry> entries_;
    HANDLE fileHandle_ = INVALID_HANDLE_VALUE;
    HANDLE fileMapping_ = nullptr;
    const uint8_t* mappedData_ = nullptr;
    uint64_t fileSize_ = 0;
    bool isOpen_ = false;
};

// Глобальный экземпляр открытого ZIP-архива
class ZipArchiveManager {
public:
    static ZipArchiveManager& Instance();
    
    // Открыть ZIP как виртуальную папку
    bool OpenAsVirtualFolder(const std::wstring& zipPath);
    
    // Закрыть текущий архив
    void Close();
    
    // Проверить, работаем ли мы с ZIP
    bool IsZipMode() const { return archive_ && archive_->IsOpen(); }
    
    // Получить список изображений из архива
    std::vector<std::wstring> GetImageFiles(const std::vector<std::wstring>& extensions);
    
    // Извлечь изображение в память
    bool ExtractImage(const std::wstring& entryName, std::vector<uint8_t>& outData);
    
    // Путь к архиву
    std::wstring GetArchivePath() const;

private:
    ZipArchiveManager() = default;
    std::unique_ptr<ZipArchive> archive_;
};
