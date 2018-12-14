#pragma once

#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include "memory_stream.h"

#undef MIN
#undef MAX
#define MIN(a, b) (a) < (b) ? (a) : (b)
#define MAX(a, b) (a) > (b) ? (a) : (b)

typedef struct LZSSNodeMeta
{
	union
	{
		unsigned int cost;
		size_t next_node_index;
	};
	size_t previous_node_index;
	size_t match_length;
	size_t match_offset;
} LZSSNodeMeta;

#define MAKE_FIND_MATCHES_FUNCTION(NAME, TYPE, MAX_MATCH_LENGTH, MAX_MATCH_DISTANCE, FIND_EXTRA_MATCHES, LITERAL_COST, LITERAL_CALLBACK, MATCH_COST_CALLBACK, MATCH_CALLBACK)\
void NAME(TYPE *data, size_t data_size, void *user)\
{\
	LZSSNodeMeta *node_meta_array = (LZSSNodeMeta*)malloc((data_size + 1) * sizeof(LZSSNodeMeta));	/* +1 for the end-node */\
\
	node_meta_array[0].cost = 0;\
	for (size_t i = 1; i < data_size + 1; ++i)\
		node_meta_array[i].cost = UINT_MAX;\
\
	for (size_t i = 0; i < data_size; ++i)\
	{\
		const size_t max_read_ahead = MIN(MAX_MATCH_LENGTH, data_size - i);\
		const size_t max_read_behind = MAX_MATCH_DISTANCE > i ? 0 : i - MAX_MATCH_DISTANCE;\
\
		FIND_EXTRA_MATCHES(data, data_size, i, node_meta_array, user);\
\
		for (size_t j = i; j-- > max_read_behind;)\
		{\
			for (size_t k = 0; k < max_read_ahead; ++k)\
			{\
				if (data[i + k] == data[j + k])\
				{\
					const unsigned int cost = MATCH_COST_CALLBACK(i - j, k + 1, user);\
\
					if (cost && node_meta_array[i + k + 1].cost > node_meta_array[i].cost + cost)\
					{\
						node_meta_array[i + k + 1].cost = node_meta_array[i].cost + cost;\
						node_meta_array[i + k + 1].previous_node_index = i;\
						node_meta_array[i + k + 1].match_length = k + 1;\
						node_meta_array[i + k + 1].match_offset = j;\
					}\
				}\
				else\
					break;\
			}\
		}\
\
		if (node_meta_array[i + 1].cost >= node_meta_array[i].cost + LITERAL_COST)\
		{\
			node_meta_array[i + 1].cost = node_meta_array[i].cost + LITERAL_COST;\
			node_meta_array[i + 1].previous_node_index = i;\
			node_meta_array[i + 1].match_length = 0;\
		}\
	}\
\
	node_meta_array[0].previous_node_index = SIZE_MAX;\
	node_meta_array[data_size].next_node_index = SIZE_MAX;\
	for (size_t node_index = data_size; node_meta_array[node_index].previous_node_index != SIZE_MAX; node_index = node_meta_array[node_index].previous_node_index)\
		node_meta_array[node_meta_array[node_index].previous_node_index].next_node_index = node_index;\
\
	for (size_t node_index = 0; node_meta_array[node_index].next_node_index != SIZE_MAX; node_index = node_meta_array[node_index].next_node_index)\
	{\
		const size_t next_index = node_meta_array[node_index].next_node_index;\
		const size_t length = node_meta_array[next_index].match_length;\
		const size_t offset = node_meta_array[next_index].match_offset;\
\
		if (length)\
			MATCH_CALLBACK(next_index - length - offset, length, offset, user);\
		else\
			LITERAL_CALLBACK(data[node_index], user);\
	}\
\
	free(node_meta_array);\
}

unsigned char* RegularWrapper(unsigned char *data, size_t data_size, size_t *compressed_size, void (*function)(unsigned char *data, size_t data_size, MemoryStream *output_stream));
unsigned char* ModuledCompressionWrapper(unsigned char *data, size_t data_size, size_t *compressed_size, void (*function)(unsigned char *data, size_t data_size, MemoryStream *output_stream), size_t module_size, size_t module_alignment);
