==============================
LLVM Emulator Framework
==============================

.. contents::
   :local:

Introduction
============

The LLVM Emulator Framework provides infrastructure for executing compiled
programs within LLVM tooling. It enables:

* **Program execution**: Run compiled programs without physical hardware using
  :program:`llvm-emu`
* **Interactive debugging**: Debug programs with LLDB using the ProcessSimulator
  plugin
* **Execution tracing**: Capture instruction-by-instruction execution logs in
  text, JSON, or VCD format
* **Semihosting**: Allow programs to perform host file I/O through a sandboxed
  interface
* **Custom tools**: Build specialized analysis or simulation tools using the
  framework libraries

The framework is target-agnostic. Any LLVM target can add emulation support by
implementing a Context subclass and registering it with the target registry.

Architecture Overview
=====================

The framework consists of several cooperating components:

::

   +-------------------------------------------------------------+
   |                        User Tools                           |
   |   llvm-emu    ProcessSimulator (LLDB)    Custom Tools       |
   +-----------------------------+-------------------------------+
                                 |
                                 v
   +-----------------------------+-------------------------------+
   |                      emu::System                            |
   |   - Routes memory access to devices                         |
   |   - Manages breakpoints and watchpoints                     |
   |   - Maintains undo journal for reverse debugging            |
   |   - Coordinates multi-CPU execution                         |
   +-----------------------------+-------------------------------+
                                 |
                                 v
   +-----------------------------+-------------------------------+
   |                      emu::Context                           |
   |   - Abstract CPU interface (step, reset, getPC, setPC)      |
   |   - Register access for debugger integration                |
   |   - Implemented by each target (e.g., MOS::Context)         |
   +-----------------------------+-------------------------------+
                                 |
                                 v
   +---------------+-------------+-------------+-----------------+
   |  emu::Memory  |   emu::Semihost           |  Custom Devices |
   |  (RAM/ROM)    |   (Host I/O)              |  (emu::Device)  |
   +---------------+---------------------------+-----------------+

Framework Components
====================

emu::System
-----------

``emu::System`` is the central coordinator for emulation. It:

* **Routes memory access**: Maintains a list of memory-mapped devices and
  dispatches read/write operations to the appropriate device. Later-added
  devices shadow earlier ones at overlapping addresses.

* **Manages execution**: Provides ``run()``, ``step()``, ``stepReverse()``,
  and ``runReverse()`` for forward and reverse execution.

* **Handles breakpoints/watchpoints**: Checks addresses on each memory access
  and instruction execution, stopping when a breakpoint or watchpoint fires.

* **Maintains the undo journal**: Records all memory and register writes,
  enabling restoration to previous checkpoints for reverse debugging.

* **Supports multi-CPU**: Can manage multiple ``Context`` instances with
  independent clock rates (experimental).

Key methods:

.. code-block:: cpp

   // Create a system with memory and semihosting
   static std::unique_ptr<System> create(unsigned AddrBits,
                                         const std::string &SandboxDir = "");

   // Device management
   void addDevice(uint64_t Start, uint64_t End, std::unique_ptr<Device> Dev);

   // CPU management
   void addContext(Context *Ctx, uint64_t ClockHz = 1000000);

   // Execution control
   bool run();           // Run until halt/breakpoint
   void step();          // Execute one instruction
   bool stepReverse();   // Step backward one checkpoint
   bool runReverse();    // Run backward to breakpoint

   // Breakpoints and watchpoints
   bool addBreakpoint(uint64_t Addr);
   bool addWatchpoint(uint64_t Addr, size_t Size, WatchType Type);

   // Checkpointing
   void checkpoint();
   bool restoreToCheckpoint(size_t Idx);

emu::Context
------------

``emu::Context`` is the abstract base class for CPU execution contexts. Each
target architecture provides a concrete implementation that knows how to:

* Decode and execute instructions (``step()``)
* Reset to initial state (``reset()``)
* Access program counter and cycle count
* Read and write registers (for debugger integration)
* Handle interrupts

Required virtual methods for subclasses:

.. code-block:: cpp

   bool step() override;              // Execute one instruction
   void reset() override;             // Reset to initial state
   uint64_t getPC() const override;   // Get program counter
   void setPC(uint64_t PC) override;  // Set program counter
   uint64_t getCycles() const override;
   bool isHalted() const override;
   void halt(int ExitCode = 0) override;

Optional overrides for debugger integration:

.. code-block:: cpp

   unsigned getNumRegisters() const override;
   bool readRegister(unsigned RegNum, void *Buf, size_t BufSize) const override;
   bool writeRegister(unsigned RegNum, const void *Buf, size_t BufSize) override;

Memory access routes through the parent ``System``:

.. code-block:: cpp

   uint8_t read(uint64_t Addr);       // Reads via System
   void write(uint64_t Addr, uint8_t Value);  // Writes via System

emu::Device
-----------

``emu::Device`` is the abstract interface for memory-mapped peripherals.
Anything that responds to memory reads and writes can be a device: RAM, ROM,
I/O controllers, timers, semihosting, etc.

.. code-block:: cpp

   class Device {
   public:
     virtual ~Device() = default;

     // Read a byte at the given offset from device base
     virtual uint8_t read(uint64_t Offset) = 0;

     // Write a byte at the given offset from device base
     virtual void write(uint64_t Offset, uint8_t Value) = 0;

     // Block operations (default: call read/write in a loop)
     virtual void readBlock(uint8_t *Dest, uint64_t Offset, uint64_t Size);
     virtual void writeBlock(uint64_t Offset, const uint8_t *Src, uint64_t Size);
   };

Devices are registered with ``System::addDevice(Start, End, Device)``. The
offset passed to ``read()``/``write()`` is relative to the device's start
address.

emu::Memory
-----------

``emu::Memory`` is a simple RAM/ROM device that provides a byte array with
optional write protection:

.. code-block:: cpp

   // Create 64KB RAM
   auto RAM = std::make_unique<Memory>(65536);

   // Create ROM from data
   auto ROM = std::make_unique<Memory>(data, size, /*ReadOnly=*/true);

   // Load an object file
   Memory::loadObject(ObjectFile, RAM);

Out-of-bounds reads return 0xFF (floating bus behavior). Out-of-bounds writes
and writes to read-only memory are silently ignored.

emu::Semihost
-------------

``emu::Semihost`` provides host file I/O for embedded programs using the ZBC
semihosting protocol. Programs communicate with the host by reading and
writing to a special memory-mapped region.

Key features:

* **Sandboxing**: All file operations are restricted to a specified directory
* **Policy-based access**: Configurable read/write/create permissions
* **Standard operations**: open, close, read, write, seek, fstat
* **Program exit**: Programs can request termination with an exit code

emu::Trace
----------

The tracing system captures instruction execution for debugging and analysis.
Three output formats are provided:

``TextTraceWriter``
  Human-readable, tab-separated format. Easy to grep and eyeball::

    0       $0600   A=00 X=00 Y=00 S=FF     LDA #$42
    2       $0602   A=42 X=00 Y=00 S=FF     STA $0200

``JSONTraceWriter``
  JSON Lines format (one object per line). Suitable for automated analysis::

    {"cycle":0,"pc":"0600","regs":{"A":"00","X":"00"},"inst":"LDA #$42"}

``VCDTraceWriter``
  IEEE 1364 waveform format for visualization in GTKWave or similar tools.
  Shows register value changes over time as a timing diagram.

Adding Emulation to a Target
============================

To add emulation support to an LLVM target, you need to:

1. Create a ``Context`` subclass that integrates with generated code
2. Define instruction semantics (via TableGen Emulate fields, SAIL, or both)
3. Register it with ``TargetRegistry::RegisterEmulator()``

Instruction semantics are defined per-instruction, not by writing a monolithic
``step()`` function. The framework handles instruction fetch, decode, and
dispatch. You provide the semantic action for each instruction.

.. list-table::
   :header-rows: 1
   :widths: 20 40 40

   * - Path
     - Description
     - Best For
   * - TableGen Emulate
     - Inline C++ per instruction in TableGen
     - Compositional patterns, existing targets
   * - SAIL Specification
     - Formal ISA spec compiled to C++
     - Complex ISAs, verification needed
   * - Mixed
     - SAIL for complex instructions, inline C++ for simple ones
     - Best of both worlds

Path A: TableGen Emulate Fields
-------------------------------

For targets with many instructions sharing common patterns, define semantics
inline in TableGen using the ``Emulate`` code field. The EmulatorEmitter
processes these fields to generate C++ switch cases.

**Step 1: Define the Emulate field in your instruction format**

.. code-block:: text

   // In YourTargetInstrFormats.td
   class YourTargetInst<...> : Instruction {
     // ... existing fields ...

     // Emulation: C++ code to execute this instruction
     // Empty means "not emulatable"
     code Emulate = [{}];

     // Compositional fields for addressing modes
     code EA = [{}];      // Effective address
     code Value = [{}];   // Operand value
   }

**Step 2: Define reusable addressing mode patterns**

.. code-block:: text

   class AddressingMode<int size> {
     int OperandSize = size;
     code EA = [{}];
     code Value = [{}];
   }

   def Immediate : AddressingMode<1> {
     let Value = [{ (uint8_t)Inst.getOperand(0).getImm() }];
   }

   def ZeroPage : AddressingMode<1> {
     let EA = [{ (uint16_t)Inst.getOperand(0).getImm() }];
     let Value = [{ read($EA) }];  // $EA resolved from above
   }

   def AbsoluteX : AddressingMode<2> {
     let Base = [{ (uint16_t)Inst.getOperand(0).getImm() }];
     let EA = [{ (uint16_t)($Base + X) }];
     let Value = [{ read($EA) }];
   }

**Step 3: Define instructions with shared Emulate code**

.. code-block:: text

   // All ORA variants share the same semantic: A |= operand
   let Emulate = [{ A |= $Value; setNZ(A); }] in {
     def ORA_Immediate : Inst<0x09, "ora", Immediate>;
     def ORA_ZeroPage  : Inst<0x05, "ora", ZeroPage>;
     def ORA_AbsoluteX : Inst<0x1D, "ora", AbsoluteX>;
   }

   let Emulate = [{ A &= $Value; setNZ(A); }] in {
     def AND_Immediate : Inst<0x29, "and", Immediate>;
     def AND_ZeroPage  : Inst<0x25, "and", ZeroPage>;
   }

**Step 4: Generated output**

The EmulatorEmitter resolves ``$Variable`` references and generates:

.. code-block:: cpp

   // In YourTargetGenEmulator.inc
   #ifdef GET_EMULATOR_CASES
   case YourTarget::ORA_ZeroPage: {
     auto EA = (uint16_t)Inst.getOperand(0).getImm();
     auto Value = read(EA);
     A |= Value;
     setNZ(A);
     break;
   }
   case YourTarget::ORA_AbsoluteX: {
     auto Base = (uint16_t)Inst.getOperand(0).getImm();
     auto EA = (uint16_t)(Base + X);
     auto Value = read(EA);
     A |= Value;
     setNZ(A);
     break;
   }
   #endif

**Key features of variable substitution**:

* ``$Foo`` references look up field ``Foo`` on the instruction record
* Variables can reference other variables (dependencies resolved automatically)
* Multi-line code blocks: all lines except last are setup, last line is the value
* Works with TableGen inheritance - fields propagate through class hierarchy

**Pros**: DRY code (one semantic definition per operation), leverages existing
TableGen infrastructure, easy to compose patterns.

**Cons**: More complex setup, limited to what can be expressed inline.

Path B: SAIL Formal Specification
---------------------------------

For complex ISAs or when formal verification is needed, use the `SAIL language
<https://github.com/rems-project/sail>`_ to formally specify instruction
semantics, then compile to C++ via the EmulatorEmitter.

**Step 1: Write SAIL specifications**

SAIL is a formal ISA specification language. A minimal example:

.. code-block:: text

   // registers.sail
   register PC : bits(16)
   register A : bits(8)
   register X : bits(8)

   // instructions.sail
   function clause execute(LDA_IMM(imm)) = {
     A = imm;
     update_flags(A)
   }

See the `SAIL documentation <https://github.com/rems-project/sail>`_ for
complete language reference.

**Step 2: Compile to SAIL IR**

Use the isla-sail plugin to compile SAIL to intermediate representation:

.. code-block:: bash

   sail -plugin sail_plugin_isla.cmxs -isla -o output specs.sail

This produces a ``.ir`` file containing the lowered specification.

**Step 3: Generate C++ with EmulatorEmitter**

Add to your target's CMakeLists.txt:

.. code-block:: cmake

   set(SAIL_IR ${CMAKE_CURRENT_SOURCE_DIR}/Sail/yourtarget.ir)
   tablegen(LLVM YourTargetGenEmulator.inc -gen-emulator
            -sail-ir=${SAIL_IR} DEPENDS ${SAIL_IR})

**Step 4: Integrate generated code**

The generated code produces two preprocessor sections:

* ``GET_SAIL_CLASS_TYPES``: Type definitions (namespace scope)
* ``GET_SAIL_CLASS_BODY``: Class body with members and methods

Include them in your Context:

.. code-block:: cpp

   namespace YourTarget {

   // Types at namespace scope
   #define GET_SAIL_CLASS_TYPES
   #include "YourTargetGenEmulator.inc"
   #undef GET_SAIL_CLASS_TYPES

   // Base class with generated SAIL code
   class YourTargetSail {
   public:
     virtual ~YourTargetSail() = default;

   #define GET_SAIL_CLASS_BODY
   #include "YourTargetGenEmulator.inc"
   #undef GET_SAIL_CLASS_BODY
   };

   // Implementation class
   class SailImpl : public YourTargetSail {
     Context &Ctx;
   public:
     SailImpl(Context &C) : Ctx(C) {}

     // Implement external functions (memory access)
     uint64_t read_mem(...) override { return Ctx.read(addr); }
     bool write_mem(...) override { Ctx.write(addr, data); return true; }
   };

   class Context : public emu::Context {
     SailImpl Sail{*this};
   public:
     bool step() override { return Sail.zstep(); }
     // ... rest of interface
   };

   } // namespace YourTarget

See the MOS target (``llvm/lib/Target/MOS/MCTargetDesc/MOSContext.h``) for a
complete example.

**Pros**: Formal specification can be verified, auto-generated code is
consistent with spec, suitable for complex ISAs.

**Cons**: Requires SAIL toolchain, learning curve for SAIL language.

Target Registration
-------------------

Register your emulator factory in the target's MCTargetDesc initialization:

.. code-block:: cpp

   // In YourTargetMCTargetDesc.cpp
   #include "llvm/MC/TargetRegistry.h"
   #include "YourTargetContext.h"

   static emu::Context *createYourTargetEmulator(
       const Target &T, const MCSubtargetInfo &STI, MCContext &Ctx) {
     std::unique_ptr<MCDisassembler> Disasm(T.createMCDisassembler(STI, Ctx));
     std::unique_ptr<MCInstrInfo> II(T.createMCInstrInfo());
     return new YourTarget::Context(Disasm.release(), II.release());
   }

   void LLVMInitializeYourTargetTargetMC() {
     // ... other registrations ...

     TargetRegistry::RegisterEmulator(getTheYourTargetTarget(),
                                       createYourTargetEmulator);
   }

Using the Framework
===================

llvm-emu
--------

:program:`llvm-emu` is the command-line tool for program execution. See
:doc:`CommandGuide/llvm-emu` for complete documentation.

Basic usage:

.. code-block:: bash

   # Run an object file (target auto-detected)
   llvm-emu program.o

   # Run assembly source
   llvm-emu --triple=mos test.s

   # Enable tracing
   llvm-emu --trace --trace-format=json program.o 2> trace.jsonl

   # Use semihosting
   llvm-emu --semihost=./sandbox program.o

Custom Tool Integration
-----------------------

To build custom tools using the framework, link against ``LLVMEmulator``:

.. code-block:: cmake

   add_executable(my_tool main.cpp)
   target_link_libraries(my_tool PRIVATE LLVMEmulator LLVMMOSDesc)

Example custom tool:

.. code-block:: cpp

   #include "llvm/Emulator/System.h"
   #include "llvm/Emulator/Memory.h"
   #include "llvm/MC/TargetRegistry.h"

   int main() {
     // Create system with 64KB address space
     auto Sys = emu::System::create(16);

     // Load program
     // ...

     // Get target and create emulator
     std::string Error;
     const Target *T = TargetRegistry::lookupTarget("mos", Error);
     auto Ctx = T->createEmulator(...);

     Sys->addContext(Ctx);
     Sys->run();

     return Ctx->getExitCode();
   }

Reference Implementation
========================

The MOS target provides a complete reference implementation:

.. list-table::
   :header-rows: 1

   * - Component
     - Location
   * - Context implementation
     - ``llvm/lib/Target/MOS/MCTargetDesc/MOSContext.h``
   * - Context source
     - ``llvm/lib/Target/MOS/MCTargetDesc/MOSContext.cpp``
   * - SAIL specifications
     - ``llvm/lib/Target/MOS/Sail/*.sail``
   * - SAIL IR (generated)
     - ``llvm/lib/Target/MOS/Sail/mos6502.ir``
   * - CMake integration
     - ``llvm/lib/Target/MOS/CMakeLists.txt``
   * - Target registration
     - ``llvm/lib/Target/MOS/MCTargetDesc/MOSMCTargetDesc.cpp``

The SAIL specifications are organized as:

* ``prelude.sail`` - Operator overloads, type aliases
* ``core.sail`` - Register definitions, extensions
* ``sys.sail`` - Memory interface, stack, interrupts
* ``instructions.sail`` - All 6502 instruction definitions
* ``model.sail`` - Fetch, decode, reset, step loop

See Also
========

* :doc:`CommandGuide/llvm-emu` - llvm-emu command reference
* :doc:`LLDBSimulatorPlugin` - LLDB ProcessSimulator integration
* :doc:`TableGen/BackEnds` - EmulatorEmitter TableGen backend
* `SAIL Language <https://github.com/rems-project/sail>`_ - Formal ISA
  specification language
