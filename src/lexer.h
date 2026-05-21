/*
  the tokens used and the lexer iterator that pulls them
  could be extended to have source metadata for better errors
*/

#ifndef LEXER_H
#define LEXER_H

#include <stdio.h>

// tokens: <STRING>, #{, }#, $(, )$, <EOF>
typedef enum {
	TOKEN_NONE, // option<token>
	TOKEN_STRING,
	TOKEN_QUOTE_START,
	TOKEN_QUOTE_END,
	TOKEN_CTIMEDEF_START,
	TOKEN_CTIMEDEF_END,
	TOKEN_INSERTION_START,
	TOKEN_INSERTION_END,
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
	int scope;
	char *string_data;
} Token;

typedef struct Lexer Lexer;

Lexer *lexer_new(FILE *in_stream);
void lexer_free(Lexer *lex);
Token lexer_next(Lexer *lex);

void token_free(Token tok);
char *token_to_str(Token tok);

#endif

