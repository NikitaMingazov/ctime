// CLI interface for the ctime transpiler

// nob supposedly also has portable filesystem utilities
// but needing to define posix source is concerning, I may raise an issue
// TODO: make nob compile main.c with 'cc' because it is the only unportable C here
#define NOB_TEMP_CAPACITY 256
#define _POSIX_C_SOURCE 200809L
#define NOB_IMPLEMENTATION
#include "../nob.h"

#include <libgen.h> // basename (posix)

#include "ctt.h"
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* TODO: in addition to -d, have a -g that produces layer N as a compilable .c with original line origins and a build script with accompanying layerN_artifacts/layer_[0..N-1).{h,c}
   and the target program with .ct lines in the source? */

static const char *msg =
" usage: %s <in_file> [OPTIONS]\n"
" options:\n"
"   -o <out_file>   print transpilation to file.\n"
"                     (if absent, <_.ct> -> <_.c> is the default)\n"
"   -N <N>          limit how many comptime layers to compile\n"
"                     (those that remain are decremented in output)\n"
"   -d              print only the code about to be compiled\n"
"                     (requires -N)\n"
"   -I <dir>        also search <dir> for .h files\n"
"   -L <dir>        also search <dir> for .so files\n"
"   -l<name>        include lib<name>.so in linking\n"
"   -D <def>        pass #define <def> to the comptime preprocessor\n"
"                     ('-D primus=2' corresponds to #define primus 2)"
"   -h              show this message\n"
;

// for errors that imply the user had some idea of what they were doing
#define ERR(ERRMSG) \
	fprintf(stderr, "%s: err: %s\n", argv[0], ERRMSG); \
	status = 1; \
	goto err_cleanup;

// for errors that imply the user has no idea what they are doing
// (prints the help message)
#define PAR_ERR(ERRMSG) \
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

#define ARG_LIST_APPEND_FN(field, fname) \
	void fname (CTime_Args *args, const char *new_arg, size_t *num_args) { \
		++*num_args; \
		args-> field = realloc(args-> field, (*num_args+1) * sizeof(char*)); \
		args-> field [*num_args-1] = new_arg; \
		args-> field [*num_args] = NULL; \
	}

// generate functions for appending a string to a field in args
ARG_LIST_APPEND_FN(include_dirs, append_import)
ARG_LIST_APPEND_FN(defines, append_define)
ARG_LIST_APPEND_FN(lib_dirs, append_lib_dir)
ARG_LIST_APPEND_FN(lib_names, append_lib_name)

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

// add the -x case and do with the arg what is defined in DO(ARG)
#define CASE_CHAR_WITH_PARAMETER(C, C_AS_STR) \
	case C: \
		if (!argv[i][2]) { \
			if (++i < argc) { \
				DO(argv[i]); \
			} else { \
				PAR_ERR("missing -"C_AS_STR" parameter") \
			} \
		} else { \
			DO(&argv[i][2]); \
		} \
		break;

int main(int argc, char **argv) {
	int status = 0;
	size_t num_includes = 0;
	size_t num_lib_dirs = 0;
	size_t num_lib_names = 0;
	size_t num_defines = 0;
	CTime_Args *args = ctime_default_args();
	/* if ctt was invoked using a path, look for libctime in that dir */
	if (strcmp(argv[0], "ctt") != 0) {
		char *libdir = strdup(nob_temp_dir_name(argv[0]));
		libdir = realloc(libdir, strlen(libdir) + strlen("/libctime") + 1);
		strcat(libdir, "/libctime");
		append_import(args, libdir, &num_includes);
		append_lib_dir(args, libdir, &num_lib_dirs);
	}
	bool optsdone = false;
	char *source_path = NULL;
	for (int i = 1; i < argc; ++i) {
		if (optsdone) { // after --
			SOURCE_ARG
		}
		if (argv[i][0] != '-') {
			append_import(args, nob_temp_dir_name(argv[i]), &num_includes);
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
							PAR_ERR("extra -o arg")
						}
						if (++i < argc) {
							if (strlen(argv[i]) == 1 && argv[i][0] == '-') {
								args->out_stream = stdout;
							} else {
								args->out_stream = fopen(argv[i], "w");
								if (!args->out_stream) {
									ERR("Could not open target file for writing")
								}
							}
						} else {
							PAR_ERR("missing -o parameter")
						}
					} else {
						INVALID
					}
					break;
				#define DO(ARG) append_import(args, ARG, &num_includes)
				CASE_CHAR_WITH_PARAMETER('I', "I")
				#define DO(ARG) append_lib_dir(args, ARG, &num_lib_dirs)
				CASE_CHAR_WITH_PARAMETER('L', "L")
				#define DO(ARG) append_lib_name(args, ARG, &num_lib_names)
				CASE_CHAR_WITH_PARAMETER('l', "l")
				#define DO(ARG) append_define(args, ARG, &num_defines)
				CASE_CHAR_WITH_PARAMETER('D', "D")
				#define DO(ARG) args->transpile_n_layers = atoi(ARG)
				CASE_CHAR_WITH_PARAMETER('N', "N")
				case 'd':
					if (!argv[i][2]) {
						args->print_comptime = true;
					} else {
						INVALID
					}
					break;
				case '-': // --arg support would be here if I cared
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
		PAR_ERR("missing source file")
	}
	if (args->print_comptime && args->transpile_n_layers == SIZE_MAX) {
		PAR_ERR("-d requires -N to be set")
	}
	// set .ct to c and .ht to h as the default, otherwise stdout
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

