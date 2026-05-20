# Compile-time C execution and code-insertion

A partial imitation of Jai's metaprogramming, but for C. Implemented using PHP-style templates for C scripts within your source code for execution and string replacement into source code, including for later passes of the transpiler.

*Due to the genius of Copilot, you cannot escape ​$ on GitHub's parsing even by using &#x24​;, therefore zero-width spaces are inserted before every ​$ in this text*

## Building

```cc -o nob nob.c && ./nob``` produces the transpiler binary in builds/

libtcc is a dependency, which is usually found in your distro's "tcc" package. I plan to later add to the build script an option to statically link from a tcc git repo, because vendoring tcc is complicated by the GPL.

## Usage

```ctime <source.ct>``` emits the transpiled source to source.c in the same directory.

```ctime -o <target.c> <source.ct>``` emits the transpiled source to the provided file path.

```ctime -o - <source.ct>``` emits the transpiled source to stdout.

```ctime -h``` displays the full list of options

## How comptime works

\#\{N ... N\}\# is a block of statements, all such blocks of a scope N are concatenated and compiled into a temporary binary in memory using tinycc. they are compiled in order from 0, and 1+ can use ​$(0 0)​$ to insert expressions that are in the scope of 0, such as function calls, as source code as in \#{1 ​$(0 0)​$ 1\}\#. the same applies for higher layers, where N is the order of compilation.

/\*\$N ... N\$\*/ is an expression block that takes a char*, inserting it into the source code. Note that all such functions must be stateless due to being baked into the aforementioned binary.

\#\{ \}\# without N is valid, and is equivalent to \#\{0 0\}\#. However, I want to overhaul this scoping system to remove the ordinal scopes, and have any \#\{ \}\# use any ​$( )​$ from a previously defined \#\{ \}\#, the scope of symbols being global.

The target source code is that which is not in any \#\{ \}\# block, and can use any ​$(N N)​$ to produce new C source code, that you pass to your real compiler, import as a header etc.
