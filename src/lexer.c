/*
  #{N N}# $(N N)$ are tokens, but the N can be missing for an implicit 0
*/

#include "lexer.h"
#include "buffer.h"
#include "ctime_utils.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

Token token_new(TokenType type, int layer, char *data) {
	return (Token) { .type = type, .scope = layer, .string_data = data };
}

void token_free(Token tok) {
	if (tok.type == TOKEN_STRING) {
		free(tok.string_data);
	}
}

char *token_to_str(Token tok) {
	return ctt_format("%s: [\n%s\n]", TOKENTYPE_TO_STR[tok.type], tok.string_data);
}

struct Lexer {
	FILE *in_stream;
	Token next;
	Buffer *buffer;
};

Lexer *lexer_new(FILE *in_stream) {
	Lexer *lex = malloc(sizeof(*lex));
	if (!lex) {
		fprintf(stderr, "out of memory\n");
		return NULL;
	}
	*lex = (Lexer) {
		.in_stream = in_stream,
		.next = token_new(TOKEN_NONE, -2, NULL),
		.buffer = buffer_new(),
	};
	return lex;
}

void lexer_free(Lexer *lex) {
	if (lex) {
		buffer_free(lex->buffer);
		free(lex);
	}
}

#define SWITCH_TRANSITION(CHAR, STATE) \
	case CHAR: \
		state = STATE; \
		break;

#define PUSH_TOKEN(TYPE) \
	state = S_CSOURCE; \
	if (TYPE == TOKEN_QUOTE_START) { \
		next = buffer_pop_end(lex->buffer); \
	} \
	/* clear down to previous string */ \
	buffer_pop_end(lex->buffer); \
	buffer_pop_end(lex->buffer); \
	if (TYPE == TOKEN_QUOTE_START) { \
		ungetc(next, lex->in_stream); \
	} \
	/* a quote's layer is never read */ \
	if (TYPE == TOKEN_QUOTE_START || TYPE == TOKEN_QUOTE_END) { \
		layer = -3; \
	} \
	/* take the scope number from the token */ \
	if (TYPE == TOKEN_CTIMEDEF_END || TYPE == TOKEN_INSERTION_END) { \
		Buffer *tmpbuf = buffer_new(); \
		while (lex->buffer->len > 0) { \
			if (isdigit(prev = buffer_pop_end(lex->buffer))) { \
				buffer_append_char(tmpbuf, prev); \
			} else { \
				buffer_append_char(lex->buffer, prev); \
				break; \
			} \
		} \
		layer_str = buffer_to_cstr(tmpbuf);	\
		if (strlen(layer_str) > 0) \
			layer = atoi(layer_str); \
		else \
			layer = 0; /* empty number is implicit 0 */ \
		buffer_free(tmpbuf); \
	} \
	if (TYPE == TOKEN_CTIMEDEF_START || TYPE == TOKEN_INSERTION_START) { \
		Buffer *tmpbuf = buffer_new(); \
		c = fgetc(lex->in_stream); \
		while (isdigit(c)) { \
			buffer_append_char(tmpbuf, c); \
			c = fgetc(lex->in_stream); \
		} \
		ungetc(c, lex->in_stream); \
		layer_str = buffer_to_cstr(tmpbuf); \
		if (strlen(layer_str) > 0) \
			layer = atoi(layer_str); \
		else \
			layer = 0; \
		buffer_free(tmpbuf); \
	} \
	if (lex->buffer->len == 0) { /* no string before */ \
		return token_new(TYPE, layer, NULL); \
	} \
	s = buffer_to_cstr(lex->buffer); \
	buffer_clear(lex->buffer); \
	/* string before, return it and queue this token */ \
	lex->next = token_new(TYPE, layer, NULL); \
	return token_new(TOKEN_STRING, -1, s);

Token lexer_next(Lexer *lex) {
	/* lexer holds up to 1 token queued */
	if (lex->next.type != TOKEN_NONE) {
		Token t = lex->next;
		lex->next = token_new(TOKEN_NONE, -2, NULL);
		return t;
	}
	/* State machine */
	enum {
		S_CSOURCE,
		S_CSOURCE_CHAR, /*  '  */
		S_CSOURCE_CHAR_LCURL, /*  '{  */
		S_CSOURCE_CHAR_ESCAPE, /*  '\   */
		S_CSOURCE_CHAR_DONE, /*  '\_   */
		S_CSTRLIT,
		S_CSTRLIT_ESCAPE,
		S_HASH,
		S_RCURL,
		S_DOLLAR,
		S_RPAR,
		S_SLASH,
		S_C_COMMENT,
		S_C_COMMENT_STAR,
		S_CPP_COMMENT,
	} state = S_CSOURCE;

	char next;
	char prev;
	char *s;
	char *layer_str;
	int layer;
	int c;
	while ((c = fgetc(lex->in_stream)) != EOF) {
		buffer_append_char(lex->buffer, (char) c);
	epsilon:
		switch (state) {
			case S_CSOURCE:
				switch (c) {
					SWITCH_TRANSITION('#', S_HASH)
					SWITCH_TRANSITION('$', S_DOLLAR)
					SWITCH_TRANSITION('}', S_RCURL)
					SWITCH_TRANSITION(')', S_RPAR)
					SWITCH_TRANSITION('/', S_SLASH)
					SWITCH_TRANSITION('"', S_CSTRLIT)
					SWITCH_TRANSITION('\'', S_CSOURCE_CHAR)
				}
				break;

			case S_HASH:
				switch (c) {
					case '{':
						PUSH_TOKEN(TOKEN_CTIMEDEF_START)
					default:
						state = S_CSOURCE;
						goto epsilon;
				}
				break;

			case S_RCURL:
				switch (c) {
					case '#':
						PUSH_TOKEN(TOKEN_CTIMEDEF_END)
					case '\'':
						PUSH_TOKEN(TOKEN_QUOTE_END)
					default:
						state = S_CSOURCE;
						goto epsilon;
				}
				break;

			case S_DOLLAR:
				switch (c) {
					case '(':
						PUSH_TOKEN(TOKEN_INSERTION_START)
					default:
						state = S_CSOURCE;
						goto epsilon;
				}
				break;

			case S_RPAR:
				switch (c) {
					case '$':
						PUSH_TOKEN(TOKEN_INSERTION_END)
					default:
						state = S_CSOURCE;
						goto epsilon;
				}
				break;

			case S_CSTRLIT:
				switch (c) {
					SWITCH_TRANSITION('\"', S_CSOURCE)
					SWITCH_TRANSITION('\\', S_CSTRLIT_ESCAPE)
				}
				break;

			case S_CSTRLIT_ESCAPE:
				state = S_CSTRLIT;
				break;

			case S_SLASH:
				switch (c) {
					SWITCH_TRANSITION('*', S_C_COMMENT)
					SWITCH_TRANSITION('/', S_CPP_COMMENT)
					default:
						state = S_CSOURCE;
						goto epsilon;
				}
				break;

			case S_C_COMMENT:
				if (c == '*')
					state = S_C_COMMENT_STAR;
				break;

			case S_C_COMMENT_STAR:
				if (c == '/') {
					state = S_CSOURCE;
				} else {
					state = S_C_COMMENT;
				}
				break;

			case S_CPP_COMMENT:
				if (c == '\n' && lex->buffer->data[lex->buffer->len - 2] != '\\')
					state = S_CSOURCE;
				break;

			case S_CSOURCE_CHAR:
				if (c == '{') {
					state = S_CSOURCE_CHAR_LCURL;
				} else if (c == '\\') {
					state = S_CSOURCE_CHAR_ESCAPE;
				} else {
					state = S_CSOURCE_CHAR_DONE;
				}
				break;

			case S_CSOURCE_CHAR_LCURL:
				if (c != '\'') {
					PUSH_TOKEN(TOKEN_QUOTE_START)
				} else {
					state = S_CSOURCE;
					goto epsilon;
				}
				break;

			case S_CSOURCE_CHAR_ESCAPE:
				state = S_CSOURCE_CHAR_DONE;
				break;

			case S_CSOURCE_CHAR_DONE:
				/* if (c == '\'') { */
				state = S_CSOURCE;
				break;

		}
	}
	if (lex->buffer->len > 0) {
		char *final_source = buffer_to_cstr(lex->buffer);
		buffer_clear(lex->buffer);
		return token_new(TOKEN_STRING, -1, final_source);
	}
	return token_new(TOKEN_EOF, -1, NULL);
}

