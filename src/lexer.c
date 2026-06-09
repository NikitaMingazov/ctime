#include "lexer.h"
#include "arena.h"
#include "buffer.h"
#include "libctt.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

char *token_to_str(Token tok) {
	// return ct_format("%s (%d:%d)", TOKENTYPE_TO_STR[tok.type], tok.row, tok.col);
	return ct_format("%s (%d:%d): [\n%s\n]", TOKENTYPE_TO_STR[tok.type], tok.row, tok.col, tok.string_data);
}

int k = 0; // for debugging
Token token_new(TokenType type, char *data, unsigned row, unsigned col, bool debug) {
	if (debug && type != TOKEN_NONE)
		fprintf(stderr, "%d: %s\n", k++, token_to_str((Token) { .type = type, .string_data = data, .row = row, .col = col }));
	return (Token) { .type = type, .string_data = data, .row = row, .col = col };
}

void token_free(Token tok) {
	if (tok.type == TOKEN_STRING) {
		free(tok.string_data);
	}
}

struct Lexer {
	FILE *in_stream;
	Arena *str_lifetime;
	Token next;
	Buffer *buffer;
	unsigned prev_row;
	unsigned prev_col;
	unsigned cur_row;
	unsigned cur_col;
	unsigned hard_tab_width;
	bool debug_tokens;
};

Lexer *lexer_new(FILE *in_stream, unsigned hard_tab_width, bool debug, Arena *str_lifetime) {
	Lexer *lex = malloc(sizeof(*lex));
	if (!lex) {
		fprintf(stderr, "out of memory\n");
		return NULL;
	}
	*lex = (Lexer) {
		.in_stream = in_stream,
		.str_lifetime = str_lifetime,
		.next = token_new(TOKEN_NONE, NULL, 0, 0, debug),
		.buffer = buffer_new(),
		.prev_col = 1,
		.prev_row = 1,
		.cur_row = 1,
		.cur_col = 1,
		.hard_tab_width = hard_tab_width,
		.debug_tokens = debug,
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

#define CONSUME_NEWLINE \
	/* consume trailing \n */ \
	c = fgetc(lex->in_stream); \
	if (c == '\n') { \
		lex->cur_row++; \
		lex->cur_col = 1; \
	} else { \
		ungetc(c, lex->in_stream); \
	} \

// this is called in the comptime blocks
#define PUSH_TOKEN(TYPE) \
	state = S_CSOURCE; \
	/* clear down to previous string */ \
	buffer_pop_end(lex->buffer); \
	buffer_pop_end(lex->buffer); \
	/* remove preceding space for block closes */ \
	if (TYPE == TOKEN_CTIMEDEF_END || TYPE == TOKEN_INSERTION_END) { \
		if (lex->buffer->len > 0 && lex->buffer->data[lex->buffer->len-1] == ' ') \
			buffer_pop_end(lex->buffer); \
	} \
	/* remove postceding space for block opens */ \
	if (TYPE == TOKEN_CTIMEDEF_START || TYPE == TOKEN_INSERTION_START) { \
		c = fgetc(lex->in_stream); \
		if (c == ' ') \
			lex->cur_col++; \
		else \
			ungetc(c, lex->in_stream); \
	} \
	if (lex->buffer->len == 0) { /* no string before */ \
		new_token = token_new(TYPE, NULL, lex->cur_row, lex->cur_col-2, lex->debug_tokens); \
		if (TYPE == TOKEN_CTIMEDEF_START) { \
			CONSUME_NEWLINE \
		} \
		lex->prev_row = lex->cur_row; \
		lex->prev_col = lex->cur_col; \
		return new_token; \
	} \
	/* remove preceding newline for comptime blocks */ \
	if (TYPE == TOKEN_CTIMEDEF_START) { \
		if (lex->buffer->len > 0 && lex->buffer->data[lex->buffer->len-1] == '\n') \
			buffer_pop_end(lex->buffer); \
	} \
	/* copy out the string in the buffer */ \
	buffer_null_terminate(lex->buffer); \
	s = arena_alloc(lex->str_lifetime, lex->buffer->len); \
	memcpy(s, lex->buffer->data, lex->buffer->len); \
	buffer_clear(lex->buffer); \
	/* a string is before, return it and queue this token */ \
	prev_row = lex->prev_row; \
	prev_col = lex->prev_col; \
	new_token = token_new(TOKEN_STRING, s, prev_row, prev_col, lex->debug_tokens); \
	lex->next = token_new(TYPE, NULL, lex->cur_row, lex->cur_col-2, lex->debug_tokens); \
	if (TYPE == TOKEN_CTIMEDEF_END) { \
		CONSUME_NEWLINE \
	} \
	lex->prev_row = lex->cur_row; \
	lex->prev_col = lex->cur_col; \
	return new_token;

Token lexer_next(Lexer *lex) {
	/* lexer holds up to 1 token queued */
	if (lex->next.type != TOKEN_NONE) {
		Token t = lex->next;
		lex->next = token_new(TOKEN_NONE, NULL, 0, 0, lex->debug_tokens);
		return t;
	}
	/* State machine */
	enum {
		S_CSOURCE,
		S_CSOURCE_CHAR, /*  '  */
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

	char *s;
	int c;
	int peek;
	unsigned prev_row, prev_col;
	Token new_token;
	while ((c = fgetc(lex->in_stream)) != EOF) {
		buffer_append_char(lex->buffer, (char) c);
		if (c == '\n') {
			lex->cur_row++;
			lex->cur_col = 1;
		} else if (c == '\t')
			lex->cur_col += lex->hard_tab_width;
		else
			lex->cur_col++;
	epsilon:
		switch (state) {
			case S_CSOURCE: // TODO: warn on $) and #}
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
					case '$':
						PUSH_TOKEN(TOKEN_COMPARGS_REF)
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
					peek = fgetc(lex->in_stream);
					ungetc(peek, lex->in_stream);
					if (peek != '\'') {
						PUSH_TOKEN(TOKEN_QUOTE_START)
					}
				} else if (c == '\\') {
					state = S_CSOURCE_CHAR_ESCAPE;
				} else {
					state = S_CSOURCE_CHAR_DONE;
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
		buffer_null_terminate(lex->buffer);
		char *final_source = arena_alloc(lex->str_lifetime, lex->buffer->len);
		memcpy(final_source, lex->buffer->data, lex->buffer->len);
		buffer_clear(lex->buffer);
		return token_new(TOKEN_STRING, final_source, lex->prev_row, lex->prev_col, lex->debug_tokens);
	}
	return token_new(TOKEN_EOF, NULL, 0, 0, lex->debug_tokens);
}

