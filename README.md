# Compile-time C execution and code-generation

A subset of Jai's metaprogramming, but for C. Implemented using PHP-style templates for C scripts within your source code.

Currently limited, ideas to make it more general include being able to emit comptime code for the transpiler to iterate over, so you can metaprogram in the scripts themselves.

Alternatively, numbered <N N> and <N$ $N> blocks, where <0 0> is comptime root and can be referred to within <1+ 1+> blocks by using <0$ $0>, where layers get compiled in order 0,1 ... 255

## Usage

```sh ctime <source.ct>``` emits the resolved source to stdout.

```sh ctime -o <target.c> <source.ct>``` emits the resolved source to the provided file.

## How comptime works

/\*\# ... \#\*/ defines a program, all such blocks are concatenated and compiled into a temporary binary in memory using tinycc.

/\*\$ ... \$\*/ takes a char* expression (usually a function call), inserting it into the source code. note that all such functions must be stateless due to being baked into the aforementioned binary.
