========
TableGen
========

.. contents::
   :local:

.. toctree::
   :hidden:

   BackEnds
   LangRef
   LangIntro
   Deficiencies

Introduction
============

TableGen is a domain-specific, declarative language for compactly expressing
detailed information about target platforms, including their processor
families and register sets, as well as detailed information about every 
machine-languge instruction that each platform supports.

TableGen .td files act as a single source of truth for the source and binary 
representations of machine-language instructions for each target platform. 
Additionally, TableGen files describe the operation of each target instruction 
abstractly as a SelectionDAG.  This SelectionDAG includes the types of inputs
and outputs for each instruction, as well as an algebraic representation of
the logic that each instruction performs.

TableGen's functionality can be roughly compared to yacc or antlr or other parser 
generators, in that it reads files describing data structures, and writes out
code and tables for representing and parsing those data structures. However,
unlike yacc or antlr, TableGen is not a general-purpose parsing framework. It
is intended specifically to fill out large quantities of default information
specifically for LLVM, including the myriad tables and objects that your LLVM
backend code will consume.

The llvm-tblgen tool takes as input files with a .td (TableGen Description) 
extension. Normally, it produces as output .inc files, which are snippets of
C++ code that you can #include in the classes that implement your LLVM backend.  
Additionally, command-line options to the TableGen tool permit dumping of
per-platform information in JSON and other text formats. 

TableGen uses a template system for creating more complex objects from simple
ones.  This template system has some syntax in common with C++, but it is
not C++, and should not be confused with C++.  C++ is procedural, but TableGen
is declarative.  With the exception of short bits of inlined C++ code within
TableGen, you should not think of TableGen as a procedural language.

Within TableGen, abstract collections of values and properties are denoted
as class types.  Concrete instances of a class are represented as def types.

TableGen's design is intentionally, and extremely, DRY (Don't Repeat Yourself).
As a result, the TableGen syntax may initially seem terse to the point of
rudeness. Reviewing the TableGen syntax as used in simpler LLVM backends, such
as the .td files in Sparc backend, can serve as an object introduction to TableGen.
 
The base class types in TableGen are not arbitrary.  LLVM backends expect your
TableGen code to describe and define at least a base set of TableGen objects.
Most platform .td files start by including llvm/include/llvm/Target/Target.td .
This file is required reading for anyone who needs to create or hack TableGen
files. Most of your implementation time in TableGen will
be spent extending these base classes and then instancing your subclasses,
in order to explain all the nitpicky details of your target to LLVM.
 
Much of the responsibility for implementing assembly and disassembly parsers 
can be subsumed by TableGen.  As TableGen has been developed over the years, 
more and more compiler features which would normally be dealt with by 
custom-written C++, have been pushed into TableGen's responsibility.  As a 
result, TableGen can handle much of the heavy lifting that would go into 
representing the details of your target, at some cost to readability of the
TableGen .td files.  This includes generating optimized code for critical
parts of back-end assemblers and disassemblers.

TableGen shines when you need to express all the details of your machine-code
formats and instructions to LLVM.  For example, support exists in TableGen
to describe: the input and output properties of any instruction; its
equivalent SelectionDAG; its length; whether it is a branch, or a return,
or whether it ends a scope, or is a barrier, or whether it reads or writes 
memory, or whether it needs special register allocation requirements, 
whether it operates on subregisters, and so forth.  Because LLVM already
supports so many different platforms, it is difficult to find any instruction
concepts that are not already supported to some extent in TableGen.

However, because TableGen ultimately spits out C++ code fragments, nothing
prevents you from writing your own custom C++ code to override or reimplement
touchy bits of the parsing and generation for your platform.
 
TableGen can be convinced to generate different assembly language parsers for 
different variants of assembly sources (e.g. AT&T versus Intel); it can handle 
parsing of complicated platform-specific operand formats; and it can handle 
automatic generation of near-miss information for assembler instructions ("did 
you mean ADDI?").  Before implementing a new feature in your particular 
backend, it is worthwhile to ask whether the feature has already been 
partially or completely implemented in TableGen, before you write any new
code. 
 
TableGen has specific export functionality built in for parsing, analyzing, 
and emitting at least the following types of information:
 
- Assembler intrinsics 
- Machine-code instruction formats and actions, in exhaustive detail
- Assembly language matchers, per assembly language variant
- Assembly language writers, per assembly language variant
- Calling conventions
- Instruction selection: fast, DAG, and global
- Pseudo-instruction lowering 
- Registers, register banks, aliases, and bank priorities
- Platform subtargets
- Option parsers
- Exegesis (instruction benchmarking) information

Some backends add platform-specific export functionality into TableGen itself.
See the ``llvm/utils/TableGen`` directory in your distribution for details.

The core part of TableGen parses a file, instantiates the declarations, and
hands the result off to a domain-specific `backend`_ for processing.

The current major users of TableGen are :doc:`../CodeGenerator`
and the
`Clang diagnostics and attributes <http://clang.llvm.org/docs/UsersManual.html#controlling-errors-and-warnings>`_.

TableGen's syntax has become sufficiently stable that formatters for several
common editors exist in ``llvm/utils``.  See the emacs, vim, vscode, and jedit
subdirectories in your LLVM source distribution.

.. _intro:


The TableGen program
====================

TableGen files are interpreted by the TableGen program: `llvm-tblgen` available
on your build directory under `bin`. It is not installed in the system (or where
your sysroot is set to), since it has no use beyond LLVM's build process.

Running TableGen
----------------

TableGen runs just like any other LLVM tool.  The first (optional) argument
specifies the file to read.  If a filename is not specified, ``llvm-tblgen``
reads from standard input.

To be useful, one of the `backends`_ must be used.  These backends are
selectable on the command line (type '``llvm-tblgen -help``' for a list).  For
example, to get a list of all of the definitions that subclass a particular type
(which can be useful for building up an enum list of these records), use the
``-print-enums`` option:

.. code-block:: bash

  $ llvm-tblgen X86.td -print-enums -class=Register
  AH, AL, AX, BH, BL, BP, BPL, BX, CH, CL, CX, DH, DI, DIL, DL, DX, EAX, EBP, EBX,
  ECX, EDI, EDX, EFLAGS, EIP, ESI, ESP, FP0, FP1, FP2, FP3, FP4, FP5, FP6, IP,
  MM0, MM1, MM2, MM3, MM4, MM5, MM6, MM7, R10, R10B, R10D, R10W, R11, R11B, R11D,
  R11W, R12, R12B, R12D, R12W, R13, R13B, R13D, R13W, R14, R14B, R14D, R14W, R15,
  R15B, R15D, R15W, R8, R8B, R8D, R8W, R9, R9B, R9D, R9W, RAX, RBP, RBX, RCX, RDI,
  RDX, RIP, RSI, RSP, SI, SIL, SP, SPL, ST0, ST1, ST2, ST3, ST4, ST5, ST6, ST7,
  XMM0, XMM1, XMM10, XMM11, XMM12, XMM13, XMM14, XMM15, XMM2, XMM3, XMM4, XMM5,
  XMM6, XMM7, XMM8, XMM9,

  $ llvm-tblgen X86.td -print-enums -class=Instruction 
  ABS_F, ABS_Fp32, ABS_Fp64, ABS_Fp80, ADC32mi, ADC32mi8, ADC32mr, ADC32ri,
  ADC32ri8, ADC32rm, ADC32rr, ADC64mi32, ADC64mi8, ADC64mr, ADC64ri32, ADC64ri8,
  ADC64rm, ADC64rr, ADD16mi, ADD16mi8, ADD16mr, ADD16ri, ADD16ri8, ADD16rm,
  ADD16rr, ADD32mi, ADD32mi8, ADD32mr, ADD32ri, ADD32ri8, ADD32rm, ADD32rr,
  ADD64mi32, ADD64mi8, ADD64mr, ADD64ri32, ...

The default backend prints out all of the records. There is also a general
backend which outputs all the records as a JSON data structure, enabled using
the `-dump-json` option.

If you plan to use TableGen, you will most likely have to write a `backend`_
that extracts the information specific to what you need and formats it in the
appropriate way. You can do this by extending TableGen itself in C++, or by
writing a script in any language that can consume the JSON output.

Example
-------

With no other arguments, `llvm-tblgen` parses the specified file and prints out all
of the classes, then all of the definitions.  This is a good way to see what the
various definitions expand to fully.  Running this on the ``X86.td`` file prints
this (at the time of this writing):

.. code-block:: text

  ...
  def ADD32rr {   // Instruction X86Inst I
    string Namespace = "X86";
    dag OutOperandList = (outs GR32:$dst);
    dag InOperandList = (ins GR32:$src1, GR32:$src2);
    string AsmString = "add{l}\t{$src2, $dst|$dst, $src2}";
    list<dag> Pattern = [(set GR32:$dst, (add GR32:$src1, GR32:$src2))];
    list<Register> Uses = [];
    list<Register> Defs = [EFLAGS];
    list<Predicate> Predicates = [];
    int CodeSize = 3;
    int AddedComplexity = 0;
    bit isReturn = 0;
    bit isBranch = 0;
    bit isIndirectBranch = 0;
    bit isBarrier = 0;
    bit isCall = 0;
    bit canFoldAsLoad = 0;
    bit mayLoad = 0;
    bit mayStore = 0;
    bit isImplicitDef = 0;
    bit isConvertibleToThreeAddress = 1;
    bit isCommutable = 1;
    bit isTerminator = 0;
    bit isReMaterializable = 0;
    bit isPredicable = 0;
    bit hasDelaySlot = 0;
    bit usesCustomInserter = 0;
    bit hasCtrlDep = 0;
    bit isNotDuplicable = 0;
    bit hasSideEffects = 0;
    InstrItinClass Itinerary = NoItinerary;
    string Constraints = "";
    string DisableEncoding = "";
    bits<8> Opcode = { 0, 0, 0, 0, 0, 0, 0, 1 };
    Format Form = MRMDestReg;
    bits<6> FormBits = { 0, 0, 0, 0, 1, 1 };
    ImmType ImmT = NoImm;
    bits<3> ImmTypeBits = { 0, 0, 0 };
    bit hasOpSizePrefix = 0;
    bit hasAdSizePrefix = 0;
    bits<4> Prefix = { 0, 0, 0, 0 };
    bit hasREX_WPrefix = 0;
    FPFormat FPForm = ?;
    bits<3> FPFormBits = { 0, 0, 0 };
  }
  ...

This definition corresponds to the 32-bit register-register ``add`` instruction
of the x86 architecture.  ``def ADD32rr`` defines a record named
``ADD32rr``, and the comment at the end of the line indicates the superclasses
of the definition.  The body of the record contains all of the data that
TableGen assembled for the record, indicating that the instruction is part of
the "X86" namespace, the pattern indicating how the instruction is selected by
the code generator, that it is a two-address instruction, has a particular
encoding, etc.  The contents and semantics of the information in the record are
specific to the needs of the X86 backend, and are only shown as an example.

As you can see, a lot of information is needed for every instruction supported
by the code generator, and specifying it all manually would be unmaintainable,
prone to bugs, and tiring to do in the first place.  Because we are using
TableGen, all of the information was derived from the following definition:

.. code-block:: text

  let Defs = [EFLAGS],
      isCommutable = 1,                  // X = ADD Y,Z --> X = ADD Z,Y
      isConvertibleToThreeAddress = 1 in // Can transform into LEA.
  def ADD32rr  : I<0x01, MRMDestReg, (outs GR32:$dst),
                                     (ins GR32:$src1, GR32:$src2),
                   "add{l}\t{$src2, $dst|$dst, $src2}",
                   [(set GR32:$dst, (add GR32:$src1, GR32:$src2))]>;

This definition makes use of the custom class ``I`` (extended from the custom
class ``X86Inst``), which is defined in the X86-specific TableGen file, to
factor out the common features that instructions of its class share.  A key
feature of TableGen is that it allows the end-user to define the abstractions
they prefer to use when describing their information.

Syntax
======

TableGen has a syntax that is loosely based on C++ templates, with built-in
types and specification. In addition, TableGen's syntax introduces some
automation concepts like multiclass, foreach, let, etc.

Basic concepts
--------------

TableGen files consist of two key parts: 'classes' and 'definitions', both of
which are considered 'records'.

**TableGen records** have a unique name, a list of values, and a list of
superclasses.  The list of values is the main data that TableGen builds for each
record; it is this that holds the domain specific information for the
application.  The interpretation of this data is left to a specific `backend`_,
but the structure and format rules are taken care of and are fixed by
TableGen.

**TableGen definitions** are the concrete form of 'records'.  These generally do
not have any undefined values, and are marked with the '``def``' keyword.

.. code-block:: text

  def FeatureFPARMv8 : SubtargetFeature<"fp-armv8", "HasFPARMv8", "true",
                                        "Enable ARMv8 FP">;

In this example, FeatureFPARMv8 is ``SubtargetFeature`` record initialised
with some values. The names of the classes are defined via the
keyword `class` either on the same file or some other included. Most target
TableGen files include the generic ones in ``include/llvm/Target``.

**TableGen classes** are abstract records that are used to build and describe
other records.  These classes allow the end-user to build abstractions for
either the domain they are targeting (such as "Register", "RegisterClass", and
"Instruction" in the LLVM code generator) or for the implementor to help factor
out common properties of records (such as "FPInst", which is used to represent
floating point instructions in the X86 backend).  TableGen keeps track of all of
the classes that are used to build up a definition, so the backend can find all
definitions of a particular class, such as "Instruction".

.. code-block:: text

 class ProcNoItin<string Name, list<SubtargetFeature> Features>
       : Processor<Name, NoItineraries, Features>;

Here, the class ProcNoItin, receiving parameters `Name` of type `string` and
a list of target features is specializing the class Processor by passing the
arguments down as well as hard-coding NoItineraries.

**TableGen multiclasses** are groups of abstract records that are instantiated
all at once.  Each instantiation can result in multiple TableGen definitions.
If a multiclass inherits from another multiclass, the definitions in the
sub-multiclass become part of the current multiclass, as if they were declared
in the current multiclass.

.. code-block:: text

  multiclass ro_signed_pats<string T, string Rm, dag Base, dag Offset, dag Extend,
                          dag address, ValueType sty> {
  def : Pat<(i32 (!cast<SDNode>("sextload" # sty) address)),
            (!cast<Instruction>("LDRS" # T # "w_" # Rm # "_RegOffset")
              Base, Offset, Extend)>;

  def : Pat<(i64 (!cast<SDNode>("sextload" # sty) address)),
            (!cast<Instruction>("LDRS" # T # "x_" # Rm # "_RegOffset")
              Base, Offset, Extend)>;
  }

  defm : ro_signed_pats<"B", Rm, Base, Offset, Extend,
                        !foreach(decls.pattern, address,
                                 !subst(SHIFT, imm_eq0, decls.pattern)),
                        i8>;



See the :doc:`TableGen Language Introduction <LangIntro>` for more generic
information on the usage of the language, and the
:doc:`TableGen Language Reference <LangRef>` for more in-depth description
of the formal language specification.

.. _backend:
.. _backends:

TableGen backends
=================

TableGen files have no real meaning without a back-end. The default operation
of running ``llvm-tblgen`` is to print the information in a textual format, but
that's only useful for debugging of the TableGen files themselves. The power
in TableGen is, however, to interpret the source files into an internal 
representation that can be generated into anything you want.

Current usage of TableGen is to create huge include files with tables that you
can either include directly (if the output is in the language you're coding),
or be used in pre-processing via macros surrounding the include of the file.

Direct output can be used if the back-end already prints a table in C format
or if the output is just a list of strings (for error and warning messages).
Pre-processed output should be used if the same information needs to be used
in different contexts (like Instruction names), so your back-end should print
a meta-information list that can be shaped into different compile-time formats.

See the `TableGen BackEnds <BackEnds.html>`_ for more information.