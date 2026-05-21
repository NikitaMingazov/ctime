/*
  Currently the grammar is flat and not a tree.
  That might change later, hence the general approach.
*/
#include "parser.h"
#include "lexer.h"
#include "ctime_utils.h"
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

source_block *make_block(size_t layer, block_type type, char *data) {
	source_block *new = malloc(sizeof(*new));
	*new = (source_block) {
		.compilation_layer = layer,
		.type = type,
		.data = data,
	};
	return new;
}

typedef struct block_node {
	source_block *data;
	struct block_node *next;
} block_node;

// returns new tail
block_node *block_list_append(block_node *old_tail, source_block *data) {
	block_node *new_tail = malloc(sizeof(*new_tail));
	*new_tail = (struct block_node) {
		.data = data,
		.next = NULL,
	};
	old_tail->next = new_tail;
	return new_tail;
}

void block_list_free(block_node *node) {
	block_node *next;
	while (node) {
		next = node->next;
		free(node->data);
		// the str in data has moved owner
		free(node);
		node = next;
	}
}

int k = 0;
// returns next token if it matches grammar, exits with error otherwise
Token match(Lexer *lex, TokenType expected) {
	Token t = lexer_next(lex);
	/* printf("%d: %s\n", k++, token_to_str(t)); */
	if (t.type != expected) {
		fprintf(stderr, "Expected token type %s, but saw %s\n", TOKENTYPE_TO_STR[expected], TOKENTYPE_TO_STR[t.type]);
		exit(1);
	}
	return t;
}

#define UPDATE_SCOPE \
	scope = tok.scope; \
	if (scope > highest_comptime_scope) { \
		insert_counters = realloc(insert_counters, (scope+1) * sizeof(*insert_counters)); \
		layers = realloc(layers, (scope+1) * sizeof(*layers)); \
		layer_tails = realloc(layer_tails, (scope+1) * sizeof(*layer_tails)); \
		for (int i = highest_comptime_scope+1; i < scope+1; ++i) { \
			insert_counters[i] = 0; \
			layers[i] = malloc(sizeof(*layers[i])); \
			*layers[i] = (block_node) { .data = NULL, .next = NULL }; \
			layer_tails[i] = layers[i]; \
		} \
		highest_comptime_scope = scope; \
	}

#define APPEND_INSERTION todo

code_blocks parse_into_blocks(Lexer *lex) {
	char *return_expr_fn;

	block_node *source_head = malloc(sizeof(*source_head));
	*source_head = (block_node) { .data = NULL, .next = NULL };
	block_node *source_tail = source_head;
	int source_insert_counter = 0;

	block_node **layers = malloc(sizeof(*layers));
	block_node **layer_tails = malloc(sizeof(*layer_tails));
	layers[0] = malloc(sizeof(**layers));
	*layers[0] = (block_node) { .data = NULL, .next = NULL };
	layer_tails[0] = layers[0];
	int *insert_counters = malloc(sizeof(insert_counters));
	insert_counters[0] = 0;

	int highest_comptime_scope = -1;
	int scope;
	int parent_scope;
	Token tok;
	while ((tok = lexer_next(lex)).type != TOKEN_EOF) {
		switch (tok.type) {
			case TOKEN_STRING:
				source_tail = block_list_append(source_tail, make_block(SIZE_MAX, B_SOURCE_CODE, tok.string_data));
				break;
			case TOKEN_CTIMEDEF_START:
				UPDATE_SCOPE
				parent_scope = scope;
				while ((tok = lexer_next(lex)).type != TOKEN_CTIMEDEF_END) {
					if (tok.type == TOKEN_STRING) {
						layer_tails[parent_scope] = block_list_append(layer_tails[parent_scope], make_block(parent_scope, B_SOURCE_CODE, tok.string_data));
					} else if (tok.type == TOKEN_QUOTE_START) {
						tok = match(lex, TOKEN_STRING);
						layer_tails[parent_scope] = block_list_append(layer_tails[parent_scope], make_block(parent_scope, B_SOURCE_CODE, ctt_format("\"%s\"", ctt_quote(tok.string_data))));
						match(lex, TOKEN_QUOTE_END);
					} else if (tok.type == TOKEN_INSERTION_START) {
						UPDATE_SCOPE
						tok = match(lex, TOKEN_STRING);
						return_expr_fn = ctt_format("char *__comptime_insert_layer%d_%d() { return %s; }\n", parent_scope, insert_counters[parent_scope]++, tok.string_data);
						layer_tails[scope] = block_list_append(layer_tails[scope], make_block(scope, B_INSERTION_ORIGIN_HOOK, return_expr_fn));
						layer_tails[parent_scope] = block_list_append(layer_tails[parent_scope], make_block(scope, B_SOURCE_INSERT, tok.string_data));
						tok = match(lex, TOKEN_INSERTION_END);
						if (tok.scope != scope) {
							fprintf(stderr, "Start and end of ctime insertion different scopes\n");
							exit(1);
						}
						if (scope >= parent_scope) {
							fprintf(stderr, "Cannot use an expr from layer %d in layer %d\n", scope, parent_scope);
							exit(1);
						}
					} else {
						fprintf(stderr, "Syntax error, saw token type %s\n", TOKENTYPE_TO_STR[tok.type]);
						exit(1);
					}
				}
				if (tok.scope != parent_scope) {
					fprintf(stderr, "Start and end of ctime statement block have different scopes");
					exit(1);
				}
				break;
			case TOKEN_INSERTION_START:
				UPDATE_SCOPE
				tok = match(lex, TOKEN_STRING);
				/* only 1 digit means it is inserting into runtime source */
				return_expr_fn = ctt_format("char *__comptime_insert_target_%d() { return %s; }\n", source_insert_counter++, tok.string_data);
				layer_tails[scope] = block_list_append(layer_tails[scope], make_block(scope, B_INSERTION_ORIGIN_HOOK, return_expr_fn));
				source_tail = block_list_append(source_tail, make_block(scope, B_SOURCE_INSERT, tok.string_data));
				tok = match(lex, TOKEN_INSERTION_END);
				if (tok.scope != scope) {
					fprintf(stderr, "Start and end of ctime insertion block have different scopes");
					exit(1);
				}
				break;
			default: abort();
		}
	}
	size_t *num_comptime_blocks = calloc(highest_comptime_scope+1, sizeof(size_t));
	source_block **block_arr_2d = calloc(highest_comptime_scope+1, sizeof(*block_arr_2d));
	for (int i = 0; i < highest_comptime_scope+1; ++i) {
		size_t len = 0;
		block_node *cursor = layers[i];
		cursor = cursor->next; // head is sentinel
		while (cursor) {
			cursor = cursor->next;
			++len;
		}
		num_comptime_blocks[i] = len;
		block_arr_2d[i] = calloc(len, sizeof(source_block));
		cursor = layers[i];
		cursor = cursor->next; // head is sentinel
		for (size_t j = 0; j < len; ++j) {
			block_arr_2d[i][j] = *cursor->data;
			cursor = cursor->next;
		}
		block_list_free(layers[i]);
	}
	size_t source_len = 0;
	block_node *cursor = source_head;
	cursor = cursor->next; // head is sentinel
	while (cursor) {
		cursor = cursor->next;
		++source_len;
	}
	source_block *source_arr = calloc(source_len, sizeof(*source_arr));
	cursor = source_head;
	cursor = cursor->next; // head is sentinel
	for (size_t i = 0; i < source_len; ++i) {
		source_arr[i] = *cursor->data;
		cursor = cursor->next;
	}
	return (code_blocks) { .comptime_layers = block_arr_2d, .num_blocks_in_comptime_layer = num_comptime_blocks, .num_comptime_layers = highest_comptime_scope+1, .source_code = source_arr, .num_blocks_in_source = source_len };
}

