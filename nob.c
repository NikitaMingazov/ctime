/*
   options: local, static, install
   local uses "" in the compilation unit for linking instead of <>
   static produces a .a
   install puts ctt and libctt in /usr/bin, /usr/lib and /usr/include
*/

#include <stdio.h>
#include <stdlib.h>
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

#define LDFLAGS "-lctt", "-ltcc"
#define BUILD_DIR "build/"
#define SRC_DIR "src/"

#define PRE_C_SRC(X) SRC_DIR X
#define POST_C_SRC(X) X ".c" ,

#define BIN_TARGET BUILD_DIR "ctt"
#define BIN_SRC SRC_DIR "ctt.c"

#define LIB_TARGET_DIR BUILD_DIR "libctt/"
#define LIB_SOURCE_DIR SRC_DIR
#define LIB_SOURCE_H LIB_SOURCE_DIR "libctt.h"
#define LIB_SOURCE_C LIB_SOURCE_DIR "libctt.c"
#define LIB_SOURCE_NAMES(PRE, POST) \
	PRE(POST("libctt")) \
	PRE(POST("buffer")) \
	PRE(POST("comptime-backend")) \
	PRE(POST("parser")) \
	PRE(POST("lexer")) \
	PRE(POST("ctime_utils"))
#define LIB_C_SOURCES LIB_SOURCE_NAMES(PRE_C_SRC, POST_C_SRC)
const char *lib_c_sources[] = { LIB_C_SOURCES NULL };
#define PRE_O_TARGET(X) BUILD_DIR X
#define POST_O_TARGET(X) X ".o" ,
#define LIB_O_TARGETS LIB_SOURCE_NAMES(PRE_O_TARGET, POST_O_TARGET)
const char *lib_o_targets[] = { LIB_O_TARGETS NULL };
#define LIB_FLAGS "-fPIC", "-c"

#define LIB_NAME "ctt"
#define LIB_TARGET_H LIB_TARGET_DIR "lib" LIB_NAME ".h"
#define LIB_TARGET_SO LIB_TARGET_DIR "lib" LIB_NAME ".so"
#define LIB_TARGET_A LIB_TARGET_DIR "lib" LIB_NAME ".a"


int main(int argc, char **argv)
{
	bool local = true; // TODO: figure out build system API
	bool install = false;
	bool make_static_libctt = false;
	bool clean = false;
	for (size_t i = 1; argv[i]; ++i) {
		if (strcmp(argv[i], "static") == 0)
			make_static_libctt = true;
		else if (strcmp(argv[i], "install") == 0)
			install = true;
		else if (strcmp(argv[i], "local") == 0)
			local = true;
		else if (strcmp(argv[i], "clean") == 0)
			clean = true;
	}
	NOB_GO_REBUILD_URSELF(argc, argv);
	if (nob_file_exists("./nob.old"))
		nob_delete_file("./nob.old");
	if (!nob_mkdir_if_not_exists(BUILD_DIR))
		return 1;
	Nob_Cmd cmd = {0};
	// building libctt
	if (!nob_mkdir_if_not_exists(LIB_TARGET_DIR))
		return 1;
	// make the .o files
	for (size_t i = 0; lib_c_sources[i]; ++i) {
		nob_cmd_append(&cmd, CC, CFLAGS, LIB_FLAGS, WARNFLAGS, "-o", lib_o_targets[i], lib_c_sources[i]);
		if (!nob_cmd_run(&cmd)) return 1;
	}
	if (make_static_libctt) {
		nob_cmd_append(&cmd, "ar", "rcs", LIB_TARGET_A, LIB_O_TARGETS);
		if (!nob_cmd_run(&cmd)) return 1;
	}
	nob_cmd_append(&cmd, CC, "-shared", "-o", LIB_TARGET_SO, LIB_O_TARGETS);
	if (!nob_cmd_run(&cmd)) return 1;
	// copy libctt.h to be adjacent to libctt.so
	if (!nob_copy_file(LIB_SOURCE_H, LIB_TARGET_H)) return 1;
	// installing to /usr
	if (install) {
		if (!nob_copy_file(LIB_TARGET_H, SYSTEM_HEADER_DIR "lib"LIB_NAME".h")) {
			fprintf(stderr, "Could not install libctt.h, are you sure this was run as root?\n");
			return 1;
		}
		if (!nob_copy_file(LIB_TARGET_SO, SYSTEM_LIB_DIR "lib"LIB_NAME".so")) {
			fprintf(stderr, "Could not install libctt.so, are you sure this was run as root?\n");
			return 1;
		}
	}
	// building ctt
	if (local) {
		nob_cmd_append(&cmd, CC, CFLAGS, WARNFLAGS, "-DLOCAL_LIBCTT", "-o", BIN_TARGET, BIN_SRC, LIB_O_TARGETS/*,*/ LDFLAGS);
	} else if (make_static_libctt) {
		nob_cmd_append(&cmd, CC, CFLAGS, WARNFLAGS, "-o", BIN_TARGET, BIN_SRC, LIB_TARGET_A, LDFLAGS);
	} else { // include /usr/includes/libctt.h
		nob_cmd_append(&cmd, CC, CFLAGS, WARNFLAGS, "-o", BIN_TARGET, BIN_SRC, LDFLAGS);
	}
	if (!nob_cmd_run(&cmd)) return 1;
	if (install) {
		if (!nob_copy_file(BIN_TARGET, SYSTEM_BIN_DIR "ctt")) {
			fprintf(stderr, "Could not install ctt, are you sure this was run as root?\n");
			return 1;
		}
	}
	for (size_t i = 0; lib_o_targets[i]; ++i) {
		nob_delete_file(lib_o_targets[i]);
	}
	if (clean) {
		if (nob_file_exists(LIB_TARGET_SO))
			nob_delete_file(LIB_TARGET_SO);
		if (nob_file_exists(LIB_TARGET_A))
			nob_delete_file(LIB_TARGET_A);
		if (nob_file_exists(LIB_TARGET_H))
			nob_delete_file(LIB_TARGET_H);
		if (nob_file_exists(BIN_TARGET))
			nob_delete_file(BIN_TARGET);
	}
	return 0;
}

