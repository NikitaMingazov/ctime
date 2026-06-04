# Compile-time C execution and code-insertion

A Jai-inspired metaprogramming transpiler for a superset of C. Implemented using PHP-style templates for C scripts within your source code for execution and string replacement into source code, including for later passes of the transpiler.

*Due to the genius of Copilot, you cannot escape ​$ on GitHub's parsing even by using &#x24​;, therefore zero-width spaces are inserted before every ​$ in this text*

## Building

```cc -o nob nob.c && ./nob``` produces the transpiler binary and the ctime library in builds/

```cc -o nob nob.c && sudo ./nob install``` installs ctt and libctime to /usr on a unix system

libtcc is a dependency, which is usually found in your distro's "tcc" package. I plan to later add to the build script an option to statically link from a tcc git repo, because vendoring tcc is complicated by the GPL.

## Usage

```ctt <source.ct>``` emits the transpiled source to source.c in the same directory.

```ctt -o <target.c> <source.ct>``` emits the transpiled source to the provided file path.

```ctt -o - <source.ct>``` emits the transpiled source to stdout.

```ctt -h``` displays the full list of options

## CTime syntax

\#\{ ... \}\# is a block of statements, all such blocks are concatenated and compiled into a temporary binary in memory using tinycc or a shell compiler with the -cc arg.

\(\$ ... ​\$\) is an expression block that takes a char* expression, and replaces the block in source with the it. Note that all such expressions cannot mutate state due to being evaluated by compiling the aforementioned binary. The char* expression must have been defined in a \#\{ \}\# block preceding its scope, but these blocks can be used anywhere, including inside other comptime expressions and even quotes (and of course, to insert source code into the transpiled C program).

\'\{ ... \}\' is a quote block that runs the preprocessor according to the comptime state before the quote, and replaces itself with its contents as string literal that is formatted such that when printed, is identical to the former block contents. There are no syntax or recursion concerns for it to use the previous comptime block state for the preprocessor, but these cannot be used in runtime C code.

The target source code is that which is not in any \#\{ \}\# block, and can use ​$( )​$ blocks to produce new C source code that you pass to your real compiler, import as a header etc.
