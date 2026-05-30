#ifndef CTIMET_H
#define CTIMET_H

#include "comptime-backend.h"

#include <stdbool.h>
#include <stdio.h>

typedef struct ctime_args {
	FILE *in_stream;
	FILE *out_stream;
	struct compiler_args *compiler_args;
	size_t transpile_n_layers;
	unsigned tab_width;
	bool print_ast;
} CTime_Args;

CTime_Args *ctime_default_args();

// Transpiles a .ct/.ht into a .c/.h, writing it to out_file.
// If out_file is NULL, it will print to stdout.
int transpile_ct(CTime_Args *args);

#endif

