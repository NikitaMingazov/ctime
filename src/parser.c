/*
  Currently the grammar is flat and not a tree.
  That might change later, hence the general approach.
*/
#include "parser.h"
#include "buffer.h"
#include "lexer.h"
#include "ctime_utils.h"
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

source_block *make_block(block_type type, char *data) {
	source_block *new = malloc(sizeof(*new));
	*new = (source_block) {
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
// returns data of next token if it matches grammar, exits with error otherwise
char *match(Lexer *lex, TokenType expected) {
	Token t = lexer_next(lex);
	/* printf("%d: %s\n", k++, token_to_str(t)); */
	if (t.type != expected) {
		fprintf(stderr, "Expected token type %s, but saw %s\n", TOKENTYPE_TO_STR[expected], TOKENTYPE_TO_STR[t.type]);
		exit(1);
	}
	return t.string_data;
}

static bool is_strlit_expr(const char *s) {
	int len = strlen(s);
	int trim_start = 0;
	while (trim_start < len && isspace((unsigned char)s[trim_start]))
		++trim_start;
	int trim_end = len - 1;
	while (trim_end >= 0 && isspace((unsigned char)s[trim_end]))
		--trim_end;
	return s[trim_start] == '"' && s[trim_end] == '"';
}

code_blocks parse_into_blocks(Lexer *lex) {
	Buffer *comptime_code = buffer_new();
	char *s;
	char *return_expr_fn;
	block_node *head = malloc(sizeof(*head));
	*head = (block_node) { .data = NULL, .next = NULL };
	block_node *tail = head;
	int insert_counter = 0;
	int num_source_blocks = 0;
	Token tok;
	while ((tok = lexer_next(lex)).type != TOKEN_EOF) {
		/* printf("%d: %s\n", k++, token_to_str(tok)); */
		switch (tok.type) {
			case TOKEN_STRING:
				tail = block_list_append(tail, make_block(B_SOURCE_CODE, tok.string_data));
				++num_source_blocks;
				break;
			case TOKEN_CTIMEDEF_START:
				s = match(lex, TOKEN_STRING);
				buffer_append_cstr(comptime_code, s);
				free(s);
				free(match(lex, TOKEN_CTIMEDEF_END));
				break;
			case TOKEN_INSERTION_START:
				s = match(lex, TOKEN_STRING);
				/* insert a function to return the string expression when comptime is over */
				return_expr_fn = format("char *__comptime_insert_%d() { return %s; }\n", insert_counter++, s);
				buffer_append_cstr(comptime_code, return_expr_fn);
				free(return_expr_fn);
				bool is_strlit = is_strlit_expr(s);
				if (!is_strlit)
					tail = block_list_append(tail, make_block(B_SOURCE_INSERT, NULL));
				else
					tail = block_list_append(tail, make_block(B_SOURCE_INSERT, "sentinel"));
				++num_source_blocks;
				free(s);
				free(match(lex, TOKEN_INSERTION_END));
				break;
			default: abort();
		}
	}
	source_block *block_arr = calloc(num_source_blocks, sizeof(source_block));
	block_node *cursor = head;
	for (int i = 0; i < num_source_blocks; ++i) {
		/* head is a sentinel, so step belongs here */
		cursor = cursor->next;
		block_arr[i] = *(cursor->data);
	}
	char *comptime_block_str = buffer_to_cstr(comptime_code);
	buffer_free(comptime_code);
	block_list_free(head);
	return (code_blocks) { .comptime_block = comptime_block_str, .source_blocks = block_arr, .num_source_blocks = num_source_blocks };
}

