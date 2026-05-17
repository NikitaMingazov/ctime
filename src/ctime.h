#ifndef CCOMPTIME_H
#define CCOMPTIME_H

#include <stdbool.h>
#include <stdio.h>

// Transpiles a .ct/.ht into a .c/.h, writing it to out_file.
// If out_file is NULL, it will print to stdout.
int transpile_ct_by_filename(FILE *in_stream, FILE *out_stream, bool debug_comptime, const char **include_dirs);

#endif
