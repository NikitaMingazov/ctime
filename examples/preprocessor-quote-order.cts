#{
// also example of use as a scripting language.
#include <stdio.h>
#define pre "post"
const char *post = '{ pre }';
char *main() {
	fprintf(stderr, post);
	return "0";
}
}#
$( main() )$
