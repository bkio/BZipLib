/// MIT License, Copyright Burak Kara, burak@burak.io, https://en.wikipedia.org/wiki/MIT_License
/// Petr Benes - https://bitbucket.org/wbenny/ziplib

#include "BZipArchiveEntry.h"
#include "BZipArchive.h"
#include "BZipFile.h"

#include "detail/ZipLocalFileHeader.h"

#include "methods/ZipMethodResolver.h"

#include "streams/zip_cryptostream.h"
#include "streams/compression_encoder_stream.h"
#include "streams/compression_decoder_stream.h"
#include "streams/nullstream.h"

#include "utils/stream_utils.h"
#include "utils/time_utils.h"

#include <iostream>
#include <cassert>
#include <sstream>

namespace
{
	bool IsValidFilename(const FString& fullPath)
	{
		// this function ensures, that the filename will have non-zero
		// length, when the filename will be normalized

		if (fullPath.Len() > 0)
		{
			FString tmpFilename = fullPath.Replace(TEXT("\\"), TEXT("/"));

			// if the filename is built only from '/', then it is invalid path
			for (auto Chr : tmpFilename)
			{
				if (Chr != '/')
				{
					return true;
				}
			}
		}
		return false;
	}

	bool IsDirectoryPath(const FString& fullPath)
	{
		return (fullPath.Len() > 0 && fullPath[fullPath.Len() - 1] == '/');
	}
}

BZipArchiveEntry::BZipArchiveEntry()
	: _archive(nullptr)
	, _rawStream(nullptr)
	, _compressionStream(nullptr)
	, _encryptionStream(nullptr)
	, _archiveStream(nullptr)

	, _inputStream(nullptr)

	, _originallyInArchive(false)
	, _isNewOrChanged(false)
	, _hasLocalFileHeader(false)

	, _offsetOfCompressedData(-1)
	, _offsetOfSerializedLocalFileHeader(-1)

{

}

BZipArchiveEntry::~BZipArchiveEntry()
{
	this->CloseRawStream();
	this->CloseDecompressionStream();
}

TSharedPtr<BZipArchiveEntry> BZipArchiveEntry::CreateNew(BZipArchive* zipArchive, const FString& fullPath)
{
	TSharedPtr<BZipArchiveEntry> result;

	assert(zipArchive != nullptr);

	if (IsValidFilename(fullPath))
	{
		result = MakeShareable(new BZipArchiveEntry());

		result->_archive = zipArchive;
		result->_isNewOrChanged = true;
		result->SetAttributes(Attributes::Archive);
		result->SetVersionToExtract(VERSION_NEEDED_DEFAULT);
		result->SetVersionMadeBy(VERSION_MADEBY_DEFAULT);
		result->SetLastWriteTime(time(nullptr));

		result->SetFullName(fullPath);

		result->SetCompressionMethod(StoreMethod::CompressionMethod);
		result->SetGeneralPurposeBitFlag(BitFlag::None);
	}

	return result;
}

TSharedPtr<BZipArchiveEntry> BZipArchiveEntry::CreateExisting(BZipArchive* zipArchive, detail::ZipCentralDirectoryFileHeader& cd)
{
	TSharedPtr<BZipArchiveEntry> result;

	assert(zipArchive != nullptr);

	if (IsValidFilename(FString(cd.Filename.c_str())))
	{
		result = MakeShareable(new BZipArchiveEntry());

		result->_archive = zipArchive;
		result->_centralDirectoryFileHeader = cd;
		result->_originallyInArchive = true;
		result->CheckFilenameCorrection();

		// determining folder by path has more priority
		// than attributes. however, if attributes
		// does not correspond with path, they will be fixed.
		result->SetAttributes(IsDirectoryPath(result->GetFullName())
			? Attributes::Directory
			: Attributes::Archive);
	}

	return result;
}

//////////////////////////////////////////////////////////////////////////
// public methods & getters & setters

FString BZipArchiveEntry::GetFullName()
{
	return FString(_centralDirectoryFileHeader.Filename.c_str());
}

void BZipArchiveEntry::SetFullName(const FString& fullName)
{
	FString filename = fullName.Replace(TEXT("\\"), TEXT("/"));
	FString correctFilename;

	bool isDirectory = IsDirectoryPath(filename);

	// if slash is first char, remove it
	while (filename.Len() > 0 && filename[0] == '/')
	{
		filename.RemoveAt(0);
	}

	// find multiply slashes
	bool prevWasSlash = false;
	for (std::string::size_type i = 0; i < filename.Len(); ++i)
	{
		if (filename[i] == '/' && prevWasSlash) continue;
		prevWasSlash = (filename[i] == '/');

		correctFilename += filename[i];
	}

	_centralDirectoryFileHeader.Filename = std::string(TCHAR_TO_UTF8(*correctFilename));
	_name = BZipFile::GetFilenameFromPath(correctFilename);

	this->SetAttributes(isDirectory ? Attributes::Directory : Attributes::Archive);
}

const FString& BZipArchiveEntry::GetName() const
{
	return _name;
}

void BZipArchiveEntry::SetName(const FString& name)
{
	FString folder;
	std::string::size_type dirDelimiterPos;

	// search for '/' in path name.
	// if this entry is directory, search up to one character
	// before the last one (which is '/')
	// if this entry is file, just search until last '/'
	// will be found
	
	std::string FullNameStr = TCHAR_TO_UTF8(*this->GetFullName());
	dirDelimiterPos = FullNameStr.find_last_of('/',
		(uint32)this->GetAttributes() & (uint32)Attributes::Archive
		? std::string::npos
		: FullNameStr.length() - 1);

	if (dirDelimiterPos != std::string::npos)
	{
		folder = FString(FullNameStr.substr(0, dirDelimiterPos).c_str());
	}

	this->SetFullName(folder + name);

	if (this->IsDirectory())
	{
		this->SetFullName(this->GetFullName() + '/');
	}
}

FString BZipArchiveEntry::GetComment()
{
	return FString(_centralDirectoryFileHeader.FileComment.c_str());
}

void BZipArchiveEntry::SetComment(const FString& comment)
{
	_centralDirectoryFileHeader.FileComment = std::string(TCHAR_TO_UTF8(*comment));
}

time_t BZipArchiveEntry::GetLastWriteTime() const
{
	return utils::time::datetime_to_timestamp(_centralDirectoryFileHeader.LastModificationDate, _centralDirectoryFileHeader.LastModificationTime);
}

void BZipArchiveEntry::SetLastWriteTime(time_t modTime)
{
	utils::time::timestamp_to_datetime(modTime, _centralDirectoryFileHeader.LastModificationDate, _centralDirectoryFileHeader.LastModificationTime);
}

BZipArchiveEntry::Attributes BZipArchiveEntry::GetAttributes() const
{
	return static_cast<Attributes>(_centralDirectoryFileHeader.ExternalFileAttributes);
}

uint16 BZipArchiveEntry::GetCompressionMethod() const
{
	return _centralDirectoryFileHeader.CompressionMethod;
}

void BZipArchiveEntry::SetAttributes(Attributes value)
{
	Attributes prevVal = this->GetAttributes();
	Attributes newVal = prevVal | value;

	// if we're changing from directory to file
	if (!!(prevVal & Attributes::Directory) && !!(newVal & Attributes::Archive))
	{
		newVal &= ~Attributes::Directory;

		if (IsDirectoryPath(FString(_centralDirectoryFileHeader.Filename.c_str())))
		{
			_centralDirectoryFileHeader.Filename.pop_back();
		}
	}

	// if we're changing from file to directory
	else if (!!(prevVal & Attributes::Archive) && !!(newVal & Attributes::Directory))
	{
		newVal &= ~Attributes::Archive;

		if (!IsDirectoryPath(FString(_centralDirectoryFileHeader.Filename.c_str())))
		{
			_centralDirectoryFileHeader.Filename += '/';
		}
	}

	// if this entry is directory, ensure that crc32 & sizes
	// are set to 0 and do not include any stream
	if (!!(newVal & Attributes::Directory))
	{
		_centralDirectoryFileHeader.Crc32 = 0;
		_centralDirectoryFileHeader.CompressedSize = 0;
		_centralDirectoryFileHeader.UncompressedSize = 0;
	}

	_centralDirectoryFileHeader.ExternalFileAttributes = static_cast<uint32>(newVal);
}

bool BZipArchiveEntry::IsPasswordProtected() const
{
	return !!(this->GetGeneralPurposeBitFlag() & BitFlag::Encrypted);
}

const FString& BZipArchiveEntry::GetPassword() const
{
	return _password;
}

void BZipArchiveEntry::SetPassword(const FString& password)
{
	_password = password;

	// allow unset password only for empty files
	if (!_originallyInArchive || (_hasLocalFileHeader && this->GetSize() == 0))
	{
		this->SetGeneralPurposeBitFlag(BitFlag::Encrypted, !_password.IsEmpty());
	}
}

uint32 BZipArchiveEntry::GetCrc32() const
{
	return _centralDirectoryFileHeader.Crc32;
}

uint64 BZipArchiveEntry::GetSize() const
{
	return static_cast<uint64>(_centralDirectoryFileHeader.UncompressedSize);
}

uint64 BZipArchiveEntry::GetCompressedSize() const
{
	return static_cast<uint64>(_centralDirectoryFileHeader.CompressedSize);
}


bool BZipArchiveEntry::CanExtract() const
{
	return (this->GetVersionToExtract() <= VERSION_MADEBY_DEFAULT);
}

bool BZipArchiveEntry::IsDirectory() const
{
	return !!(this->GetAttributes() & Attributes::Directory);
}

bool BZipArchiveEntry::IsUsingDataDescriptor() const
{
	return !!(this->GetGeneralPurposeBitFlag() & BitFlag::DataDescriptor);
}

void BZipArchiveEntry::UseDataDescriptor(bool use)
{
	this->SetGeneralPurposeBitFlag(BitFlag::DataDescriptor, use);
}

std::istream* BZipArchiveEntry::GetRawStream()
{
	if (_rawStream == nullptr)
	{
		if (_originallyInArchive)
		{
			auto offsetOfCompressedData = this->SeekToCompressedData();
			_rawStream = MakeShareable<isubstream>(new isubstream(*_archive->_zipStream, offsetOfCompressedData, this->GetCompressedSize()));
		}
		else
		{
			_rawStream = MakeShareable<isubstream>(new isubstream(*_immediateBuffer));
		}
	}

	return _rawStream.Get();
}

std::istream* BZipArchiveEntry::GetDecompressionStream()
{
	TSharedPtr<std::istream> intermediateStream;

	// there shouldn't be opened another stream
	if (this->CanExtract() && _archiveStream == nullptr && _encryptionStream == nullptr)
	{
		auto offsetOfCompressedData = this->SeekToCompressedData();
		bool needsPassword = !!(this->GetGeneralPurposeBitFlag() & BitFlag::Encrypted);
		bool needsDecompress = this->GetCompressionMethod() != StoreMethod::CompressionMethod;

		if (needsPassword && _password.IsEmpty())
		{
			// we need password, but we does not have it
			return nullptr;
		}

		// make correctly-ended sub stream of the input stream
		intermediateStream = _archiveStream = MakeShareable<isubstream>(new isubstream(*_archive->_zipStream, offsetOfCompressedData, this->GetCompressedSize()));

		if (needsPassword)
		{
			TSharedPtr<zip_cryptostream> cryptoStream = MakeShareable<zip_cryptostream>(new zip_cryptostream(*intermediateStream, TCHAR_TO_UTF8(*_password)));
			cryptoStream->set_final_byte(this->GetLastByteOfEncryptionHeader());
			bool hasCorrectPassword = cryptoStream->prepare_for_decryption();

			// set it here, because in case the hasCorrectPassword is false
			// the method CloseDecompressionStream() will properly delete the stream
			intermediateStream = _encryptionStream = cryptoStream;

			if (!hasCorrectPassword)
			{
				this->CloseDecompressionStream();
				return nullptr;
			}
		}

		if (needsDecompress)
		{
			TSharedPtr<ICompressionMethod> zipMethod = ZipMethodResolver::GetZipMethodInstance(this->GetCompressionMethod());

			if (zipMethod != nullptr)
			{
				intermediateStream = _compressionStream = MakeShareable<compression_decoder_stream>(new compression_decoder_stream(zipMethod->GetDecoder(), zipMethod->GetDecoderProperties(), *intermediateStream));
			}
		}
	}

	return intermediateStream.Get();
}

bool BZipArchiveEntry::IsRawStreamOpened() const
{
	return _rawStream != nullptr;
}

bool BZipArchiveEntry::IsDecompressionStreamOpened() const
{
	return _compressionStream != nullptr;
}

void BZipArchiveEntry::CloseRawStream()
{
	_rawStream.Reset();
}

void BZipArchiveEntry::CloseDecompressionStream()
{
	_compressionStream.Reset();
	_encryptionStream.Reset();
	_archiveStream.Reset();
	_immediateBuffer.Reset();
}

bool BZipArchiveEntry::SetCompressionStream(std::istream& stream, TSharedPtr<ICompressionMethod> method /* = DeflateMethod::Create() */, CompressionMode mode /* = CompressionMode::Deferred */)
{
	// if _inputStream is set, we already have some stream to compress
	// so we discard it
	if (_inputStream != nullptr)
	{
		this->UnloadCompressionData();
	}

	_isNewOrChanged = true;

	_inputStream = &stream;
	_compressionMethod = method;
	_compressionMode = mode;
	this->SetCompressionMethod(method->GetZipMethodDescriptor().GetCompressionMethod());

	if (_inputStream != nullptr && _compressionMode == CompressionMode::Immediate)
	{
		_immediateBuffer = MakeShareable<std::stringstream>(new std::stringstream());
		this->InternalCompressStream(*_inputStream, *_immediateBuffer);

		// we have everything we need, let's act like we were loaded from archive :)
		_isNewOrChanged = false;
		_inputStream = nullptr;
	}

	return true;
}

void BZipArchiveEntry::UnsetCompressionStream()
{
	if (!this->HasCompressionStream())
	{
		this->FetchLocalFileHeader();
	}

	this->UnloadCompressionData();
	this->SetPassword(FString());
}

//////////////////////////////////////////////////////////////////////////
// private getters & setters

void BZipArchiveEntry::SetCompressionMethod(uint16 value)
{
	_centralDirectoryFileHeader.CompressionMethod = value;
}

BZipArchiveEntry::BitFlag BZipArchiveEntry::GetGeneralPurposeBitFlag() const
{
	return static_cast<BitFlag>(_centralDirectoryFileHeader.GeneralPurposeBitFlag);
}

void BZipArchiveEntry::SetGeneralPurposeBitFlag(BitFlag value, bool set)
{
	if (set)
	{
		_centralDirectoryFileHeader.GeneralPurposeBitFlag |= static_cast<uint16>(value);
	}
	else
	{
		_centralDirectoryFileHeader.GeneralPurposeBitFlag &= static_cast<uint16>(~value);
	}
}

uint16 BZipArchiveEntry::GetVersionToExtract() const
{
	return _centralDirectoryFileHeader.VersionNeededToExtract;
}

void BZipArchiveEntry::SetVersionToExtract(uint16 value)
{
	_centralDirectoryFileHeader.VersionNeededToExtract = value;
}

uint16 BZipArchiveEntry::GetVersionMadeBy() const
{
	return _centralDirectoryFileHeader.VersionMadeBy;
}

void BZipArchiveEntry::SetVersionMadeBy(uint16 value)
{
	_centralDirectoryFileHeader.VersionMadeBy = value;
}

int32 BZipArchiveEntry::GetOffsetOfLocalHeader() const
{
	return _centralDirectoryFileHeader.RelativeOffsetOfLocalHeader;
}

void BZipArchiveEntry::SetOffsetOfLocalHeader(int32 value)
{
	_centralDirectoryFileHeader.RelativeOffsetOfLocalHeader = static_cast<int32>(value);
}

bool BZipArchiveEntry::HasCompressionStream() const
{
	return _inputStream != nullptr;
}

//////////////////////////////////////////////////////////////////////////
// private working methods

void BZipArchiveEntry::FetchLocalFileHeader()
{
	if (!_hasLocalFileHeader && _originallyInArchive && _archive != nullptr)
	{
		_archive->_zipStream->seekg(this->GetOffsetOfLocalHeader(), std::ios::beg);
		_localFileHeader.Deserialize(*_archive->_zipStream);

		_offsetOfCompressedData = _archive->_zipStream->tellg();
	}

	// sync data
	this->SyncLFH_with_CDFH();
	_hasLocalFileHeader = true;
}

void BZipArchiveEntry::CheckFilenameCorrection()
{
	// this forces recheck of the filename.
	// this is useful when the check is needed after
	// deserialization
	this->SetFullName(this->GetFullName());
}

void BZipArchiveEntry::FixVersionToExtractAtLeast(uint16 value)
{
	if (this->GetVersionToExtract() < value)
	{
		this->SetVersionToExtract(value);
	}
}

void BZipArchiveEntry::SyncLFH_with_CDFH()
{
	_localFileHeader.SyncWithCentralDirectoryFileHeader(_centralDirectoryFileHeader);
}

void BZipArchiveEntry::SyncCDFH_with_LFH()
{
	_centralDirectoryFileHeader.SyncWithLocalFileHeader(_localFileHeader);

	this->FixVersionToExtractAtLeast(this->IsDirectory()
		? VERSION_NEEDED_EXPLICIT_DIRECTORY
		: _compressionMethod->GetZipMethodDescriptor().GetVersionNeededToExtract());
}

std::ios::pos_type BZipArchiveEntry::GetOffsetOfCompressedData()
{
	if (!_hasLocalFileHeader)
	{
		this->FetchLocalFileHeader();
	}

	return _offsetOfCompressedData;
}

std::ios::pos_type BZipArchiveEntry::SeekToCompressedData()
{
	// check for fail bit?
	_archive->_zipStream->seekg(this->GetOffsetOfCompressedData(), std::ios::beg);
	return this->GetOffsetOfCompressedData();
}

void BZipArchiveEntry::SerializeLocalFileHeader(std::ostream& stream)
{
	// ensure opening the stream
	std::istream* compressedDataStream = nullptr;

	if (!this->IsDirectory())
	{
		if (_inputStream == nullptr)
		{
			if (!_isNewOrChanged)
			{
				// the file was either compressed in immediate mode,
				// or was in previous archive
				compressedDataStream = this->GetRawStream();
			}

			// if file is new and empty or stream has been set to nullptr,
			// just do not set any compressed data stream
		}
		else
		{
			assert(_isNewOrChanged);
			compressedDataStream = _inputStream;
		}
	}

	if (!_hasLocalFileHeader)
	{
		this->FetchLocalFileHeader();
	}

	// save offset of stream here
	_offsetOfSerializedLocalFileHeader = stream.tellp();

	if (this->IsUsingDataDescriptor())
	{
		_localFileHeader.CompressedSize = 0;
		_localFileHeader.UncompressedSize = 0;
		_localFileHeader.Crc32 = 0;
	}

	_localFileHeader.Serialize(stream);

	// if this entry is a directory, it should not contain any data
	// nor crc.
	assert(
		this->IsDirectory()
		? !GetCrc32() && !GetSize() && !GetCompressedSize() && !_inputStream
		: true
	);

	if (!this->IsDirectory() && compressedDataStream != nullptr)
	{
		if (_isNewOrChanged)
		{
			this->InternalCompressStream(*compressedDataStream, stream);

			if (this->IsUsingDataDescriptor())
			{
				_localFileHeader.SerializeAsDataDescriptor(stream);
			}
			else
			{
				// actualize local file header
				// make non-seekable version?
				stream.seekp(_offsetOfSerializedLocalFileHeader);
				_localFileHeader.Serialize(stream);
				stream.seekp(this->GetCompressedSize(), std::ios::cur);
			}
		}
		else
		{
			utils::stream::copy(*compressedDataStream, stream);
		}
	}
}

void BZipArchiveEntry::SerializeCentralDirectoryFileHeader(std::ostream& stream)
{
	_centralDirectoryFileHeader.RelativeOffsetOfLocalHeader = static_cast<int32>(_offsetOfSerializedLocalFileHeader);
	_centralDirectoryFileHeader.Serialize(stream);
}

void BZipArchiveEntry::UnloadCompressionData()
{
	// unload stream
	_immediateBuffer->clear();
	_inputStream = nullptr;

	_centralDirectoryFileHeader.CompressedSize = 0;
	_centralDirectoryFileHeader.UncompressedSize = 0;
	_centralDirectoryFileHeader.Crc32 = 0;
}

void BZipArchiveEntry::InternalCompressStream(std::istream& inputStream, std::ostream& outputStream)
{
	std::ostream* intermediateStream = &outputStream;

	TUniquePtr<zip_cryptostream> cryptoStream;
	if (!_password.IsEmpty())
	{
		this->SetGeneralPurposeBitFlag(BitFlag::Encrypted);

		// std::make_unique<zip_cryptostream>();
		cryptoStream = TUniquePtr<zip_cryptostream>(new zip_cryptostream());

		cryptoStream->init(outputStream, TCHAR_TO_UTF8(*_password));
		cryptoStream->set_final_byte(this->GetLastByteOfEncryptionHeader());
		intermediateStream = cryptoStream.Get();
	}

	crc32stream crc32Stream;
	crc32Stream.init(inputStream);

	compression_encoder_stream compressionStream(
		_compressionMethod->GetEncoder(),
		_compressionMethod->GetEncoderProperties(),
		*intermediateStream);
	intermediateStream = &compressionStream;
	utils::stream::copy(crc32Stream, *intermediateStream);

	intermediateStream->flush();

	_localFileHeader.UncompressedSize = static_cast<uint32>(compressionStream.get_bytes_read());
	_localFileHeader.CompressedSize = static_cast<uint32>(compressionStream.get_bytes_written() + (!_password.IsEmpty() ? 12 : 0));
	_localFileHeader.Crc32 = crc32Stream.get_crc32();

	this->SyncCDFH_with_LFH();
}

void BZipArchiveEntry::FigureCrc32()
{
	if (this->IsDirectory() || _inputStream == nullptr || !_isNewOrChanged)
	{
		return;
	}

	// stream must be seekable
	auto position = _inputStream->tellg();

	// compute crc32
	crc32stream crc32Stream;
	crc32Stream.init(*_inputStream);

	// just force to read all from crc32stream
	nullstream nulldev;
	utils::stream::copy(crc32Stream, nulldev);

	// seek back
	_inputStream->clear();
	_inputStream->seekg(position);

	_centralDirectoryFileHeader.Crc32 = crc32Stream.get_crc32();
}

uint8 BZipArchiveEntry::GetLastByteOfEncryptionHeader()
{
	if (!!(this->GetGeneralPurposeBitFlag() & BitFlag::DataDescriptor))
	{
		// In the case that bit 3 of the general purpose bit flag is set to
		// indicate the presence of a 'data descriptor' (signature
		// 0x08074b50), the last byte of the decrypted header is sometimes
		// compared with the high-order byte of the lastmodified time,
		// rather than the high-order byte of the CRC, to verify the
		// password.
		//
		// This is not documented in the PKWare Appnote.txt.
		// This was discovered this by analysis of the Crypt.c source file in the
		// InfoZip library
		// http://www.info-zip.org/pub/infozip/

		// Also, winzip insists on this!
		return uint8((_centralDirectoryFileHeader.LastModificationTime >> 8) & 0xff);
	}
	else
	{
		// When bit 3 is not set, the CRC value is required before
		// encryption of the file data begins. In this case there is no way
		// around it: must read the stream in its entirety to compute the
		// actual CRC before proceeding.
		this->FigureCrc32();
		return uint8((this->GetCrc32() >> 24) & 0xff);
	}
}
