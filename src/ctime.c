/*
   Implementation of the transpiler
*/

// for $(  )$, the format is
// "char *__comptime_%d() { return %s; }\n", counter, expr

#define CTIMEUTILS_IMPLEMENTATION
#include "ctime_utils.h"

#include "ctime.h"
#include <libtcc.h>
#include "lexer.h"
#include "parser.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static TCCState *compile_str_in_memory(const char *to_compile, const char **include_dirs) {
	TCCState *s = tcc_new();
	if (!s) {
		fprintf(stderr, "Failed to create TCC state\n");
		return NULL;
	}
	for (int i = 0; include_dirs[i]; ++i) {
		if (tcc_add_include_path(s, include_dirs[i])) {
			fprintf(stderr, "Failed to add include path\n");
			goto cleanup;
		}
	}
	if (tcc_set_output_type(s, TCC_OUTPUT_MEMORY) < 0) {
		fprintf(stderr, "Failed to set output type\n");
		goto cleanup;
	}
	// Compile the comptime string
	if (tcc_compile_string(s, to_compile) < 0) {
		fprintf(stderr, "Compilation of comptime code failed\n");
		goto cleanup;
	}
	if (tcc_relocate(s) < 0) {
		fprintf(stderr, "Relocation failed\n");
		goto cleanup;
	}
	return s;
cleanup:
	tcc_delete(s);
	return NULL;
}

static void emit_code(code_blocks *code, TCCState *comptime_obj, FILE *out) {
	int nth_insert = 0;
	for (size_t i = 0; i < code->num_source_blocks; ++i) {
		source_block *cur = &code->source_blocks[i];
		if (cur->type == B_SOURCE_INSERT) {
			// todo: factor out __comptime_insert_
			char *fname = format("__comptime_insert_%d", nth_insert++);
			char *(*insertion_string_fn)(void) = tcc_get_symbol(comptime_obj, fname);
			free(fname);
			char *insert = insertion_string_fn();
			fprintf(out, "%s", insert);
			/* NULL => runtime string, non-NULL => static string */
			if (!cur->data)
				free(insert);
		} else if (cur->type == B_SOURCE_CODE) {
			fprintf(out, "%s", cur->data);
		}
	}
}

int transpile_ct_by_filename(FILE *in_stream, FILE *out_stream, bool debug_comptime, const char **include_dirs) {
	Lexer *lex = lexer_new(in_stream);
	code_blocks code = parse_into_blocks(lex);
	if (debug_comptime) {
		return fprintf(out_stream, "%s\n", code.comptime_block) < 0;
	}

	TCCState *comptime_obj = compile_str_in_memory(code.comptime_block, include_dirs);
	if (!comptime_obj) {
		fprintf(stderr, "Compilation failed\n");
		return 1;
	}
	emit_code(&code, comptime_obj, out_stream);
	tcc_delete(comptime_obj);

	free(code.comptime_block);
	for (size_t i = 0; i < code.num_source_blocks; ++i) {
		if (code.source_blocks[i].type == B_SOURCE_CODE) {
			free(code.source_blocks[i].data);
		}
	}
	free(code.source_blocks);
	lexer_free(lex);
	return 0;
}

