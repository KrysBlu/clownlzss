/*
	(C) 2018-2019 Clownacy

	This software is provided 'as-is', without any express or implied
	warranty.  In no event will the authors be held liable for any damages
	arising from the use of this software.

	Permission is granted to anyone to use this software for any purpose,
	including commercial applications, and to alter it and redistribute it
	freely, subject to the following restrictions:

	1. The origin of this software must not be misrepresented; you must not
	   claim that you wrote the original software. If you use this software
	   in a product, an acknowledgment in the product documentation would be
	   appreciated but is not required.
	2. Altered source versions must be plainly marked as such, and must not be
	   misrepresented as being the original software.
	3. This notice may not be removed or altered from any source distribution.
*/

#include "rocket.h"

#ifndef __cplusplus
#include <stdbool.h>
#endif
#include <stddef.h>

#include "clownlzss.h"
#include "common.h"
#include "memory_stream.h"

#define TOTAL_DESCRIPTOR_BITS 8

typedef struct RocketInstance
{
	MemoryStream *output_stream;
	MemoryStream *match_stream;

	unsigned char descriptor;
	unsigned int descriptor_bits_remaining;
} RocketInstance;

static void FlushData(RocketInstance *instance)
{
	MemoryStream_WriteByte(instance->output_stream, instance->descriptor);

	const size_t match_buffer_size = MemoryStream_GetPosition(instance->match_stream);
	unsigned char *match_buffer = MemoryStream_GetBuffer(instance->match_stream);

	MemoryStream_WriteBytes(instance->output_stream, match_buffer, match_buffer_size);
}

static void PutMatchByte(RocketInstance *instance, unsigned char byte)
{
	MemoryStream_WriteByte(instance->match_stream, byte);
}

static void PutDescriptorBit(RocketInstance *instance, bool bit)
{
	if (instance->descriptor_bits_remaining == 0)
	{
		FlushData(instance);

		instance->descriptor_bits_remaining = TOTAL_DESCRIPTOR_BITS;
		MemoryStream_Rewind(instance->match_stream);
	}

	--instance->descriptor_bits_remaining;

	instance->descriptor >>= 1;

	if (bit)
		instance->descriptor |= 1 << (TOTAL_DESCRIPTOR_BITS - 1);
}

static void DoLiteral(unsigned char value, void *user)
{
	RocketInstance *instance = (RocketInstance*)user;

	PutDescriptorBit(instance, 1);
	PutMatchByte(instance, value);
}

static void DoMatch(size_t distance, size_t length, size_t offset, void *user)
{
	(void)distance;

	RocketInstance *instance = (RocketInstance*)user;

	const unsigned short offset_adjusted = (offset + 0x3C0) & 0x3FF;

	PutDescriptorBit(instance, 0);
	PutMatchByte(instance, (unsigned char)(((offset_adjusted >> 8) & 3) | ((length - 1) << 2)));
	PutMatchByte(instance, offset_adjusted & 0xFF);
}

static unsigned int GetMatchCost(size_t distance, size_t length, void *user)
{
	(void)distance;
	(void)length;
	(void)user;

	return 1 + 16;	// Descriptor bit, offset/length bytes
}

static void FindExtraMatches(unsigned char *data, size_t data_size, size_t offset, ClownLZSS_GraphEdge *node_meta_array, void *user)
{
	(void)data;
	(void)data_size;
	(void)offset;
	(void)node_meta_array;
	(void)user;
}

static CLOWNLZSS_MAKE_COMPRESSION_FUNCTION(CompressData, unsigned char, 0x40, 0x400, FindExtraMatches, 1 + 8, DoLiteral, GetMatchCost, DoMatch)

static void RocketCompressStream(unsigned char *data, size_t data_size, MemoryStream *output_stream, void *user)
{
	(void)user;

	RocketInstance instance;
	instance.output_stream = output_stream;
	instance.match_stream = MemoryStream_Create(true);
	instance.descriptor_bits_remaining = TOTAL_DESCRIPTOR_BITS;

	const size_t file_offset = MemoryStream_GetPosition(output_stream);

	// Incomplete header
	MemoryStream_WriteByte(output_stream, (data_size >> 8) & 0xFF);
	MemoryStream_WriteByte(output_stream, data_size & 0xFF);
	MemoryStream_WriteByte(output_stream, 0);
	MemoryStream_WriteByte(output_stream, 0);

	CompressData(data, data_size, &instance);

	instance.descriptor >>= instance.descriptor_bits_remaining;
	FlushData(&instance);

	MemoryStream_Destroy(instance.match_stream);

	unsigned char *buffer = MemoryStream_GetBuffer(output_stream);
	const size_t compressed_size = MemoryStream_GetPosition(output_stream) - file_offset - 2;

	// Finish header
	buffer[file_offset + 2] = (compressed_size >> 8) & 0xFF;
	buffer[file_offset + 3] = compressed_size & 0xFF;
}

unsigned char* ClownLZSS_RocketCompress(unsigned char *data, size_t data_size, size_t *compressed_size)
{
	return RegularWrapper(data, data_size, compressed_size, NULL, RocketCompressStream);
}

unsigned char* ClownLZSS_ModuledRocketCompress(unsigned char *data, size_t data_size, size_t *compressed_size, size_t module_size)
{
	return ModuledCompressionWrapper(data, data_size, compressed_size, NULL, RocketCompressStream, module_size, 1);
}
