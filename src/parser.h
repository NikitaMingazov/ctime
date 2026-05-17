#ifndef PARSER_H
#define PARSER_H

#include "lexer.h"
#include <stddef.h>

typedef enum block_type {
	B_SOURCE_CODE,
	B_SOURCE_INSERT,
} block_type;

typedef struct source_block {
	block_type type;
	char *data;
} source_block;

/* there is currently only one comptime block,
   that will probably change to allow for comptime layers.
   after which, source_block will have a "compilation_layer" u8,
   and comptime_block will cease to be special, being the 'layer > 0' case */
typedef struct code_blocks {
	char *comptime_block;
	source_block *source_blocks;
	size_t num_source_blocks;
} code_blocks;

code_blocks parse_into_blocks(Lexer *lex);

#endif

