// CLI interface for the ctime transpiler

#include "ctime.h"
#include <stdio.h>
#include <stdbool.h>

static const char *msg =
" usage: %s <source.ct> [OPTIONS]\n"
" options:\n"
"   -o <out_path>   print transpilation to file (to stdout if empty)\n"
"   -d              print comptime code to the output and exit instead of compiling it\n"
"   -h              show this message\n"
;

struct args {
	char *in_file;
	char *out_file;
	bool print_comptime;
};

#define SOURCE_ARG \
	if (args.in_file) { \
		fprintf(stderr, "Only one source file is permitted"); \
		fprintf(stderr, msg, argv[0]); \
		return 1; \
	} \
	args.in_file = argv[i]; \
	continue;

#define INVALID \
	fprintf(stderr, "%s: invalid option -- %s\n", argv[0], argv[i]); \
	fprintf(stderr, msg, argv[0]); \
	return 1;

int main(int argc, char **argv) {
	struct args args = (struct args) {
		.in_file = NULL,
		.out_file = NULL,
		.print_comptime = false
	};
	bool optsdone = false;
	for (int i = 1; i < argc; ++i) {
		if (optsdone) { // after --
			SOURCE_ARG
		}
		if (argv[i][0] != '-') {
			SOURCE_ARG
		} else {
			if (!argv[i][1]) { // TODO: figure out what this one means
				fprintf(stderr, "empty option '-'\n");
				fprintf(stderr, msg, argv[0]);
				return 1;
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
						if (++i < argc) {
							args.out_file = argv[i];
						} else {
							fprintf(stderr, "missing -o parameter\n");
							fprintf(stderr, msg, argv[0]);
							return 1;
						}
					} else {
						INVALID
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
	if (!args.in_file) {
		fprintf(stderr, "missing source file\n");
		fprintf(stderr, msg, argv[0]);
		return 1;
	}
	return transpile_ct_by_filename(args.in_file, args.out_file, args.print_comptime);
}

