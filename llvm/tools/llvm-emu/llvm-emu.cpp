//===-- llvm-emu.cpp - LLVM Instruction Emulator ----------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This utility loads and executes programs using target-specific emulators.
// It can load object files (.o/.elf) or assemble source files (.s) in memory.
//
//===----------------------------------------------------------------------===//

#include "llvm/BinaryFormat/Magic.h"
#include "llvm/Emulator/Context.h"
#include "llvm/Emulator/Memory.h"
#include "llvm/Emulator/Semihost.h"
#include "llvm/Emulator/System.h"
#include "llvm/Emulator/Trace.h"
#include "llvm/MC/MCAsmBackend.h"
#include "llvm/MC/MCAsmInfo.h"
#include "llvm/MC/MCCodeEmitter.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCDisassembler/MCDisassembler.h"
#include "llvm/MC/MCInstPrinter.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/MCObjectFileInfo.h"
#include "llvm/MC/MCObjectWriter.h"
#include "llvm/MC/MCParser/AsmLexer.h"
#include "llvm/MC/MCParser/MCTargetAsmParser.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/MCTargetOptionsCommandFlags.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Object/ObjectFile.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/InitLLVM.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/WithColor.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/TargetParser/Host.h"
#include <memory>

using namespace llvm;

static mc::RegisterMCTargetOptionsFlags MOF;

static cl::OptionCategory EmuCategory("Emulator Options");

static cl::opt<std::string> InputFilename(cl::Positional,
                                          cl::desc("<input file>"),
                                          cl::Required, cl::cat(EmuCategory));

// Target options
static cl::opt<std::string>
    TripleName("triple",
               cl::desc("Target triple (auto-detected for object files)"),
               cl::cat(EmuCategory));

static cl::opt<std::string>
    ArchName("arch", cl::desc("Target arch to emulate for"),
             cl::cat(EmuCategory));

static cl::opt<std::string>
    MCPU("mcpu", cl::desc("Target a specific cpu type"),
         cl::value_desc("cpu-name"), cl::init(""), cl::cat(EmuCategory));

static cl::list<std::string>
    MAttrs("mattr", cl::CommaSeparated,
           cl::desc("Target specific attributes"),
           cl::value_desc("a1,+a2,-a3,..."), cl::cat(EmuCategory));

// Execution options
static cl::opt<bool>
    EnableTrace("trace", cl::desc("Trace instruction execution"),
                cl::cat(EmuCategory));

static cl::opt<std::string>
    SemihostDir("semihost",
                cl::desc("Enable semihosting with sandbox directory for file I/O"),
                cl::value_desc("sandbox-dir"), cl::cat(EmuCategory));

static cl::opt<uint64_t>
    MaxCycles("max-cycles",
              cl::desc("Maximum cycles to run before stopping (default 1M)"),
              cl::init(1000000), cl::cat(EmuCategory));

enum TraceFormatType {
  TF_Text,
  TF_JSON,
  TF_VCD,
};

static cl::opt<TraceFormatType> TraceFormat(
    "trace-format", cl::init(TF_Text),
    cl::desc("Choose trace output format:"),
    cl::values(clEnumValN(TF_Text, "text", "Tab-separated text (default)"),
               clEnumValN(TF_JSON, "json", "JSON Lines (one object per line)"),
               clEnumValN(TF_VCD, "vcd",
                          "VCD (Value Change Dump) for waveform viewers")),
    cl::cat(EmuCategory));

// Assembly options (for .s files)
static cl::list<std::string> IncludeDirs("I",
                                         cl::desc("Directory of include files"),
                                         cl::value_desc("directory"),
                                         cl::Prefix, cl::cat(EmuCategory));

static cl::list<std::string>
    DefineSymbol("defsym",
                 cl::desc("Defines a symbol to be an integer constant"),
                 cl::cat(EmuCategory));

static cl::opt<bool>
    LexMotorolaIntegers("motorola-integers",
                        cl::desc("Enable Motorola integer syntax (%110 and $ABC)"),
                        cl::cat(EmuCategory));

//===----------------------------------------------------------------------===//
// Helper Functions
//===----------------------------------------------------------------------===//

static const Target *GetTarget(const char *ProgName, Triple &TheTriple) {
  std::string Error;
  const Target *TheTarget =
      TargetRegistry::lookupTarget(ArchName, TheTriple, Error);
  if (!TheTarget) {
    WithColor::error(errs(), ProgName) << Error;
    return nullptr;
  }
  return TheTarget;
}

static const Target *GetTargetFromObjectFile(const char *ProgName,
                                             const object::ObjectFile &Obj,
                                             Triple &OutTriple) {
  // Get architecture from ELF e_machine field
  Triple::ArchType Arch = static_cast<Triple::ArchType>(Obj.getArch());
  if (Arch == Triple::UnknownArch) {
    WithColor::error(errs(), ProgName)
        << "unknown architecture in object file\n";
    return nullptr;
  }

  // Build a triple from the detected architecture
  OutTriple.setArch(Arch);

  std::string Error;
  const Target *TheTarget = TargetRegistry::lookupTarget("", OutTriple, Error);
  if (!TheTarget) {
    WithColor::error(errs(), ProgName)
        << "no target for architecture: " << Error << "\n";
    return nullptr;
  }

  return TheTarget;
}

static int fillCommandLineSymbols(MCAsmParser &Parser) {
  for (const auto &I : DefineSymbol) {
    auto Pair = StringRef(I).split('=');
    auto Sym = Pair.first;
    auto Val = Pair.second;

    if (Sym.empty() || Val.empty()) {
      WithColor::error() << "defsym must be of the form: sym=value: " << I
                         << "\n";
      return 1;
    }
    int64_t Value;
    if (Val.getAsInteger(0, Value)) {
      WithColor::error() << "value is not an integer: " << Val << "\n";
      return 1;
    }
    Parser.getContext().setSymbolValue(Parser.getStreamer(), Sym, Value);
  }
  return 0;
}

//===----------------------------------------------------------------------===//
// Object File Execution
//===----------------------------------------------------------------------===//

static int RunObject(const char *ProgName, const Target *TheTarget,
                     const MCSubtargetInfo &STI, const object::ObjectFile &Obj,
                     MCContext &Ctx, const MCAsmInfo &MAI,
                     const MCInstrInfo &MCII, const MCRegisterInfo &MRI) {
  // Create the target-specific emulator
  std::unique_ptr<emu::Context> Emu(TheTarget->createEmulator(STI, Ctx));
  if (!Emu) {
    WithColor::error(errs(), ProgName)
        << "no emulator available for target " << TheTarget->getName() << "\n";
    return 1;
  }

  // Create system with memory and semihosting
  unsigned AddrBits = Emu->getAddressBits();
  auto Sys = emu::System::create(AddrBits, SemihostDir);

  // Load sections from object file into memory
  if (auto E = emu::Memory::loadObject(Obj, *Sys->getMemory())) {
    WithColor::error(errs(), ProgName)
        << "failed to load object: " << toString(std::move(E)) << "\n";
    return 1;
  }

  // Register emulator with system
  Sys->addContext(Emu.get());

  // Reset CPU (reads reset vector)
  Emu->reset();

  // Configure tracing if requested
  std::unique_ptr<MCInstPrinter> IP;
  std::unique_ptr<emu::TraceWriter> TraceWriter;
  if (EnableTrace) {
    IP.reset(TheTarget->createMCInstPrinter(STI.getTargetTriple(), 0, MAI, MCII,
                                            MRI));

    switch (TraceFormat) {
    case TF_Text:
      TraceWriter =
          std::make_unique<emu::TextTraceWriter>(errs(), IP.get(), &STI);
      break;
    case TF_JSON:
      TraceWriter =
          std::make_unique<emu::JSONTraceWriter>(errs(), IP.get(), &STI);
      break;
    case TF_VCD:
      TraceWriter = std::make_unique<emu::VCDTraceWriter>(errs());
      break;
    }

    Emu->setInstPrinter(IP.get());
    Emu->setSubtargetInfo(&STI);
    Emu->setTraceWriter(TraceWriter.get());
    Emu->setTracing(true);
    TraceWriter->traceStart();
  }

  // Run until halt or cycle limit
  Sys->setMaxCycles(MaxCycles);
  Sys->run();

  if (!Emu->isHalted()) {
    if (TraceWriter)
      TraceWriter->traceEnd();
    WithColor::error(errs(), ProgName)
        << "emulator reached cycle limit (" << MaxCycles
        << ") without halting\n";
    return 1;
  }

  if (TraceWriter)
    TraceWriter->traceEnd();

  return Sys->getExitCode();
}

//===----------------------------------------------------------------------===//
// Assembly and Execution
//===----------------------------------------------------------------------===//

static int AssembleAndRun(const char *ProgName, const Target *TheTarget,
                          SourceMgr &SrcMgr, Triple &TheTriple,
                          const MCTargetOptions &MCOptions) {
  // Create MC infrastructure
  std::unique_ptr<MCRegisterInfo> MRI(
      TheTarget->createMCRegInfo(TheTriple));
  if (!MRI) {
    WithColor::error(errs(), ProgName) << "unable to create register info\n";
    return 1;
  }

  std::unique_ptr<MCAsmInfo> MAI(
      TheTarget->createMCAsmInfo(*MRI, TheTriple, MCOptions));
  if (!MAI) {
    WithColor::error(errs(), ProgName) << "unable to create asm info\n";
    return 1;
  }

  SubtargetFeatures Features;
  for (const auto &Attr : MAttrs)
    Features.AddFeature(Attr);

  std::unique_ptr<MCSubtargetInfo> STI(
      TheTarget->createMCSubtargetInfo(TheTriple, MCPU, Features.getString()));
  if (!STI) {
    WithColor::error(errs(), ProgName) << "unable to create subtarget info\n";
    return 1;
  }

  MCContext Ctx(TheTriple, MAI.get(), MRI.get(), STI.get(), &SrcMgr,
                &MCOptions);
  std::unique_ptr<MCObjectFileInfo> MOFI(
      TheTarget->createMCObjectFileInfo(Ctx, /*PIC=*/false,
                                        /*LargeCodeModel=*/false));
  Ctx.setObjectFileInfo(MOFI.get());

  std::unique_ptr<MCInstrInfo> MCII(TheTarget->createMCInstrInfo());
  if (!MCII) {
    WithColor::error(errs(), ProgName) << "unable to create instruction info\n";
    return 1;
  }

  // Assemble to in-memory buffer
  SmallVector<char, 0> ObjBuffer;
  raw_svector_ostream ObjOS(ObjBuffer);

  MCCodeEmitter *CE = TheTarget->createMCCodeEmitter(*MCII, Ctx);
  MCAsmBackend *MAB = TheTarget->createMCAsmBackend(*STI, *MRI, MCOptions);
  std::unique_ptr<MCStreamer> Str(TheTarget->createMCObjectStreamer(
      TheTriple, Ctx, std::unique_ptr<MCAsmBackend>(MAB),
      MAB->createObjectWriter(ObjOS), std::unique_ptr<MCCodeEmitter>(CE),
      *STI));

  // Create parser
  std::unique_ptr<MCAsmParser> Parser(
      createMCAsmParser(SrcMgr, Ctx, *Str, *MAI));
  std::unique_ptr<MCTargetAsmParser> TAP(
      TheTarget->createMCAsmParser(*STI, *Parser, *MCII, MCOptions));

  if (!TAP) {
    WithColor::error(errs(), ProgName)
        << "this target does not support assembly parsing\n";
    return 1;
  }

  int SymbolResult = fillCommandLineSymbols(*Parser);
  if (SymbolResult)
    return SymbolResult;

  Parser->setTargetParser(*TAP);

  // Configure lexer
  if (LexMotorolaIntegers.getNumOccurrences() > 0)
    Parser->getLexer().setLexMotorolaIntegers(LexMotorolaIntegers);

  // Assemble
  int AssembleRes = Parser->Run(/*NoInitialTextSection=*/false);
  if (AssembleRes != 0)
    return AssembleRes;

  // Destroy parser before streamer (parser holds reference to streamer)
  TAP.reset();
  Parser.reset();
  Str.reset();

  // Parse the assembled object
  auto ObjMemBuffer = MemoryBuffer::getMemBuffer(
      StringRef(ObjBuffer.data(), ObjBuffer.size()), "", false);

  Expected<std::unique_ptr<object::ObjectFile>> ObjOrErr =
      object::ObjectFile::createObjectFile(ObjMemBuffer->getMemBufferRef());
  if (!ObjOrErr) {
    WithColor::error(errs(), ProgName)
        << "failed to parse assembled object: "
        << toString(ObjOrErr.takeError()) << "\n";
    return 1;
  }

  return RunObject(ProgName, TheTarget, *STI, **ObjOrErr, Ctx, *MAI, *MCII,
                   *MRI);
}

//===----------------------------------------------------------------------===//
// Main
//===----------------------------------------------------------------------===//

int main(int argc, char **argv) {
  InitLLVM X(argc, argv);

  // Initialize targets
  llvm::InitializeAllTargetInfos();
  llvm::InitializeAllTargetMCs();
  llvm::InitializeAllAsmParsers();
  llvm::InitializeAllDisassemblers();

  cl::AddExtraVersionPrinter(TargetRegistry::printRegisteredTargetsForVersion);
  cl::HideUnrelatedOptions({&EmuCategory, &getColorCategory()});
  cl::ParseCommandLineOptions(argc, argv, "LLVM instruction emulator\n");

  const char *ProgName = argv[0];
  MCTargetOptions MCOptions = mc::InitMCTargetOptionsFromFlags();

  // Read input file
  ErrorOr<std::unique_ptr<MemoryBuffer>> BufferPtr =
      MemoryBuffer::getFileOrSTDIN(InputFilename, /*IsText=*/true);
  if (std::error_code EC = BufferPtr.getError()) {
    WithColor::error(errs(), ProgName)
        << InputFilename << ": " << EC.message() << '\n';
    return 1;
  }
  MemoryBuffer *Buffer = BufferPtr->get();

  // Detect input type
  file_magic Magic = identify_magic(Buffer->getBuffer());
  bool IsObjectFile = Magic.is_object();

  if (IsObjectFile) {
    // Parse object file and auto-detect target
    Expected<std::unique_ptr<object::ObjectFile>> ObjOrErr =
        object::ObjectFile::createObjectFile(Buffer->getMemBufferRef());
    if (!ObjOrErr) {
      WithColor::error(errs(), ProgName)
          << "failed to parse object file: " << toString(ObjOrErr.takeError())
          << "\n";
      return 1;
    }
    object::ObjectFile &Obj = **ObjOrErr;

    // Get target from object file or command line
    Triple TheTriple;
    const Target *TheTarget;

    if (!TripleName.empty()) {
      // User specified triple - use it
      TheTriple = Triple(Triple::normalize(TripleName));
      TheTarget = GetTarget(ProgName, TheTriple);
    } else {
      // Auto-detect from object file
      TheTarget = GetTargetFromObjectFile(ProgName, Obj, TheTriple);
    }

    if (!TheTarget)
      return 1;

    // Create MC infrastructure
    std::unique_ptr<MCRegisterInfo> MRI(
        TheTarget->createMCRegInfo(TheTriple));
    std::unique_ptr<MCAsmInfo> MAI(
        TheTarget->createMCAsmInfo(*MRI, TheTriple, MCOptions));

    SubtargetFeatures Features;
    for (const auto &Attr : MAttrs)
      Features.AddFeature(Attr);

    std::unique_ptr<MCSubtargetInfo> STI(TheTarget->createMCSubtargetInfo(
        TheTriple, MCPU, Features.getString()));

    SourceMgr SrcMgr;
    MCContext Ctx(TheTriple, MAI.get(), MRI.get(), STI.get(), &SrcMgr,
                  &MCOptions);
    std::unique_ptr<MCObjectFileInfo> MOFI(
        TheTarget->createMCObjectFileInfo(Ctx, false, false));
    Ctx.setObjectFileInfo(MOFI.get());

    std::unique_ptr<MCInstrInfo> MCII(TheTarget->createMCInstrInfo());

    return RunObject(ProgName, TheTarget, *STI, Obj, Ctx, *MAI, *MCII, *MRI);
  } else {
    // Assembly source - require triple (or use default)
    Triple TheTriple;
    if (!TripleName.empty()) {
      TheTriple = Triple(Triple::normalize(TripleName));
    } else {
      TheTriple = Triple(sys::getDefaultTargetTriple());
    }

    const Target *TheTarget = GetTarget(ProgName, TheTriple);
    if (!TheTarget)
      return 1;

    // Set up source manager
    SourceMgr SrcMgr;
    SrcMgr.AddNewSourceBuffer(std::move(*BufferPtr), SMLoc());
    SrcMgr.setIncludeDirs(IncludeDirs);

    return AssembleAndRun(ProgName, TheTarget, SrcMgr, TheTriple, MCOptions);
  }
}
