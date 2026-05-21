#ifndef PARSER_H
#define PARSER_H

#include "lexer.h"
#include <stddef.h>

typedef enum block_type {
	B_SOURCE_QUOTE,
	B_SOURCE_CODE,
	B_SOURCE_INSERT,
	B_INSERTION_ORIGIN_HOOK,
} block_type;

typedef struct source_block {
	// for source this isn't read
	// 0.. are comptime, which are compiled in ascending order
	size_t compilation_layer; // TODO: make this a u16
	block_type type;
	char *data;
} source_block;

typedef struct code_blocks {
	size_t num_comptime_layers;
	source_block **comptime_layers;
	size_t *num_blocks_in_comptime_layer;
	source_block *source_code;
	size_t num_blocks_in_source;
} code_blocks;

code_blocks parse_into_blocks(Lexer *lex);

#endif // PARSER_H
