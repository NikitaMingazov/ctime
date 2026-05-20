// CLI interface for the ctime transpiler

// nob supposedly also has portable filesystem utilities
// but needing to define posix source is concerning, I may raise an issue
#define NOB_TEMP_CAPACITY 256
#define _POSIX_C_SOURCE 200809L
#define NOB_IMPLEMENTATION
#include "../nob.h"

#include <libgen.h> // basename

#include "ctime.h"
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

static const char *msg =
" usage: %s <in_file> [OPTIONS]\n"
" options:\n"
"   -o <out_file>   print transpilation to file.\n"
"                     (if absent, <_.ct> -> <_.c> is the default)\n"
"   -I <dir>        search the directory in #include \"\" statements\n"
"   -N <N>          limit how many comptime layers to compile\n"
"                     (those that remain are decremented in output)\n"
"   -d              print only the layer about to be compiled\n"
"                     (requires -N)\n"
"   -h              show this message\n"
;

#define ERR(ERRMSG) \
	fprintf(stderr, "%s: err: %s\n", argv[0], ERRMSG); \
	fprintf(stderr, msg, argv[0]); \
	status = 1; \
	goto err_cleanup;

#define SOURCE_ARG \
	if (args->in_stream) { \
		ERR("Only one source file is permitted") \
	} \
	source_path = argv[i]; \
	args->in_stream = fopen(argv[i], "r"); \
	if (!args->in_stream) { \
		ERR("Could not open source file for reading"); \
	} \
	continue;

void append_import(CTime_Args *args, const char *import_dir) {
	args->num_includes++;
	args->include_dirs = realloc(args->include_dirs, (args->num_includes+1) * sizeof(char*));
	args->include_dirs[args->num_includes-1] = import_dir;
	args->include_dirs[args->num_includes] = NULL;
}

#define INVALID \
	fprintf(stderr, "%s: invalid option -- %s\n", argv[0], argv[i]); \
	fprintf(stderr, msg, argv[0]); \
	status = 1; \
	goto err_cleanup;

char *replace_extension(char *path, const char *extension) {
	char *new_ext = strdup(path);
	char *dot = strrchr(new_ext, '.');
	if (dot)
		strcpy(dot, extension);
	return new_ext;
}

int main(int argc, char **argv) {
	int status = 0;
	CTime_Args *args = ctime_default_args();
	bool optsdone = false;
	char *source_path = NULL;
	for (int i = 1; i < argc; ++i) {
		if (optsdone) { // after --
			SOURCE_ARG
		}
		if (argv[i][0] != '-') {
			append_import(args, nob_temp_dir_name(argv[i]));
			SOURCE_ARG
		} else {
			if (!argv[i][1]) { // '-'
				args->in_stream = stdin;
				continue;
			}
			switch (argv[i][1]) {
				case 'h':
					if (!argv[i][2]) {
						printf(msg, argv[0]);
						return 0;
					} else {
						INVALID
					}
					break;
				case 'o':
					if (!argv[i][2]) {
						if (args->out_stream) {
							ERR("extra -o arg")
						}
						if (++i < argc) {
							if (strlen(argv[i]) == 1 && argv[i][0] == '-') {
								args->out_stream = stdout;
							} else {
								args->out_stream = fopen(argv[i], "w");
								if (args->out_stream) {
									ERR("Could not open target file for writing")
								}
							}
						} else {
							ERR("missing -o parameter")
						}
					} else {
						INVALID
					}
					break;
				case 'I':
					if (!argv[i][2]) {
						if (++i < argc) {
							append_import(args, argv[i]);
						} else {
							ERR("missing -I parameter")
						}
					} else {
						append_import(args, &argv[i][2]);
					}
					break;
				case 'N':
					if (!argv[i][2]) {
						if (++i < argc) {
							args->transpile_n_layers = atoi(argv[i]);
						} else {
							ERR("missing -N parameter")
						}
					} else {
						args->transpile_n_layers = atoi(&argv[i][2]);
					}
					break;
				case 'd':
					if (!argv[i][2]) {
						args->print_comptime = true;
					} else {
						INVALID
					}
					break;
				case '-':
					if (!argv[i][2]) {
						optsdone = true;
						continue;
					} else {
						INVALID
					}
					break;
				default:
					INVALID
			}
		}
	}
	if (!args->in_stream) {
		ERR("missing source file")
	}
	if (args->print_comptime && args->transpile_n_layers == SIZE_MAX) {
		ERR("-d requires -N to be set")
	}
	// set .ct to c and .ht to h as default, otherwise stdout
	if (!args->out_stream) {
		if (source_path) {
			int len = strlen(source_path);
			if (len >= 3 && memcmp(source_path+len-3, ".ct", 3) == 0) {
				char *to_c = replace_extension(source_path, ".c");
				args->out_stream = fopen(to_c, "w");
				if (!args->out_stream) {
					fprintf(stderr, "Could not open %s for writing\n", to_c);
					status = 1;
					goto err_cleanup;
				}
			} else if (len >= 3 && memcmp(source_path+len-3, ".ht", 3) == 0) {
				char *to_h = replace_extension(source_path, ".h");
				args->out_stream = fopen(to_h, "w");
				if (!args->out_stream) {
					fprintf(stderr, "Could not open %s for writing\n", to_h);
					status = 1;
					goto err_cleanup;
				}
			} else {
				args->out_stream = stdout;
			}
		} else {
			args->out_stream = stdout;
		}
	}
	status = transpile_ct(args);
err_cleanup:
	if (args->in_stream != stdin && args->in_stream)
		fclose(args->in_stream);
	if (args->out_stream != stdout && args->out_stream)
		fclose(args->out_stream);
	return status;
}

