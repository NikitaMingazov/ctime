#include "buffer.h"
#include <stdlib.h>
#include <string.h>
/* #include <assert.h> */

Buffer *buffer_new_with_capacity(size_t capacity) {
	Buffer *buf = malloc(sizeof(*buf));
	*buf = (Buffer){
		.data = malloc(capacity),
		.len = 0,
		.cap = capacity,
	};
	return buf;
}

Buffer *buffer_new() {
	return buffer_new_with_capacity(16);
}

void buffer_free(Buffer *buf) {
	/* assert(buf); */
	free(buf->data);
	free(buf);
}

void buffer_clear(Buffer *buf) {
	buf->len = 0;
}

// returns \0 if empty
char buffer_pop_end(Buffer *buf) {
	if (buf->len == 0)
		return '\0';
	char c = buf->data[buf->len-1];
	buf->len--;
	return c;
}

char *buffer_to_cstr(const Buffer *buf) {
	char *s = malloc(buf->len + 1);
	if (!s)
		return NULL;
	if (buf->len > 0) {
		memcpy(s, buf->data, buf->len);
	}
	s[buf->len] = '\0';
	return s;
}

int buffer_reserve(Buffer *buf, size_t needed) {
	if (needed <= buf->cap)
		return 0;
	size_t new_cap = buf->cap;
	while (new_cap < needed) {
		new_cap *= 2;
	}
	char *new_data = realloc(buf->data, new_cap);
	if (!new_data)
		return -1;
	buf->data = new_data;
	buf->cap = new_cap;
	return 0;
}

int buffer_append_char(Buffer *buf, const char c) {
	if (buffer_reserve(buf, buf->len + 1) != 0)
		return -1;
	buf->data[buf->len++] = c;
	return 0;
}

int buffer_prepend_char(Buffer *buf, const char c) {
	if (buffer_reserve(buf, buf->len + 1) != 0)
		return -1;
	memmove(buf->data + 1, buf->data, buf->len);
	buf->data[0] = c;
	buf->len++;
	return 0;
}

int buffer_append_cstr(Buffer *buf, const char *s) {
	if (!buf || !s)
		return -1;
	size_t slen = strlen(s);
	if (buffer_reserve(buf, buf->len + slen) != 0)
		return -1;
	memcpy(buf->data + buf->len, s, slen);
	buf->len += slen;
	return 0;
}

int buffer_prepend_cstr(Buffer *buf, const char *s) {
	if (!buf || !s)
		return -1;
	size_t slen = strlen(s);
	if (buffer_reserve(buf, buf->len + slen) != 0)
		return -1;
	memmove(buf->data + slen, buf->data, buf->len);
	memcpy(buf->data, s, slen);
	buf->len += slen;
	return 0;
}
