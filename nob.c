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

const char *help =
"usage: %s [OPTIONS]\n"
"options:\n"
" split-source     use multiple compilation units instead of unity build\n"
" no-install       only build into ./build/ without installing to /usr\n"
" clean            instead of building, remove build targets\n"
" dynamic-libctt   dynamically link libctt in ctt\n"
" build-static     build libctt.a\n"
" -h               print this message\n"
;

int main(int argc, char **argv)
{
	NOB_GO_REBUILD_URSELF(argc, argv);
	if (nob_file_exists("./nob.old"))
		nob_delete_file("./nob.old");
	bool split_source = false;
	bool no_install = false;
	bool clean = false;
	bool dynamic_libctt = false;
	bool build_static = false;
	for (size_t i = 1; argv[i]; ++i) {
		if (strcmp(argv[i], "-h") == 0) {
			printf(help, argv[0]);
			return 0;
		} else if (strcmp(argv[i], "build-static") == 0)
			build_static = true;
		else if (strcmp(argv[i], "dynamic-libctt") == 0)
			dynamic_libctt = true;
		else if (strcmp(argv[i], "no-install") == 0)
			no_install = true;
		else if (strcmp(argv[i], "split-source") == 0)
			split_source = true;
		else if (strcmp(argv[i], "clean") == 0)
			clean = true;
		else {
			printf("%s: invalid argument: %s\n", argv[0], argv[i]);
			printf(help, argv[0]);
			return 0;
		}
	}
	if (dynamic_libctt)
		split_source = true;
	char *define_dynamic_libctt = nob_temp_sprintf("-DDYNAMIC_LIBCTT=%d", dynamic_libctt);
	char *define_one_source = nob_temp_sprintf("-DONE_SOURCE=%d", !split_source);
	if (clean) {
		for (size_t i = 0; lib_o_targets[i]; ++i) {
			if (nob_file_exists(lib_o_targets[i]))
				nob_delete_file(lib_o_targets[i]);
		}
		if (clean) {
			if (nob_file_exists(LIB_TARGET_SO))
				nob_delete_file(LIB_TARGET_SO);
			if (nob_file_exists(LIB_TARGET_A))
				nob_delete_file(LIB_TARGET_A);
			if (nob_file_exists(LIB_TARGET_H))
				nob_delete_file(LIB_TARGET_H);
			if (nob_file_exists(LIB_TARGET_DIR))
				nob_delete_file(LIB_TARGET_DIR);
			if (nob_file_exists(BIN_TARGET))
				nob_delete_file(BIN_TARGET);
		}
		return 0;
	}
	if (!nob_mkdir_if_not_exists(BUILD_DIR))
		return 1;
	Nob_Cmd cmd = {0};
	// building libctt
	if (!nob_mkdir_if_not_exists(LIB_TARGET_DIR))
		return 1;
	// make the .o files
	if (split_source) {
		for (size_t i = 0; lib_c_sources[i]; ++i) {
			nob_cmd_append(&cmd, CC, CFLAGS, LIB_FLAGS, WARNFLAGS, define_one_source, "-o", lib_o_targets[i], lib_c_sources[i]);
			if (!nob_cmd_run(&cmd)) return 1;
		}
	} else {
		nob_cmd_append(&cmd, CC, CFLAGS, LIB_FLAGS, WARNFLAGS, define_one_source, "-o", BUILD_DIR "libctt.o", LIB_SOURCE_C);
		if (!nob_cmd_run(&cmd)) return 1;
	}
	if (build_static) {
		if (split_source)
			nob_cmd_append(&cmd, "ar", "rcs", LIB_TARGET_A, LIB_O_TARGETS);
		else
			nob_cmd_append(&cmd, "ar", "rcs", LIB_TARGET_A, BUILD_DIR "libctt.o");
		if (!nob_cmd_run(&cmd)) return 1;
	}
	if (split_source)
		nob_cmd_append(&cmd, CC, "-shared", "-o", LIB_TARGET_SO, LIB_O_TARGETS);
	else
		nob_cmd_append(&cmd, CC, "-shared", "-o", LIB_TARGET_SO, BUILD_DIR "libctt.o");
	if (!nob_cmd_run(&cmd)) return 1;
	// copy libctt.h to be adjacent to libctt.so
	if (!nob_copy_file(LIB_SOURCE_H, LIB_TARGET_H)) return 1;
	// installing to /usr
	if (!no_install) {
		if (build_static) {
			if (!nob_copy_file(LIB_TARGET_H, SYSTEM_HEADER_DIR "lib"LIB_NAME".a")) {
				fprintf(stderr, "Could not install libctt.a, are you sure this was run as root?\n");
				return 1;
			}
		}
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
	if (split_source)
		nob_cmd_append(&cmd, CC, CFLAGS, WARNFLAGS, define_dynamic_libctt, define_one_source, "-o", BIN_TARGET, BIN_SRC, LIB_O_TARGETS/*,*/ LDFLAGS);
	else
		nob_cmd_append(&cmd, CC, CFLAGS, WARNFLAGS, define_dynamic_libctt, define_one_source, "-o", BIN_TARGET, BIN_SRC, LDFLAGS);
	// if (build_static) {
	// 	nob_cmd_append(&cmd, CC, CFLAGS, WARNFLAGS, "-o", BIN_TARGET, BIN_SRC, LIB_TARGET_A, LDFLAGS);
	// }
	if (!nob_cmd_run(&cmd)) return 1;
	if (!no_install) {
		if (!nob_copy_file(BIN_TARGET, SYSTEM_BIN_DIR "ctt")) {
			fprintf(stderr, "Could not install ctt, are you sure this was run as root?\n");
			return 1;
		}
	}
	for (size_t i = 0; lib_o_targets[i]; ++i) {
		if (nob_file_exists(lib_o_targets[i]))
			nob_delete_file(lib_o_targets[i]);
	}
	return 0;
}

