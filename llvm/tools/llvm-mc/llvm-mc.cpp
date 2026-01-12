//===-- llvm-mc.cpp - Machine Code Hacking Driver ---------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This utility is a simple driver that allows command line hacking on machine
// code.
//
//===----------------------------------------------------------------------===//

#include "Disassembler.h"
#include "llvm/ADT/ScopeExit.h"
#include "llvm/DWARFCFIChecker/DWARFCFIFunctionFrameAnalyzer.h"
#include "llvm/DWARFCFIChecker/DWARFCFIFunctionFrameStreamer.h"
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
#include "llvm/Support/Compression.h"
#include "llvm/Support/FileUtilities.h"
#include "llvm/Support/FormattedStream.h"
#include "llvm/Support/InitLLVM.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/TimeProfiler.h"
#include "llvm/Support/ToolOutputFile.h"
#include "llvm/Support/VirtualFileSystem.h"
#include "llvm/Support/WithColor.h"
#include "llvm/TargetParser/Host.h"
#include <memory>

using namespace llvm;

static mc::RegisterMCTargetOptionsFlags MOF;

static cl::OptionCategory MCCategory("MC Options");

static cl::opt<std::string> InputFilename(cl::Positional,
                                          cl::desc("<input file>"),
                                          cl::init("-"), cl::cat(MCCategory));

static cl::list<std::string> InstPrinterOptions("M",
                                                cl::desc("InstPrinter options"),
                                                cl::cat(MCCategory));

static cl::opt<std::string> OutputFilename("o", cl::desc("Output filename"),
                                           cl::value_desc("filename"),
                                           cl::init("-"), cl::cat(MCCategory));

static cl::opt<std::string> SplitDwarfFile("split-dwarf-file",
                                           cl::desc("DWO output filename"),
                                           cl::value_desc("filename"),
                                           cl::cat(MCCategory));

static cl::opt<bool> ShowEncoding("show-encoding",
                                  cl::desc("Show instruction encodings"),
                                  cl::cat(MCCategory));

static cl::opt<DebugCompressionType> CompressDebugSections(
    "compress-debug-sections", cl::ValueOptional,
    cl::init(DebugCompressionType::None),
    cl::desc("Choose DWARF debug sections compression:"),
    cl::values(clEnumValN(DebugCompressionType::None, "none", "No compression"),
               clEnumValN(DebugCompressionType::Zlib, "zlib", "Use zlib"),
               clEnumValN(DebugCompressionType::Zstd, "zstd", "Use zstd")),
    cl::cat(MCCategory));

static cl::opt<bool>
    ShowInst("show-inst", cl::desc("Show internal instruction representation"),
             cl::cat(MCCategory));

static cl::opt<bool>
    ShowInstOperands("show-inst-operands",
                     cl::desc("Show instructions operands as parsed"),
                     cl::cat(MCCategory));

static cl::opt<unsigned>
    OutputAsmVariant("output-asm-variant",
                     cl::desc("Syntax variant to use for output printing"),
                     cl::cat(MCCategory));

static cl::opt<bool>
    PrintImmHex("print-imm-hex", cl::init(false),
                cl::desc("Prefer hex format for immediate values"),
                cl::cat(MCCategory));

static cl::opt<bool>
    HexBytes("hex",
             cl::desc("Take raw hexadecimal bytes as input for disassembly. "
                      "Whitespace is ignored"),
             cl::cat(MCCategory));

static cl::list<std::string>
    DefineSymbol("defsym",
                 cl::desc("Defines a symbol to be an integer constant"),
                 cl::cat(MCCategory));

static cl::opt<bool>
    PreserveComments("preserve-comments",
                     cl::desc("Preserve Comments in outputted assembly"),
                     cl::cat(MCCategory));

static cl::opt<unsigned> CommentColumn("comment-column",
                                       cl::desc("Asm comments indentation"),
                                       cl::init(40));

enum OutputFileType {
  OFT_Null,
  OFT_AssemblyFile,
  OFT_ObjectFile
};
static cl::opt<OutputFileType>
    FileType("filetype", cl::init(OFT_AssemblyFile),
             cl::desc("Choose an output file type:"),
             cl::values(clEnumValN(OFT_AssemblyFile, "asm",
                                   "Emit an assembly ('.s') file"),
                        clEnumValN(OFT_Null, "null",
                                   "Don't emit anything (for timing purposes)"),
                        clEnumValN(OFT_ObjectFile, "obj",
                                   "Emit a native object ('.o') file")),
             cl::cat(MCCategory));

static cl::list<std::string> IncludeDirs("I",
                                         cl::desc("Directory of include files"),
                                         cl::value_desc("directory"),
                                         cl::Prefix, cl::cat(MCCategory));

static cl::opt<std::string>
    ArchName("arch",
             cl::desc("Target arch to assemble for, "
                      "see -version for available targets"),
             cl::cat(MCCategory));

static cl::opt<std::string>
    TripleName("triple",
               cl::desc("Target triple to assemble for, "
                        "see -version for available targets"),
               cl::cat(MCCategory));

static cl::opt<std::string>
    MCPU("mcpu",
         cl::desc("Target a specific cpu type (-mcpu=help for details)"),
         cl::value_desc("cpu-name"), cl::init(""), cl::cat(MCCategory));

static cl::list<std::string>
    MAttrs("mattr", cl::CommaSeparated,
           cl::desc("Target specific attributes (-mattr=help for details)"),
           cl::value_desc("a1,+a2,-a3,..."), cl::cat(MCCategory));

static cl::opt<bool> PIC("position-independent",
                         cl::desc("Position independent"), cl::init(false),
                         cl::cat(MCCategory));

static cl::opt<bool>
    LargeCodeModel("large-code-model",
                   cl::desc("Create cfi directives that assume the code might "
                            "be more than 2gb away"),
                   cl::cat(MCCategory));

static cl::opt<bool>
    NoInitialTextSection("n",
                         cl::desc("Don't assume assembly file starts "
                                  "in the text section"),
                         cl::cat(MCCategory));

static cl::opt<bool>
    GenDwarfForAssembly("g",
                        cl::desc("Generate dwarf debugging info for assembly "
                                 "source files"),
                        cl::cat(MCCategory));

static cl::opt<std::string>
    DebugCompilationDir("fdebug-compilation-dir",
                        cl::desc("Specifies the debug info's compilation dir"),
                        cl::cat(MCCategory));

static cl::list<std::string> DebugPrefixMap(
    "fdebug-prefix-map", cl::desc("Map file source paths in debug info"),
    cl::value_desc("= separated key-value pairs"), cl::cat(MCCategory));

static cl::opt<std::string> MainFileName(
    "main-file-name",
    cl::desc("Specifies the name we should consider the input file"),
    cl::cat(MCCategory));

static cl::opt<bool> LexMasmIntegers(
    "masm-integers",
    cl::desc("Enable binary and hex masm integers (0b110 and 0ABCh)"),
    cl::cat(MCCategory));

static cl::opt<bool> LexMasmHexFloats(
    "masm-hexfloats",
    cl::desc("Enable MASM-style hex float initializers (3F800000r)"),
    cl::cat(MCCategory));

static cl::opt<bool> LexMotorolaIntegers(
    "motorola-integers",
    cl::desc("Enable binary and hex Motorola integers (%110 and $ABC)"),
    cl::cat(MCCategory));

static cl::opt<bool> NoExecStack("no-exec-stack",
                                 cl::desc("File doesn't need an exec stack"),
                                 cl::cat(MCCategory));

static cl::opt<bool> ValidateCFI("validate-cfi",
                                 cl::desc("Validate the CFI directives"),
                                 cl::cat(MCCategory));

enum ActionType {
  AC_AsLex,
  AC_Assemble,
  AC_Disassemble,
  AC_MDisassemble,
  AC_CDisassemble,
  AC_Run,
};

static cl::opt<ActionType> Action(
    cl::desc("Action to perform:"), cl::init(AC_Assemble),
    cl::values(clEnumValN(AC_AsLex, "as-lex", "Lex tokens from a .s file"),
               clEnumValN(AC_Assemble, "assemble",
                          "Assemble a .s file (default)"),
               clEnumValN(AC_Disassemble, "disassemble",
                          "Disassemble strings of hex bytes"),
               clEnumValN(AC_MDisassemble, "mdis",
                          "Marked up disassembly of strings of hex bytes"),
               clEnumValN(AC_CDisassemble, "cdis",
                          "Colored disassembly of strings of hex bytes"),
               clEnumValN(AC_Run, "run",
                          "Load and execute an ELF file")),
    cl::cat(MCCategory));

static cl::opt<bool>
    EmulatorTrace("trace", cl::desc("Trace instruction execution"),
                  cl::cat(MCCategory));

static cl::opt<bool>
    EmulatorSemihost("semihost",
                     cl::desc("Enable semihosting for host I/O"),
                     cl::cat(MCCategory));

static cl::opt<bool>
    EmulatorSemihostInsecure("semihost-insecure",
                             cl::desc("Allow unrestricted filesystem access (DANGEROUS)"),
                             cl::cat(MCCategory));

enum TraceFormatType {
  TF_Text,
  TF_JSON,
  TF_VCD,
};

static cl::opt<TraceFormatType> TraceFormat(
    "trace-format", cl::init(TF_Text),
    cl::desc("Choose trace output format:"),
    cl::values(clEnumValN(TF_Text, "text",
                          "Tab-separated text (default)"),
               clEnumValN(TF_JSON, "json",
                          "JSON Lines (one object per line)"),
               clEnumValN(TF_VCD, "vcd",
                          "VCD (Value Change Dump) for waveform viewers")),
    cl::cat(MCCategory));

static cl::opt<unsigned>
    NumBenchmarkRuns("runs", cl::desc("Number of runs for benchmarking"),
                     cl::cat(MCCategory));

static cl::opt<bool> TimeTrace("time-trace", cl::desc("Record time trace"));

static cl::opt<unsigned> TimeTraceGranularity(
    "time-trace-granularity",
    cl::desc(
        "Minimum time granularity (in microseconds) traced by time profiler"),
    cl::init(500), cl::Hidden);

static cl::opt<std::string>
    TimeTraceFile("time-trace-file",
                  cl::desc("Specify time trace file destination"),
                  cl::value_desc("filename"));

static const Target *GetTarget(const char *ProgName) {
  // Figure out the target triple.
  if (TripleName.empty())
    TripleName = sys::getDefaultTargetTriple();
  Triple TheTriple(Triple::normalize(TripleName));

  // Get the target specific parser.
  std::string Error;
  const Target *TheTarget = TargetRegistry::lookupTarget(ArchName, TheTriple,
                                                         Error);
  if (!TheTarget) {
    WithColor::error(errs(), ProgName) << Error;
    return nullptr;
  }

  // Update the triple name and return the found target.
  TripleName = TheTriple.getTriple();
  return TheTarget;
}

static std::unique_ptr<ToolOutputFile> GetOutputStream(StringRef Path,
    sys::fs::OpenFlags Flags) {
  std::error_code EC;
  auto Out = std::make_unique<ToolOutputFile>(Path, EC, Flags);
  if (EC) {
    WithColor::error() << EC.message() << '\n';
    return nullptr;
  }

  return Out;
}

static std::string DwarfDebugFlags;
static void setDwarfDebugFlags(int argc, char **argv) {
  if (!getenv("RC_DEBUG_OPTIONS"))
    return;
  for (int i = 0; i < argc; i++) {
    DwarfDebugFlags += argv[i];
    if (i + 1 < argc)
      DwarfDebugFlags += " ";
  }
}

static std::string DwarfDebugProducer;
static void setDwarfDebugProducer() {
  if(!getenv("DEBUG_PRODUCER"))
    return;
  DwarfDebugProducer += getenv("DEBUG_PRODUCER");
}

static int AsLexInput(SourceMgr &SrcMgr, MCAsmInfo &MAI,
                      raw_ostream &OS) {

  AsmLexer Lexer(MAI);
  Lexer.setBuffer(SrcMgr.getMemoryBuffer(SrcMgr.getMainFileID())->getBuffer());

  bool Error = false;
  while (Lexer.Lex().isNot(AsmToken::Eof)) {
    Lexer.getTok().dump(OS);
    OS << "\n";
    if (Lexer.getTok().getKind() == AsmToken::Error)
      Error = true;
  }

  return Error;
}

static int fillCommandLineSymbols(MCAsmParser &Parser) {
  for (auto &I: DefineSymbol) {
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

static int AssembleInput(const char *ProgName, const Target *TheTarget,
                         SourceMgr &SrcMgr, MCContext &Ctx, MCStreamer &Str,
                         MCAsmInfo &MAI, MCSubtargetInfo &STI,
                         MCInstrInfo &MCII, MCTargetOptions const &MCOptions) {
  std::unique_ptr<MCAsmParser> Parser(
      createMCAsmParser(SrcMgr, Ctx, Str, MAI));
  std::unique_ptr<MCTargetAsmParser> TAP(
      TheTarget->createMCAsmParser(STI, *Parser, MCII, MCOptions));

  if (!TAP) {
    WithColor::error(errs(), ProgName)
        << "this target does not support assembly parsing.\n";
    return 1;
  }

  int SymbolResult = fillCommandLineSymbols(*Parser);
  if(SymbolResult)
    return SymbolResult;
  Parser->setShowParsedOperands(ShowInstOperands);
  Parser->setTargetParser(*TAP);
  Parser->getLexer().setLexMasmIntegers(LexMasmIntegers);
  Parser->getLexer().setLexMasmHexFloats(LexMasmHexFloats);
  Parser->getLexer().setLexMotorolaIntegers(LexMotorolaIntegers);

  int Res = Parser->Run(NoInitialTextSection);

  return Res;
}

//===----------------------------------------------------------------------===//
// Object File Execution (--run)
//===----------------------------------------------------------------------===//

static int RunObject(const char *ProgName, const Target *TheTarget,
                     const MCSubtargetInfo &STI, MemoryBuffer &Buffer,
                     MCContext &Ctx, const MCAsmInfo &MAI,
                     const MCInstrInfo &MCII, const MCRegisterInfo &MRI) {
  // Parse the object file (auto-detects format from magic bytes)
  Expected<std::unique_ptr<object::ObjectFile>> ObjOrErr =
      object::ObjectFile::createObjectFile(Buffer.getMemBufferRef());
  if (!ObjOrErr) {
    WithColor::error(errs(), ProgName)
        << "failed to parse object file: " << toString(ObjOrErr.takeError())
        << "\n";
    return 1;
  }
  object::ObjectFile &Obj = **ObjOrErr;

  // Determine memory size from address width (cap at 4GB)
  unsigned AddrBits = Obj.getBytesInAddress() * 8;
  uint64_t MemSize = (AddrBits >= 32) ? (4ULL * 1024 * 1024 * 1024)
                                      : (1ULL << AddrBits);

  // Create system with RAM covering the entire address space
  emu::System Sys;
  auto RAM = std::make_unique<emu::Memory>(MemSize);

  // Load sections from object file into RAM
  // Only load runtime sections (text, data, bss), skip debug/metadata
  for (const object::SectionRef &Section : Obj.sections()) {
    // Skip non-runtime sections (debug info, symbol tables, etc.)
    if (!Section.isText() && !Section.isData() && !Section.isBSS())
      continue;

    Expected<StringRef> ContentsOrErr = Section.getContents();
    if (!ContentsOrErr) {
      WithColor::error(errs(), ProgName)
          << "failed to read section: " << toString(ContentsOrErr.takeError())
          << "\n";
      return 1;
    }
    StringRef Contents = *ContentsOrErr;
    uint64_t Addr = Section.getAddress();

    // Copy section data to RAM
    RAM->writeBlock(Addr, reinterpret_cast<const uint8_t *>(Contents.data()),
                    Contents.size());
  }

  // Add RAM to system
  Sys.addOwnedDevice(0, MemSize - 1, std::move(RAM));

  // Add semihosting device if requested
  std::unique_ptr<emu::Semihost> Semihost;
  if (EmulatorSemihost || EmulatorSemihostInsecure) {
    if (EmulatorSemihostInsecure) {
      Semihost = emu::Semihost::createInsecure(Sys);
    } else {
      // Use current working directory as sandbox
      SmallString<256> SandboxDir;
      sys::fs::current_path(SandboxDir);
      Semihost = emu::Semihost::create(Sys, std::string(SandboxDir));
    }

    // Compute semihost address per ZBC specification:
    // semihost_base = 2^n - 2^(n/2) - 32, where n = address bus width
    // This places the device at a proportional distance from the top of
    // the address space, leaving room for other peripherals above it.
    uint64_t SemihostBase = MemSize - (1ULL << (AddrBits / 2)) - 32;
    uint64_t SemihostEnd = SemihostBase + 31;
    Sys.addDevice(SemihostBase, SemihostEnd, Semihost.get());
  }

  // Create the target-specific emulator
  std::unique_ptr<emu::Context> Emu(TheTarget->createEmulator(STI, Ctx));
  if (!Emu) {
    WithColor::error(errs(), ProgName)
        << "no emulator available for target " << TheTarget->getName() << "\n";
    return 1;
  }

  // Register emulator with system
  Sys.addContext(Emu.get());

  // Hook up semihost exit callback to halt emulator
  if (Semihost) {
    Semihost->setExitCallback(
        [&Emu](unsigned Reason, unsigned Subcode) {
          // Reason 0x20026 = ADP_Stopped_ApplicationExit, Subcode is exit code
          // For simplicity, use Subcode as exit code for all reasons
          Emu->halt(static_cast<int>(Subcode));
        });
  }

  // Set PC to entry point (if available) or reset vector
  // For now, let the CPU reset itself (which reads the reset vector)
  Emu->reset();

  // Configure tracing if requested
  std::unique_ptr<MCInstPrinter> IP;
  std::unique_ptr<emu::TraceWriter> TraceWriter;
  if (EmulatorTrace) {
    IP.reset(TheTarget->createMCInstPrinter(STI.getTargetTriple(),
                                            0, MAI, MCII, MRI));

    // Create the appropriate trace writer based on format
    switch (TraceFormat) {
    case TF_Text:
      TraceWriter = std::make_unique<emu::TextTraceWriter>(errs(), IP.get(), &STI);
      break;
    case TF_JSON:
      TraceWriter = std::make_unique<emu::JSONTraceWriter>(errs(), IP.get(), &STI);
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

  // Run until halt
  if (!Emu->run()) {
    if (TraceWriter)
      TraceWriter->traceEnd();
    WithColor::error(errs(), ProgName) << "emulator execution failed\n";
    return 1;
  }

  if (TraceWriter)
    TraceWriter->traceEnd();

  return Emu->getExitCode();
}

int main(int argc, char **argv) {
  InitLLVM X(argc, argv);

  // Initialize targets and assembly printers/parsers.
  llvm::InitializeAllTargetInfos();
  llvm::InitializeAllTargetMCs();
  llvm::InitializeAllAsmParsers();
  llvm::InitializeAllDisassemblers();

  // Register the target printer for --version.
  cl::AddExtraVersionPrinter(TargetRegistry::printRegisteredTargetsForVersion);

  cl::HideUnrelatedOptions({&MCCategory, &getColorCategory()});
  cl::ParseCommandLineOptions(argc, argv, "llvm machine code playground\n");

  if (TimeTrace)
    timeTraceProfilerInitialize(TimeTraceGranularity, argv[0]);

  llvm::scope_exit TimeTraceScopeExit([]() {
    if (!TimeTrace)
      return;
    if (auto E = timeTraceProfilerWrite(TimeTraceFile, OutputFilename)) {
      logAllUnhandledErrors(std::move(E), errs());
      return;
    }
    timeTraceProfilerCleanup();
  });

  MCTargetOptions MCOptions = mc::InitMCTargetOptionsFromFlags();
  MCOptions.CompressDebugSections = CompressDebugSections.getValue();
  MCOptions.ShowMCInst = ShowInst;
  MCOptions.AsmVerbose = true;
  MCOptions.MCUseDwarfDirectory = MCTargetOptions::EnableDwarfDirectory;
  MCOptions.InstPrinterOptions = InstPrinterOptions;

  setDwarfDebugFlags(argc, argv);
  setDwarfDebugProducer();

  const char *ProgName = argv[0];
  const Target *TheTarget = GetTarget(ProgName);
  if (!TheTarget)
    return 1;
  // Now that GetTarget() has (potentially) replaced TripleName, it's safe to
  // construct the Triple object.
  Triple TheTriple(TripleName);

  ErrorOr<std::unique_ptr<MemoryBuffer>> BufferPtr =
      MemoryBuffer::getFileOrSTDIN(InputFilename, /*IsText=*/true);
  if (std::error_code EC = BufferPtr.getError()) {
    WithColor::error(errs(), ProgName)
        << InputFilename << ": " << EC.message() << '\n';
    return 1;
  }
  MemoryBuffer *Buffer = BufferPtr->get();

  SourceMgr SrcMgr;

  // Tell SrcMgr about this buffer, which is what the parser will pick up.
  SrcMgr.AddNewSourceBuffer(std::move(*BufferPtr), SMLoc());

  // Record the location of the include directories so that the lexer can find
  // it later.
  SrcMgr.setIncludeDirs(IncludeDirs);
  SrcMgr.setVirtualFileSystem(vfs::getRealFileSystem());

  std::unique_ptr<MCRegisterInfo> MRI(TheTarget->createMCRegInfo(TheTriple));
  assert(MRI && "Unable to create target register info!");

  std::unique_ptr<MCAsmInfo> MAI(
      TheTarget->createMCAsmInfo(*MRI, TheTriple, MCOptions));
  assert(MAI && "Unable to create target asm info!");

  if (CompressDebugSections != DebugCompressionType::None) {
    if (const char *Reason = compression::getReasonIfUnsupported(
            compression::formatFor(CompressDebugSections))) {
      WithColor::error(errs(), ProgName)
          << "--compress-debug-sections: " << Reason;
      return 1;
    }
  }
  MAI->setPreserveAsmComments(PreserveComments);
  MAI->setCommentColumn(CommentColumn);

  // Package up features to be passed to target/subtarget
  SubtargetFeatures Features;
  std::string FeaturesStr;

  // Replace -mcpu=native with Host CPU and features.
  if (MCPU == "native") {
    MCPU = std::string(llvm::sys::getHostCPUName());

    llvm::StringMap<bool> TargetFeatures = llvm::sys::getHostCPUFeatures();
    for (auto const &[FeatureName, IsSupported] : TargetFeatures)
      Features.AddFeature(FeatureName, IsSupported);
  }

  // Handle features passed to target/subtarget.
  for (unsigned i = 0; i != MAttrs.size(); ++i)
    Features.AddFeature(MAttrs[i]);
  FeaturesStr = Features.getString();

  std::unique_ptr<MCSubtargetInfo> STI(
      TheTarget->createMCSubtargetInfo(TheTriple, MCPU, FeaturesStr));
  if (!STI) {
    WithColor::error(errs(), ProgName) << "unable to create subtarget info\n";
    return 1;
  }

  // FIXME: This is not pretty. MCContext has a ptr to MCObjectFileInfo and
  // MCObjectFileInfo needs a MCContext reference in order to initialize itself.
  MCContext Ctx(TheTriple, MAI.get(), MRI.get(), STI.get(), &SrcMgr,
                &MCOptions);
  std::unique_ptr<MCObjectFileInfo> MOFI(
      TheTarget->createMCObjectFileInfo(Ctx, PIC, LargeCodeModel));
  Ctx.setObjectFileInfo(MOFI.get());

  Ctx.setGenDwarfForAssembly(GenDwarfForAssembly);
  // Default to 4 for dwarf version.
  unsigned DwarfVersion = MCOptions.DwarfVersion ? MCOptions.DwarfVersion : 4;
  if (DwarfVersion < 2 || DwarfVersion > 5) {
    errs() << ProgName << ": Dwarf version " << DwarfVersion
           << " is not supported." << '\n';
    return 1;
  }
  Ctx.setDwarfVersion(DwarfVersion);
  if (MCOptions.Dwarf64) {
    // The 64-bit DWARF format was introduced in DWARFv3.
    if (DwarfVersion < 3) {
      errs() << ProgName
             << ": the 64-bit DWARF format is not supported for DWARF versions "
                "prior to 3\n";
      return 1;
    }
    // 32-bit targets don't support DWARF64, which requires 64-bit relocations.
    if (MAI->getCodePointerSize() < 8) {
      errs() << ProgName
             << ": the 64-bit DWARF format is only supported for 64-bit "
                "targets\n";
      return 1;
    }
    // If needsDwarfSectionOffsetDirective is true, we would eventually call
    // MCStreamer::emitSymbolValue() with IsSectionRelative = true, but that
    // is supported only for 4-byte long references.
    if (MAI->needsDwarfSectionOffsetDirective()) {
      errs() << ProgName << ": the 64-bit DWARF format is not supported for "
             << TheTriple.normalize() << "\n";
      return 1;
    }
    Ctx.setDwarfFormat(dwarf::DWARF64);
  }
  if (!DwarfDebugFlags.empty())
    Ctx.setDwarfDebugFlags(StringRef(DwarfDebugFlags));
  if (!DwarfDebugProducer.empty())
    Ctx.setDwarfDebugProducer(StringRef(DwarfDebugProducer));
  if (!DebugCompilationDir.empty())
    Ctx.setCompilationDir(DebugCompilationDir);
  else {
    // If no compilation dir is set, try to use the current directory.
    SmallString<128> CWD;
    if (!sys::fs::current_path(CWD))
      Ctx.setCompilationDir(CWD);
  }
  for (const auto &Arg : DebugPrefixMap) {
    const auto &KV = StringRef(Arg).split('=');
    Ctx.addDebugPrefixMapEntry(std::string(KV.first), std::string(KV.second));
  }
  if (!MainFileName.empty())
    Ctx.setMainFileName(MainFileName);
  if (GenDwarfForAssembly)
    Ctx.setGenDwarfRootFile(InputFilename, Buffer->getBuffer());

  sys::fs::OpenFlags Flags = (FileType == OFT_AssemblyFile)
                                 ? sys::fs::OF_TextWithCRLF
                                 : sys::fs::OF_None;
  std::unique_ptr<ToolOutputFile> Out = GetOutputStream(OutputFilename, Flags);
  if (!Out)
    return 1;

  std::unique_ptr<ToolOutputFile> DwoOut;
  if (!SplitDwarfFile.empty()) {
    if (FileType != OFT_ObjectFile) {
      WithColor::error() << "dwo output only supported with object files\n";
      return 1;
    }
    DwoOut = GetOutputStream(SplitDwarfFile, sys::fs::OF_None);
    if (!DwoOut)
      return 1;
  }

  std::unique_ptr<buffer_ostream> BOS;
  raw_pwrite_stream *OS = &Out->os();
  std::unique_ptr<MCStreamer> Str;

  std::unique_ptr<MCInstrInfo> MCII(TheTarget->createMCInstrInfo());
  assert(MCII && "Unable to create instruction info!");

  std::unique_ptr<MCInstPrinter> IP;
  if (ValidateCFI) {
    // TODO: The DWARF CFI checker support for emitting anything other than
    // errors and warnings has not been implemented yet. Because of this, it is
    // assert-checked that the filetype output is null.
    assert(FileType == OFT_Null);
    auto FFA = std::make_unique<CFIFunctionFrameAnalyzer>(Ctx, *MCII);
    auto FFS = std::make_unique<CFIFunctionFrameStreamer>(Ctx, std::move(FFA));
    TheTarget->createNullTargetStreamer(*FFS);
    Str = std::move(FFS);
  } else if (FileType == OFT_AssemblyFile) {
    IP.reset(TheTarget->createMCInstPrinter(
        Triple(TripleName), OutputAsmVariant, *MAI, *MCII, *MRI));

    if (!IP) {
      WithColor::error()
          << "unable to create instruction printer for target triple '"
          << TheTriple.normalize() << "' with assembly variant "
          << OutputAsmVariant << ".\n";
      return 1;
    }

    for (StringRef Opt : InstPrinterOptions)
      if (!IP->applyTargetSpecificCLOption(Opt)) {
        WithColor::error() << "invalid InstPrinter option '" << Opt << "'\n";
        return 1;
      }

    // Set the display preference for hex vs. decimal immediates.
    IP->setPrintImmHex(PrintImmHex);

    switch (Action) {
    case AC_MDisassemble:
      IP->setUseMarkup(true);
      break;
    case AC_CDisassemble:
      IP->setUseColor(true);
      break;
    default:
      break;
    }

    // Set up the AsmStreamer.
    std::unique_ptr<MCCodeEmitter> CE;
    if (ShowEncoding)
      CE.reset(TheTarget->createMCCodeEmitter(*MCII, Ctx));

    std::unique_ptr<MCAsmBackend> MAB(
        TheTarget->createMCAsmBackend(*STI, *MRI, MCOptions));
    auto FOut = std::make_unique<formatted_raw_ostream>(*OS);
    Str.reset(TheTarget->createAsmStreamer(Ctx, std::move(FOut), std::move(IP),
                                           std::move(CE), std::move(MAB)));

  } else if (FileType == OFT_Null) {
    Str.reset(TheTarget->createNullStreamer(Ctx));
  } else {
    assert(FileType == OFT_ObjectFile && "Invalid file type!");

    if (!Out->os().supportsSeeking()) {
      BOS = std::make_unique<buffer_ostream>(Out->os());
      OS = BOS.get();
    }

    MCCodeEmitter *CE = TheTarget->createMCCodeEmitter(*MCII, Ctx);
    MCAsmBackend *MAB = TheTarget->createMCAsmBackend(*STI, *MRI, MCOptions);
    Str.reset(TheTarget->createMCObjectStreamer(
        TheTriple, Ctx, std::unique_ptr<MCAsmBackend>(MAB),
        DwoOut ? MAB->createDwoObjectWriter(*OS, DwoOut->os())
               : MAB->createObjectWriter(*OS),
        std::unique_ptr<MCCodeEmitter>(CE), *STI));
    if (NoExecStack)
      Str->switchSection(
          Ctx.getAsmInfo()->getStackSection(Ctx, /*Exec=*/false));
    Str->emitVersionForTarget(TheTriple, VersionTuple(), nullptr,
                              VersionTuple());
  }

  int Res = 1;
  bool disassemble = false;
  switch (Action) {
  case AC_AsLex:
    Res = AsLexInput(SrcMgr, *MAI, Out->os());
    break;
  case AC_Assemble:
    Res = AssembleInput(ProgName, TheTarget, SrcMgr, Ctx, *Str, *MAI, *STI,
                        *MCII, MCOptions);
    break;
  case AC_MDisassemble:
  case AC_CDisassemble:
  case AC_Disassemble:
    disassemble = true;
    break;
  case AC_Run:
    Res = RunObject(ProgName, TheTarget, *STI, *Buffer, Ctx, *MAI, *MCII, *MRI);
    break;
  }
  if (disassemble)
    Res = Disassembler::disassemble(*TheTarget, *STI, *Str, *Buffer, SrcMgr,
                                    Ctx, MCOptions, HexBytes, NumBenchmarkRuns);

  // Keep output if no errors.
  if (Res == 0) {
    Out->keep();
    if (DwoOut)
      DwoOut->keep();
  }

  return Res;
}
