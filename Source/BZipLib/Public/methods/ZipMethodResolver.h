//Petr Benes - https://bitbucket.org/wbenny/ziplib

#pragma once
#include <cstdint>
#include <memory>
#include "methods/ICompressionMethod.h"

#include "methods/StoreMethod.h"
#include "methods/DeflateMethod.h"

#define ZIP_METHOD_TABLE          \
  ZIP_METHOD_ADD(StoreMethod);    \
  ZIP_METHOD_ADD(DeflateMethod);

#define ZIP_METHOD_ADD(method_class)                                                            \
  if (compressionMethod == method_class::GetZipMethodDescriptorStatic().GetCompressionMethod()) \
    return method_class::Create()

struct ZipMethodResolver
{
	static TSharedPtr<ICompressionMethod> GetZipMethodInstance(uint16_t compressionMethod)
	{
		ZIP_METHOD_TABLE;
		return TSharedPtr<ICompressionMethod>();
	}
};

#undef ZIP_METHOD
