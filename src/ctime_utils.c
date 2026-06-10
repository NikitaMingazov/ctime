#include "libctt.h"
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

char *CT_NAME(strdup)(const char *s) {
	const size_t len = strlen(s);
	char *new_s = malloc(len+1);
	memcpy(new_s, s, len+1);
	return new_s;
}

