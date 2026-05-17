#define NOB_IMPLEMENTATION
#include "nob.h"

#define CC "clang"
#define CFLAGS "-std=c99", "-g"
#define WARNFLAGS "-Wall", "-Wextra", "-Wno-format-security"
#define LDFLAGS "-ltcc"
#define BUILD_DIR "build/"
#define TARGET BUILD_DIR "ctime"
#define SRC_DIR "src/"
#define SOURCE_NAMES(PRE, POST) \
	PRE(POST("main")) \
	PRE(POST("buffer")) \
	PRE(POST("ctime")) \
	PRE(POST("parser")) \
	PRE(POST("lexer"))

#define PRE(X) SRC_DIR X
#define POST(X) X ".c" ,

#define SOURCES SOURCE_NAMES(PRE, POST)

/* char *sources_arr[] = {SOURCES}; */
/* int num_sources = sizeof(sources_arr)/sizeof(sources_arr[1]); */

int main(int argc, char **argv)
{
	NOB_GO_REBUILD_URSELF(argc, argv);
	if (nob_file_exists("./nob.old"))
		nob_delete_file("./nob.old");
	if (!nob_mkdir_if_not_exists(BUILD_DIR))
		return 1;
	Nob_Cmd cmd = {0};
	nob_cmd_append(&cmd, CC, CFLAGS, WARNFLAGS, "-o", TARGET, SOURCES/*,*/ LDFLAGS);
	if (!nob_cmd_run(&cmd)) return 1;
	return 0;
}

