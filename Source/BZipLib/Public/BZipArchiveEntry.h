/// MIT License, Copyright Burak Kara, burak@burak.io, https://en.wikipedia.org/wiki/MIT_License
/// Petr Benes - https://bitbucket.org/wbenny/ziplib

#pragma once

#include "CoreMinimal.h"

#include "detail/ZipLocalFileHeader.h"
#include "detail/ZipCentralDirectoryFileHeader.h"

#include "methods/ICompressionMethod.h"
#include "methods/StoreMethod.h"
#include "methods/DeflateMethod.h"

#include "streams/substream.h"
#include "utils/enum_utils.h"

#include <cstdint>
#include <ctime>
#include <string>

class BZipArchive;

/**
 * \brief Represents a compressed file within a zip archive.
 */
class BZIPLIB_API BZipArchiveEntry : public TSharedFromThis<BZipArchiveEntry>
{
    friend class ZipFile;
    friend class BZipArchive;

public:
    /**
     * \brief Values that represent the way the zip entry will be compressed.
     */
    enum class CompressionMode
    {
        Immediate,
        Deferred
    };

    /**
     * \brief Values that represent the MS-DOS file attributes.
     */
    enum class Attributes : uint32
    {
        None = 0,
        ReadOnly = 1,
        Hidden = 2,
        System = 4,
        Directory = 16,
        Archive = 32,
        Device = 64,
        Normal = 128,
        Temporary = 256,
        SparseFile = 512,
        ReparsePoint = 1024,
        Compressed = 2048,
    };

    MARK_AS_TYPED_ENUMFLAGS_FRIEND(Attributes);
    MARK_AS_TYPED_ENUMFLAGS_FRIEND(CompressionMode);

    /**
     * \brief Destructor.
     */
    ~BZipArchiveEntry();

    /**
     * \brief Gets full path of the entry.
     *
     * \return  The full name with the path.
     */
    FString GetFullName();

    /**
     * \brief Sets full name with the path of the entry.
     *
     * \param fullName The full name with the path.
     */
    void SetFullName(const FString& fullName);

    /**
     * \brief Gets only the file name of the entry (without path).
     *
     * \return  The file name.
     */
    const FString& GetName() const;

    /**
     * \brief Sets only a file name of the entry.
     *        If the file is located within some folder, the path is kept.
     *
     * \param name  The file name.
     */
    void SetName(const FString& name);

    /**
     * \brief Gets the comment of this zip entry.
     *
     * \return  The comment.
     */
    FString GetComment();

    /**
     * \brief Sets a comment of this zip entry.
     *
     * \param comment The comment.
     */
    void SetComment(const FString& comment);

    /**
     * \brief Gets the time the file was last modified.
     *
     * \return  The last write time.
     */
    time_t GetLastWriteTime() const;

    /**
     * \brief Sets the time the file was last modified.
     *
     * \param modTime Time of the modifier.
     */
    void SetLastWriteTime(time_t modTime);

    /**
     * \brief Gets the file attributes of this zip entry.
     *
     * \return  The file attributes.
     */
    Attributes GetAttributes() const;

    /**
     * \brief Gets the compression method.
     *
     * \return  The compression method.
     */
    uint16 GetCompressionMethod() const;

    /**
     * \brief Sets the file attributes of this zip entry.
     *
     * \param value The file attributes.
     */
    void SetAttributes(Attributes value);

    /**
     * \brief Query if this entry is password protected.
     *
     * \return  true if password protected, false if not.
     */
    bool IsPasswordProtected() const;

    /**
     * \brief Gets the password of the zip entry. If the password is empty string, the password is not set.
     *
     * \return  The password.
     */
    const FString& GetPassword() const;

    /**
     * \brief Sets a password of the zip entry. If the password is empty string, the password is not set.
     *        Use before GetDecompressionStream or SetCompressionStream.
     *
     * \param password  The password.
     */
    void SetPassword(const FString& password);

    /**
     * \brief Gets CRC 32 of the file.
     *
     * \return  The CRC 32.
     */
    uint32 GetCrc32() const;

    /**
     * \brief Gets the size of the uncompressed data.
     *
     * \return  The size.
     */
    uint64 GetSize() const;

    /**
     * \brief Gets the size of compressed data.
     *
     * \return  The compressed size.
     */
    uint64 GetCompressedSize() const;

    /**
     * \brief Determine if we can extract the entry.
     *        It depends on which version was the zip archive created with.
     *
     * \return  true if we can extract, false if not.
     */
    bool CanExtract() const;

    /**
     * \brief Query if this entry is a directory.
     *
     * \return  true if directory, false if not.
     */
    bool IsDirectory() const;

    /**
     * \brief Query if this object is using data descriptor.
     *        Data descriptor is small chunk of information written after the compressed data.
     *        It's most useful when encrypting a zip entry.
     *        When it is not using, the CRC32 value is required before
     *        encryption of the file data begins. In this case there is no way
     *        around it: must read the stream in its entirety to compute the
     *        actual CRC32 before proceeding.
     *
     * \return  true if using data descriptor, false if not.
     */
    bool IsUsingDataDescriptor() const;

    /**
     * \brief Use data descriptor.
     *        Data descriptor is small chunk of information written after the compressed data.
     *        It's most useful when encrypting a zip entry.
     *        When it is not using, the CRC32 value is required before
     *        encryption of the file data begins. In this case there is no way
     *        around it: must read the stream in its entirety to compute the
     *        actual CRC32 before proceeding.
     * \param use (Optional) If true, use the data descriptor, false to not use.
     */
    void UseDataDescriptor(bool use = true);


    /**
     * \brief Sets the input stream to fetch the data to compress from.
     *
     * \param stream  The input stream to compress.
     * \param method  (Optional) The method of compression.
     * \param mode    (Optional) The mode of compression.
     *                If deferred mode is chosen, the data are compressed when the zip archive is about to be written.
     *                The stream instance must exist when the BZipArchive::WriteToStream method is called.
     *                The advantage of deferred compression mode is the compressed data needs not to be loaded
     *                into the memory, because they are streamed into the final output stream.
     *
     *                If immediate mode is chosen, the data are compressed immediately into the memory buffer.
     *                It is not recommended to use this method for large files.
     *                The advantage of immediate mode is the input stream can be destroyed (i.e. by scope)
     *                even before the BZipArchive::WriteToStream method is called.
     *
     * \return  true if it succeeds, false if it fails.
     */
    bool SetCompressionStream(std::istream& stream, TSharedPtr<ICompressionMethod> method = DeflateMethod::Create(), CompressionMode mode = CompressionMode::Deferred);

    /**
     * \brief Sets compression stream to be null and unsets the password. The entry would contain no data with zero size.
     */
    void UnsetCompressionStream();

    /**
     * \brief Gets raw stream of the compressed data.
     *
     * \return  null if it fails, else the stream of raw data.
     */
    std::istream* GetRawStream();

    /**
     * \brief Gets decompression stream.
     *        If the file is encrypted and correct password is not provided, it returns nullptr.
     *
     * \return  null if it fails, else the decompression stream.
     */
    std::istream* GetDecompressionStream();

    /**
     * \brief Query if the GetRawStream method has been already called.
     *
     * \return  true if the raw stream is opened, false if not.
     */
    bool IsRawStreamOpened() const;

    /**
     * \brief Query if the GetDecompressionStream method has been already called.
     *
     * \return  true if the decompression stream is opened, false if not.
     */
    bool IsDecompressionStreamOpened() const;

    /**
     * \brief Closes the raw stream, opened by GetRawStream.
     */
    void CloseRawStream();

    /**
     * \brief Closes the decompression stream, opened by GetDecompressionStream.
     */
    void CloseDecompressionStream();

private:
    static const uint16 VERSION_MADEBY_DEFAULT = 63;

    static const uint16 VERSION_NEEDED_DEFAULT = 10;
    static const uint16 VERSION_NEEDED_EXPLICIT_DIRECTORY = 20;
    static const uint16 VERSION_NEEDED_ZIP64 = 45;

    enum class BitFlag : uint16
    {
        None = 0,
        Encrypted = 1,
        DataDescriptor = 8,
        UnicodeFileName = 0x800
    };

    MARK_AS_TYPED_ENUMFLAGS_FRIEND(BitFlag);

    BZipArchiveEntry();
    BZipArchiveEntry(const BZipArchiveEntry&);
    BZipArchiveEntry& operator = (BZipArchiveEntry&);

    // static methods
    static TSharedPtr<BZipArchiveEntry> CreateNew(BZipArchive* zipArchive, const FString& fullPath);
    static TSharedPtr<BZipArchiveEntry> CreateExisting(BZipArchive* zipArchive, detail::ZipCentralDirectoryFileHeader& cd);

    // methods
    void SetCompressionMethod(uint16 value);

    BitFlag GetGeneralPurposeBitFlag() const;
    void SetGeneralPurposeBitFlag(BitFlag value, bool set = true);

    uint16 GetVersionToExtract() const;
    void SetVersionToExtract(uint16 value);

    uint16 GetVersionMadeBy() const;
    void SetVersionMadeBy(uint16 value);

    int32 GetOffsetOfLocalHeader() const;
    void SetOffsetOfLocalHeader(int32 value);

    bool HasCompressionStream() const;

    void FetchLocalFileHeader();
    void CheckFilenameCorrection();
    void FixVersionToExtractAtLeast(uint16 value);

    void SyncLFH_with_CDFH();
    void SyncCDFH_with_LFH();

    std::ios::pos_type GetOffsetOfCompressedData();
    std::ios::pos_type SeekToCompressedData();

    void SerializeLocalFileHeader(std::ostream& stream);
    void SerializeCentralDirectoryFileHeader(std::ostream& stream);

    void UnloadCompressionData();
    void InternalCompressStream(std::istream& inputStream, std::ostream& outputStream);

    // for encryption
    void FigureCrc32();
    uint8 GetLastByteOfEncryptionHeader();

    //////////////////////////////////////////////////////////////////////////
    BZipArchive* _archive;           //< pointer to the owning zip archive

    TSharedPtr<std::istream>   _rawStream;         //< stream of raw compressed data
    TSharedPtr<std::istream>   _compressionStream; //< stream of uncompressed data
    TSharedPtr<std::istream>   _encryptionStream;  //< underlying encryption stream
    TSharedPtr<std::istream>   _archiveStream;     //< substream of owning zip archive file

    // internal compression data
    TSharedPtr<std::iostream>  _immediateBuffer;   //< stream used in the immediate mode, stores compressed data in memory
    std::istream* _inputStream;       //< input stream

    TSharedPtr<ICompressionMethod>  _compressionMethod; //< compression method
    CompressionMode                 _compressionMode;   //< compression mode, either deferred or immediate

    FString _name;

    // TODO: make as flags
    bool _originallyInArchive;
    bool _isNewOrChanged;
    bool _hasLocalFileHeader;

    detail::ZipLocalFileHeader _localFileHeader;
    detail::ZipCentralDirectoryFileHeader _centralDirectoryFileHeader;

    std::ios::pos_type _offsetOfCompressedData;
    std::ios::pos_type _offsetOfSerializedLocalFileHeader;

    FString _password;
};
