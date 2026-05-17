/*
  A single header library intended for use inside of comptime code
*/
#ifndef CTIMEUTILS_H
#define CTIMEUTILS_H

#include <stdarg.h>

char *format(const char *fmt, ...);

#endif /* CTIMEUTILS_H */

#ifdef CTIMEUTILS_IMPLEMENTATION

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

// returns a new formatted string
char *format(const char *fmt, ...) {
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

#endif /* CTIMEUTILS_IMPLEMENTATION */

