#include "ZipArchive.h"
#include <algorithm>
#include <cctype>

#include <zlib.h>

// ZIP-сигнатуры
constexpr uint32_t ZIP_LOCAL_FILE_HEADER_SIG = 0x04034b50;
constexpr uint32_t ZIP_CENTRAL_DIR_SIG = 0x02014b50;
constexpr uint32_t ZIP_END_OF_CENTRAL_DIR_SIG = 0x06054b50;
constexpr uint32_t ZIP64_END_OF_CENTRAL_DIR_SIG = 0x06064b50;
constexpr uint32_t ZIP64_END_OF_CENTRAL_DIR_LOCATOR_SIG = 0x07064b50;

// Методы сжатия
constexpr uint16_t ZIP_METHOD_STORED = 0;
constexpr uint16_t ZIP_METHOD_DEFLATE = 8;

// ZIP64 extra field tag
constexpr uint16_t ZIP64_EXTRA_FIELD_TAG = 0x0001;

#pragma pack(push, 1)
struct ZipEndOfCentralDir {
	uint32_t signature;
	uint16_t diskNumber;
	uint16_t diskWithCentralDir;
	uint16_t entriesOnDisk;
	uint16_t totalEntries;
	uint32_t centralDirSize;
	uint32_t centralDirOffset;
	uint16_t commentLength;
};

struct Zip64EndOfCentralDirLocator {
	uint32_t signature;
	uint32_t diskWithZip64EOCD;
	uint64_t zip64EOCDOffset;
	uint32_t totalDisks;
};

struct Zip64EndOfCentralDir {
	uint32_t signature;
	uint64_t sizeOfRecord;
	uint16_t versionMadeBy;
	uint16_t versionNeeded;
	uint32_t diskNumber;
	uint32_t diskWithCentralDir;
	uint64_t entriesOnDisk;
	uint64_t totalEntries;
	uint64_t centralDirSize;
	uint64_t centralDirOffset;
};

struct ZipCentralDirHeader {
	uint32_t signature;
	uint16_t versionMadeBy;
	uint16_t versionNeeded;
	uint16_t flags;
	uint16_t compressionMethod;
	uint16_t lastModTime;
	uint16_t lastModDate;
	uint32_t crc32;
	uint32_t compressedSize;
	uint32_t uncompressedSize;
	uint16_t fileNameLength;
	uint16_t extraFieldLength;
	uint16_t commentLength;
	uint16_t diskStart;
	uint16_t internalAttrs;
	uint32_t externalAttrs;
	uint32_t localHeaderOffset;
};

struct ZipLocalFileHeader {
	uint32_t signature;
	uint16_t versionNeeded;
	uint16_t flags;
	uint16_t compressionMethod;
	uint16_t lastModTime;
	uint16_t lastModDate;
	uint32_t crc32;
	uint32_t compressedSize;
	uint32_t uncompressedSize;
	uint16_t fileNameLength;
	uint16_t extraFieldLength;
};
#pragma pack(pop)

// Парсинг ZIP64 extra field из central directory
static void ParseZip64ExtraField(const uint8_t* extraData, uint16_t extraLen,
	const ZipCentralDirHeader* header, ZipEntry& entry)
{
	const uint8_t* ptr = extraData;
	const uint8_t* end = extraData + extraLen;

	while (ptr + 4 <= end) {
		uint16_t tag = *reinterpret_cast<const uint16_t*>(ptr);
		uint16_t size = *reinterpret_cast<const uint16_t*>(ptr + 2);
		ptr += 4;

		if (ptr + size > end) break;

		if (tag == ZIP64_EXTRA_FIELD_TAG) {
			const uint8_t* fieldPtr = ptr;
			// Поля присутствуют только если соответствующее значение в заголовке == 0xFFFFFFFF
			if (header->uncompressedSize == 0xFFFFFFFF && fieldPtr + 8 <= ptr + size) {
				entry.uncompressedSize = *reinterpret_cast<const uint64_t*>(fieldPtr);
				fieldPtr += 8;
			}
			if (header->compressedSize == 0xFFFFFFFF && fieldPtr + 8 <= ptr + size) {
				entry.compressedSize = *reinterpret_cast<const uint64_t*>(fieldPtr);
				fieldPtr += 8;
			}
			if (header->localHeaderOffset == 0xFFFFFFFF && fieldPtr + 8 <= ptr + size) {
				entry.localHeaderOffset = *reinterpret_cast<const uint64_t*>(fieldPtr);
				fieldPtr += 8;
			}
			break;
		}

		ptr += size;
	}
}

ZipArchive::ZipArchive() = default;

ZipArchive::~ZipArchive() {
	Close();
}

bool ZipArchive::Open(const std::wstring& path) {
	Close();

	fileHandle_ = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ,
		nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);

	if (fileHandle_ == INVALID_HANDLE_VALUE) {
		return false;
	}

	LARGE_INTEGER size;
	if (!GetFileSizeEx(fileHandle_, &size)) {
		Close();
		return false;
	}
	fileSize_ = size.QuadPart;

	fileMapping_ = CreateFileMappingW(fileHandle_, nullptr, PAGE_READONLY, 0, 0, nullptr);
	if (!fileMapping_) {
		Close();
		return false;
	}

	mappedData_ = static_cast<const uint8_t*>(MapViewOfFile(fileMapping_, FILE_MAP_READ, 0, 0, 0));
	if (!mappedData_) {
		Close();
		return false;
	}

	archivePath_ = path;
	isOpen_ = true;

	if (!ReadCentralDirectory()) {
		Close();
		return false;
	}

	return true;
}

void ZipArchive::Close() {
	if (mappedData_) {
		UnmapViewOfFile(mappedData_);
		mappedData_ = nullptr;
	}
	if (fileMapping_) {
		CloseHandle(fileMapping_);
		fileMapping_ = nullptr;
	}
	if (fileHandle_ != INVALID_HANDLE_VALUE) {
		CloseHandle(fileHandle_);
		fileHandle_ = INVALID_HANDLE_VALUE;
	}
	entries_.clear();
	archivePath_.clear();
	fileSize_ = 0;
	isOpen_ = false;
}

bool ZipArchive::ReadCentralDirectory() {
	if (fileSize_ < sizeof(ZipEndOfCentralDir)) {
		return false;
	}

	// Ищем End of Central Directory (с конца файла)
	const uint8_t* endPtr = nullptr;
	size_t searchLimit = std::min<uint64_t>(fileSize_, 65535 + sizeof(ZipEndOfCentralDir));

	for (size_t i = sizeof(ZipEndOfCentralDir); i <= searchLimit; ++i) {
		const auto* candidate = reinterpret_cast<const ZipEndOfCentralDir*>(
			mappedData_ + fileSize_ - i);
		if (candidate->signature == ZIP_END_OF_CENTRAL_DIR_SIG) {
			endPtr = mappedData_ + fileSize_ - i;
			break;
		}
	}

	if (!endPtr) {
		OutputDebugStringW(L"[ZIP] End of Central Directory not found\n");
		return false;
	}

	const auto* eocd = reinterpret_cast<const ZipEndOfCentralDir*>(endPtr);

	// Определяем количество записей и смещение Central Directory
	uint64_t totalEntries = eocd->totalEntries;
	uint64_t centralDirOffset = eocd->centralDirOffset;

	// Проверяем наличие ZIP64 End of Central Directory Locator
	// Он находится прямо перед EOCD
	if (endPtr - mappedData_ >= static_cast<ptrdiff_t>(sizeof(Zip64EndOfCentralDirLocator))) {
		const auto* locator = reinterpret_cast<const Zip64EndOfCentralDirLocator*>(
			endPtr - sizeof(Zip64EndOfCentralDirLocator));

		if (locator->signature == ZIP64_END_OF_CENTRAL_DIR_LOCATOR_SIG) {
			// Нашли ZIP64 Locator — читаем ZIP64 EOCD
			uint64_t zip64EOCDOffset = locator->zip64EOCDOffset;

			if (zip64EOCDOffset + sizeof(Zip64EndOfCentralDir) <= fileSize_) {
				const auto* eocd64 = reinterpret_cast<const Zip64EndOfCentralDir*>(
					mappedData_ + zip64EOCDOffset);

				if (eocd64->signature == ZIP64_END_OF_CENTRAL_DIR_SIG) {
					totalEntries = eocd64->totalEntries;
					centralDirOffset = eocd64->centralDirOffset;

					OutputDebugStringW(L"[ZIP] ZIP64 format detected\n");
				}
			}
		}
	}

	if (centralDirOffset >= fileSize_) {
		return false;
	}

	// Читаем записи Central Directory
	const uint8_t* dirPtr = mappedData_ + centralDirOffset;
	const uint8_t* dirEnd = mappedData_ + fileSize_;

	for (uint64_t i = 0; i < totalEntries && dirPtr < dirEnd; ++i) {
		if (dirPtr + sizeof(ZipCentralDirHeader) > dirEnd) {
			break;
		}

		const auto* header = reinterpret_cast<const ZipCentralDirHeader*>(dirPtr);
		if (header->signature != ZIP_CENTRAL_DIR_SIG) {
			break;
		}

		const char* namePtr = reinterpret_cast<const char*>(dirPtr + sizeof(ZipCentralDirHeader));

		if (dirPtr + sizeof(ZipCentralDirHeader) + header->fileNameLength > dirEnd) {
			break;
		}

		ZipEntry entry;

		// Конвертируем имя файла из UTF-8 в wstring
		std::string nameUtf8(namePtr, header->fileNameLength);
		int wideLen = MultiByteToWideChar(CP_UTF8, 0, nameUtf8.c_str(),
			static_cast<int>(nameUtf8.length()), nullptr, 0);
		if (wideLen > 0) {
			entry.name.resize(wideLen);
			MultiByteToWideChar(CP_UTF8, 0, nameUtf8.c_str(),
				static_cast<int>(nameUtf8.length()), &entry.name[0], wideLen);
		}

		// Заменяем / на backslash для консистентности с Windows путями
		std::replace(entry.name.begin(), entry.name.end(), L'/', L'\\');

		entry.compressedSize = header->compressedSize;
		entry.uncompressedSize = header->uncompressedSize;
		entry.localHeaderOffset = header->localHeaderOffset;
		entry.compressionMethod = header->compressionMethod;
		entry.isDirectory = !entry.name.empty() && entry.name.back() == L'\\';

		// Парсим ZIP64 extra field если нужно
		if (header->extraFieldLength > 0 &&
			(header->compressedSize == 0xFFFFFFFF ||
				header->uncompressedSize == 0xFFFFFFFF ||
				header->localHeaderOffset == 0xFFFFFFFF))
		{
			const uint8_t* extraData = reinterpret_cast<const uint8_t*>(
				namePtr + header->fileNameLength);
			if (dirPtr + sizeof(ZipCentralDirHeader) + header->fileNameLength +
				header->extraFieldLength <= dirEnd)
			{
				ParseZip64ExtraField(extraData, header->extraFieldLength, header, entry);
			}
		}

		if (!entry.isDirectory) {
			entries_.push_back(std::move(entry));
		}

		dirPtr += sizeof(ZipCentralDirHeader) + header->fileNameLength +
			header->extraFieldLength + header->commentLength;
	}

	wchar_t msg[256];
	swprintf_s(msg, L"[ZIP] Loaded %zu entries from %s\n", entries_.size(), archivePath_.c_str());
	OutputDebugStringW(msg);

	return !entries_.empty();
}

bool ZipArchive::Inflate(const uint8_t* compressedData, uint64_t compressedSize, uint8_t* uncompressedData, uint64_t uncompressedSize) {
	z_stream strm{};
	strm.next_in = const_cast<Bytef*>(compressedData);
	strm.avail_in = static_cast<uInt>(std::min<uint64_t>(compressedSize, UINT_MAX));
	strm.next_out = uncompressedData;
	strm.avail_out = static_cast<uInt>(std::min<uint64_t>(uncompressedSize, UINT_MAX));

	// -MAX_WBITS = raw deflate (без zlib/gzip заголовка), как в ZIP
	int ret = inflateInit2(&strm, -MAX_WBITS);
	if (ret != Z_OK) {
		return false;
	}

	ret = inflate(&strm, Z_FINISH);
	inflateEnd(&strm);

	if (ret != Z_STREAM_END) {
		wchar_t msg[128];
		swprintf_s(msg, L"[ZIP] zlib inflate failed: %d\n", ret);
		OutputDebugStringW(msg);
		return false;
	}

	return strm.total_out == uncompressedSize;
}

bool ZipArchive::ExtractToMemory(const ZipEntry& entry, std::vector<uint8_t>& outData) {
	if (!isOpen_ || entry.localHeaderOffset >= fileSize_) {
		return false;
	}

	const auto* localHeader = reinterpret_cast<const ZipLocalFileHeader*>(
		mappedData_ + entry.localHeaderOffset);

	if (localHeader->signature != ZIP_LOCAL_FILE_HEADER_SIG) {
		return false;
	}

	// Данные начинаются после заголовка, имени файла и extra field
	uint64_t dataOffset = entry.localHeaderOffset + sizeof(ZipLocalFileHeader) +
		localHeader->fileNameLength + localHeader->extraFieldLength;

	if (dataOffset + entry.compressedSize > fileSize_) {
		return false;
	}

	const uint8_t* compressedData = mappedData_ + dataOffset;

	if (entry.compressionMethod == ZIP_METHOD_STORED) {
		// Без сжатия — просто копируем
		outData.assign(compressedData, compressedData + entry.uncompressedSize);
		return true;
	}
	else if (entry.compressionMethod == ZIP_METHOD_DEFLATE) {
		outData.resize(entry.uncompressedSize);

		if (Inflate(compressedData, entry.compressedSize, outData.data(), entry.uncompressedSize)) {
			return true;
		}

		outData.clear();
		return false;
	}

	OutputDebugStringW(L"[ZIP] Unsupported compression method\n");
	return false;
}

// ZipArchiveManager implementation
static std::unique_ptr<ZipArchiveManager> g_zipManager;

ZipArchiveManager& ZipArchiveManager::Instance() {
	if (!g_zipManager) {
		g_zipManager = std::unique_ptr<ZipArchiveManager>(new ZipArchiveManager());
	}
	return *g_zipManager;
}

bool ZipArchiveManager::OpenAsVirtualFolder(const std::wstring& zipPath) {
	Close();

	archive_ = std::make_unique<ZipArchive>();
	if (!archive_->Open(zipPath)) {
		archive_.reset();
		return false;
	}

	return true;
}

void ZipArchiveManager::Close() {
	if (archive_) {
		archive_->Close();
		archive_.reset();
	}
}

std::vector<std::wstring> ZipArchiveManager::GetImageFiles(const std::vector<std::wstring>& extensions) {
	std::vector<std::wstring> result;

	if (!archive_ || !archive_->IsOpen()) {
		return result;
	}

	for (const auto& entry : archive_->GetEntries()) {
		if (entry.isDirectory) continue;

		// Получаем расширение
		size_t dotPos = entry.name.find_last_of(L'.');
		if (dotPos == std::wstring::npos)
			continue;

		std::wstring ext = entry.name.substr(dotPos);

		// Проверяем, поддерживается ли расширение
		for (const auto& supportedExt : extensions) {
			if (lstrcmpiW(ext.c_str(), supportedExt.c_str()) == 0) {
				result.push_back(entry.name);
				break;
			}
		}
	}

	// Сортируем по имени
	std::sort(result.begin(), result.end());

	return result;
}

bool ZipArchiveManager::ExtractImage(const std::wstring& entryName, std::vector<uint8_t>& outData) {
	if (!archive_ || !archive_->IsOpen()) {
		return false;
	}

	for (const auto& entry : archive_->GetEntries()) {
		if (entry.name == entryName) {
			return archive_->ExtractToMemory(entry, outData);
		}
	}

	return false;
}

std::wstring ZipArchiveManager::GetArchivePath() const {
	if (archive_ && archive_->IsOpen()) {
		return archive_->GetPath();
	}
	return L"";
}
