llvm-emu - LLVM Instruction Emulator
====================================

.. program:: llvm-emu

SYNOPSIS
--------

:program:`llvm-emu` [*options*] *input-file*

DESCRIPTION
-----------

:program:`llvm-emu` loads and executes programs using target-specific
instruction emulators. It can execute object files (``.o``, ``.elf``) directly
or assemble and run source files (``.s``).

Unlike :program:`llvm-mca` which performs static performance analysis,
:program:`llvm-emu` dynamically executes instructions, making it useful for:

* Testing compiled code without physical hardware
* Validating instruction implementations
* Tracing execution for debugging or analysis
* Running programs with semihosting for host I/O

For object files, :program:`llvm-emu` automatically detects the target
architecture from the ELF header. For assembly source files, the target must
be specified with :option:`--triple` or :option:`--arch`.

Emulation continues until:

* The program halts (via a halt instruction or semihosting exit)
* The cycle limit is reached (:option:`--max-cycles`)
* An unrecoverable error occurs

OPTIONS
-------

Target Selection
~~~~~~~~~~~~~~~~

.. option:: --triple=<triple>

  Target triple (e.g., ``mos``, ``riscv32``). Auto-detected for object files.

.. option:: --arch=<arch>

  Target architecture to emulate for. Use ``--version`` to see available
  targets.

.. option:: --mcpu=<cpu-name>

  Target a specific CPU type. Use ``--mcpu=help`` for available options.

.. option:: --mattr=<a1,+a2,-a3,...>

  Target specific attributes. Use ``--mattr=help`` for available options.

Execution Control
~~~~~~~~~~~~~~~~~

.. option:: --max-cycles=<n>

  Maximum cycles to run before stopping. Default is 1,000,000. Set to 0 for
  unlimited (not recommended for untrusted code).

  If the cycle limit is reached before the program halts, :program:`llvm-emu`
  exits with status 1 and prints an error message.

Tracing
~~~~~~~

.. option:: --trace

  Enable instruction-level execution tracing. Trace output goes to standard
  error.

.. option:: --trace-format=<format>

  Choose trace output format. Valid options:

  ``text`` (default)
    Tab-separated text with columns for address, instruction, and register
    state. Human-readable but verbose.

  ``json``
    JSON Lines format (one JSON object per line). Suitable for automated
    analysis and tooling.

  ``vcd``
    Value Change Dump format for viewing in waveform viewers like GTKWave.
    Shows register value changes over time.

Semihosting
~~~~~~~~~~~

.. option:: --semihost=<directory>

  Enable semihosting with the specified directory as the sandbox root. All
  file operations are restricted to this directory for security.

  Semihosting allows the emulated program to perform host I/O operations such
  as reading and writing files, using a target-specific calling convention.

Assembly Options
~~~~~~~~~~~~~~~~

These options apply when the input is an assembly source file (``.s``).

.. option:: -I <directory>

  Add ``<directory>`` to the include search path for assembly directives.

.. option:: --defsym=<symbol>=<value>

  Define ``<symbol>`` as an integer constant ``<value>`` before assembly.

.. option:: --motorola-integers

  Enable Motorola-style integer syntax: ``$ABC`` for hexadecimal and ``%110``
  for binary. This is commonly used with 6502 and 68000 family assembly.

EXAMPLES
--------

Execute an object file
~~~~~~~~~~~~~~~~~~~~~~

Run an ELF object file (target auto-detected from ELF header):

.. code-block:: bash

   $ llvm-emu program.o
   $ echo $?
   0

Execute assembly source
~~~~~~~~~~~~~~~~~~~~~~~

Assemble and run a source file, specifying the target:

.. code-block:: bash

   $ llvm-emu --triple=mos test.s

Trace execution
~~~~~~~~~~~~~~~

Enable instruction tracing to see each executed instruction:

.. code-block:: bash

   $ llvm-emu --trace program.o 2> trace.txt

Generate JSON trace for analysis:

.. code-block:: bash

   $ llvm-emu --trace --trace-format=json program.o 2> trace.jsonl

Generate VCD waveform for timing analysis:

.. code-block:: bash

   $ llvm-emu --trace --trace-format=vcd program.o 2> trace.vcd
   $ gtkwave trace.vcd

Use semihosting for file I/O
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Run a program that reads and writes files, sandboxed to a specific directory:

.. code-block:: bash

   $ mkdir sandbox
   $ echo "input data" > sandbox/input.txt
   $ llvm-emu --semihost=sandbox program.o
   $ cat sandbox/output.txt

Limit execution cycles
~~~~~~~~~~~~~~~~~~~~~~

Prevent runaway programs by limiting execution:

.. code-block:: bash

   $ llvm-emu --max-cycles=10000 program.o
   error: emulator reached cycle limit (10000) without halting
   $ echo $?
   1

EXIT STATUS
-----------

:program:`llvm-emu` returns 0 if the emulated program halts normally (via a
halt instruction or semihosting exit with code 0). Otherwise, it returns
a non-zero value:

* 1 - Cycle limit reached, load error, or other emulator error
* N - Semihosting exit with code N (if the program requests non-zero exit)

SEE ALSO
--------

:program:`llvm-mca` - Static machine code analyzer for performance analysis

:program:`lldb` - LLVM debugger (use ``process launch --plugin simulator`` for
interactive debugging with the same emulator framework)
