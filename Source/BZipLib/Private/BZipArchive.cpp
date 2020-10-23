/// MIT License, Copyright Burak Kara, burak@burak.io, https://en.wikipedia.org/wiki/MIT_License
/// Petr Benes - https://bitbucket.org/wbenny/ziplib

#include "BZipArchive.h"
#include "streams/serialization.h"
#include <cassert>

#define CALL_CONST_METHOD(expression) \
  const_cast<      std::remove_pointer<std::remove_const<decltype(expression)>::type>::type*>( \
  const_cast<const std::remove_pointer<std::remove_const<decltype(this)      >::type>::type*>(this)->expression)

//////////////////////////////////////////////////////////////////////////
// zip archive

TSharedPtr<BZipArchive> BZipArchive::Create()
{
	return TSharedPtr<BZipArchive>(new BZipArchive());
}

TSharedPtr<BZipArchive> BZipArchive::Create(TSharedPtr<BZipArchive>&& other)
{
	TSharedPtr<BZipArchive> result(new BZipArchive());

	result->_endOfCentralDirectoryBlock = other->_endOfCentralDirectoryBlock;
	result->_entries = std::move(other->_entries);
	result->_zipStream = other->_zipStream;
	result->_owningStream = other->_owningStream;

	// clean "other"
	other->_zipStream = nullptr;
	other->_owningStream = false;

	return result;
}

TSharedPtr<BZipArchive> BZipArchive::Create(std::istream& stream)
{
	TSharedPtr<BZipArchive> result(new BZipArchive());

	result->_zipStream = &stream;
	result->_owningStream = false;

	result->ReadEndOfCentralDirectory();
	result->EnsureCentralDirectoryRead();

	return result;
}

TSharedPtr<BZipArchive> BZipArchive::Create(std::istream* stream, bool takeOwnership)
{
	TSharedPtr<BZipArchive> result(new BZipArchive());

	result->_zipStream = stream;
	result->_owningStream = stream != nullptr ? takeOwnership : false;

	if (stream != nullptr)
	{
		result->ReadEndOfCentralDirectory();
		result->EnsureCentralDirectoryRead();
	}

	return result;
}

BZipArchive::BZipArchive()
	: _zipStream(nullptr)
	, _owningStream(false)
{

}

BZipArchive::~BZipArchive()
{
	this->InternalDestroy();
}

BZipArchive& BZipArchive::operator = (BZipArchive&& other)
{
	_endOfCentralDirectoryBlock = other._endOfCentralDirectoryBlock;
	_entries = std::move(other._entries);
	_zipStream = other._zipStream;
	_owningStream = other._owningStream;

	// clean "other"
	other._zipStream = nullptr;
	other._owningStream = false;

	return *this;
}

TSharedPtr<BZipArchiveEntry> BZipArchive::CreateEntry(const FString& fileName)
{
	TSharedPtr<BZipArchiveEntry> result = nullptr;

	if ((result = this->GetEntry(fileName)) == nullptr)
	{
		if ((result = BZipArchiveEntry::CreateNew(this, fileName)) != nullptr)
		{
			_entries.Add(result);
		}
	}

	return result;
}

FString BZipArchive::GetComment()
{
	return FString(_endOfCentralDirectoryBlock.Comment.c_str());
}

void BZipArchive::SetComment(const FString& comment)
{
	_endOfCentralDirectoryBlock.Comment = TCHAR_TO_UTF8(*comment);
}

TSharedPtr<BZipArchiveEntry> BZipArchive::GetEntry(const FString& entryName)
{
	for (int32 i = 0; i < _entries.Num(); i++)
	{
		if (_entries[i].IsValid() && _entries[i]->GetFullName() == entryName)
		{
			return _entries[i];
		}
	}
	return nullptr;
}

TSharedPtr<BZipArchiveEntry> BZipArchive::GetEntry(int32 index)
{
	if (index >= 0 && index < _entries.Num())
	{
		return _entries[index];
	}
	return nullptr;
}

int32 BZipArchive::GetEntriesCount() const
{
	return _entries.Num();
}

TSharedPtr<BZipArchiveEntry> BZipArchive::RemoveEntry(const FString& entryName)
{
	for (int32 i = 0; i < _entries.Num(); i++)
	{
		if (_entries[i].IsValid() && _entries[i]->GetFullName() == entryName)
		{
			TSharedPtr<BZipArchiveEntry> Removed = _entries[i];
			_entries.RemoveAt(i);
			return Removed;
		}
	}
	return nullptr;
}

TSharedPtr<BZipArchiveEntry> BZipArchive::RemoveEntry(int32 index)
{
	if (index >= _entries.Num()) return nullptr;
	TSharedPtr<BZipArchiveEntry> Removed = _entries[index];
	_entries.RemoveAt(index);
	return Removed;
}

bool BZipArchive::EnsureCentralDirectoryRead()
{
	detail::ZipCentralDirectoryFileHeader zipCentralDirectoryFileHeader;

	_zipStream->seekg(_endOfCentralDirectoryBlock.OffsetOfStartOfCentralDirectoryWithRespectToTheStartingDiskNumber, std::ios::beg);

	while (zipCentralDirectoryFileHeader.Deserialize(*_zipStream))
	{
		TSharedPtr<BZipArchiveEntry> newEntry;

		if ((newEntry = BZipArchiveEntry::CreateExisting(this, zipCentralDirectoryFileHeader)) != nullptr)
		{
			_entries.Add(newEntry);
		}

		// ensure clearing of the CDFH struct
		zipCentralDirectoryFileHeader = detail::ZipCentralDirectoryFileHeader();
	}

	return true;
}

bool BZipArchive::ReadEndOfCentralDirectory()
{
	const int EOCDB_SIZE = 22; // sizeof(EndOfCentralDirectoryBlockBase);
	const int SIGNATURE_SIZE = 4;  // sizeof(std::declval<EndOfCentralDirectoryBlockBase>().Signature);
	const int MIN_SHIFT = (EOCDB_SIZE - SIGNATURE_SIZE);

	_zipStream->seekg(-MIN_SHIFT, std::ios::end);

	if (this->SeekToSignature(detail::EndOfCentralDirectoryBlock::SignatureConstant, SeekDirection::Backward))
	{
		_endOfCentralDirectoryBlock.Deserialize(*_zipStream);
		return true;
	}

	return false;
}

bool BZipArchive::SeekToSignature(uint32 signature, SeekDirection direction)
{
	std::streampos streamPosition = _zipStream->tellg();
	uint32 buffer = 0;
	int appendix = static_cast<int>(direction == SeekDirection::Backward ? 0 - 1 : 1);

	while (!_zipStream->eof() && !_zipStream->fail())
	{
		deserialize(*_zipStream, buffer);

		if (buffer == signature)
		{
			_zipStream->seekg(streamPosition, std::ios::beg);
			return true;
		}

		streamPosition += appendix;
		_zipStream->seekg(streamPosition, std::ios::beg);
	}

	return false;
}

void BZipArchive::WriteToStream(std::ostream& stream)
{
	auto startPosition = stream.tellp();

	for (auto& entry : _entries)
	{
		entry->SerializeLocalFileHeader(stream);
	}

	auto offsetOfStartOfCDFH = stream.tellp() - startPosition;
	for (auto& entry : _entries)
	{
		entry->SerializeCentralDirectoryFileHeader(stream);
	}

	_endOfCentralDirectoryBlock.NumberOfThisDisk = 0;
	_endOfCentralDirectoryBlock.NumberOfTheDiskWithTheStartOfTheCentralDirectory = 0;

	_endOfCentralDirectoryBlock.NumberOfEntriesInTheCentralDirectory = static_cast<uint16>(_entries.Num());
	_endOfCentralDirectoryBlock.NumberOfEntriesInTheCentralDirectoryOnThisDisk = static_cast<uint16>(_entries.Num());

	_endOfCentralDirectoryBlock.SizeOfCentralDirectory = static_cast<uint32>(stream.tellp() - offsetOfStartOfCDFH);
	_endOfCentralDirectoryBlock.OffsetOfStartOfCentralDirectoryWithRespectToTheStartingDiskNumber = static_cast<uint32>(offsetOfStartOfCDFH);
	_endOfCentralDirectoryBlock.Serialize(stream);
}

void BZipArchive::Swap(TSharedPtr<BZipArchive> other)
{
	//if (this == other) return;
	if (other == nullptr) return;

	std::swap(_endOfCentralDirectoryBlock, other->_endOfCentralDirectoryBlock);
	std::swap(_entries, other->_entries);
	std::swap(_zipStream, other->_zipStream);
	std::swap(_owningStream, other->_owningStream);
}

void BZipArchive::InternalDestroy()
{
	if (_owningStream && _zipStream != nullptr)
	{
		delete _zipStream;
		_zipStream = nullptr;
	}
}