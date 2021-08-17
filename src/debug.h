#pragma once

#include <stdio.h>
#include <stdlib.h>

#define VERIFY(x) ((x) || verify_fail(#x,__FILE__,__LINE__))

static inline int verify_fail(const char *desc, const char *file, long line) {
	fprintf(stderr, "ERROR: (%s:%ld): verify() failed: '%s'\n", file, line, desc);
	fflush(stderr);
#ifdef DEBUG
	abort();
#endif
	return 0;
}
