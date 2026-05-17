#ifndef CCOMPTIME_H
#define CCOMPTIME_H

#include <stdbool.h>

// Transpiles a .ct/.ht into a .c/.h, writing it to out_file.
// If out_file is NULL, it will print to stdout.
int transpile_ct_by_filename(const char *source_file, const char *out_file, bool debug_comptime);

#endif
