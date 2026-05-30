#ifndef PARSER_H
#define PARSER_H

#include "lexer.h"
#include <stddef.h>

typedef enum node_type {
	N_ROOT,
	N_COMPTIME,
	N_INSERT,
	N_QUOTE,
	N_SOURCE_CODE,
} node_type;

static const char *NODE_TYPE_STR[] = {
	"root", "comptime", "insertion", "quote", "source_code",
};

typedef struct ctime_node {
	enum node_type type;
	char *code;
	unsigned row;
	unsigned col;
	struct ctime_node* *children;
	size_t num_children;
} CTimeNode;

CTimeNode *parse_into_tree(Lexer *lex);

void ctime_node_free(CTimeNode *n);
void ctime_print_tree(CTimeNode *n);

#endif // PARSER_H

