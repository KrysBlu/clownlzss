#include "chameleon.h"

#include <stdbool.h>
#include <stddef.h>

#include "clownlzss.h"
#include "memory_stream.h"

#define TOTAL_DESCRIPTOR_BITS 8

static MemoryStream *output_stream;
static MemoryStream *match_stream;
static MemoryStream *descriptor_stream;

static unsigned char descriptor;
static unsigned int descriptor_bits_remaining;

static void FlushData(void)
{
	descriptor <<= descriptor_bits_remaining;

	MemoryStream_WriteByte(descriptor_stream, descriptor);

//	const size_t match_buffer_size = MemoryStream_GetIndex(match_stream);
//	unsigned char *match_buffer = MemoryStream_GetBuffer(match_stream);

//	MemoryStream_WriteBytes(output_stream, match_buffer, match_buffer_size);
}

static void PutMatchByte(unsigned char byte)
{
	MemoryStream_WriteByte(match_stream, byte);
}

static void PutDescriptorBit(bool bit)
{
	if (descriptor_bits_remaining == 0)
	{
		MemoryStream_WriteByte(descriptor_stream, descriptor);

		descriptor_bits_remaining = TOTAL_DESCRIPTOR_BITS;
//		MemoryStream_Reset(match_stream);
	}

	--descriptor_bits_remaining;

	descriptor <<= 1;

	descriptor |= bit;
}

static void DoLiteral(unsigned char value, void *user)
{
	(void)user;

	PutDescriptorBit(true);
	PutMatchByte(value);
}

static void DoMatch(size_t distance, size_t length, size_t offset, void *user)
{
	(void)offset;
	(void)user;

	if (length >= 2 && length <= 3 && distance < 256)
	{
		PutDescriptorBit(false);
		PutDescriptorBit(false);
		PutMatchByte(distance);
		PutDescriptorBit(length == 3);
	}
	else if (length >= 3 && length <= 5)
	{
		PutDescriptorBit(false);
		PutDescriptorBit(true);
		PutDescriptorBit(distance & (1 << 10));
		PutDescriptorBit(distance & (1 << 9));
		PutDescriptorBit(distance & (1 << 8));
		PutMatchByte(distance & 0xFF);
		PutDescriptorBit(length == 5);
		PutDescriptorBit(length == 4);
	}
	else //if (length >= 6)
	{
		PutDescriptorBit(false);
		PutDescriptorBit(true);
		PutDescriptorBit(distance & (1 << 10));
		PutDescriptorBit(distance & (1 << 9));
		PutDescriptorBit(distance & (1 << 8));
		PutMatchByte(distance & 0xFF);
		PutDescriptorBit(true);
		PutDescriptorBit(true);
		PutMatchByte(length);
	}
}

static unsigned int GetMatchCost(size_t distance, size_t length, void *user)
{
	(void)user;

	if (length >= 2 && length <= 3 && distance < 256)
		return 2 + 8 + 1;		// Offset byte, length bits, descriptor bits
	else if (length >= 3 && length <= 5)
		return 2 + 3 + 8 + 2;		// Offset/length bytes, descriptor bits
	else if (length >= 6)
		return 2 + 3 + 8 + 2 + 8;	// Offset/length bytes, descriptor bits
	else
		return 0; 		// In the event a match cannot be compressed
}

static void FindExtraMatches(unsigned char *data, size_t data_size, size_t offset, ClownLZSS_NodeMeta *node_meta_array, void *user)
{
	(void)data;
	(void)data_size;
	(void)offset;
	(void)node_meta_array;
	(void)user;
}

static CLOWNLZSS_MAKE_FUNCTION(FindMatches, unsigned char, 0xFF, 0x7FF, FindExtraMatches, 8 + 1, DoLiteral, GetMatchCost, DoMatch)

unsigned char* ChameleonCompress(unsigned char *data, size_t data_size, size_t *compressed_size)
{
	output_stream = MemoryStream_Create(0x100, false);
	match_stream = MemoryStream_Create(0x100, true);
	descriptor_stream = MemoryStream_Create(0x100, true);
	descriptor_bits_remaining = TOTAL_DESCRIPTOR_BITS;

	FindMatches(data, data_size, NULL);

	// Terminator match
	PutDescriptorBit(false);
	PutDescriptorBit(true);
	PutDescriptorBit(false);
	PutDescriptorBit(false);
	PutDescriptorBit(false);
	PutMatchByte(0);
	PutDescriptorBit(true);
	PutDescriptorBit(true);
	PutMatchByte(0);

	FlushData();

	const size_t descriptor_buffer_size = MemoryStream_GetIndex(descriptor_stream);
	unsigned char *descriptor_buffer = MemoryStream_GetBuffer(descriptor_stream);

	MemoryStream_WriteByte(output_stream, descriptor_buffer_size >> 8);
	MemoryStream_WriteByte(output_stream, descriptor_buffer_size & 0xFF);

	MemoryStream_WriteBytes(output_stream, descriptor_buffer, descriptor_buffer_size);

	const size_t match_buffer_size = MemoryStream_GetIndex(match_stream);
	unsigned char *match_buffer = MemoryStream_GetBuffer(match_stream);

	MemoryStream_WriteBytes(output_stream, match_buffer, match_buffer_size);

	unsigned char *out_buffer = MemoryStream_GetBuffer(output_stream);

	if (compressed_size)
		*compressed_size = MemoryStream_GetIndex(output_stream);

	MemoryStream_Destroy(descriptor_stream);
	MemoryStream_Destroy(match_stream);
	MemoryStream_Destroy(output_stream);

	return out_buffer;
}
