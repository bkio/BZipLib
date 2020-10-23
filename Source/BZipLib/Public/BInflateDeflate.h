/// MIT License, Copyright Burak Kara, burak@burak.io, https://en.wikipedia.org/wiki/MIT_License

#pragma once

#include "CoreMinimal.h"

/**
 * \brief Provides static methods inflating/deflating
 */
class BZIPLIB_API BInflateDeflate
{
public:
    /**
     * \brief Inflates(Decompresses) the bytes from the given input stream. Input stream must be closed exclusively.
     *
     * \param InputStream Stream to read from
     *
     */
    static bool DecompressBytes(std::istream* InputStream, TFunction<void(uint8*, int32)> OnBytesDecompressed, TFunction<void(const FString&)> OnErrorAction);
	
	/**
     * \brief Deflates(Compresses) the bytes from the given input stream. Input stream must be closed exclusively.
     *
     * \param InputStream Stream to read from
     *
     */
    static bool CompressBytes(std::istream* InputStream, std::ostream* OutputStream, TFunction<void(const FString&)> OnErrorAction);
};