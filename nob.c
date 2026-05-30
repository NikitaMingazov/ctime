#include <stdio.h>
#include <string.h>
#define NOB_IMPLEMENTATION
#include "nob.h"

#define CC "clang"
#define CFLAGS "-std=c99", "-g"
#define WARNFLAGS "-Wall", "-Wextra", "-Wno-format-security", "-Wno-macro-redefined"

#ifdef __unix__
#define SYSTEM_HEADER_DIR "/usr/include/"
#define SYSTEM_LIB_DIR "/usr/lib/"
#define SYSTEM_BIN_DIR "/usr/bin/"
#endif // ifdef __unix__

#define LDFLAGS "-ltcc"
#define BUILD_DIR "build/"
#define SRC_DIR "src/"

#define BIN_TARGET BUILD_DIR "ctt"
#define BIN_SOURCE_NAMES(PRE, POST) \
	PRE(POST("main")) \
	PRE(POST("buffer")) \
	PRE(POST("ctt")) \
	PRE(POST("comptime-backend")) \
	PRE(POST("parser")) \
	PRE(POST("lexer"))

#define PRE(X) SRC_DIR X
#define POST(X) X ".c" ,

#define BIN_SOURCES BIN_SOURCE_NAMES(PRE, POST)

#define LIB_TARGET_DIR BUILD_DIR "libctime/"
// #define LIB_TARGET LIB_TARGET_DIR "libctime.so"
#define LIB_SOURCE_DIR SRC_DIR
#define LIB_NAME "ctime"
#define LIB_SOURCE_C LIB_SOURCE_DIR LIB_NAME ".c"
#define LIB_SOURCE_H LIB_SOURCE_DIR LIB_NAME ".h"
#define LIB_FLAGS "-fPIC", "-c"

#define LIB_TARGET_H LIB_TARGET_DIR LIB_NAME ".h"
#define LIB_TARGET_SO LIB_TARGET_DIR "lib" LIB_NAME ".so"

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
	// building ctt
	nob_cmd_append(&cmd, CC, CFLAGS, WARNFLAGS, "-o", BIN_TARGET, BIN_SOURCES/*,*/ LDFLAGS);
	if (!nob_cmd_run(&cmd)) return 1;
	// building libctime.so
	if (!nob_mkdir_if_not_exists(LIB_TARGET_DIR))
		return 1;
	nob_cmd_append(&cmd, CC, CFLAGS, LIB_FLAGS, WARNFLAGS, "-o", LIB_TARGET_DIR "libctime.o", LIB_SOURCE_C);
	if (!nob_cmd_run(&cmd)) return 1;
	nob_cmd_append(&cmd, CC, "-shared", "-o", LIB_TARGET_SO, LIB_TARGET_DIR "libctime.o");
	if (!nob_cmd_run(&cmd)) return 1;
	if (nob_file_exists(LIB_TARGET_DIR "libctime.o"))
		nob_delete_file(LIB_TARGET_DIR "libctime.o");
	// copy ctime.h to be adjacent to libctime.so
	if (!nob_copy_file(LIB_SOURCE_H, LIB_TARGET_H)) return 1;
	if (argc >= 2 && memcmp(argv[1], "install", strlen("install")+1) == 0) {
#ifndef __unix__
#error "nob install is only supported on unix systems"
#endif
		if (!nob_copy_file(BIN_TARGET, SYSTEM_BIN_DIR "ctt")) {
			fprintf(stderr, "Could not install ctt, are you sure this was run as root?\n");
			return 1;
		}
		if (!nob_copy_file(LIB_TARGET_H, SYSTEM_HEADER_DIR LIB_NAME".h")) {
			fprintf(stderr, "Could not install libctime, are you sure this was run as root?\n");
			return 1;
		}
		if (!nob_copy_file(LIB_TARGET_SO, SYSTEM_LIB_DIR LIB_NAME".so")) {
			fprintf(stderr, "Could not install libctime, are you sure this was run as root?\n");
			return 1;
		}
	}
	return 0;
}

