#ifndef CTIMET_H
#define CTIMET_H

#include <stdbool.h>
#include <stdio.h>

// sections: transpiler API, compile-time runtime, utility functions
// =================================================================================
// transpiler API (ctt_)

// args to the cc backend
typedef struct compiler_args {
	// NULL for libtcc in memory
	const char *cc;
	// null terminated arrays of strings
	const char **include_dirs;
	const char **lib_dirs;
	const char **lib_names;
	const char **defines;
	// arbitrary args (for non-null cc)
	const char **cc_args;
	// whether this header is to be imported
	bool is_freestanding;
} CompilerArgs;

// create a blank args instance
CompilerArgs *compiler_args_init();
// args does not own its strings
CompilerArgs *compiler_args_clone(const CompilerArgs *args);
void compiler_args_free(CompilerArgs *args);

void compiler_args_set_cc(CompilerArgs *args, const char *cc);
void compiler_args_add_include_dir(CompilerArgs *args, const char *include_dir);
void compiler_args_add_define(CompilerArgs *args, const char *define);
void compiler_args_add_lib_dir(CompilerArgs *args, const char *lib_dir);
void compiler_args_add_lib_name(CompilerArgs *args, const char *lib_name);
void compiler_args_add_shell_cc_arg(CompilerArgs *args, const char *cc_arg);

typedef struct dubugging_args {
	size_t transpile_n_layers; // default: SIZE_MAX
	unsigned tab_width; // default: 4
	bool print_ast; // default: false
	bool print_tokens; // default: false
} DebuggingArgs;

// create a default args instance, which corresponds to not debugging at all
DebuggingArgs *debugging_args_init();

// Transpiles a .ct into a .c, writing it to target_path (for CLI)
// if source or target are NULL, stdin/stdout are used, respectively
// args can be NULL, if so, source_path's dir is added as an include dir
// nonzero return on error
int ctt_transpile_ct(const char *source_path, const char *target_path, const CompilerArgs *c_args, const DebuggingArgs *d_args);

// Transpiles a cstring from memory into another cstring
// args can be NULL, but #include "" directives will need absolute paths
// Returns NULL if there was an error
char *ctt_transpile_str(const char *source_code, const CompilerArgs *args);

// =================================================================================
// transpiler runtime (ctt_)

// I might make a new type to return more information to the transpiler
// As far as I have in mind, NULL for error is sufficient
// But this API will always be valid

// the type returned by insertion macros
#define ctt_str char*

// Transpiles a .ct and returns it
// Returns NULL if there was an error
ctt_str ctt_include(const char *source_path, CompilerArgs *args);

// Managed memory arena to be freed by ctt_return (WIP)
#define ctt_alloc(ALLOC_SIZE) \
	malloc(ALLOC_SIZE)

// Returns a string for use in macros (currently just a normal char*)
#define ctt_return(STR) \
	return (ctt_str)STR

// Returns an error to transpiler and prints a given string to stderr
#define ctt_error(STR) \
	fprintf(stderr, "%s\n", STR); \
	return (ctt_str)NULL; \

// If an expression is false, returns an error to the transpiler
// and prints a given string to stderr
#define ctt_assert(BOOL, STR) \
	do { \
		if (!(BOOL)) { \
			fprintf(stderr, "assertion failed: %s\n", STR); \
			return (ctt_str)NULL; \
		} \
	} while (0)

// =================================================================================
// string utility functions (ct_)
// implemented in ctime_utils.c

#ifndef CTIME_PREFIX
#define CTIME_PREFIX ct_
#endif

// name mangling (ct_X is the default)
#define CT_CONCAT(a, b) a ## b
#define CT_CONCAT_EXPAND(a, b) CT_CONCAT(a, b)  // forces expansion of arguments
#define CT_NAME(name) CT_CONCAT_EXPAND(CTIME_PREFIX, name)

// returns a string that when printed, is identical to itself as source code minus the ""
char *CT_NAME(quote)(const char *s);

// returns a new formatted string
char *CT_NAME(format)(const char *fmt, ...);

// replaces substrings with a given string, and returns a new string
char *CT_NAME(substr_replace)(const char *s, const char *find, const char *replace);

// counts how many occurrences of a substring are present
int CT_NAME(substr_count)(const char *s, const char *substr);

#endif // ifndef CTIMET_H

