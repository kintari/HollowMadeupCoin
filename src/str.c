
#include "str.h"
#include "debug.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

//#define OFFSET(p,x) ((void *)(((uint8_t *) p) + (x)))

static str *str_alloc(size_t num_bytes) {
	str *s = malloc(sizeof(str));
	s->buf = calloc(num_bytes + 1, 1);
	s->num_bytes = num_bytes;
	s->num_chars = 0;
	return s;
}

str *str_new(const char *s) {
	size_t sz = s ? strlen(s) : 0;
	str *r = str_alloc(sz);
	if (s) {
		memcpy(r->buf, s, sz);
		r->num_chars = sz;
	}
	return r;
}

void str_delete(str *s) {
	free(s->buf);
	free(s);
}

str *str_concat(const str *p, const str *q) {
	size_t nb_p = p->num_bytes, nb_q = q->num_bytes;
	str *s = str_alloc(nb_p + nb_q);
	memcpy(s->buf, p, nb_p);
	memcpy(s->buf + nb_p, q, nb_q);
	s->num_chars = p->num_chars + q->num_chars;
	return s;
}

str *str_append(const str *p, uchar_t ch) {
	str *s = str_alloc(p->num_bytes + 1);
	memcpy(s->buf, p->buf, p->num_bytes);
	s->buf[p->num_bytes] = (uchar_t) ch;
	s->num_chars = p->num_chars + 1;
	return s;
}

bool str_next_char(const str *s, uchar_t *ch, size_t *offset) {
	if (*offset >= s->num_bytes) {
		return false;
	}
	else {
		*ch = s->buf[(*offset)++];
		return true;
	}
}