// CLI interface for the ctime transpiler

// nob supposedly also has portable filesystem utilities
// but needing to define posix source is concerning, I may raise an issue
#define NOB_TEMP_CAPACITY 256
#define _POSIX_C_SOURCE 200809L
#define NOB_IMPLEMENTATION
#include "../nob.h"

#include "ctime.h"
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>

static const char *msg =
" usage: %s <source.ct> [OPTIONS]\n"
" options:\n"
"   -o <out_path>   print transpilation to file (to stdout if empty)\n"
"   -I <dir>        include the directory when resolving #include "" statements\n"
"   -d              print comptime code to the output and exit instead of compiling it\n"
"   -h              show this message\n"
;

struct args {
	FILE *in_stream;
	FILE *out_stream;
	const char **include_dirs;
	int num_includes;
	bool print_comptime;
};

#define SOURCE_ARG \
	if (args.in_stream) { \
		fprintf(stderr, "Only one source file is permitted"); \
		fprintf(stderr, msg, argv[0]); \
		return 1; \
	} \
	args.in_stream = fopen(argv[i], "r"); \
	if (!args.in_stream) { \
		fprintf(stderr, "Could not open source file for reading");	\
		return 1;													\
	}																\
	continue;

void append_import(struct args *args, const char *import_dir) {
	args->num_includes++;
	args->include_dirs = realloc(args->include_dirs, (args->num_includes+1) * sizeof(char*));
	args->include_dirs[args->num_includes-1] = import_dir;
	args->include_dirs[args->num_includes] = NULL;
}

#define INVALID \
	fprintf(stderr, "%s: invalid option -- %s\n", argv[0], argv[i]); \
	fprintf(stderr, msg, argv[0]); \
	return 1;

int main(int argc, char **argv) {
	struct args args = (struct args) {
		.in_stream = NULL,
		.out_stream = NULL,
		.print_comptime = false
	};
	bool optsdone = false;
	for (int i = 1; i < argc; ++i) {
		if (optsdone) { // after --
			SOURCE_ARG
		}
		if (argv[i][0] != '-') {
			append_import(&args, nob_temp_dir_name(argv[i]));
			SOURCE_ARG
		} else {
			if (!argv[i][1]) { // '-'
				args.in_stream = stdin;
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
						if (args.out_stream) {
							fprintf(stderr, "extra -o arg\n");
							fprintf(stderr, msg, argv[0]);
							return 1;
						}
						if (++i < argc) {
							args.out_stream = fopen(argv[i], "w");
							if (args.out_stream) {
								fprintf(stderr, "Could not open target file for writing\n");
								fprintf(stderr, msg, argv[0]);
								return 1;
							}
						} else {
							fprintf(stderr, "missing -o parameter\n");
							fprintf(stderr, msg, argv[0]);
							return 1;
						}
					} else {
						INVALID
					}
					break;
				case 'I':
					if (!argv[i][2]) {
						if (++i < argc) {
							append_import(&args, argv[i]);
						} else {
							fprintf(stderr, "missing -I parameter\n");
							fprintf(stderr, msg, argv[0]);
							return 1;
						}
					} else {
						append_import(&args, &argv[i][2]);
					}
					break;
				case 'd':
					if (!argv[i][2]) {
						args.print_comptime = true;
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
	if (!args.in_stream) {
		fprintf(stderr, "missing source file\n");
		fprintf(stderr, msg, argv[0]);
		return 1;
	}
	if (!args.out_stream)
		args.out_stream = stdout;
	int status = transpile_ct_by_filename(args.in_stream, args.out_stream, args.print_comptime, args.include_dirs);
	if (args.in_stream != stdin)
		fclose(args.in_stream);
	if (args.out_stream != stdout)
		fclose(args.out_stream);
	return status;
}

