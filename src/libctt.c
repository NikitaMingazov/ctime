/*
   Implementation of the transpiler
*/
// for fmemopen //TODO: make a generic char iterator with an unget method
#ifdef __unix__
#define _POSIX_C_SOURCE 200809L
#endif

#define ARENA_IMPLEMENTATION
#include "arena.h"

#include <assert.h>
#include <libgen.h>

#include "libctt.h"
#include <libtcc.h>
#include "comptime-backend.h"
#include "buffer.h"
#include "lexer.h"
#include "parser.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#ifdef __unix__
#define UNIX_SEGFAULT_GUARD
#define HAVE_SEGFAULT_GUARD
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#endif // ifdef __unix__

// length of null-terminated array
static size_t arrlen(const char * const *arr) {
	size_t num_args = 0;
	for (size_t i = 0; arr[i]; ++i) {
		++num_args;
	}
	return num_args;
}

// converts a path to its parent dir
static void convert_dirname(char *path) {
    // Stolen from nob.h's theft of musl's implementation of dirname.
    if (!path || !*path) return;
    size_t i = strlen(path) - 1;
    for (; path[i] == '/'; i--) if (!i) return;
    for (; path[i] != '/'; i--) {
		if (!i) {
			path[0] = '.';
			path[1] = '\0';
			return;
		}
	}
    for (; path[i] == '/'; i--) {
		if (!i) {
			path[0] = '/';
			path[1] = '\0';
			return;
		}
	}
	path[i+1] = '\0';
}

CompilerArgs *compiler_args_init() {
	CompilerArgs *args = malloc(sizeof(*args));
	args->defines = malloc(sizeof(char**));
	*args->defines = NULL;
	args->include_dirs = malloc(sizeof(char**));
	*args->include_dirs = NULL;
	args->lib_dirs = malloc(sizeof(char**));
	*args->lib_dirs = NULL;
	args->lib_names = malloc(sizeof(char**));
	*args->lib_names = NULL;
	args->cc_args = malloc(sizeof(char**));
	*args->cc_args = NULL;
	args->cc = NULL;
	args->is_freestanding = false;
	return args;
}

static char *compiler_args_serialise(const CompilerArgs *args) {
    if (!args) return strdup("NULL");
	Buffer *buf = buffer_new();
	buffer_append_cstr(buf, "(CompilerArgs) {\n\t.cc = ");
	if (args->cc) {
		buffer_append_char(buf, '"');
		buffer_append_cstr(buf, args->cc);
		buffer_append_char(buf, '"');
	} else
		buffer_append_cstr(buf, "NULL");
	buffer_append_cstr(buf, ",\n");
    #define SERIALISE_STR_ARRAY(FIELD) do { \
		buffer_append_cstr(buf, "\t." #FIELD " = (const char*[]) { "); \
        for (size_t i = 0; args->FIELD[i]; i++) { \
			buffer_append_cstr(buf, "\""); \
            buffer_append_cstr(buf, args->FIELD[i]); \
			buffer_append_cstr(buf, "\", "); \
		} \
		buffer_append_cstr(buf, "NULL },\n"); \
    } while(0)
    SERIALISE_STR_ARRAY(include_dirs);
	SERIALISE_STR_ARRAY(lib_dirs);
	SERIALISE_STR_ARRAY(lib_names);
	SERIALISE_STR_ARRAY(defines);
	SERIALISE_STR_ARRAY(cc_args);
	#undef SERIALISE_STR_ARRAY
	buffer_append_cstr(buf, "\t.is_freestanding = ");
	if (args->is_freestanding)
		buffer_append_cstr(buf, "true,\n");
	else
		buffer_append_cstr(buf, "false,\n");
	buffer_append_cstr(buf, "};\n");
	return buffer_to_cstr_move(buf);
}

CompilerArgs *compiler_args_clone(const CompilerArgs *args_other) {
	CompilerArgs *cloned = malloc(sizeof(*cloned));
	if (args_other->cc)
		cloned->cc = strdup(args_other->cc);
	else
	 	cloned->cc = NULL;
	cloned->is_freestanding = args_other->is_freestanding;
	#define CLONE_FIELD(field) \
		cloned->field = calloc(arrlen((const char * const *)args_other->field)+1, sizeof(char*)); \
		/* calloc 0-inits, so it is already null-terminated */ \
		for (size_t i = 0; i < arrlen((const char * const *)args_other->field); ++i) { \
			cloned->field[i] = strdup(args_other->field[i]); \
		}
    CLONE_FIELD(defines)
    CLONE_FIELD(include_dirs)
	CLONE_FIELD(lib_dirs)
	CLONE_FIELD(lib_names)
	CLONE_FIELD(cc_args)
	#undef CLONE_FIELD
	return cloned;
}

void compiler_args_free(CompilerArgs *c) {
	free(c->defines);
	free(c->include_dirs);
	free(c->lib_dirs);
	free(c->lib_names);
	free(c->cc_args);
	free(c);
}

char *strdup_impl(const char *s) {
	size_t len = strlen(s);
	char *s_dup = malloc(len+1);
	memcpy(s_dup, s, len+1);
	return s_dup;
}

void compiler_args_set_cc(CompilerArgs *args, const char *cc) {
	args->cc = strdup(cc);
}
#define ARG_LIST_APPEND_FN(field, fname) \
void fname(CompilerArgs *args, const char *new_arg) { \
	size_t num_args = 0; \
	for (size_t i = 0; args->field[i]; ++i) { \
		++num_args; \
	} \
	args->field = realloc(args->field, (num_args+2)*sizeof(*args->field)); \
	args->field[num_args] = strdup_impl(new_arg); \
	args->field[num_args+1] = NULL; \
}
ARG_LIST_APPEND_FN(defines, compiler_args_add_define)
ARG_LIST_APPEND_FN(include_dirs, compiler_args_add_include_dir)
ARG_LIST_APPEND_FN(lib_dirs, compiler_args_add_lib_dir)
ARG_LIST_APPEND_FN(lib_names, compiler_args_add_lib_name)
ARG_LIST_APPEND_FN(cc_args, compiler_args_add_shell_cc_arg)
#undef ARG_LIST_APPEND_FN

DebuggingArgs *debugging_args_init() {
	DebuggingArgs *args = malloc(sizeof(*args));
	*args = (DebuggingArgs) {
		.transpile_n_layers = SIZE_MAX,
		.tab_width = 4,
		.print_ast = false,
		.print_tokens = false,
	};
	return args;
}

#ifdef UNIX_SEGFAULT_GUARD
// forks a new process to call the comptime function from, to catch segfaults
// as a side effect, it also cleans up memory
static char* segfault_safe_call(char *(*unsafe_fn)(void)) {
	int pipefd[2];
	if (pipe(pipefd) == -1) {
		perror("pipe");
		return NULL;
	}
	pid_t pid = fork();
	if (pid == -1) {
		perror("fork");
		close(pipefd[0]);
		close(pipefd[1]);
		return NULL;
	}
	if (pid == 0) { // Child process
		close(pipefd[0]);  // close read end
		char *comptime_str = unsafe_fn();   // may segfault
		if (!comptime_str) {
			fprintf(stderr, "Comptime fn returned NULL\n");
			write(pipefd[1], NULL, sizeof(NULL));
			_exit(0); // 0 result means successful operation in transpiler space
		}
		size_t len = strlen(comptime_str) + 1;
		if (write(pipefd[1], &len, sizeof(len)) != sizeof(len)) {
			_exit(1);
		}
		if (write(pipefd[1], comptime_str, len) != (ssize_t)len) {
			_exit(1);
		}
		close(pipefd[1]);
		_exit(0);  // success
	} else { // Parent process
		close(pipefd[1]);  // close write end
		int status;
		waitpid(pid, &status, 0);
		char *result = NULL;
		if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
			// Child exited normally → read the string
			size_t len;
			if (read(pipefd[0], &len, sizeof(len)) == sizeof(len)) {
				result = malloc(len);
				if (result) {
					if (read(pipefd[0], result, len) != (ssize_t)len) {
						free(result);
						result = NULL;
					}
				}
			}
		} else if (WIFSIGNALED(status) && WTERMSIG(status) == SIGSEGV) {
			fprintf(stderr, "Segfault in comptime function\n");
		} else if (WIFEXITED(status)) {
			// Other failure (e.g., comptime code called _exit(1))
			fprintf(stderr, "Comptime function exited with status %d\n", WEXITSTATUS(status));
		} else if (WIFSIGNALED(status)) {
			fprintf(stderr, "Comptime function killed by signal %d\n", WTERMSIG(status));
		}
		close(pipefd[0]);
		return result;  // may be NULL
	}
}
#endif // ifdef UNIX_SEGFAULT_GUARD

// only works for char* expressions because I don't have a real frontend
static char *evaluate_expr(Buffer *source_code, const char *expr, CompilerArgs *args) {
	const size_t code_len = source_code->len;
	#define eval_symbol_name "__COMPTIME_EVAL"
	char *eval_fn = ct_format("\nchar* "eval_symbol_name"() { return %s; }", expr);
	buffer_append_cstr(source_code, eval_fn);
	free(eval_fn);
	buffer_null_terminate(source_code);
	ComptimeBackend *be = comptime_compile(source_code->data, args);
	if (!be) {
		// fprintf(stderr, "compilation error\n");
		return NULL;
	}
	char *(*void_to_str)(void) = comptime_get_str_void_fn(be, eval_symbol_name);
#ifdef HAVE_SEGFAULT_GUARD
	char *result = segfault_safe_call(void_to_str);
#else
	char *result = void_to_str();
#endif
	comptime_close(be);
	source_code->len = code_len;
	return result;
}

// globals that would otherwise be passed in a closure struct
// due to transpilation having serial dependencies, there can be no parallelism regardless
unsigned num_insertions;
size_t max_insertions;
bool terminated;
// TODO: make the parser an iterator to print raw source post termination

static char *resolve_insert_node(const CTimeNode *insert, Buffer *comptime_code, CompilerArgs *args) {
	Buffer *inner = buffer_new();
	char *s;
	for (size_t i = 0; i < insert->num_children; ++i) {
		const CTimeNode *cur = insert->children[i];
		switch (cur->type) {
			case N_SOURCE_CODE:
				buffer_append_cstr(inner, cur->code);
				break;
			case N_INSERT:
				s = resolve_insert_node(cur, comptime_code, args);
				if (!s) return NULL;
				buffer_append_cstr(inner, s);
				free(s);
				break;
			default: return NULL;
		}
	}
	char *expr = buffer_to_cstr_move(inner);
	if (terminated)
		return NULL;
	if (++num_insertions > max_insertions) {
		terminated = true;
		buffer_append_cstr(comptime_code, "\n// about to resolve expr:\n// $( ");
		buffer_append_cstr(comptime_code, expr);
		buffer_append_cstr(comptime_code, " )$");
		free(expr);
		return NULL;
	}
	char *insertion = evaluate_expr(comptime_code, expr, args);
	free(expr);
	if (!insertion) {
		fprintf(stderr, "comptime terminated in insertion %d starting at at %d:%d\n", num_insertions, insert->row, insert->col);
	}
	return insertion;
}

static char *preprocessed_str(Buffer *comptime_code, const char *str, CompilerArgs *args) {
	const size_t code_len = comptime_code->len;
	const char *sentinel = "__123456789COMPTIME\n";
	buffer_append_cstr(comptime_code, sentinel);
	buffer_append_cstr(comptime_code, str);
	buffer_append_char(comptime_code, '\0');
	char *preprocessed_code = comptime_preprocess_str(comptime_code->data, args);
	char *start = strstr(preprocessed_code, sentinel) + strlen(sentinel);
	size_t len = strlen(start);
	// strip trailing newline inserted by preprocessor
	if (len > 0 && start[len-1] == '\n') {
		start[len-1] = '\0';
		--len;
	}
	char *preprocessed_slice = malloc(len+1);
	memcpy(preprocessed_slice, start, len);
	preprocessed_slice[len] = '\0';
	free(preprocessed_code);
	comptime_code->len = code_len;
	return preprocessed_slice;
}

static char *resolve_quote_node(const CTimeNode *quote_node, Buffer *compilable_code, CompilerArgs *args) {
	Buffer *inner = buffer_new();
	char *s;
	for (size_t i = 0; i < quote_node->num_children; ++i) {
		const CTimeNode *cur = quote_node->children[i];
		switch (cur->type) {
			case N_SOURCE_CODE:
				buffer_append_cstr(inner, cur->code);
				break;
			case N_INSERT:
				s = resolve_insert_node(cur, compilable_code, args);
				if (!s) return NULL;
				buffer_append_cstr(inner, s);
				free(s);
				break;
			default: exit(1);
		}
	}
	// wrap in quotes to produce a strlit of the quote
	s = buffer_to_cstr_move(inner);
	char *quoted = ct_quote(s);
	char *formatted = ct_format("\"%s\"", quoted);
	free(quoted);
	free(s);
	return formatted;
}

static char *resolve_comptime_node(const CTimeNode *comptime, Buffer *comptime_code, CompilerArgs *args) {
	Buffer *inner = buffer_new();
	char *s;
	for (size_t i = 0; i < comptime->num_children; ++i) {
		const CTimeNode *cur = comptime->children[i];
		switch (cur->type) {
			case N_SOURCE_CODE:
				buffer_append_cstr(inner, cur->code);
				break;
			case N_INSERT:
				s = resolve_insert_node(cur, comptime_code, args);
				if (!s) return NULL;
				buffer_append_cstr(inner, s);
				free(s);
				break;
			case N_QUOTE:
				s = resolve_quote_node(cur, comptime_code, args);
				if (!s) return NULL;
				buffer_append_cstr(inner, s);
				free(s);
				break;
			default: exit(1);
		}
	}
	char *result = buffer_to_cstr_move(inner);
	return result;
}

// buffers are in/out args
// but only in content should be the args header in comptime
static bool transpile_ast_into_buffers(Buffer *target_code, Buffer *comptime_code, CTimeNode *root, CompilerArgs *c_args_nonnull, DebuggingArgs *d_args_nonnull) {
	char *s;
	bool error = false;
	terminated = false;
	num_insertions = 0;
	max_insertions = d_args_nonnull->transpile_n_layers;
	for (size_t i = 0; i < root->num_children; ++i) {
		const CTimeNode *cur = root->children[i];
		switch (cur->type) {
			case N_SOURCE_CODE:
				buffer_append_cstr(target_code, cur->code);
				break;
			case N_COMPTIME:
				s = resolve_comptime_node(cur, comptime_code, c_args_nonnull);
				if (terminated)
					goto done;
				if (!s) {
					error = true;
					goto done;
				}
				buffer_append_cstr(comptime_code, s);
				free(s);
				break;
			case N_INSERT:
				s = resolve_insert_node(cur, comptime_code, c_args_nonnull);
				if (terminated)
					goto done;
				if (!s) {
					error = true;
					goto done;
				}
				buffer_append_cstr(target_code, s);
				free(s);
				break;
			default: abort();
		}
	}
done:
	return error;
}

int ctt_transpile_ct(const char *source_path, const char *target_path, const CompilerArgs *c_args, const DebuggingArgs *d_args) {
	int status = 0;
	Arena lexer_arena = {0};
	DebuggingArgs *d_args_nonnull;
	if (d_args) {
		d_args_nonnull = malloc(sizeof(*d_args_nonnull));
		*d_args_nonnull = *d_args;
	} else
		d_args_nonnull = debugging_args_init();
	CompilerArgs *c_args_nonnull;
	if (c_args)
		c_args_nonnull = compiler_args_clone(c_args);
	else
		c_args_nonnull = compiler_args_init();

	// the implicit -I/dir/of/source_path is not an arg, inherit args before it exists
	Buffer *comptime_code = buffer_new();
	if (!c_args->is_freestanding) {
		buffer_append_cstr(comptime_code, "#include <libctt.h>\n");
		buffer_append_cstr(comptime_code, "const CompilerArgs __COMPTIME_ARGS_ = ");
		char *serialised = compiler_args_serialise(c_args_nonnull);
		buffer_append_cstr(comptime_code, serialised);
		buffer_append_cstr(comptime_code, "#define __CC_ARGS__ &__COMPTIME_ARGS_\n");
		free(serialised);
	}

	FILE *in_stream = stdin;
	if (source_path) {
		in_stream = fopen(source_path, "r");
		if (!in_stream) {
			fprintf(stderr, "Could not open %s for reading\n", source_path);
			status = 1;
			goto defer_comptime;
		}
		// add source dir to #include "" directives
		char *dir_of_source = strdup_impl(source_path);
		convert_dirname(dir_of_source);
		compiler_args_add_include_dir(c_args_nonnull, dir_of_source);
		free(dir_of_source);
	}

	Lexer *lex = lexer_new(in_stream, d_args->tab_width, d_args->print_tokens, &lexer_arena);
	CTimeNode *root = parse_into_tree(lex);

	if (d_args_nonnull->print_ast) {
		ctime_print_tree(root);
		status = 1;
		goto defer_ast;
	}

	Buffer *target_code = buffer_new();

	bool error_cancel = transpile_ast_into_buffers(target_code, comptime_code, root, c_args_nonnull, d_args_nonnull);

	if (error_cancel) {
		status = 1;
		goto defer_target_buffer;
	}
	FILE *out_stream = stdout;
	if (target_path) {
		out_stream = fopen(target_path, "w");
		if (!out_stream) {
			fprintf(stderr, "Could not open %s for writing\n", target_path);
			status = 1;
			goto defer_target_buffer;
		}
	}
	if (terminated) { // global variable for non-error early return
		buffer_null_terminate(comptime_code);
		fprintf(out_stream, "%s\n", comptime_code->data);
	} else {
		buffer_null_terminate(target_code);
		fprintf(out_stream, "%s\n", target_code->data);
		if (d_args_nonnull->transpile_n_layers != SIZE_MAX)
			fprintf(stderr, "\narg '-N (>=%d)' is ignored due to being complete\n", num_insertions);
	}

	if (out_stream && out_stream != stdout)
		fclose(out_stream);
defer_target_buffer:
	buffer_free(target_code);
defer_ast:
	if (in_stream && in_stream != stdin)
		fclose(in_stream);
	ctime_node_free(root);
	lexer_free(lex);
defer_comptime:
	buffer_free(comptime_code);
	compiler_args_free(c_args_nonnull);
	free(d_args_nonnull);
	arena_free(&lexer_arena);
	return status;
}

char *ctt_transpile_str(const char *source_code, const CompilerArgs *args) {
	Arena lexer_arena = {0};
	CompilerArgs *c_args_nonnull;
	if (args)
		c_args_nonnull = compiler_args_clone(args);
	else
		c_args_nonnull = compiler_args_init();
	FILE* in_stream = fmemopen((void*)source_code, strlen(source_code), "r");
	DebuggingArgs *d_args = debugging_args_init();


	Lexer *lex = lexer_new(in_stream, d_args->tab_width, d_args->print_tokens, &lexer_arena);
	CTimeNode *root = parse_into_tree(lex);
	fclose(in_stream);

	Buffer *comptime_code = buffer_new();
	Buffer *target_code = buffer_new();

	bool error_cancel = transpile_ast_into_buffers(target_code, comptime_code, root, c_args_nonnull, d_args);

	lexer_free(lex);
	ctime_node_free(root);
	compiler_args_free(c_args_nonnull);
	free(d_args);

	buffer_free(comptime_code);
	if (error_cancel) {
		buffer_free(target_code);
		return NULL;
	}
	char *transpiled = buffer_to_cstr_move(target_code);
	arena_free(&lexer_arena);
	return transpiled;
}

static char *read_file(const char *path) {
	FILE *f = fopen(path, "rb");
	if (!f) return NULL;

	fseek(f, 0, SEEK_END);
	long size = ftell(f);
	rewind(f);

	char *buf = malloc(size + 1);
	if (!buf) { fclose(f); return NULL; }

	fread(buf, 1, size, f);
	buf[size] = '\0';
	fclose(f);
	return buf;
}

ctt_str ctt_include(const char *filename, CompilerArgs *args) {
	char *source_code = NULL;
	for (size_t i = 0; args->include_dirs[i]; ++i) {
		char *source_path = malloc(strlen(args->include_dirs[i])+1+strlen(filename)+1);
		*source_path = '\0';
		strcat(source_path, args->include_dirs[i]);
		strcat(source_path, "/");
		strcat(source_path, filename);
		if (access(source_path, F_OK) == 0) {
			source_code = read_file(source_path);
			free(source_path);
			break;
		}
		free(source_path);
	}
	if (!source_code) {
		ctt_error(ct_format("Could not find/read %s", filename));
	}
	char *transpiled = ctt_transpile_str(source_code, args);
	free(source_code);
	if (transpiled) {
		ctt_return(transpiled);
	} else {
		ctt_error(ct_format("Transpilation of ctt_include \"%s\" failed", filename));
	}
}

char *CT_NAME(strdup)(const char *s) {
	const size_t len = strlen(s);
	char *new_s = malloc(len+1);
	memcpy(new_s, s, len+1);
	return new_s;
}

