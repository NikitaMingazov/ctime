/*
  A single header library intended for use inside of comptime code
*/
#ifndef CTIMEUTILS_H
#define CTIMEUTILS_H

#include <stdarg.h>

// useful functions

// returns a new formatted string
char *ctt_format(const char *fmt, ...);

// returns a string that when printed, is identical minus the ""
char *ctt_quote(const char *s);

#endif /* CTIMEUTILS_H */

#ifdef CTIMEUTILS_IMPLEMENTATION

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

char *ctt_format(const char *fmt, ...) {
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

char *ctt_quote(const char *s) {
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
                //     *p++ = *s;  // printable ASCII
                // } else {
                //     // Octal escape for other control chars
                //     *p++ = '\\';
                //     *p++ = '0' + ((*s >> 6) & 7);
                //     *p++ = '0' + ((*s >> 3) & 7);
                //     *p++ = '0' + (*s & 7);
                // }
                break;
        }
        s++;
    }
    *p = '\0';
    return out;
}

#endif /* CTIMEUTILS_IMPLEMENTATION */

