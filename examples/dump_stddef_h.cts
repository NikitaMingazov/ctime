#{ // script that prints to stderr the preprocessed #include <HEADER> for cc
// currently broken, WIP to restore
#include <ctime.h>
#ifndef HEADER
#define HEADER stddef.h
#endif
char *header_name = '{ HEADER }';
}#
#{
char *header_str = ctt_preprocess('{
	#include <$( header_name )$>
}', $$) ; // $$ is a pointer to the parsed args to ctt
#include <stdio.h>
char *main() {
	fprintf(stderr, header_str);
	fprintf(stderr, "\n");
	return "0";
}
}#
// also try with -cc gcc or -cc clang args
// or -D HEADER=stdlib.h
$( main() )$
