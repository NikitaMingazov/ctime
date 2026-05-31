// CLI interface for the ctime transpiler

#define NOB_TEMP_CAPACITY 256
#define NOB_NO_ECHO
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

/* TODO: in addition to -d, have a -g that produces layer N as a compilable .c with original line origins instead of live insertions, a build script using ctt with accompanying layerN_artifacts/layer_[0..N-1).{h,c}
   and the target program with .ct lines in the source? */

static const char *msg =
" usage: %s <in_file> [OPTIONS]\n"
" options:\n"
"   -o <out_file>   print transpilation to file\n"
"                     (default: <in.ct> -> <in.c>, otherwise stdout)\n"
"   -N <N>          limit how many comptime insertions to perform\n"
"                     (the one about to be computed is emitted)\n"
"   -cc <cc>        use <cc> as the compiler backend instead of libtcc\n"
"   -a <arg>        pass an arbitrary argument into cc\n"
"   -I <dir>        also search <dir> for .h files\n"
"   -L <dir>        also search <dir> for .so files\n"
"   -l<name>        include lib<name>.so in linking\n"
"   -D <def>        pass #define <def> to the comptime preprocessor\n"
"                     ('-D primus=2' corresponds to #define primus 2)\n"
"   -A              print ctime AST to stdout and cancel transpilation\n"
"   -T              print ctime tokens to stderr\n"
"   -w <N>          change hard tab width for error reports (default:4)\n"
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
		args->compiler_args-> field = realloc(args->compiler_args-> field, (*num_args+1) * sizeof(char*)); \
		args->compiler_args-> field [*num_args-1] = new_arg; \
		args->compiler_args-> field [*num_args] = NULL; \
	}

// generate functions for appending a string to a field in args
ARG_LIST_APPEND_FN(include_dirs, append_import)
ARG_LIST_APPEND_FN(defines, append_define)
ARG_LIST_APPEND_FN(lib_dirs, append_lib_dir)
ARG_LIST_APPEND_FN(lib_names, append_lib_name)
ARG_LIST_APPEND_FN(cc_args, append_cc_arg)

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
	size_t num_cc_args = 0; // arbitrary arg
	CTime_Args *args = ctime_default_args();
	/* if ctt was invoked using a path, look for libctime in that dir */
	if (strcmp(argv[0], "ctt") != 0) {
		char *libdir = strdup(nob_temp_dir_name(argv[0]));
		libdir = realloc(libdir, strlen(libdir) + strlen("/libctime") + 1);
		strcat(libdir, "/libctime");
		append_lib_dir(args, libdir, &num_lib_dirs);
	}
	append_lib_name(args, "ctime", &num_lib_names);
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
				case 'A':
					if (!argv[i][2]) {
						args->print_ast = true;
					} else {
						INVALID
					}
					break;
				case 'T':
					if (!argv[i][2]) {
						args->print_tokens = true;
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
				case 'c': // -cc
					if (!argv[i][2]) {
						INVALID
					} else {
						if (argv[i][2] != 'c' || argv[i][3]) {
							INVALID
						}
						if (!argv[++i]) {
							PAR_ERR("missing -cc parameter")
						}
						args->compiler_args->cc = argv[i];
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
				#define DO(ARG) append_cc_arg(args, ARG, &num_cc_args)
				CASE_CHAR_WITH_PARAMETER('a', "a")
				// duplicate of these should be error?
				#define DO(ARG) args->transpile_n_layers = atoi(ARG)
				CASE_CHAR_WITH_PARAMETER('N', "N")
				#define DO(ARG) args->tab_width = atoi(ARG)
				CASE_CHAR_WITH_PARAMETER('w', "w")
				case '-':
					if (!argv[i][2]) {
						optsdone = true;
						continue;
					} else {
						// --arg support would be here if I cared
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
	// set .ct to c and .ht to h as the default, otherwise stdout
	if (!args->out_stream && !args->print_ast) {
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

