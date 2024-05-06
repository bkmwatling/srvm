# The virtual machine approach to optimised regular expression matching in C

The repo contains separate C source files for each stage of compiling
_regular expressions_ into _virtual machine (VM)_ instructions. The main
framework source code is contained over the C files `bru.c`, `fa/smir.[hc]`,
`re/parser.[hc]`, `re/sre.[hc]`, `vm/compiler.[hc]`, `vm/program.[hc]`, and
`vm/srvm.[hc]` in the `src/` directory.

The `re/sre.[hc]` and `vm/srvm.[hc]` files contain the necessary definitions for
the parse tree of a regular expression, and the definitions for the VM,
respectively. The `re/parser.c` file contains the code to parse regular
expressions and produce a parse tree as an intermediate representation for
generating VM instructions.

The `vm/compiler.c` file contains the code to generate VM instructions from the
parse tree using the constructions defined in other files (e.g.
`fa/constructions/thompson.[hc]` and `fa/constructions/glushkov.[hc]`). The
constructions actually convert the regular expression parse tree into the _state
machine intermediate representation (SMIR)_ which is then compiled to VM
instructions.

Finally, the actual regular expression matcher is contained in the `vm/srvm.c`
file which contains the source code for the actual execution of the VM with
compiled VM instructions.

## Cloning this repository

This project uses a C library (made by
[@bkmwatling](https://www.gitlab.com/bkmwatling)) which is included as a git
submodule. As such the submodule needs to be cloned into the project. This can
be done during cloning by running:

```bash
git clone git@git.cs.sun.ac.za:rvrg/srvm-c.git --recurse-submodules
```

if using the internal Stellenbosch Computer Science GitLab repository, or if
using the mirrored GitHub repository, simply run:

```bash
git clone git@github.com:bkmwatling/srvm.git --recurse-submodules
```

### Working with submodule after cloning

If the repository is already cloned and the submodule was not initialised, then
run:

```bash
git submodule init
```

After this, the submodule can be updated/installed to the correct commit
(necessary if library is updated) by running:

```bash
git submodule update
```

## Compiling the source files

This project includes a `Makefile` in which you can compile all the binaries by
executing:

```bash
make
```

## Executing the binaries

You can run the parser to print the parse tree of a given regular expression as
follows:

```bash
./bin/bru parse [OPTIONS] <regex>
```

To compile a VM program from a regular expression and print out the instructions
of the program, you can run:

```bash
./bin/bru compile [OPTIONS] <regex>
```

To run _Brendan's regex utility (BRU)_ matcher, simply run:

```bash
./bin/bru match [OPTIONS] <regex> <input>
```

The program compiles the regular expression into VM instructions and tries to
match it over the given input string, and will print whether the regular
expression matched the input string, as well as the capturing information.
