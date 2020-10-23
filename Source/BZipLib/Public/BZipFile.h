/// MIT License, Copyright Burak Kara, burak@burak.io, https://en.wikipedia.org/wiki/MIT_License
/// Petr Benes - https://bitbucket.org/wbenny/ziplib

#pragma once

#include "CoreMinimal.h"
#include "BZipArchive.h"
#include <istream>

/**
 * \brief Provides static methods for creating, extracting, and opening zip archives.
 */
class BZIPLIB_API BZipFile
{
public:
    /**
     * \brief Opens the zip archive file with the given filename.
     *
     * \param ZipPath Full pathname of the zip file.
     *
     * \return The BZipArchive instance.
     */
    static bool Open(TSharedPtr<BZipArchive>& OutArchive, const FString& ZipPath, FString& ErrorMessage);

    /**
     * \brief Saves the zip archive file with the given filename.
     *        The BZipArchive class will stay open.
     *
     * \param BZipArchive  The zip archive to save.
     * \param ZipPath     Full pathname of the zip archive file.
     */
    static bool Save(TSharedPtr<BZipArchive>& ZArchive, const FString& ZipPath, FString& ErrorMessage);

    /**
     * \brief Saves the zip archive file and close it.
     *        The BZipArchive class will be clear after this method call.
     *
     * \param BZipArchive  The zip archive to save.
     * \param ZipPath     Full pathname of the zip archive file.
     */
    static bool SaveAndClose(TSharedPtr<BZipArchive>& ZArchive, const FString& ZipPath, FString& ErrorMessage);

    /**
     * \brief Checks if file with the given path is contained in the archive.
     *
     * \param ZipPath   Full pathname of the zip file.
     * \param FileName  Filename or the path of the file to check.
     *
     * \return  true if in archive, false if not.
     */
    static bool IsInArchive(const FString& ZipPath, const FString& FileName, FString& ErrorMessage);

    /**
     * \brief Adds a file to the zip archive.
     *        The name of the file in the archive will be the same as the added file name.
     *
     * \param ZipPath   Full pathname of the zip file.
     * \param FileName  Filename of the file to add.
     */
    static bool AddFile(const FString& ZipPath, const FString& FileName, FString& ErrorMessage, TSharedPtr<ICompressionMethod> Method = DeflateMethod::Create());

    /**
     * \brief Adds a file to the zip archive.
     *
     * \param ZipPath       Full pathname of the zip file.
     * \param FileName      Filename of the file to add.
     * \param InArchiveName Final name of the file in the archive.
     */
    static bool AddFile(const FString& ZipPath, const FString& FileName, const FString& InArchiveName, FString& ErrorMessage, TSharedPtr<ICompressionMethod> Method = DeflateMethod::Create());

    /**
     * \brief Adds an encrypted file to the zip archive.
     *        The name of the file in the archive will be the same as the added file name.
     *
     * \param ZipPath   Full pathname of the zip file.
     * \param FileName  Filename of the file to add.
     * \param Password  The password.
     */
    static bool AddEncryptedFile(const FString& ZipPath, const FString& FileName, const FString& Password, FString& ErrorMessage, TSharedPtr<ICompressionMethod> Method = DeflateMethod::Create());

    /**
     * \brief Adds an encrypted file to the zip archive.
     *
     * \param ZipPath       Full pathname of the zip file.
     * \param FileName      Filename of the file to add.
     * \param InArchiveName Final name of the file in the archive.
     * \param Password      The password.
     */
    static bool AddEncryptedFile(const FString& ZipPath, const FString& FileName, const FString& InArchiveName, const FString& Password, FString& ErrorMessage, TSharedPtr<ICompressionMethod> Method = DeflateMethod::Create());

    /**
     * \brief Extracts the file from the zip archive.
     *        The extracted filename will have the same file name as the name of the file in the archive.
     *
     * \param ZipPath   Full pathname of the zip file.
     * \param FileName  Filename of the file to extract.
     */
    static bool ExtractFile(const FString& ZipPath, const FString& FileName, FString& ErrorMessage);

    /**
     * \brief Extracts the file from the zip archive.
     *
     * \param ZipPath         Full pathname of the zip file.
     * \param FileName        Filename of the file in the archive.
     * \param DestinationPath Full pathname of the extracted file.
     */
    static bool ExtractFile(const FString& ZipPath, const FString& FileName, const FString& DestinationPath, FString& ErrorMessage);

    /**
     * \brief Extracts all files in the zip archive to the given directory.
     *
     * \param ZipAbsolutePath               Full pathname of the zip file.
     * \param ExtractFolderAbsolutePath     Full pathname of the destination folder.
     * \param ErrorMessage                  Error message will be set to this.
     */
	static bool ExtractAll(const FString& ZipAbsolutePath, const FString& ExtractFolderAbsolutePath, FString& ErrorMessage);

    /**
     * \brief Compresses all files in the given directory to the given zip file path.
     *
     * \param InputFolderAbsolutePath       Full pathname of the source folder.
     * \param DestinationZipAbsolutePath    Full pathname of the destination zip file.
     */
	static bool CompressAll(const FString& InputFolderAbsolutePath, const FString& DestinationZipAbsolutePath, FString& ErrorMessage);

    /**
     * \brief Extracts an encrypted file from the zip archive.
     *        The extracted filename will have the same file name as the name of the file in the archive.
     *
     * \param zipPath   Full pathname of the zip file.
     * \param fileName  Filename of the file to extract.
     * \param password  The password.
     */
    static bool ExtractEncryptedFile(const FString& ZipPath, const FString& FileName, const FString& Password, FString& ErrorMessage);

    /**
     * \brief Extracts an encrypted file from the zip archive.
     *
     * \param ZipPath         Full pathname of the zip file.
     * \param FileName        Filename of the file to extract.
     * \param DestinationPath Full pathname of the extracted file.
     * \param Password        The password.
     */
    static bool ExtractEncryptedFile(const FString& ZipPath, const FString& FileName, const FString& DestinationPath, const FString& Password, FString& ErrorMessage);

    /**
     * \brief Removes the file from the zip archive.
     *
     * \param ZipPath   Full pathname of the zip file.
     * \param FileName  Filename of the file to remove.
     */
    static bool RemoveEntry(const FString& ZipPath, const FString& FileName, FString& ErrorMessage);

    friend class BZipArchiveEntry;

private:
	static FString MakeTempFilename(const FString& FileName);
	static FString GetFilenameFromPath(const FString& FullPath);
};
