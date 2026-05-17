// dynamic array for chars

#ifndef BUFFER_H
#define BUFFER_H

#include <stddef.h>

typedef struct buffer {
	char *data;
	size_t len;
	size_t cap;
} Buffer;

Buffer *buffer_new();
void buffer_free(Buffer *buf);

void buffer_clear(Buffer *buf);
int buffer_reserve(Buffer *buf, size_t needed);
char buffer_pop_end(Buffer *buf);

char *buffer_to_cstr(const Buffer *buf);

int buffer_append_char(Buffer *buf, const char c);
int buffer_prepend_char(Buffer *buf, const char c);

int buffer_append_cstr(Buffer *buf, const char *s);
int buffer_prepend_cstr(Buffer *buf, const char* s);

#endif
