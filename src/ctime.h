/*
  A single header library intended for use inside of comptime code
  Provides an API for ctt, and useful string functions under ct_
*/
#ifndef CTIME_H
#define CTIME_H

#include <stdio.h>
#include <stdarg.h>
#include <stddef.h>

// transpiler comp-runtime (ctt)

// I might make a new type to return more information to the transpiler
// As far as I have in mind, NULL for error is sufficient
// But this API will always be valid

// the type returned by insertion macros
#define ctt_str char*

// Managed memory to be freed by ctt_return (WIP)
#define ctt_alloc(ALLOC_SIZE) \
	malloc(ALLOC_SIZE)

// Returns a string for use in macros (currently just a normal char*)
#define ctt_return(STR) \
	return (ctt_str)STR

// Returns an error to transpiler and prints a given string to stderr
#define ctt_error(STR) \
	fprintf(stderr, "%s\n", STR); \
	return (ctt_str)NULL; \

// If an expression is false, returns an error to the transpiler
// and prints a given string to stderr
#define ctt_assert(BOOL, STR) \
	do { \
		if (!(BOOL)) { \
			fprintf(stderr, "assertion failed: %s\n", STR); \
			return (ctt_str)NULL; \
		} \
	} while (0)

// useful functions (ct)

#ifndef CTIME_PREFIX
#define CTIME_PREFIX ct_
#endif

#define CT_CONCAT(a, b) a ## b
#define CT_CONCAT_EXPAND(a, b) CT_CONCAT(a, b)  // forces expansion of arguments
#define CT_NAME(name) CT_CONCAT_EXPAND(CTIME_PREFIX, name)
/* #define CT_CONCAT(CT_FIRST, CT_SECOND) CT_FIRST ## CT_SECOND */
/* #define CT_NAME(name) CTIME_PREFIX ## name */

// returns a string that when printed, is identical to itself as source code minus the ""
// defaults to ct_quote
char *CT_NAME(quote)(const char *s);

// returns a new formatted string
char *CT_NAME(format)(const char *fmt, ...);

// replaces substrings with a given string, and returns a new string
char *CT_NAME(substr_replace)(const char *s, const char *find, const char *replace);

// counts how many occurrences of a substring are present
int CT_NAME(substr_count)(const char *s, const char *substr);

#ifdef CTIME_IMPLEMENTATION

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

char *CT_NAME(format)(const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	int len = vsnprintf(NULL, 0, fmt, args);
	va_end(args);
	if (len < 0)
		return NULL;
	char *buffer = malloc(len + 1);
	if (!buffer)
		return NULL;
	va_start(args, fmt);
	vsnprintf(buffer, len + 1, fmt, args);
	va_end(args);
	return buffer;
}

char *CT_NAME(quote)(const char *s) {
	if (!s) return NULL;
	// Worst case: every char becomes 4 chars (\ooo) + quotes, but we don't add outer quotes
	size_t len = strlen(s);
	size_t out_len = len * 4 + 1;  // very safe
	char *out = malloc(out_len);
	if (!out) return NULL;

	char *p = out;
	while (*s) {
		switch (*s) {
			case '\n':
				*p++ = '\\';
				*p++ = 'n';
				break;
			case '\r':
				*p++ = '\\';
				*p++ = 'r';
				break;
			case '\t':
				*p++ = '\\';
				*p++ = 't';
				break;
			case '\"':
				*p++ = '\\';
				*p++ = '\"';
				break;
			case '\\':
				*p++ = '\\';
				*p++ = '\\';
				break;
			case '\0':  // shouldn't happen due to strlen, but I might not use strlen later
				*p++ = '\\';
				*p++ = '0';
				break;
			default:
				*p++ = *s;
				// if (*s >= 32 && *s < 127) {
					// *p++ = *s;  // printable ASCII
				// } else {
					// 	// Octal escape for other control chars
					// 	*p++ = '\\';
					// 	*p++ = '0' + ((*s >> 6) & 7);
					// 	*p++ = '0' + ((*s >> 3) & 7);
					// 	*p++ = '0' + (*s & 7);
				// }
				break;
		}
		s++;
	}
	*p = '\0';
	return out;
}

char *CT_NAME(substr_replace)(const char *s, const char *find, const char *replace) {
	#define PUSH(C) \
		do { \
			if (len == cap) { \
				cap *= 2; \
				char *new_buf = realloc(replaced, cap); \
				if (!new_buf) { \
					free(replaced); \
					return NULL; \
				} \
				replaced = new_buf; \
			} \
			replaced[len++] = (C); \
		} while (0)

	size_t cap = 8;			   // initial capacity
	char *replaced = malloc(cap);
	if (!replaced) return NULL;

	size_t len = 0;			   // current length (excluding final '\0')
	size_t len_s = strlen(s);
	size_t len_find = strlen(find);
	size_t len_rep = strlen(replace);

	// Special case: empty search string – nothing to replace, return a copy
	if (len_find == 0) {
		char *copy = malloc(len_s + 1);
		if (copy) memcpy(copy, s, len_s + 1);
		free(replaced);
		return copy;
	}

	for (size_t i = 0; i < len_s; ++i) {
		// Check if the substring starting at i matches 'find'
		if (i + len_find <= len_s && memcmp(s + i, find, len_find) == 0) {
			// Match found: push the replacement string
			for (size_t j = 0; j < len_rep; ++j) {
				PUSH(replace[j]);
			}
			i += len_find - 1;   // skip the matched substring (loop will increment i)
		} else {
			// No match: push the current character
			PUSH(s[i]);
		}
	}

	PUSH('\0');  // null-terminate the result
	return replaced;

	#undef PUSH
}

int CT_NAME(substr_count)(const char *s, const char *substr) {
	int count = 0;
	for (size_t i = 0; i + strlen(substr) < strlen(s); ++i) {
		for (size_t j = 0; j < strlen(substr); ++j) {
			if (s[i+j] == substr[j]) {
				++count;
			} else {
				break;
			}
		}
	}
	return count;
}

#endif /* CTIME_IMPLEMENTATION */

#endif /* CTIME_H */

