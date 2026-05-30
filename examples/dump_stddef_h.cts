#{ // script that prints to stderr the preprocessed #include <HEADER> for cc
#ifndef HEADER
#define HEADER stddef.h
#endif
char *header_name = '{ HEADER }';
}#
#{
// quote blocks are preprocessed before conversion to strings
// and str insertion is before preprocessing
char *header_str = '{
	#include <$( header_name )$>
}' ;
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
