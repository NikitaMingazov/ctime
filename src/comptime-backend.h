// A polymorphic abstraction for the codegen backend,
//  to reduce coupling with TCC and allow a custom backend

#ifndef CTT_BACKEND_H
#define CTT_BACKEND_H

#include <libtcc.h>
#include <stddef.h>

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
} CompilerArgs;

enum backend_type {
	BACKEND_TCC_MEM,
	BACKEND_SHELL_CC,
};

struct ctt_tcc_backend {
	TCCState *s;
};

struct ctt_shell_backend {
	char *so_file;
	void *dl_handle;
};

union backend_data {
	struct ctt_tcc_backend tcc_mem;
	struct ctt_shell_backend shell_be;
};

typedef struct comptime_backend {
	enum backend_type type;
	union backend_data data;
} ComptimeBackend;

ComptimeBackend *comptime_compile(const char *source_code, CompilerArgs *args);
char *(*comptime_get_str_void_fn(ComptimeBackend *be, const char *sym))(void);
void comptime_close(ComptimeBackend *be);

char *comptime_preprocess_str(const char *to_preprocess, CompilerArgs *args);

#endif // CTT_BACKEND_H

