#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

typedef int uchar_t;

typedef struct str {
	uint8_t *buf;
	size_t num_chars, num_bytes;
} str;

str *str_new(const char *);

void str_delete(str *);

static inline size_t str_num_chars(const str *s) {
	return s->num_chars;
}

str *str_append(const str *, int ch);

str *str_concat(const str *, const str *);

bool str_next_char(const str *, uchar_t *ch, size_t *offset);