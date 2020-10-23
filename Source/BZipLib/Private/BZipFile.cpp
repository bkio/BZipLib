/// MIT License, Copyright Burak Kara, burak@burak.io, https://en.wikipedia.org/wiki/MIT_License
/// Petr Benes - https://bitbucket.org/wbenny/ziplib

#include "BZipFile.h"
#include "utils/stream_utils.h"
#include <fstream>
#include <cassert>
#include <stdexcept>
#include "Misc/Paths.h"

bool BZipFile::Open(TSharedPtr<BZipArchive>& OutArchive, const FString& ZipPath, FString& ErrorMessage)
{
	std::ifstream* zipFile = new std::ifstream();
	zipFile->open(TCHAR_TO_UTF8(*ZipPath), std::ios::binary);

	if (!zipFile->is_open())
	{
		// if file does not exist, try to create it
		std::ofstream tmpFile;
		tmpFile.open(TCHAR_TO_UTF8(*ZipPath), std::ios::binary);
		tmpFile.close();

		zipFile->open(TCHAR_TO_UTF8(*ZipPath), std::ios::binary);

		// if attempt to create file failed, throw an exception
		if (!zipFile->is_open())
		{
			ErrorMessage = TEXT("Unable to create/open file");
			return false;
		}
	}

	OutArchive = BZipArchive::Create(zipFile, true);
	return true;
}

bool BZipFile::Save(TSharedPtr<BZipArchive>& BZipArchive, const FString& ZipPath, FString& ErrorMessage)
{
	if (!BZipFile::SaveAndClose(BZipArchive, ZipPath, ErrorMessage))
	{
		return false;
	}
	return BZipFile::Open(BZipArchive, ZipPath, ErrorMessage);
}

bool BZipFile::SaveAndClose(TSharedPtr<BZipArchive>& ZArchive, const FString& ZipPath, FString& ErrorMessage)
{
	// check if file exist
	FString tempZipPath = MakeTempFilename(ZipPath);
	std::ofstream outZipFile;
	outZipFile.open(TCHAR_TO_UTF8(*tempZipPath), std::ios::binary | std::ios::trunc);

	if (!outZipFile.is_open())
	{
		ErrorMessage = TEXT("Cannot save zip file");
		return false;
	}

	ZArchive->WriteToStream(outZipFile);
	outZipFile.close();

	ZArchive->InternalDestroy();

	IFileManager::Get().Delete(*ZipPath);
	IFileManager::Get().Move(*ZipPath, *tempZipPath);

	return true;
}

bool BZipFile::IsInArchive(const FString& ZipPath, const FString& FileName, FString& ErrorMessage)
{
	TSharedPtr<BZipArchive> ZArchive;
	if (!BZipFile::Open(ZArchive, ZipPath, ErrorMessage)) return false;
	return ZArchive->GetEntry(FileName).IsValid();
}

bool BZipFile::AddFile(const FString& ZipPath, const FString& FileName, FString& ErrorMessage, TSharedPtr<ICompressionMethod> Method)
{
	return AddFile(ZipPath, FileName, GetFilenameFromPath(FileName), ErrorMessage, Method);
}

bool BZipFile::AddFile(const FString& ZipPath, const FString& FileName, const FString& InArchiveName, FString& ErrorMessage, TSharedPtr<ICompressionMethod> Method)
{
	return AddEncryptedFile(ZipPath, FileName, InArchiveName, FString(), ErrorMessage, Method);
}

bool BZipFile::AddEncryptedFile(const FString& ZipPath, const FString& FileName, const FString& Password, FString& ErrorMessage, TSharedPtr<ICompressionMethod> Method)
{
	return AddEncryptedFile(ZipPath, FileName, GetFilenameFromPath(FileName), FString(), ErrorMessage, Method);
}

bool BZipFile::AddEncryptedFile(const FString& ZipPath, const FString& FileName, const FString& InArchiveName, const FString& Password, FString& ErrorMessage, TSharedPtr<ICompressionMethod> Method)
{
	FString tmpName = MakeTempFilename(ZipPath);

	{
		TSharedPtr<BZipArchive> zipArchive;
		if (!BZipFile::Open(zipArchive, ZipPath, ErrorMessage)) return false;

		std::ifstream fileToAdd;
		fileToAdd.open(TCHAR_TO_UTF8(*FileName), std::ios::binary);

		if (!fileToAdd.is_open())
		{
			ErrorMessage = TEXT("Cannot open input file");
			return false;
		}

		auto fileEntry = zipArchive->CreateEntry(InArchiveName);

		if (fileEntry == nullptr)
		{
			zipArchive->RemoveEntry(InArchiveName);
			fileEntry = zipArchive->CreateEntry(InArchiveName);
		}

		if (!Password.IsEmpty())
		{
			fileEntry->SetPassword(Password);
			fileEntry->UseDataDescriptor();
		}

		fileEntry->SetCompressionStream(fileToAdd, Method);

		//////////////////////////////////////////////////////////////////////////

        std::ofstream outFile;
		outFile.open(TCHAR_TO_UTF8(*tmpName), std::ios::binary);

		if (!outFile.is_open())
		{
			ErrorMessage = TEXT("Cannot open output file");
			return false;
		}

		zipArchive->WriteToStream(outFile);
		outFile.close();

		// force closing the input zip stream
	}

	IFileManager::Get().Delete(*ZipPath);
	IFileManager::Get().Move(*ZipPath, *tmpName);

	return true;
}

bool BZipFile::CompressAll(const FString& InputFolderAbsolutePath, const FString& DestinationZipAbsolutePath, FString& ErrorMessage)
{
	class FFileRecursiveVisitor : public IPlatformFile::FDirectoryVisitor
	{
	public:
		FString BaseFolderAbsolutePath;
		FString RelativePrePath;
		TArray<FString>& Result;
		FFileRecursiveVisitor(const FString& InBaseFolderAbsolutePath, const FString& InRelativePrePath, TArray<FString>& InResult) : BaseFolderAbsolutePath(InBaseFolderAbsolutePath), RelativePrePath(InRelativePrePath), Result(InResult) {}
		virtual bool Visit(const TCHAR* FilenameOrDirectory, bool bIsDirectory)
		{
			const FString CleanFileOrDirectoryName = FPaths::GetCleanFilename(FilenameOrDirectory);

			FString Tmp = RelativePrePath.Len() > 0 ? (RelativePrePath + "/") : "";
			FString RelativePath = Tmp + CleanFileOrDirectoryName;

			if (!bIsDirectory)
			{
				Result.Add(RelativePath);
			}
			else
			{
				FFileRecursiveVisitor Iterator(BaseFolderAbsolutePath, Tmp + CleanFileOrDirectoryName, Result);
				IFileManager::Get().IterateDirectory(*(BaseFolderAbsolutePath + "/" + RelativePath), Iterator);
			}
			return true;
		}
	};

	TArray<FString> AllFiles;
	FFileRecursiveVisitor Iterator(InputFolderAbsolutePath, "", AllFiles);
	IFileManager::Get().IterateDirectory(*InputFolderAbsolutePath, Iterator);

	if (AllFiles.Num() == 0)
	{
		ErrorMessage = "Given directory is empty.";
		return false;
	}

	TSharedPtr<BZipArchive> Archive;

	if (!Open(Archive, DestinationZipAbsolutePath, ErrorMessage)) return false;

	bool bIterationFailed = false;
	TArray<TSharedPtr<std::ifstream>> OpenedFileStreams;

	auto Method = DeflateMethod::Create();
	Method->SetCompressionLevel(DeflateMethod::CompressionLevel::Default);

	for (auto& CurrentRelativeFilePath : AllFiles)
	{
		auto Entry = Archive->CreateEntry(CurrentRelativeFilePath);

		TSharedPtr<std::ifstream> ContentStream = MakeShareable(new std::ifstream());
		ContentStream->open(TCHAR_TO_UTF8(*(InputFolderAbsolutePath + "/" + CurrentRelativeFilePath)), std::ios::binary);

		if (!ContentStream->is_open())
		{
			ErrorMessage = FString::Printf(TEXT("Folder iteration/open file handle has failed at file: %s"), *CurrentRelativeFilePath);
			bIterationFailed = true;
			continue;
		}

		OpenedFileStreams.Add(ContentStream);

		if (!Entry->SetCompressionStream(*ContentStream.Get(), Method, BZipArchiveEntry::CompressionMode::Deferred))
		{
			ErrorMessage = FString::Printf(TEXT("Folder iteration/set compression has failed at file: %s"), *CurrentRelativeFilePath);
			bIterationFailed = true;
			break;
		}
	}

	bool bSuccess = false;
	if (!bIterationFailed)
	{
		bSuccess = BZipFile::SaveAndClose(Archive, DestinationZipAbsolutePath, ErrorMessage);
	}

	for (auto& CurrentStream : OpenedFileStreams)
	{
		if (CurrentStream.IsValid() && CurrentStream->is_open())
		{
			CurrentStream->close();
		}
	}
	OpenedFileStreams.Empty();

	return bSuccess;
}

bool BZipFile::ExtractAll(const FString& ZipAbsolutePath, const FString& ExtractFolderAbsolutePath, FString& ErrorMessage)
{
	TSharedPtr<BZipArchive> zipArchive;
	if (!BZipFile::Open(zipArchive, ZipAbsolutePath, ErrorMessage)) return false;

	if (zipArchive->GetEntriesCount() > 0 && !IFileManager::Get().DirectoryExists(*ExtractFolderAbsolutePath))
	{
		IFileManager::Get().MakeDirectory(*ExtractFolderAbsolutePath, true);
	}

	for (int32 i = 0; i < zipArchive->GetEntriesCount(); i++)
	{
		auto Entry = zipArchive->GetEntry(i);
		if (Entry.IsValid())
		{
			FString EntryFullName = Entry->GetFullName();
			FString EntryDestinationPath = ExtractFolderAbsolutePath + "/" + EntryFullName;

			std::ofstream destFile;
			destFile.open(TCHAR_TO_UTF8(*EntryDestinationPath), std::ios::binary | std::ios::trunc);

			if (!destFile.is_open())
			{
				ErrorMessage = TEXT("Cannot create destination file");
				return false;
			}

			std::istream* dataStream = Entry->GetDecompressionStream();

			if (dataStream == nullptr)
			{
				ErrorMessage = TEXT("Decompression stream is invalid.");
				return false;
			}

			utils::stream::copy(*dataStream, destFile);

			destFile.flush();
			destFile.close();
		}
	}

	return true;
}

bool BZipFile::ExtractFile(const FString& ZipPath, const FString& FileName, FString& ErrorMessage)
{
	return ExtractFile(ZipPath, FileName, GetFilenameFromPath(FileName), ErrorMessage);
}

bool BZipFile::ExtractFile(const FString& ZipPath, const FString& FileName, const FString& DestinationPath, FString& ErrorMessage)
{
	return ExtractEncryptedFile(ZipPath, FileName, DestinationPath, FString(), ErrorMessage);
}

bool BZipFile::ExtractEncryptedFile(const FString& ZipPath, const FString& FileName, const FString& Password, FString& ErrorMessage)
{
	return ExtractEncryptedFile(ZipPath, FileName, GetFilenameFromPath(FileName), Password, ErrorMessage);
}

bool BZipFile::ExtractEncryptedFile(const FString& ZipPath, const FString& FileName, const FString& DestinationPath, const FString& Password, FString& ErrorMessage)
{
	TSharedPtr<BZipArchive> zipArchive;
	if (!BZipFile::Open(zipArchive, ZipPath, ErrorMessage)) return false;

	std::ofstream destFile;
	destFile.open(TCHAR_TO_UTF8(*DestinationPath), std::ios::binary | std::ios::trunc);

	if (!destFile.is_open())
	{
		ErrorMessage = TEXT("Cannot create destination file");
		return false;
	}

	auto entry = zipArchive->GetEntry(FileName);

	if (entry == nullptr)
	{
		ErrorMessage = TEXT("File is not contained in zip file");
		return false;
	}

	if (!Password.IsEmpty())
	{
		entry->SetPassword(Password);
	}

	std::istream* dataStream = entry->GetDecompressionStream();

	if (dataStream == nullptr)
	{
		ErrorMessage = TEXT("Wrong password");
		return false;
	}

	utils::stream::copy(*dataStream, destFile);

	destFile.flush();
	destFile.close();

	return true;
}

bool BZipFile::RemoveEntry(const FString& ZipPath, const FString& FileName, FString& ErrorMessage)
{
	FString tmpName = MakeTempFilename(ZipPath);

	{
		TSharedPtr<BZipArchive> zipArchive;
		if (!BZipFile::Open(zipArchive, ZipPath, ErrorMessage)) return false;

		zipArchive->RemoveEntry(FileName);

		//////////////////////////////////////////////////////////////////////////

		std::ofstream outFile;
		outFile.open(TCHAR_TO_UTF8(*tmpName), std::ios::binary);

		if (!outFile.is_open())
		{
			ErrorMessage = TEXT("Cannot open output file");
			return false;
		}

		zipArchive->WriteToStream(outFile);
		outFile.close();

		// force closing the input zip stream
	}

	IFileManager::Get().Delete(*ZipPath);
	IFileManager::Get().Move(*ZipPath, *tmpName);

	return true;
}

FString BZipFile::MakeTempFilename(const FString& FileName)
{
	return FileName + ".tmp";
}

FString BZipFile::GetFilenameFromPath(const FString& FullPath)
{
	int32 DirSeparatorPos;
	if (FullPath.FindLastChar('/', DirSeparatorPos))
	{
		return FullPath.Mid(DirSeparatorPos + 1);
	}
	return FullPath;
}