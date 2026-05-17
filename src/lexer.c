/*
  This lexical granularity is a bit overkill for the current design where comptime def and insertion are completely separate.
  But in the future I may want to layer higher levels of comptime, for which this would be needed for using a different layer's / *$ $* /.
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
	FILE *file;
	Token next;
	Buffer *buffer;
};

static void fatal(const char *msg) {
	fprintf(stderr, "%s\n", msg);
	exit(1);
}

Lexer *lexer_new(const char *filename) {
	FILE *f = fopen(filename, "r");
	if (!f) {
		perror("fopen");
		exit(1);
	}
	Lexer *lex = malloc(sizeof(*lex));
	if (!lex) fatal("out of memory");
	*lex = (Lexer) {
		.file = f,
		.next = token_new(TOKEN_NONE, NULL),
		.buffer = buffer_new(),
	};
	return lex;
}

void lexer_free(Lexer *lex) {
	if (lex) {
		if (lex->file) fclose(lex->file);
		buffer_free(lex->buffer);
		free(lex);
	}
}

static int number_of_chars_before(const char *s, int offset, const char c) {
	int count = 0;
	for (; offset >= 0; --offset) {
		if (s[offset] != c)
			break;
		++count;
	}
	return count;
}

Token lexer_next(Lexer *lex) {
	if (lex->next.type != TOKEN_NONE) {
		Token t = lex->next;
		lex->next = (Token) { TOKEN_NONE, NULL };
		return t;
	}
	/* State machine */
	enum {
		S_CSOURCE,
		S_SLASH,			  // /
		S_SLASH_STAR,		 // /*
		S_C_COMMENT,  // /*[^#%]
		S_C_COMMENT_STAR,  // /* ... *
		S_CPP_COMMENT,
		S_HASH,  // #
		S_HASH_STAR,  // #*
		S_DOLLAR,  // $
		S_DOLLAR_STAR,  // $*
	} state = S_CSOURCE;

	bool in_c_string = false;
	char prev = '\0';

	char *s;
	int c;
	while ((c = fgetc(lex->file)) != EOF) {
		/* this needs to be checked before buffer insertion */
		int bses_before_top = number_of_chars_before(lex->buffer->data, lex->buffer->len-1, '\\');
		// string is theoretically a state, but is almost orthogonal
		buffer_append_char(lex->buffer, c);
		bool can_enter_string = !in_c_string && state != S_C_COMMENT && state != S_C_COMMENT_STAR && state != S_CPP_COMMENT && state != S_SLASH_STAR;
		/* printf("state: %d, char: %c, in_c_string: %d, \\'s before top:%d'\n", state, c, in_c_string, bses_before_top); */
		/* printf("%c.", c); */
		if (in_c_string) {
			if (c == '"' && bses_before_top % 2 == 0)
				in_c_string = false;
			if (c == '\n')
				in_c_string = false;
			prev = c; // for discerning strings
			continue;
		} else if (c == '"' && can_enter_string && prev != '\'' && bses_before_top % 2 == 0) {
			in_c_string = true;
			state = S_CSOURCE;
			prev = c; // for discerning strings
			continue;
		}
		prev = c; // for discerning strings

		switch (state) {
			case S_CSOURCE:
				switch (c) {
					case '/':
						state = S_SLASH;
						break;
					case '#':
						state = S_HASH;
						break;
					case '$':
						state = S_DOLLAR;
						break;
				}
				break;

			case S_SLASH:
				switch (c) {
					case '*':
						state = S_SLASH_STAR;
						break;
					case '/':
						state = S_CPP_COMMENT;
						break;
					case '#':
						state = S_HASH;
						break;
					case '$':
						state = S_DOLLAR;
						break;
				}
				break;

			case S_SLASH_STAR:
				switch (c) {
					case '#':
						state = S_CSOURCE;
						/* clear down to previous string */
						buffer_pop_end(lex->buffer);
						buffer_pop_end(lex->buffer);
						buffer_pop_end(lex->buffer);
						if (lex->buffer->len == 0) { // no string before
							return token_new(TOKEN_CTIMEDEF_START, NULL);
						}
						s = buffer_to_cstr(lex->buffer);
						buffer_clear(lex->buffer);
						lex->next = (Token) { .type = TOKEN_CTIMEDEF_START, .string_data = NULL };
						return token_new(TOKEN_STRING, s);
					case '$':
						state = S_CSOURCE;
						/* clear down to previous string */
						buffer_pop_end(lex->buffer);
						buffer_pop_end(lex->buffer);
						buffer_pop_end(lex->buffer);
						if (lex->buffer->len == 0) { // no string before
							return token_new(TOKEN_INSERTION_START, NULL);
						}
						s = buffer_to_cstr(lex->buffer);
						buffer_clear(lex->buffer);
						lex->next = (Token) { .type = TOKEN_INSERTION_START, .string_data = NULL };
						return token_new(TOKEN_STRING, s);
					default:
						state = S_C_COMMENT;
						break;
				}
				break;

			case S_C_COMMENT:
				if (c == '*') {
					state = S_C_COMMENT_STAR;
				}
				break;

			case S_C_COMMENT_STAR:
				if (c == '/') {
					state = S_CSOURCE;
				} else {
					state = S_C_COMMENT;
				}
				break;

			case S_CPP_COMMENT:
				if (c == '\n')
					state = S_CSOURCE;
				break;

			case S_HASH:
				switch (c) {
					case '*':
						state = S_HASH_STAR;
						break;
					case '/':
						state = S_SLASH;
						break;
					case '#':
						state = S_HASH;
						break;
					case '$':
						state = S_DOLLAR;
						break;
				}
				break;

			case S_HASH_STAR:
				switch (c) {
					case '/':
						state = S_CSOURCE;
						/* clear down to previous string */
						buffer_pop_end(lex->buffer);
						buffer_pop_end(lex->buffer);
						buffer_pop_end(lex->buffer);
						if (lex->buffer->len == 0) { // no string before
							return token_new(TOKEN_CTIMEDEF_END, NULL);
						}
						char *s = buffer_to_cstr(lex->buffer);
						buffer_clear(lex->buffer);
						lex->next = token_new(TOKEN_CTIMEDEF_END, NULL);
						return token_new(TOKEN_STRING, s);
					case '#':
						state = S_HASH;
						break;
					case '$':
						state = S_DOLLAR;
						break;
					default:
						state = S_CSOURCE;
						break;
				}
				break;

			case S_DOLLAR:
				switch (c) {
					case '*':
						state = S_DOLLAR_STAR;
						break;
					case '/':
						state = S_SLASH;
						break;
					case '#':
						state = S_HASH;
						break;
					case '$':
						state = S_DOLLAR;
						break;
				}
				break;

			case S_DOLLAR_STAR:
				switch (c) {
					case '/':
						state = S_CSOURCE;
						/* clear down to previous string */
						buffer_pop_end(lex->buffer);
						buffer_pop_end(lex->buffer);
						buffer_pop_end(lex->buffer);
						if (lex->buffer->len == 0) { // no string before
							return token_new(TOKEN_INSERTION_END, NULL);
						}
						char *s = buffer_to_cstr(lex->buffer);
						buffer_clear(lex->buffer);
						lex->next = token_new(TOKEN_INSERTION_END, NULL);
						return token_new(TOKEN_STRING, s);
						break;
					case '#':
						state = S_HASH;
						break;
					case '$':
						state = S_DOLLAR;
						break;
					default:
						state = S_CSOURCE;
						break;
				}
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
