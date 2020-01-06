# Experimental LLVM backend for MOS 6502 and variants

This is an experimental effort to create a MOS 6502 backend for the LLVM
compiler suite.  As of this writing, **this effort is incomplete**, so don't 
get all excited and try to compile C code with it just yet.

The approach taken with this backend is slightly different from the 
one recommended in the documentation.  Since there is a large base of 
existing 6502 code, early stages of the bringup is focused on getting LLVM
to cleanly interoperate with existing 6502 assembly code.

Do not fork this repository yet without co-ordinating with me personally.  This
is not just arrogance talking: the contents of this repository may be rebased
randomly at any time.  I will probably get this working to proof of concept
stage, before collapsing all my work into one clean mega-commit on the
then-current LLVM stable release.  Then you can fork to your heart's content.

## Current status

A backend for MOS architectures has been added to `llvm/lib/Target/MC/MOS` . 
Using the triple 'mos' will cause llvm-mc to use the new MOS backend.

Tablegen recognizes all valid 6502 assembly mnemonics and spits out correct
opcodes for all 6502 instructions. An operand parser exists that can handle at
least all the instruction format variants in MOS.  This means that you can do
simple 6502 assembly programming with llvm-mc now, so long as you stick with
constant values and expressions.

The standard LLVM lexer has been modified to permit recognizing the dollar sign
`$` as a prefix for a hexadecimal constant, which tons of existing 6502 code
depends on.  The lexer now queries whatever the current MCAsmInfo structure
to see whether the target wants the dollar sign to be a hex prefix.  So,
everything that depends on the lexer (which is almost everything in LLVM) will
now recognize 6502 format hexadecimal constants.

## Future work

Stubs have been added for supporting assembler-specific commands in LLVM.
Out of the gate, I hope to add support for at least some of the assembler
directives in ca65 and in xa65, again because so much code seems to depend 
on them.  However, I see this as a worthwhile, but not as a required, goal.
ca65 in particular treats its macros as a simple programming language, and
leans on them much more heavily than gas and llvm-mc do.  Since LLVM tries
to emulate gas's behavior when compiling assembly code, it is unclear what
size this undertaking might be.

Remaining to be done at the assembly level: all things related to symbol
handling, including labels, fixups, and relaxation.  Linker script formats in 
ca65 are sufficiently different from the GNU linker script format, that I
will probably just rewrite some cl65 linker scripts in an LLVM-compatible
format.  We can probably copy the linker sections and meanings from cc65
though, since they've put a lot of work into getting cc65 compatible with
tons of existing 6502 platforms.  The 6502's 8-bit relative branching 
instructions should play very nicely with LLVM's relaxation logic, as every
B** instruction can be substituted for a slightly more expensive opposite
B** instruction, plus an absolute JMP.

The LLVM codegen functions are all stubbed.  Current strategy for codegen
is to choose 32 or 64 consecutive zero-page locations, and treat them all
as LLVM registers.  At least in the first compiler pass, this will mean that 
all read-modify-write operations move all memory to zero page before doing 
stuff to it.  I'm hoping that LLVM's register allocator, the smaller code size,
and the smaller cycle count on register operations, offset the cost of
constantly moving everything to and from zero page.

I intend to get assembly and disassembly stable and moderately compatible with
other assemblers, before moving on to codegen.  I'll probably give the user a
choice of calling conventions: an uber-slow stack-only parameter passing mode,
a cc65 compatible mode, and a fastcall mode.

Wozniak correctly observed that 6502 code that works on 16 bit values, takes
twice as much code space as it should.  Therefore, it's likely that LLVM
will include an implicit SWEET16 interpreter, or something very much like
SWEET16.  This could be thought of as premature optimization, but it's 
going to be a necessary step for machines with less than 64KB of memory.

I see LLVM codegen depending on a set of 16-bit virtual registers that map to 
zero-page locations.  Exact locations will be decided at the link step, so the
same core code will work regardless of your particular zero page layout.  These
virtual registers can be represented as 8-bit or 32-bit registers by normal LLVM 
register aliasing.

I foresee the fastcall convention as a SWEET16-compatible convention that
roughly follows ARM calling conventions.  That is, r0-r3 are temporaries, r4-r11
are locals, r12 is an interprocedural call register, r13 is a stack pointer, r14
is a link register, and r15 is the pc.  Locals would be allocated from r11 
leftwards.  This would mean that saving and restoring the callstack would
just be copying a contiguous range of zero page to and from the parameter
stack.  It would also mean that 6502 and SWEET16 code could play nicely 
together.  For certain limited cases, the fastcall convention could use
6502 real registers.  If a function accepts an unsigned char and returns an
unsigned character (e.g. isspace()), then that function could send and receive
values in the 6502 accummulator only.  But I think passing parameters globally
in A, X and Y may be overrated, since the very first thing you will have to 
do is store those values in temporary memory before you work on them.  Let's
see what LLVM's register allocator can come up with.

I can foresee building up a 32-bit virtual machine in 6502 code (SOUR32?)
This would be a SWEET16-like architecture with 32-bit registers and 32-bit
virtual addresses.  The 32-bit memory could be backed by one of the many RAM
expansion chips for 6502.  Since we can restrict which opcodes read and write
memory, we can intercept these in interpreted code, and read and write
pages in 16-bit memory that get swapped in and out to RAM expansion devices.
It'll be slow as molasses in winter at 1 MHz, but it would actually work, and
might even permit running modern software (albeit at a geological pace) on
a 6502.

------------

# The LLVM Compiler Infrastructure

This directory and its sub-directories contain source code for LLVM,
a toolkit for the construction of highly optimized compilers,
optimizers, and run-time environments.

The README briefly describes how to get started with building LLVM.
For more information on how to contribute to the LLVM project, please
take a look at the
[Contributing to LLVM](https://llvm.org/docs/Contributing.html) guide.

## Getting Started with the LLVM System

Taken from https://llvm.org/docs/GettingStarted.html.

### Overview

Welcome to the LLVM project!

The LLVM project has multiple components. The core of the project is
itself called "LLVM". This contains all of the tools, libraries, and header
files needed to process intermediate representations and converts it into
object files.  Tools include an assembler, disassembler, bitcode analyzer, and
bitcode optimizer.  It also contains basic regression tests.

C-like languages use the [Clang](http://clang.llvm.org/) front end.  This
component compiles C, C++, Objective C, and Objective C++ code into LLVM bitcode
-- and from there into object files, using LLVM.

Other components include:
the [libc++ C++ standard library](https://libcxx.llvm.org),
the [LLD linker](https://lld.llvm.org), and more.

### Getting the Source Code and Building LLVM

The LLVM Getting Started documentation may be out of date.  The [Clang
Getting Started](http://clang.llvm.org/get_started.html) page might have more
accurate information.

This is an example work-flow and configuration to get and build the LLVM source:

1. Checkout LLVM (including related sub-projects like Clang):

     * ``git clone https://github.com/llvm/llvm-project.git``

     * Or, on windows, ``git clone --config core.autocrlf=false
    https://github.com/llvm/llvm-project.git``

2. Configure and build LLVM and Clang:

     * ``cd llvm-project``

     * ``mkdir build``

     * ``cd build``

     * ``cmake -G <generator> [options] ../llvm``

        Some common build system generators are:

        * ``Ninja`` --- for generating [Ninja](https://ninja-build.org)
          build files. Most llvm developers use Ninja.
        * ``Unix Makefiles`` --- for generating make-compatible parallel makefiles.
        * ``Visual Studio`` --- for generating Visual Studio projects and
          solutions.
        * ``Xcode`` --- for generating Xcode projects.

        Some Common options:

        * ``-DLLVM_ENABLE_PROJECTS='...'`` --- semicolon-separated list of the LLVM
          sub-projects you'd like to additionally build. Can include any of: clang,
          clang-tools-extra, libcxx, libcxxabi, libunwind, lldb, compiler-rt, lld,
          polly, or debuginfo-tests.

          For example, to build LLVM, Clang, libcxx, and libcxxabi, use
          ``-DLLVM_ENABLE_PROJECTS="clang;libcxx;libcxxabi"``.

        * ``-DCMAKE_INSTALL_PREFIX=directory`` --- Specify for *directory* the full
          path name of where you want the LLVM tools and libraries to be installed
          (default ``/usr/local``).

        * ``-DCMAKE_BUILD_TYPE=type`` --- Valid options for *type* are Debug,
          Release, RelWithDebInfo, and MinSizeRel. Default is Debug.

        * ``-DLLVM_ENABLE_ASSERTIONS=On`` --- Compile with assertion checks enabled
          (default is Yes for Debug builds, No for all other build types).

      * ``cmake --build . [-- [options] <target>]`` or your build system specified above
        directly.

        * The default target (i.e. ``ninja`` or ``make``) will build all of LLVM.

        * The ``check-all`` target (i.e. ``ninja check-all``) will run the
          regression tests to ensure everything is in working order.

        * CMake will generate targets for each tool and library, and most
          LLVM sub-projects generate their own ``check-<project>`` target.

        * Running a serial build will be **slow**.  To improve speed, try running a
          parallel build.  That's done by default in Ninja; for ``make``, use the option
          ``-j NNN``, where ``NNN`` is the number of parallel jobs, e.g. the number of
          CPUs you have.

      * For more information see [CMake](https://llvm.org/docs/CMake.html)

Consult the
[Getting Started with LLVM](https://llvm.org/docs/GettingStarted.html#getting-started-with-llvm)
page for detailed information on configuring and compiling LLVM. You can visit
[Directory Layout](https://llvm.org/docs/GettingStarted.html#directory-layout)
to learn about the layout of the source code tree.
