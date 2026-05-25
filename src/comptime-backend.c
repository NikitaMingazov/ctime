#define _DEFAULT_SOURCE
#include <stdlib.h>

#include "comptime-backend.h"
#include "ctime.h"
#include <libtcc.h>

#include "../nob.h"
#include <dlfcn.h>

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#ifdef __unix__
// build a .so in /tmp to call functions from
ComptimeBackend *comptime_shell_compile(const char *source_code, const char *cc, CompilerArgs *args, size_t layer) {
	#define APPEND_STRARR_TO_CMD(ARR, PREFIX) \
	for (size_t i = 0; ARR[i]; ++i) { \
		char *arg = ct_format(PREFIX"%s", ARR[i]); \
		nob_cmd_append(&cmd, arg); \
	}

	const char *tmp_dir = "/tmp/ctime";
	if (!nob_mkdir_if_not_exists(tmp_dir)) return NULL;

	char *tmp_src = malloc(256);
	*tmp_src = '\0';
	snprintf(tmp_src, 256, "%s/%d_%zu_XXXXXX.c", tmp_dir, getpid(), layer);
   	int fd = mkstemps(tmp_src, 2);
   	if (fd == -1) return NULL;
   	FILE *f = fopen(tmp_src, "w");
   	if (!f) { close(fd); return NULL; }
   	fprintf(f, "%s", source_code);
   	fclose(f);

	char *tmp_target_o = ct_format("%s.o", tmp_src);
	char *tmp_target_so = ct_format("%s.so", tmp_src);
	Nob_Cmd cmd = {0};

	nob_cmd_append(&cmd, cc, "-fPIC", "-c");
	APPEND_STRARR_TO_CMD(args->cc_args, "")
	APPEND_STRARR_TO_CMD(args->defines, "-D")
	APPEND_STRARR_TO_CMD(args->include_dirs, "-I")
	nob_cmd_append(&cmd, "-o", tmp_target_o, tmp_src);
	if (!nob_cmd_run(&cmd)) goto err;

	nob_cmd_append(&cmd, cc, "-shared");
	APPEND_STRARR_TO_CMD(args->lib_dirs, "-L")
	APPEND_STRARR_TO_CMD(args->lib_names, "-l")
	APPEND_STRARR_TO_CMD(args->lib_dirs, "-Wl,-rpath,");
	nob_cmd_append(&cmd, "-o", tmp_target_so, tmp_target_o);
	if (!nob_cmd_run(&cmd)) goto err;
	nob_delete_file(tmp_target_o);
	nob_delete_file(tmp_src);

	void *dl_handle = dlopen(tmp_target_so, RTLD_LAZY);

	ComptimeBackend *be = malloc(sizeof(*be));
	*be = (ComptimeBackend) {
		.data = (union backend_data) (struct ctt_shell_backend) { .so_file = tmp_target_so, .dl_handle = dl_handle},
		.type = BACKEND_SHELL_CC,
	};
	nob_cmd_free(cmd);
	free(tmp_target_o); free(tmp_src);
	return be;
err:
	if (nob_file_exists(tmp_target_o))
		nob_delete_file(tmp_target_o);
	if (nob_file_exists(tmp_target_so))
		nob_delete_file(tmp_target_so);
	if (nob_file_exists(tmp_src))
		nob_delete_file(tmp_src);
	nob_cmd_free(cmd);
	return NULL;
}
#else
ComptimeBackend *comptime_shell_compile(const char *source_code, const char *cc, CompilerArgs *args, size_t layer) {
	fprintf(stderr, "shell compilation is not supported");
	return NULL;
}
#endif // ifdef __unix__


ComptimeBackend *comptime_tcc_compile(const char *source_code, CompilerArgs *args, size_t layer) {
	const char **include_dirs = args->include_dirs;
	const char **lib_dirs = args->lib_dirs;
	const char **lib_names = args->lib_names;
	const char **defines = args->defines;
	TCCState *s = tcc_new();
	if (!s) {
		fprintf(stderr, "Failed to create TCC state\n");
		return NULL;
	}
	if (tcc_set_output_type(s, TCC_OUTPUT_MEMORY)) {
		fprintf(stderr, "Failed to set output type\n");
		goto cleanup;
	}
	// doesn't work
	// tcc_set_options(s, "-Wall -Werror=implicit-function-declaration");
	for (int i = 0; include_dirs[i]; ++i) {
		if (tcc_add_include_path(s, include_dirs[i])) {
			fprintf(stderr, "Failed to add include path %s\n", include_dirs[i]);
			goto cleanup;
		}
	}
	for (int i = 0; lib_dirs[i]; ++i) {
		if (tcc_add_library_path(s, lib_dirs[i])) {
			fprintf(stderr, "Failed to add library path %s\n", lib_dirs[i]);
			goto cleanup;
		}
	}
	for (int i = 0; lib_names[i]; ++i) {
		if (tcc_add_library(s, lib_names[i])) {
			fprintf(stderr, "Failed to resolve -l%s\n", lib_names[i]);
			goto cleanup;
		}
	}
	for (int i = 0; defines && defines[i]; ++i) {
		tcc_define_symbol(s, defines[i], NULL);
	}
	// Define this macro to indicate that code is in a comptime scope (and which layer)
	tcc_define_symbol(s, "__COMPTIME_LAYER", ct_format("%zu", layer));
	// Compile the comptime string
	if (tcc_compile_string(s, source_code)) {
		fprintf(stderr, "Compilation of comptime code failed at layer %zu\n", layer);
		goto cleanup;
	}
	if (tcc_relocate(s) < 0) {
		fprintf(stderr, "Relocation failed\n");
		goto cleanup;
	}
	ComptimeBackend *be = malloc(sizeof(*be));
	*be = (ComptimeBackend) {
		.data = (union backend_data) (struct ctt_tcc_backend) { .s = s },
		.type = BACKEND_TCC_MEM,
	};
	return be;
cleanup:
	tcc_delete(s);
	return NULL;
}

char *(*comptime_get_str_void_fn(ComptimeBackend *be, const char *sym))(void) {
	switch (be->type) {
		case BACKEND_TCC_MEM:
			return tcc_get_symbol(be->data.tcc_mem.s, sym);
		case BACKEND_SHELL_CC:
			return dlsym(be->data.shell_be.dl_handle, sym);
	}
}

void comptime_close(ComptimeBackend *be) {
	// release held resource
	switch (be->type) {
		case BACKEND_TCC_MEM:
			tcc_delete(be->data.tcc_mem.s);
			break;
		case BACKEND_SHELL_CC:
			if (nob_file_exists(be->data.shell_be.so_file))
				nob_delete_file(be->data.shell_be.so_file);
			dlclose(be->data.shell_be.dl_handle);
			break;
	}
	free(be);
}
