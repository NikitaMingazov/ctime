#include "parser.h"
#include "lexer.h"
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static CTimeNode *new_node(enum node_type type, char *code, unsigned row, unsigned col) {
	CTimeNode *new = malloc(sizeof(*new));
	*new = (CTimeNode) {
		.type = type,
		.code = code,
		.row = row,
		.col = col,
		.children = NULL,
		.num_children = 0,
	};
	return new;
}

static void append_child(CTimeNode *n, CTimeNode *child) {
	++n->num_children;
	n->children = realloc(n->children, n->num_children * sizeof(*n->children));
	n->children[n->num_children-1] = child;
}

// returns next token if it matches grammar, exits with error otherwise
Token match(Lexer *lex, TokenType expected) {
	Token t = lexer_next(lex);
	if (t.type != expected) {
		fprintf(stderr, "Expected token type %s, but saw %s\n", TOKENTYPE_TO_STR[expected], token_to_str(t));
		exit(1);
	}
	return t;
}

static void insert_production(Lexer *lex, CTimeNode *insert) {
	Token tok;
	CTimeNode *new_block;
	while ((tok = lexer_next(lex)).type != TOKEN_EOF) {
		switch (tok.type) {
			case TOKEN_STRING:
				append_child(insert, new_node(N_SOURCE_CODE, tok.string_data, tok.row, tok.col));
				break;
			case TOKEN_INSERTION_START:
				new_block = new_node(N_INSERT, NULL, tok.row, tok.col);
				append_child(insert, new_block);
				insert_production(lex, new_block);
				break;
			case TOKEN_COMPARGS_REF:
				append_child(insert, new_node(N_SOURCE_CODE, "__CC_ARGS__", tok.row, tok.col));
				break;
			case TOKEN_INSERTION_END:
				return;
			default:
				fprintf(stderr, "Syntax error (%d:%d)\n", tok.row, tok.col);
				exit(1);
		}
	}
}

static void quote_production(Lexer *lex, CTimeNode *quote) {
	Token tok;
	CTimeNode *new_block;
	while ((tok = lexer_next(lex)).type != TOKEN_EOF) {
		switch (tok.type) {
			case TOKEN_STRING:
				append_child(quote, new_node(N_SOURCE_CODE, tok.string_data, tok.row, tok.col));
				break;
			case TOKEN_INSERTION_START:
				new_block = new_node(N_INSERT, NULL, tok.row, tok.col);
				append_child(quote, new_block);
				insert_production(lex, new_block);
				break;
			case TOKEN_COMPARGS_REF: // TODO: figure out if this is correct
				append_child(quote, new_node(N_SOURCE_CODE, "__CC_ARGS__", tok.row, tok.col));
				break;
			case TOKEN_QUOTE_END:
				return;
			default:
				fprintf(stderr, "Syntax error (%d:%d)\n", tok.row, tok.col);
				exit(1);
		}
	}
}

static void comptime_production(Lexer *lex, CTimeNode *comptime) {
	Token tok;
	CTimeNode *new_block;
	while ((tok = lexer_next(lex)).type != TOKEN_EOF) {
		switch (tok.type) {
			case TOKEN_STRING:
				append_child(comptime, new_node(N_SOURCE_CODE, tok.string_data, tok.row, tok.col));
				break;
			case TOKEN_INSERTION_START:
				new_block = new_node(N_INSERT, NULL, tok.row, tok.col);
				append_child(comptime, new_block);
				insert_production(lex, new_block);
				break;
			case TOKEN_QUOTE_START:
				new_block = new_node(N_QUOTE, NULL, tok.row, tok.col);
				append_child(comptime, new_block);
				quote_production(lex, new_block);
				break;
			case TOKEN_COMPARGS_REF:
				append_child(comptime, new_node(N_SOURCE_CODE, "__CC_ARGS__", tok.row, tok.col));
				break;
			case TOKEN_CTIMEDEF_END:
				return;
			default:
				fprintf(stderr, "Syntax error (%d:%d)\n", tok.row, tok.col);
				exit(1);
		}
	}
}

CTimeNode *parse_into_tree(Lexer *lex) {
	CTimeNode *root = new_node(N_ROOT, NULL, 0, 0);

	CTimeNode *comptime_block;
	CTimeNode *insert_block;
	Token tok;
	while ((tok = lexer_next(lex)).type != TOKEN_EOF) {
		switch (tok.type) {
			case TOKEN_STRING:
				append_child(root, new_node(N_SOURCE_CODE, tok.string_data, tok.row, tok.col));
				break;
			case TOKEN_CTIMEDEF_START:
				comptime_block = new_node(N_COMPTIME, NULL, tok.row, tok.col);
				append_child(root, comptime_block);
				comptime_production(lex, comptime_block);
				break;
			case TOKEN_INSERTION_START:
				insert_block = new_node(N_INSERT, NULL, tok.row, tok.col);
				append_child(root, insert_block);
				insert_production(lex, insert_block);
				break;
			default:
				fprintf(stderr, "Syntax error (%d:%d)\n", tok.row, tok.col);
				exit(1);
		}
	}
	return root;
}

// the string is owned by an arena, the node array is realloc so heap ptrs
void ctime_node_free(CTimeNode *n) {
	for (size_t i = 0; i < n->num_children; ++i) {
		ctime_node_free(n->children[i]);
	}
	free(n->children);
	free(n);
}

static void print_tree_inner(CTimeNode *n, unsigned level) {
	const char *indent = "  ";
	char *prefix = malloc(level*strlen(indent)+1);
	for (size_t i = 0; i < level; ++i) {
		memcpy(prefix+i*strlen(indent), indent, strlen(indent));
	}
	prefix[level*strlen(indent)] = '\0';
	fprintf(stderr, "%s%s (%d:%d) %.0s\n", prefix, NODE_TYPE_STR[n->type], n->row, n->col, n->code);
	free(prefix);
	for (size_t i = 0; i < n->num_children; ++i) {
		print_tree_inner(n->children[i], level+1);
	}
}

void ctime_print_tree(CTimeNode *n) {
	print_tree_inner(n, 0);
}

