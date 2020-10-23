/// MIT License, Copyright Burak Kara, burak@burak.io, https://en.wikipedia.org/wiki/MIT_License

#include "BInflateDeflate.h"
#include "BInflateDeflateStream.h"
#include "zconf.h"
#include "zlib.h"

#define INFDEF_CHUNK_SIZE 8192

bool BInflateDeflate::DecompressBytes(std::istream* InputStream, TFunction<void(uint8*, int32)> OnBytesDecompressed, TFunction<void(const FString&)> OnErrorAction)
{
	BInflateDeflateStream::istream Stream(*InputStream);

	int64 TotalLengthCount = 0;

	char Chunk[INFDEF_CHUNK_SIZE];

	while (Stream.good())
	{
		int32 ReadCount = Stream.read(Chunk, INFDEF_CHUNK_SIZE).gcount();
		if (ReadCount > 0)
		{
			TotalLengthCount += ReadCount;
			OnBytesDecompressed((uint8*)Chunk, ReadCount);
		}
		else break;
	}
	
	return TotalLengthCount > 0;
}

bool BInflateDeflate::CompressBytes(std::istream* InputStream, std::ostream* OutputStream, TFunction<void(const FString&)> OnErrorAction)
{
	BInflateDeflateStream::ostream Stream(*OutputStream);

	int64 TotalLengthCount = 0;

	char Chunk[INFDEF_CHUNK_SIZE];

	while (InputStream->good())
	{
		int32 ReadCount = InputStream->read(Chunk, INFDEF_CHUNK_SIZE).gcount();
		if (ReadCount > 0)
		{
			TotalLengthCount += ReadCount;
			Stream.write(Chunk, ReadCount);
		}
		else break;
	}

	return TotalLengthCount > 0;
}