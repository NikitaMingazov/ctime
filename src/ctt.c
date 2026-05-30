/*
   Implementation of the transpiler
*/

#define CTIME_IMPLEMENTATION
#include "ctime.h"

#include "ctt.h"
#include "comptime-backend.h"
#include <libtcc.h>
#include "buffer.h"
#include "lexer.h"
#include "parser.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#ifdef __unix__
#define UNIX_SEGFAULT_GUARD
#define HAVE_SEGFAULT_GUARD
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#endif // ifdef __unix__

CTime_Args *ctime_default_args() {
	CTime_Args *args = malloc(sizeof(*args));
	*args = (CTime_Args) {
		.in_stream = NULL,
		.out_stream = NULL,
		.transpile_n_layers = SIZE_MAX,
		.compiler_args = malloc(sizeof(struct compiler_args)),
		.tab_width = 4,
		.print_ast = false,
	};
	args->compiler_args->defines = malloc(sizeof(char**));
	*args->compiler_args->defines = NULL;
	args->compiler_args->include_dirs = malloc(sizeof(char**));
	*args->compiler_args->include_dirs = NULL;
	args->compiler_args->lib_dirs = malloc(sizeof(char**));
	*args->compiler_args->lib_dirs = NULL;
	args->compiler_args->lib_names = malloc(sizeof(char**));
	*args->compiler_args->lib_names = NULL;
	args->compiler_args->cc_args = malloc(sizeof(char**));
	*args->compiler_args->cc_args = NULL;
	args->compiler_args->cc = NULL;
	return args;
}

#ifdef UNIX_SEGFAULT_GUARD
// forks a new process to call the comptime function from, to catch segfaults
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
		if (!comptime_str)
			fprintf(stderr, "Comptime fn returned NULL");
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

static char *evaluate_expr(Buffer *source_code, const char *expr, CompilerArgs *args) {
	const size_t code_len = source_code->len;
	#define eval_symbol_name "__COMPTIME_EVAL"
	char *eval_fn = ct_format("\nchar* "eval_symbol_name"() { return %s; }", expr);
	buffer_append_cstr(source_code, eval_fn);
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
// but due to this process being serial, there is no parallelism regardless
unsigned num_insertions;
size_t max_insertions;
bool terminated;

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
	char *expr = buffer_to_cstr(inner);
	buffer_free(inner); // TODO: destructive move cstr out method
	if (terminated)
		return NULL;
	if (++num_insertions > max_insertions) {
		terminated = true;
		buffer_append_cstr(comptime_code, "\nabout to resolve expr:\n $( ");
		buffer_append_cstr(comptime_code, expr);
		buffer_append_cstr(comptime_code, " )$");
		free(expr);
		return NULL;
	}
	char *insertion = evaluate_expr(comptime_code, expr, args);
	free(expr);
	if (!insertion) {
		fprintf(stderr, "error in insertion %d\n", num_insertions);
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

static char *resolve_quote_node(const CTimeNode *quote_node, Buffer *compilable_code, Buffer *all_seen_code, CompilerArgs *args) {
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
	buffer_null_terminate(inner);
	s = preprocessed_str(all_seen_code, inner->data, args);
	char *quoted = ct_format("\"%s\"", ct_quote(s));
	free(s);
	buffer_free(inner); // TODO: destructive move cstr out method
	return quoted;
}

static char *resolve_comptime_node(const CTimeNode *comptime, Buffer *comptime_code, CompilerArgs *args) {
	Buffer *inner = buffer_new();
	char *s;
	Buffer *tmp_concat;
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
				// macros do not need to be in a previous #{ }# block
				// but insertions still do
				tmp_concat = buffer_new();
				buffer_append_buffer(tmp_concat, comptime_code);
				buffer_append_buffer(tmp_concat, inner);
				s = resolve_quote_node(cur, comptime_code, tmp_concat, args);
				if (!s) return NULL;
				buffer_append_cstr(inner, s);
				buffer_free(tmp_concat);
				free(s);
				break;
			default: exit(1);
		}
	}
	char *result = buffer_to_cstr(inner);
	buffer_free(inner); // TODO: destructive move cstr out method
	return result;
}

int transpile_ct(CTime_Args *args) {
	Lexer *lex = lexer_new(args->in_stream, args->tab_width);
	CTimeNode *root = parse_into_tree(lex);

	if (args->print_ast) {
		ctime_print_tree(root);
		ctime_node_free(root);
		lexer_free(lex);
		return 0;
	}

	Buffer *comptime_code = buffer_new();
	Buffer *target_code = buffer_new();
	char *s;
	bool error = false;
	terminated = false;
	num_insertions = 0;
	max_insertions = args->transpile_n_layers;
	for (size_t i = 0; i < root->num_children; ++i) {
		const CTimeNode *cur = root->children[i];
		switch (cur->type) {
			case N_SOURCE_CODE:
				buffer_append_cstr(target_code, cur->code);
				break;
			case N_COMPTIME:
				s = resolve_comptime_node(cur, comptime_code, args->compiler_args);
				if (terminated)
					break;
				if (!s) {
					error = true;
					break;
				}
				buffer_append_cstr(comptime_code, s);
				free(s);
				break;
			case N_INSERT:
				s = resolve_insert_node(cur, comptime_code, args->compiler_args);
				if (terminated)
					break;
				if (!s) {
					error = true;
					break;
				}
				buffer_append_cstr(target_code, s);
				free(s);
				break;
			default: return 1;
		}
	}
	if (terminated) {
		buffer_null_terminate(comptime_code);
		fprintf(args->out_stream, "%s\n", comptime_code->data);
	} else if (!error) {
		buffer_null_terminate(target_code);
		fprintf(args->out_stream, "%s\n", target_code->data);
		if (args->transpile_n_layers != SIZE_MAX)
			fprintf(stderr, "\ntranspilation completed successfully ('-N' is no-op)\n");
	}

	buffer_free(comptime_code);
	buffer_free(target_code);

	ctime_node_free(root);
	lexer_free(lex);
	return 0;
}

