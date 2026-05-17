/*
  This lexical granularity is a bit overkill for the current design where comptime def and insertion are completely separate.
  But in the future I may want to layer higher levels of comptime, for which this would be needed for using a different layer's $( )$.
  Also C comments/CPP comments/strings/chars is a pain.
*/

#include "lexer.h"
#include "buffer.h"
#include "ctime_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

Token token_new(TokenType type, char *data) {
	return (Token) { .type = type, .string_data = data };
}

void token_free(Token tok) {
	if (tok.type == TOKEN_STRING) {
		free(tok.string_data);
	}
}

char *token_to_str(Token tok) {
	return format("%s: [\n%s\n]", TOKENTYPE_TO_STR[tok.type], tok.string_data);
}

struct Lexer {
	FILE *in_stream;
	Token next;
	Buffer *buffer;
};

static void fatal(const char *msg) {
	fprintf(stderr, "%s\n", msg);
	exit(1);
}

Lexer *lexer_new(FILE *in_stream) {
	Lexer *lex = malloc(sizeof(*lex));
	if (!lex) fatal("out of memory");
	*lex = (Lexer) {
		.in_stream = in_stream,
		.next = token_new(TOKEN_NONE, NULL),
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
	/* clear down to previous string */ \
	buffer_pop_end(lex->buffer); \
	buffer_pop_end(lex->buffer); \
	if (lex->buffer->len == 0) { /* no string before */ \
		return token_new(TYPE, NULL); \
	} \
	s = buffer_to_cstr(lex->buffer); \
	buffer_clear(lex->buffer); \
	lex->next = (Token) { .type = TYPE, .string_data = NULL }; \
	return token_new(TOKEN_STRING, s);

Token lexer_next(Lexer *lex) {
	/* lexer holds up to 1 token queued */
	if (lex->next.type != TOKEN_NONE) {
		Token t = lex->next;
		lex->next = (Token) { TOKEN_NONE, NULL };
		return t;
	}
	/* State machine */
	enum {
		S_CSOURCE,
		S_CSOURCE_CHAR, /*  '  */
		S_CSOURCE_CHAR_ESCAPE, /*  '\  */
		S_CSOURCE_CHAR_DONE, /*  '\_  */
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
				if (c == '\\') {
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
		char *final_source = buffer_to_cstr(lex->buffer);
		buffer_clear(lex->buffer);
		return (Token) { .type = TOKEN_STRING, final_source };
	}
	return (Token) { .type = TOKEN_EOF, NULL };
}
