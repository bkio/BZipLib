//Petr Benes - https://bitbucket.org/wbenny/ziplib

#pragma once
#include "compression/compression_interface.h"

struct deflate_decoder_properties : compression_decoder_properties_interface
{
	deflate_decoder_properties()
		: BufferCapacity(1 << 15)
	{

	}

	void normalize() override
	{

	}

	size_t BufferCapacity;
};
