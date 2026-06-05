#ifndef CTIMET_H
#define CTIMET_H

#include "comptime-backend.h"

#include <stdbool.h>
#include <stdio.h>

typedef struct ctime_args {
	// IO args (null for stdin/stdout)
	const char *in_path;
	const char *out_file;
	// args for transpiler backend
	struct compiler_args *compiler_args;
	// debugging args
	size_t transpile_n_layers;
	unsigned tab_width;
	bool print_ast;
	bool print_tokens;
} CTime_Args;

CTime_Args *ctime_default_args();

// Transpiles a .ct/.ht into a .c/.h, writing it to out_file.
// If out_file is NULL, it will print to stdout.
int transpile_ct(CTime_Args *args);

#endif

