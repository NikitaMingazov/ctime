#ifndef CCOMPTIME_H
#define CCOMPTIME_H

#include <stdbool.h>
#include <stdio.h>

typedef struct ctime_args {
	FILE *in_stream;
	FILE *out_stream;
	const char **include_dirs;
	int num_includes;
	size_t transpile_n_layers;
	bool print_comptime;
} CTime_Args;

CTime_Args *ctime_default_args();

// Transpiles a .ct/.ht into a .c/.h, writing it to out_file.
// If out_file is NULL, it will print to stdout.
int transpile_ct(CTime_Args *args);

#endif
