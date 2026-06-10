// CLI interface for the ctime transpiler
#define _DEFAULT_SOURCE

#define NOB_TEMP_CAPACITY 256
#define NOB_NO_ECHO
#define _POSIX_C_SOURCE 200809L
#define NOB_IMPLEMENTATION
#include "../nob.h"

#if DYNAMIC_LIBCTT
#include <libctt.h>
#else
#include "libctt.h"
#endif

#if ONE_SOURCE
#include "libctt.c"
#endif

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

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
"   -F              do not import libctt into the comptime context\n"
"   -h              show this message\n"
;

// for errors that imply the user had some idea of what they were doing
#define ERR(ERRMSG) \
	fprintf(stderr, "%s: err: %s\n", argv[0], ERRMSG); \
	return 1;

// for errors that imply the user has no idea what they are doing
// (prints the help message)
#define PAR_ERR(ERRMSG) \
	fprintf(stderr, "%s: err: %s\n", argv[0], ERRMSG); \
	fprintf(stderr, msg, argv[0]); \
	return 1;

#define SOURCE_ARG \
	if (in_path || stdin_as_source) { \
		ERR("Only one source file is permitted") \
	} \
	compiler_args_add_include_dir(c_args, ct_strdup(nob_temp_dir_name(argv[i]))); \
	in_path = argv[i]; \
	continue;

#define INVALID \
	fprintf(stderr, "%s: invalid option -- %s\n", argv[0], argv[i]); \
	fprintf(stderr, msg, argv[0]); \
	return 1;

static char *replace_extension(const char *path, const char *extension) {
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

#define SINGLE_CHAR_FLAG(CHAR) \
	case CHAR: \
		if (!argv[i][2]) { \
			VOID_DO \
		} else { \
			INVALID \
		} \
		break;

int main(int argc, char **argv) {
	const char *in_path = NULL;
	const char *out_path = NULL;
	CompilerArgs *c_args = compiler_args_init();
	DebuggingArgs *d_args = debugging_args_init();
	bool optsdone = false;
	bool stdin_as_source = false;
	bool stdout_as_target = false;
	for (int i = 1; i < argc; ++i) {
		if (optsdone) { // after --
			SOURCE_ARG
		}
		if (argv[i][0] != '-') {
			SOURCE_ARG
		} else {
			if (!argv[i][1]) { // '-'
				stdin_as_source = true;
				continue;
			}
			switch (argv[i][1]) {
				#define VOID_DO printf(msg, argv[0]); return 0;
				SINGLE_CHAR_FLAG('h')
				#define VOID_DO d_args->print_ast = true;
				SINGLE_CHAR_FLAG('A')
				#define VOID_DO d_args->print_tokens = true;
				SINGLE_CHAR_FLAG('T')
				#define VOID_DO c_args->is_freestanding = true;
				SINGLE_CHAR_FLAG('F')
				case 'o':
					if (!argv[i][2]) {
						if (out_path) {
							PAR_ERR("extra -o arg")
						}
						if (++i < argc) {
							if (strlen(argv[i]) == 1 && argv[i][0] == '-') {
								stdout_as_target = true;
							} else {
								out_path = argv[i];
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
						c_args->cc = argv[i];
					}
					break;
				#define DO(ARG) compiler_args_add_include_dir(c_args, ARG)
				CASE_CHAR_WITH_PARAMETER('I', "I")
				#define DO(ARG) compiler_args_add_lib_dir(c_args, ARG)
				CASE_CHAR_WITH_PARAMETER('L', "L")
				#define DO(ARG) compiler_args_add_lib_name(c_args, ARG)
				CASE_CHAR_WITH_PARAMETER('l', "l")
				#define DO(ARG) compiler_args_add_define(c_args, ARG)
				CASE_CHAR_WITH_PARAMETER('D', "D")
				#define DO(ARG) compiler_args_add_shell_cc_arg(c_args, ARG)
				CASE_CHAR_WITH_PARAMETER('a', "a")
				// duplicate of these should be error?
				#define DO(ARG) d_args->transpile_n_layers = atoi(ARG)
				CASE_CHAR_WITH_PARAMETER('N', "N")
				#define DO(ARG) d_args->tab_width = atoi(ARG)
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
	if (!c_args->is_freestanding) {
		/* if ctt was called from a path, look for libctt in it */
		if (strcmp(argv[0], "ctt") != 0) {
			size_t mark = nob_temp_save();
			char *libdir = strdup(nob_temp_dir_name(argv[0]));
			libdir = realloc(libdir, strlen(libdir) + strlen("/libctt") + 1);
			strcat(libdir, "/libctt");
			compiler_args_add_lib_dir(c_args, libdir);
			free(libdir);
			nob_temp_rewind(mark);
		}
		// link comptime with libctt
		compiler_args_add_lib_name(c_args, "ctt");
	}
	if (!in_path && !stdin_as_source) {
		PAR_ERR("missing source file")
	}
	// set .ct to c as the default, otherwise stdout
	if (!stdout_as_target && !out_path && !d_args->print_ast) {
		if (!stdin_as_source) {
			int len = strlen(in_path);
			if (len >= 3 && memcmp(in_path+len-3, ".ct", 3) == 0) {
				char *to_c = replace_extension(in_path, ".c");
				out_path = to_c;
			}
		}
	}
	int status = ctt_transpile_ct(in_path, out_path, c_args, d_args);
	compiler_args_free(c_args);
	free(d_args);
	return status;
}

