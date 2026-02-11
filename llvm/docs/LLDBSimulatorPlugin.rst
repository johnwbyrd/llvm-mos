=============================
LLDB Simulator Process Plugin
=============================

.. contents::
   :local:

Overview
========

The ProcessSimulator plugin enables LLDB to debug programs using LLVM's
in-process instruction emulator instead of requiring physical hardware or an
external emulator. This is particularly useful for:

* **Embedded development**: Debug programs for microcontrollers without the
  target hardware
* **Emulator validation**: Validate that the emulator produces correct results
  by stepping through code and inspecting state
* **Reverse debugging**: Step backward through execution to find the root cause
  of bugs
* **Testing**: Automated testing of compiled programs in CI environments

The plugin integrates with the :doc:`EmulatorFramework` (``emu::System``,
``emu::Context``) to provide a complete debugging experience including
breakpoints, watchpoints, memory inspection, and register access.

Supported Features
------------------

* Single-stepping (forward and reverse)
* Breakpoints (software)
* Watchpoints (read, write, read/write)
* Memory read/write
* Register read/write
* Reverse debugging via checkpoints
* Source-level debugging with DWARF

Limitations
-----------

* **Single-threaded**: Only single-threaded programs are supported. The
  emulator executes one instruction at a time.
* **No operating system**: There is no OS emulation. Semihosting provides
  limited host I/O.
* **Target-specific**: The target must have registered an emulator with
  ``TargetRegistry::RegisterEmulator()``.

Architecture
============

The plugin consists of three main classes:

ProcessSimulator
----------------

``ProcessSimulator`` is the main LLDB Process plugin. It:

* Creates and owns the ``emu::System`` and ``emu::Context`` instances
* Loads object files into emulator memory
* Delegates execution to the emulator framework
* Maps LLDB breakpoints/watchpoints to emulator equivalents
* Translates stop reasons between emulator and LLDB

ThreadSimulator
---------------

``ThreadSimulator`` represents the single thread of execution. It:

* Provides thread state (running, stopped, etc.)
* Creates register contexts for register access
* Reports stop reasons (breakpoint, watchpoint, single-step)

RegisterContextEmulator
-----------------------

``RegisterContextEmulator`` provides register access by:

* Mapping LLDB register numbers to emulator register numbers
* Calling ``Context::readRegister()`` and ``Context::writeRegister()``
* Building register info from the target's ABI

Data Flow
---------

::

   LLDB UI
      |
      v
   ProcessSimulator
      |
      +-- DoLaunch() --> LoadSections() --> emu::Memory
      |
      +-- DoResume() --> emu::System::run()
      |                       |
      |                       v
      |                  emu::Context::step()
      |                       |
      |                       v
      |                  Stop on breakpoint/watchpoint/halt
      |
      +-- DoReadMemory() --> emu::System::read()
      |
      +-- RefreshStateAfterStop() --> ThreadSimulator --> RegisterContextEmulator

Usage
=====

Starting a Debug Session
------------------------

To debug a program with the simulator:

.. code-block:: bash

   $ lldb program.elf
   (lldb) process launch --plugin simulator

Or from the command line:

.. code-block:: bash

   $ lldb -o "process launch --plugin simulator" program.elf

With semihosting enabled (for file I/O):

.. code-block:: bash

   (lldb) process launch --plugin simulator -- --semihost=/path/to/sandbox

Basic Debugging Commands
------------------------

Once the program is running, standard LLDB commands work as expected:

.. code-block:: text

   (lldb) breakpoint set --name main
   (lldb) run
   (lldb) step           # Step one source line
   (lldb) stepi          # Step one instruction
   (lldb) continue       # Continue to next breakpoint
   (lldb) register read  # Show register values
   (lldb) memory read 0x200  # Read memory

Reverse Debugging
-----------------

The simulator supports reverse debugging. Execution history is recorded
automatically, allowing you to step backward:

.. code-block:: text

   (lldb) process launch --plugin simulator
   Process 1 launched.
   (lldb) breakpoint set --name crash_point
   (lldb) continue
   Process 1 stopped at crash_point

   # Step backward to find the bug
   (lldb) process reverse-step
   (lldb) register read
   (lldb) process reverse-step
   ...

Reverse debugging commands:

* ``process reverse-step`` - Step backward one instruction
* ``process reverse-continue`` - Run backward to previous breakpoint

Note: Reverse debugging requires checkpoints to be created during forward
execution. The emulator creates checkpoints automatically at a configurable
interval.

Watchpoints
-----------

Watchpoints trigger when memory is accessed:

.. code-block:: text

   (lldb) watchpoint set variable my_global
   (lldb) watchpoint set expression -- 0x200  # Watch address 0x200
   (lldb) watchpoint modify -w read_write 1   # Watch reads and writes
   (lldb) continue

   Watchpoint 1 hit: old=0x00 new=0x42
   Process 1 stopped.

Setting Up for a Target
=======================

Prerequisites
-------------

Before the simulator plugin can work with your target, you need:

1. **Emulator Context**: An ``emu::Context`` subclass that implements
   instruction execution (see :doc:`EmulatorFramework`)
2. **Registered emulator**: The target must call
   ``TargetRegistry::RegisterEmulator()``
3. **ABI plugin**: LLDB needs an ABI plugin to map registers (usually already
   exists for established targets)

The plugin automatically uses the emulator registered for the target triple.

Register Mapping
----------------

The plugin builds register info from the target's ABI. If your target has
an ABI plugin in ``lldb/source/Plugins/ABI/``, register mapping should work
automatically.

For custom targets, ensure your ``emu::Context`` implements:

.. code-block:: cpp

   unsigned getNumRegisters() const override;
   bool readRegister(unsigned RegNum, void *Buf, size_t BufSize) const override;
   bool writeRegister(unsigned RegNum, const void *Buf, size_t BufSize) override;

The ``RegNum`` passed to these methods corresponds to DWARF register numbers.

Testing Your Setup
------------------

Verify that the simulator works:

.. code-block:: bash

   $ cat > test.c << 'EOF'
   int main() {
     volatile int x = 42;
     return x;
   }
   EOF

   $ clang --target=your-target -g -o test.elf test.c

   $ lldb test.elf
   (lldb) process launch --plugin simulator
   (lldb) breakpoint set --name main
   (lldb) continue
   (lldb) print x
   (int) $0 = 42

Troubleshooting
===============

"No emulator registered for target"
-----------------------------------

The target hasn't called ``TargetRegistry::RegisterEmulator()``. Ensure your
target's MCTargetDesc initialization includes:

.. code-block:: cpp

   TargetRegistry::RegisterEmulator(getTheYourTarget(), createYourEmulator);

"Failed to load executable"
---------------------------

The ELF file couldn't be loaded. Check that:

* The file is a valid ELF for the target architecture
* Loadable sections have valid addresses within the emulator's memory

Breakpoints not working
-----------------------

The emulator checks the PC against breakpoints before each instruction. Ensure:

* Breakpoints are set on valid instruction addresses
* The address matches the exact PC value (no alignment issues)

See Also
========

* :doc:`EmulatorFramework` - Adding emulation to an LLVM target
* :doc:`CommandGuide/llvm-emu` - Standalone program execution
* `LLDB Documentation <https://lldb.llvm.org/>`_ - General LLDB usage
