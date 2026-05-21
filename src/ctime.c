/*
   Implementation of the transpiler
*/

#define CTIMEUTILS_IMPLEMENTATION
#include "ctime_utils.h"

#include "ctime.h"
#include <libtcc.h>
#include "buffer.h"
#include "lexer.h"
#include "parser.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <stdint.h>

CTime_Args *ctime_default_args() {
	CTime_Args *args = malloc(sizeof(*args));
	*args = (CTime_Args) {
		.in_stream = NULL,
		.out_stream = NULL,
		.transpile_n_layers = SIZE_MAX,
		.include_dirs = NULL,
		.num_includes = 0,
	};
	return args;
}

static char *transpiled_comptime_layer(code_blocks *code, TCCState **comptime_objs, size_t layer, size_t num_compiled_layers, bool with_delimiters) {
	Buffer *buf = buffer_new();
	source_block *code_arr = code->comptime_layers[layer];
	size_t code_len = code->num_blocks_in_comptime_layer[layer];
	int nth_insert = 0;
	if (with_delimiters) {
		char *pre = ctt_format("#{%zu\n", layer-num_compiled_layers);
		buffer_append_cstr(buf, pre);
		free(pre);
	}
	for (size_t i = 0; i < code_len; ++i) {
		source_block block = code_arr[i];
		if (block.type == B_SOURCE_CODE) {
			buffer_append_cstr(buf, block.data);
		} else if (block.type == B_INSERTION_ORIGIN_HOOK) {
			/* only when debugging a single layer should this be visible */
			if (!with_delimiters) {
				buffer_append_cstr(buf, block.data);
			}
		} else if (block.type == B_SOURCE_INSERT) {
			if (num_compiled_layers > block.compilation_layer) {
				/* execute the insertion */
				// todo: factor out __comptime_insert_
				char *fname = ctt_format("__comptime_insert_layer%zu_%d", layer, nth_insert++);
				char *(*insertion_string_fn)(void) = tcc_get_symbol(comptime_objs[block.compilation_layer], fname);
				char *insert = insertion_string_fn();
				if (!insert) {
					fprintf(stderr, "Error reported in %s from layer %zu called from comptime layer %zu\n", fname, block.compilation_layer, layer);
					return NULL;
				}
				buffer_append_cstr(buf, insert);
				if (!block.data)
					free(insert);

				free(fname);
			} else {
				/* the insertion isn't compiled, only decrement it */
				if (with_delimiters) {
					char *post = ctt_format("%zu}#\n", layer-num_compiled_layers);
					buffer_append_cstr(buf, post);
					free(post);
				}
				int decremented = block.compilation_layer - num_compiled_layers;
				char *insertion_code = ctt_format("$(%d %s %d)$", decremented, block.data, decremented);
				buffer_append_cstr(buf, insertion_code);
				free(insertion_code);
				if (with_delimiters) {
					char *pre = ctt_format("#{%zu\n", layer-num_compiled_layers);
					buffer_append_cstr(buf, pre);
					free(pre);
				}
			}
		}
	}
	if (with_delimiters) {
		char *post = ctt_format("%zu}#\n", layer-num_compiled_layers);
		buffer_append_cstr(buf, post);
		free(post);
	}
	char *s = buffer_to_cstr(buf);
	buffer_free(buf);
	return s;
}

static TCCState *compile_layer(char *to_compile, size_t layer, const char **include_dirs) {
	TCCState *s = tcc_new();
	if (!s) {
		fprintf(stderr, "Failed to create TCC state\n");
		return NULL;
	}
	// doesn't work
	// tcc_set_options(s, "-Wall -Werror=implicit-function-declaration");
	for (int i = 0; include_dirs[i]; ++i) {
		if (tcc_add_include_path(s, include_dirs[i])) {
			fprintf(stderr, "Failed to add include path\n");
			goto cleanup;
		}
	}
	tcc_define_symbol(s, "__COMPTIME_LAYER", ctt_format("%zu", layer));
	if (tcc_set_output_type(s, TCC_OUTPUT_MEMORY) < 0) {
		fprintf(stderr, "Failed to set output type\n");
		goto cleanup;
	}
	// Compile the comptime string
	if (tcc_compile_string(s, to_compile) < 0) {
		fprintf(stderr, "Compilation of comptime code failed at layer %zu\n", layer);
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

static int emit_code(source_block *code, size_t num_source_blocks, TCCState **comptime_objs, size_t num_compiled_layers, FILE *out) {
	int nth_insert = -1; // incremented to 0
	for (size_t i = 0; i < num_source_blocks; ++i) {
		if (code[i].type == B_SOURCE_INSERT) {
			++nth_insert;
			size_t layer = code[i].compilation_layer;
			if (layer < num_compiled_layers) {
				// todo: factor out __comptime_insert_
				char *fname = ctt_format("__comptime_insert_target_%d", nth_insert);
				char *(*insertion_string_fn)(void) = tcc_get_symbol(comptime_objs[layer], fname);
				char *insert = insertion_string_fn();
				if (!insert) {
					fprintf(stderr, "Error occured in %s from layer %zu called within the target code\n", fname, layer);
					return 1;
				}
				fprintf(out, "%s", insert);
				/* NULL => runtime string, non-NULL => static string */
				if (!code[i].data)
					free(insert);
				free(fname);
			} else {
				int decremented = layer - num_compiled_layers;
				char *insertion_code = ctt_format("$(%d %s %d)$", decremented, code[i].data, decremented);
				fprintf(out, "%s", insertion_code);
				free(insertion_code);
			}
		} else if (code[i].type == B_SOURCE_CODE) {
			fprintf(out, "%s", code[i].data);
		}
	}
	return 0;
}

int transpile_ct(CTime_Args *args) {
	Lexer *lex = lexer_new(args->in_stream);
	code_blocks code = parse_into_blocks(lex);

	TCCState **comptime_objs = calloc(code.num_comptime_layers, sizeof(*comptime_objs));

	/* compiling the comptime layers in sequence */
	const size_t layers_to_transpile = code.num_comptime_layers < args->transpile_n_layers ? code.num_comptime_layers : args->transpile_n_layers;
	for (size_t layer = 0; layer < layers_to_transpile; ++layer) {
		char *transpiled_source = transpiled_comptime_layer(&code, comptime_objs, layer, layer, false);
		if (!transpiled_source) return 1;
		if (*transpiled_source)
			fprintf(stderr, "Transpiling layer %zu\n", layer);
		else
			fprintf(stderr, "Layer %zu is empty\n", layer);
		comptime_objs[layer] = compile_layer(transpiled_source, layer, args->include_dirs);
		if (!comptime_objs[layer]) {
			/* compile_layer already prints error messages */
			/* fprintf(stderr, "Compilation failed at layer %zu\n", layer); */
			return 1;
		}
		free(transpiled_source);
	}

	/*  now that comptime is done, output the new code */
	const bool all_transpiled = code.num_comptime_layers+1 <= args->transpile_n_layers;
	const size_t uncompiled_comptime_layers = all_transpiled ? 0 : code.num_comptime_layers - args->transpile_n_layers;
	/* print only one layer of comptime */
	if (args->print_comptime) {
		if (uncompiled_comptime_layers == 0) {
			fprintf(stderr, "arg -d was used but all comptime layers were compiled\n");
			return 1;
		}
		char *partial_source = transpiled_comptime_layer(&code, comptime_objs, args->transpile_n_layers, args->transpile_n_layers, false);
		if (!partial_source) return 1;
		if (!*partial_source) {
			fprintf(stderr, "The comptime layer is empty\n");
		}
		fprintf(args->out_stream, "%s", partial_source);
		free(partial_source);
	}
	/* the normal path */
	else if (uncompiled_comptime_layers == 0) {
		fprintf(stderr, "Transpiling target C code\n");
		int r = emit_code(code.source_code, code.num_blocks_in_source, comptime_objs, code.num_comptime_layers, args->out_stream);
		if (r) return 1;
		fprintf(stderr, "Transpilation complete\n");
	} else {
		/* printing of incomplete transpilation (arg: '-N k') */
		/* TODO: a source map to reconstruct unconcatenated partial transpilations */
		for (size_t i = args->transpile_n_layers; i < code.num_comptime_layers; ++i) {
			// very unoptimal, but this repeating transpilation should only happen when debugging a partial transpilation, so it might not matter
			char *cur_layer = transpiled_comptime_layer(&code, comptime_objs, i, args->transpile_n_layers, true);
			if (!cur_layer) return 1;
			fprintf(args->out_stream, "%s", cur_layer);
			free(cur_layer);
		}
		int r = emit_code(code.source_code, code.num_blocks_in_source, comptime_objs, args->transpile_n_layers, args->out_stream);
		if (r) return 1;
	}

	/* for '-N 1', 1 layer is untranspiled, but all are compiled */
	int patch = uncompiled_comptime_layers == 0 ? 0 : 1;
	patch = 0;
	for (size_t layer = 0; layer < code.num_comptime_layers+patch - uncompiled_comptime_layers; ++layer) {
		tcc_delete(comptime_objs[layer]);
	}

	/* free(code.comptime_block); */
	/* for (size_t i = 0; i < code.num_source_blocks; ++i) { */
	/* 	if (code.source_blocks[i].type == B_SOURCE_CODE) { */
	/* 		free(code.source_blocks[i].data); */
	/* 	} */
	/* } */
	/* free(code.source_blocks); */
	lexer_free(lex);
	return 0;
}

