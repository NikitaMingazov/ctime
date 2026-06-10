#{ // script that prints to stderr the preprocessed #include <HEADER> for cc
// this hack is needed because using tcc as cc for .so causes crashes for dlopen
CompilerArgs *with_tcc() {
	CompilerArgs *args = compiler_args_clone(__CC_ARGS__);
	if (!args->cc) {
		args->cc = "tcc";
	}
	return args;
}
#include <libctt.h>
const char *header_macro = '{
#ifndef HEADER
#define HEADER stddef.h
#endif
}' ;
}#
#{
char *header_name() {
	return ctt_preprocess(header_macro, "HEADER", with_tcc());
}
}#
#{
char* header_str() {
	return ctt_preprocess("", '{
		#include <$( header_name() )$>
	}', with_tcc()) ; // $$ is a pointer to the parsed args to ctt
}
}#
#{
#include <stdio.h>
char *main() {
	fprintf(stderr, header_str());
	fprintf(stderr, "\n");
	return "0";
}
}#
// also try with -cc gcc or -cc clang args
// or -D HEADER=stdlib.h
$( main() )$
