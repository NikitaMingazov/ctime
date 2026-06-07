/*
  the tokens used and the lexer iterator that pulls them
  could be extended to have source metadata for better errors
*/

#ifndef LEXER_H
#define LEXER_H

#include <stdbool.h>
#include <stdio.h>

// tokens: <STRING>, #{, }#, $(, )$, $$, <EOF>
typedef enum {
	TOKEN_NONE, // option<token>
	TOKEN_STRING,
	TOKEN_QUOTE_START, // TODO: make quotes run after preprocessor
	TOKEN_QUOTE_END,
	TOKEN_CTIMEDEF_START,
	TOKEN_CTIMEDEF_END,
	TOKEN_INSERTION_START,
	TOKEN_INSERTION_END,
	TOKEN_COMPARGS_REF,
	TOKEN_EOF
} TokenType;

static char *TOKENTYPE_TO_STR[] = {
	"none",
	"string",
	"'{",
	"}'",
	"#{",
	"}#",
	"$(",
	")$",
	"EOF",
};

typedef struct {
	TokenType type;
	// not read for target source tokens
	bool is_comptime;
	char *string_data;
	unsigned row;
	unsigned col;
} Token;

typedef struct Lexer Lexer;

Lexer *lexer_new(FILE *in_stream, unsigned hard_tab_width, bool debug);
void lexer_free(Lexer *lex);
Token lexer_next(Lexer *lex);

void token_free(Token tok);
char *token_to_str(Token tok);

#endif

