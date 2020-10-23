/// MIT License, Copyright Burak Kara, burak@burak.io, https://en.wikipedia.org/wiki/MIT_License
/// Petr Benes - https://bitbucket.org/wbenny/ziplib

#pragma once

#include "CoreMinimal.h"
#include "detail/EndOfCentralDirectoryBlock.h"
#include "BZipArchiveEntry.h"
#include <istream>

/**
 * \brief Represents a package of compressed files in the zip archive format.
 */
class BZIPLIB_API BZipArchive
{
    friend class BZipFile;
    friend class BZipArchiveEntry;

public:
    /**
     * \brief Default constructor.
     */
    static TSharedPtr<BZipArchive> Create();

    /**
     * \brief Move constructor.
     *
     * \param other The BZipArchive instance to move.
     */
    static TSharedPtr<BZipArchive> Create(TSharedPtr<BZipArchive>&& other);

    /**
     * \brief Constructor.
     *
     * \param stream The input stream of the zip archive content. Must be seekable.
     */
    static TSharedPtr<BZipArchive> Create(std::istream& stream);

    /**
     * \brief Constructor. It optionally allows to simultaneously destroy and dealloc the input stream
     *        with the BZipArchive.
     *
     * \param stream                The input stream of the zip archive content. Must be seekable.
     * \param takeOwnership         If true, it calls "delete stream" in the BZipArchive destructor.
     */
    static TSharedPtr<BZipArchive> Create(std::istream* stream, bool takeOwnership);

    /**
     * \brief Destructor.
     */
    ~BZipArchive();

    /**
     * \brief Move assignment operator.
     *
     * \param other The BZipArchive instance to move.
     *
     * \return  A shallow copy of this object.
     */
    BZipArchive& operator = (BZipArchive&& other);

    /**
     * \brief Creates an zip entry with given file name.
     *
     * \param fileName  Filename of the file.
     *
     * \return  nullptr if it fails, else the new entry.
     */
    TSharedPtr<BZipArchiveEntry> CreateEntry(const FString& fileName);

    /**
     * \brief Gets the comment of the zip archive.
     *
     * \return  The comment.
     */
    FString GetComment();

    /**
     * \brief Sets a comment of the zip archive.
     *
     * \param comment The comment.
     */
    void SetComment(const FString& comment);

    /**
     * \brief Gets a pointer to the zip entry located on the given index.
     *
     * \param index Zero-based index of the.
     *
     * \return  null if it fails, else the entry.
     */
    TSharedPtr<BZipArchiveEntry> GetEntry(int32 index);

    /**
     * \brief Gets a const pointer to the zip entry with given file name.
     *
     * \param entryName Name of the entry.
     *
     * \return  null if it fails, else the entry.
     */
    TSharedPtr<BZipArchiveEntry> GetEntry(const FString& entryName);

    /**
     * \brief Gets the number of the zip entries in this archive.
     *
     * \return  The number of the zip entries in this archive.
     */
    int32 GetEntriesCount() const;

    /**
     * \brief Removes the entry by the file name.
     *
     * \param entryName Name of the entry.
     */
    TSharedPtr<BZipArchiveEntry> RemoveEntry(const FString& entryName);

    /**
    * \brief Removes the entry by the index.
     *
     * \param index Zero-based index of the.
     */
    TSharedPtr<BZipArchiveEntry> RemoveEntry(int32 index);

    /**
     * \brief Writes the zip archive content to the stream. It must be seekable.
     *
     * \param stream The stream to write in.
     */
    void WriteToStream(std::ostream& stream);

    /**
     * \brief Swaps this instance of BZipArchive with another instance.
     *
     * \param other The instance to swap with.
     */
    void Swap(TSharedPtr<BZipArchive> other);

private:
    BZipArchive();
    BZipArchive(const BZipArchive&);
    BZipArchive& operator = (const BZipArchive& other);

    enum class SeekDirection
    {
        Forward,
        Backward
    };

    bool EnsureCentralDirectoryRead();
    bool ReadEndOfCentralDirectory();
    bool SeekToSignature(uint32 signature, SeekDirection direction);

    void InternalDestroy();

    detail::EndOfCentralDirectoryBlock _endOfCentralDirectoryBlock;
    TArray<TSharedPtr<BZipArchiveEntry>> _entries;
    std::istream* _zipStream;
    bool _owningStream;
};