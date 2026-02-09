//===-- ProcessSimulator.cpp ----------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "ProcessSimulator.h"
#include "ThreadSimulator.h"





#include "lldb/Breakpoint/Watchpoint.h"
#include "lldb/Core/Module.h"
#include "lldb/Core/PluginManager.h"
#include "lldb/Symbol/ObjectFile.h"
#include "lldb/Target/Target.h"
#include "lldb/Utility/LLDBLog.h"
#include "lldb/Utility/State.h"

#include "llvm/MC/TargetRegistry.h"
#include "llvm/Object/ObjectFile.h"
#include "llvm/Support/Threading.h"

LLDB_PLUGIN_DEFINE(ProcessSimulator)

using namespace lldb;
using namespace lldb_private;

llvm::StringRef ProcessSimulator::GetPluginDescriptionStatic() {
  return "In-process emulator for debugging embedded targets";
}

void ProcessSimulator::Initialize() {
  static llvm::once_flag g_once_flag;
  llvm::call_once(g_once_flag, []() {
    PluginManager::RegisterPlugin(GetPluginNameStatic(),
                                  GetPluginDescriptionStatic(), CreateInstance);
  });
}

void ProcessSimulator::Terminate() {
  PluginManager::UnregisterPlugin(ProcessSimulator::CreateInstance);
}

ProcessSP ProcessSimulator::CreateInstance(TargetSP target_sp,
                                           ListenerSP listener_sp,
                                           const FileSpec *crash_file,
                                           bool can_connect) {
  // Reject core files and connect mode
  if (crash_file || can_connect)
    return nullptr;

  if (!target_sp)
    return nullptr;

  // Check if an emulator exists for this architecture
  const ArchSpec &arch = target_sp->GetArchitecture();
  llvm::Triple triple = arch.GetTriple();

  std::string error_str;
  const llvm::Target *target =
      llvm::TargetRegistry::lookupTarget(triple, error_str);
  if (!target)
    return nullptr;

  return std::make_shared<ProcessSimulator>(target_sp, listener_sp);
}

ProcessSimulator::ProcessSimulator(TargetSP target_sp, ListenerSP listener_sp)
    : Process(target_sp, listener_sp) {}

ProcessSimulator::~ProcessSimulator() {
  Clear();
  Finalize(true);
}

bool ProcessSimulator::CanDebug(TargetSP target_sp, bool plugin_specified) {
  // For now, require explicit selection via --plugin simulator
  return plugin_specified;
}

bool ProcessSimulator::InitializeEmulator() {
  if (m_emulator_initialized)
    return true;

  const ArchSpec &arch = GetTarget().GetArchitecture();
  llvm::Triple triple = arch.GetTriple();

  std::string error_str;
  const llvm::Target *target =
      llvm::TargetRegistry::lookupTarget(triple, error_str);
  if (!target)
    return false;

  // Create MC infrastructure (pattern from DisassemblerLLVMC.cpp)
  m_reg_info.reset(target->createMCRegInfo(triple));
  if (!m_reg_info)
    return false;

  llvm::MCTargetOptions mc_options;
  m_asm_info.reset(target->createMCAsmInfo(*m_reg_info, triple, mc_options));
  if (!m_asm_info)
    return false;

  std::string cpu = "";
  std::string features = "";
  m_subtarget_info.reset(target->createMCSubtargetInfo(triple, cpu, features));
  if (!m_subtarget_info)
    return false;

  m_mc_context.reset(new llvm::MCContext(triple, m_asm_info.get(),
                                         m_reg_info.get(),
                                         m_subtarget_info.get()));
  if (!m_mc_context)
    return false;

  // Create the emulator
  m_context.reset(target->createEmulator(*m_subtarget_info, *m_mc_context));
  if (!m_context)
    return false;

  // Get address space size from emulator
  unsigned addr_bits = m_context->getAddressBits();
  uint64_t mem_size = 1ULL << addr_bits;

  // Create memory device
  m_memory = std::make_unique<llvm::emu::Memory>(mem_size);

  // Create system and wire up devices
  m_system = std::make_unique<llvm::emu::System>();
  m_system->addDevice(0, mem_size - 1, m_memory.get());
  m_system->addContext(m_context.get());

  m_emulator_initialized = true;
  return true;
}

bool ProcessSimulator::LoadSections(ObjectFile *obj_file) {
  if (!obj_file)
    return false;

  // Use LLVM's object library directly for simple flat iteration
  std::string path = obj_file->GetFileSpec().GetPath();
  auto buf_or_err = llvm::MemoryBuffer::getFile(path);
  if (!buf_or_err)
    return false;

  auto obj_or_err =
      llvm::object::ObjectFile::createObjectFile(buf_or_err->get()->getMemBufferRef());
  if (!obj_or_err) {
    llvm::consumeError(obj_or_err.takeError());
    return false;
  }

  llvm::object::ObjectFile &obj = **obj_or_err;
  for (const llvm::object::SectionRef &section : obj.sections()) {
    // Only load text and data sections
    if (!section.isText() && !section.isData())
      continue;

    // BSS is zero-initialized (memory starts zeroed)
    if (section.isBSS())
      continue;

    auto contents_or_err = section.getContents();
    if (!contents_or_err) {
      llvm::consumeError(contents_or_err.takeError());
      continue;
    }

    llvm::StringRef contents = *contents_or_err;
    uint64_t addr = section.getAddress();

    if (!contents.empty()) {
      m_memory->writeBlock(addr,
                           reinterpret_cast<const uint8_t *>(contents.data()),
                           contents.size());
    }
  }
  return true;
}

Status ProcessSimulator::DoLaunch(Module *exe_module,
                                  ProcessLaunchInfo &launch_info) {
  if (!InitializeEmulator())
    return Status::FromErrorString("Failed to initialize emulator");

  // Load ELF sections into memory
  ObjectFile *obj_file = exe_module ? exe_module->GetObjectFile() : nullptr;
  if (!obj_file)
    return Status::FromErrorString("No object file to load");

  if (!LoadSections(obj_file))
    return Status::FromErrorString("Failed to load sections");

  // Reset CPU (reads reset vector for MOS)
  m_context->reset();

  SetPrivateState(eStateStopped);
  return Status();
}

void ProcessSimulator::DidLaunch() { SetID(1); }

Status ProcessSimulator::DoResume(RunDirection direction) {
  if (direction != RunDirection::eRunForward)
    return Status::FromErrorString("Reverse execution not yet supported");

  m_system->clearStopReason();

  // Check if any thread wants to single-step
  bool single_step = false;
  {
    std::lock_guard<std::recursive_mutex> guard(GetThreadList().GetMutex());
    for (uint32_t i = 0; i < GetThreadList().GetSize(false); ++i) {
      ThreadSP thread_sp = GetThreadList().GetThreadAtIndex(i, false);
      if (thread_sp && thread_sp->GetTemporaryResumeState() == eStateStepping) {
        single_step = true;
        break;
      }
    }
  }

  // Set running state before execution
  SetPrivateState(eStateRunning);

  if (single_step) {
    m_context->step();
    // Set stop reason so CalculateStopInfo can create a trace stop
    m_system->setStopReason(llvm::emu::System::StopReason::SingleStep,
                            m_context->getPC());
  } else {
    m_system->run();

    auto reason = m_system->getStopReason();
    if (reason == llvm::emu::System::StopReason::Halted) {
      SetExitStatus(m_system->getExitCode(), "");
    }
  }

  // Synchronous execution - tell LLDB we stopped
  SetPrivateState(eStateStopped);
  return Status();
}

Status ProcessSimulator::DoDestroy() {
  m_system.reset();
  m_context.reset();
  m_memory.reset();
  m_emulator_initialized = false;
  return Status();
}

void ProcessSimulator::RefreshStateAfterStop() {
  // Nothing to do - emulator state is always current
}

bool ProcessSimulator::IsAlive() {
  return m_emulator_initialized && m_context && !m_context->isHalted();
}

size_t ProcessSimulator::DoReadMemory(addr_t addr, void *buf, size_t size,
                                      Status &error) {
  if (!m_system) {
    error = Status::FromErrorString("Emulator not initialized");
    return 0;
  }

  uint8_t *dst = static_cast<uint8_t *>(buf);
  for (size_t i = 0; i < size; ++i) {
    dst[i] = m_system->read(addr + i);
  }
  return size;
}

size_t ProcessSimulator::DoWriteMemory(addr_t addr, const void *buf,
                                       size_t size, Status &error) {
  if (!m_system) {
    error = Status::FromErrorString("Emulator not initialized");
    return 0;
  }

  const uint8_t *src = static_cast<const uint8_t *>(buf);
  for (size_t i = 0; i < size; ++i) {
    m_system->write(addr + i, src[i]);
  }
  return size;
}

Status ProcessSimulator::EnableBreakpointSite(BreakpointSite *bp_site) {
  if (!bp_site)
    return Status::FromErrorString("Invalid breakpoint site");

  m_system->addBreakpoint(bp_site->GetLoadAddress());
  bp_site->SetEnabled(true);
  return Status();
}

Status ProcessSimulator::DisableBreakpointSite(BreakpointSite *bp_site) {
  if (!bp_site)
    return Status::FromErrorString("Invalid breakpoint site");

  m_system->removeBreakpoint(bp_site->GetLoadAddress());
  bp_site->SetEnabled(false);
  return Status();
}

Status ProcessSimulator::EnableWatchpoint(WatchpointSP wp_sp, bool notify) {
  if (!wp_sp)
    return Status::FromErrorString("Invalid watchpoint");

  llvm::emu::System::WatchType type;
  if (wp_sp->WatchpointRead() && wp_sp->WatchpointWrite())
    type = llvm::emu::System::WatchType::ReadWrite;
  else if (wp_sp->WatchpointWrite())
    type = llvm::emu::System::WatchType::Write;
  else
    type = llvm::emu::System::WatchType::Read;

  m_system->addWatchpoint(wp_sp->GetLoadAddress(), wp_sp->GetByteSize(), type);
  wp_sp->SetEnabled(true, notify);
  return Status();
}

Status ProcessSimulator::DisableWatchpoint(WatchpointSP wp_sp, bool notify) {
  if (!wp_sp)
    return Status::FromErrorString("Invalid watchpoint");

  m_system->removeWatchpoint(wp_sp->GetLoadAddress());
  wp_sp->SetEnabled(false, notify);
  return Status();
}

bool ProcessSimulator::DoUpdateThreadList(ThreadList &old_thread_list,
                                          ThreadList &new_thread_list) {
  if (!m_system)
    return false;

  size_t ctx_count = m_system->getContextCount();
  for (size_t i = 0; i < ctx_count; ++i) {
    llvm::emu::Context *ctx = m_system->getContext(i);
    if (!ctx)
      continue;

    tid_t tid = i + 1;

    ThreadSP thread_sp = old_thread_list.FindThreadByID(tid, false);
    if (!thread_sp) {
      thread_sp = std::make_shared<ThreadSimulator>(*this, tid, ctx);
    }
    new_thread_list.AddThread(thread_sp);
  }
  return true;
}
