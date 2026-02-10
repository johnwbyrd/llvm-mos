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
#include "lldb/Target/ABI.h"
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

Status ProcessSimulator::InitializeEmulator() {
  if (m_emulator_initialized)
    return Status();

  const ArchSpec &arch = GetTarget().GetArchitecture();
  llvm::Triple triple = arch.GetTriple();
  std::string triple_str = triple.getTriple();

  std::string error_str;
  const llvm::Target *target =
      llvm::TargetRegistry::lookupTarget(triple, error_str);
  if (!target)
    return Status::FromErrorStringWithFormat(
        "No target registered for '%s': %s", triple_str.c_str(),
        error_str.c_str());

  // Get MCRegisterInfo from ABI if available, otherwise create our own
  ABISP abi_sp = GetABI();
  if (abi_sp) {
    m_reg_info_external = &abi_sp->GetMCRegisterInfo();
  } else {
    m_reg_info.reset(target->createMCRegInfo(triple));
    if (!m_reg_info)
      return Status::FromErrorStringWithFormat(
          "Failed to create register info for '%s'", triple_str.c_str());
    m_reg_info_external = m_reg_info.get();
  }

  // Still need MCAsmInfo, MCSubtargetInfo, MCContext for createEmulator()
  llvm::MCTargetOptions mc_options;
  m_asm_info.reset(
      target->createMCAsmInfo(*m_reg_info_external, triple, mc_options));
  if (!m_asm_info)
    return Status::FromErrorStringWithFormat(
        "Failed to create asm info for '%s'", triple_str.c_str());

  m_subtarget_info.reset(target->createMCSubtargetInfo(triple, "", ""));
  if (!m_subtarget_info)
    return Status::FromErrorStringWithFormat(
        "Failed to create subtarget info for '%s'", triple_str.c_str());

  m_mc_context = std::make_unique<llvm::MCContext>(
      triple, m_asm_info.get(), m_reg_info_external, m_subtarget_info.get());

  // Create the emulator context
  auto context = std::unique_ptr<llvm::emu::Context>(
      target->createEmulator(*m_subtarget_info, *m_mc_context));
  if (!context)
    return Status::FromErrorStringWithFormat(
        "No emulator available for '%s'", triple_str.c_str());

  // Create system with memory and semihosting
  unsigned addr_bits = context->getAddressBits();
  m_system = llvm::emu::System::create(addr_bits);
  m_system->addContext(context.get());
  m_owned_contexts.push_back(std::move(context));

  m_emulator_initialized = true;
  return Status();
}

bool ProcessSimulator::LoadSections(ObjectFile *obj_file) {
  if (!obj_file)
    return false;

  // Use LLVM's object library directly
  std::string path = obj_file->GetFileSpec().GetPath();
  auto buf_or_err = llvm::MemoryBuffer::getFile(path);
  if (!buf_or_err)
    return false;

  auto obj_or_err = llvm::object::ObjectFile::createObjectFile(
      buf_or_err->get()->getMemBufferRef());
  if (!obj_or_err) {
    llvm::consumeError(obj_or_err.takeError());
    return false;
  }

  // Use shared utility to load sections
  if (auto E = llvm::emu::Memory::loadObject(**obj_or_err, *m_system->getMemory())) {
    llvm::consumeError(std::move(E));
    return false;
  }
  return true;
}

Status ProcessSimulator::DoLaunch(Module *exe_module,
                                  ProcessLaunchInfo &launch_info) {
  if (Status error = InitializeEmulator(); error.Fail())
    return error;

  // Load ELF sections into memory
  ObjectFile *obj_file = exe_module ? exe_module->GetObjectFile() : nullptr;
  if (!obj_file)
    return Status::FromErrorString("No object file to load");

  if (!LoadSections(obj_file))
    return Status::FromErrorString("Failed to load sections");

  // Reset all CPUs
  m_system->reset();

  // Enable recording for reverse debugging
  m_system->enableRecording(true);
  // Create initial checkpoint at program start
  m_system->checkpoint();

  SetPrivateState(eStateStopped);
  return Status();
}

void ProcessSimulator::DidLaunch() { SetID(1); }

Status ProcessSimulator::DoResume(RunDirection direction) {
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

  SetPrivateState(eStateRunning);

  if (direction == RunDirection::eRunForward) {
    if (single_step) {
      m_system->step();
    } else {
      m_system->run();
      if (m_system->getStopReason() == llvm::emu::System::StopReason::Halted) {
        SetExitStatus(m_system->getExitCode(), "");
      }
    }
  } else {
    if (single_step) {
      m_system->stepReverse();
    } else {
      m_system->runReverse();
    }
  }

  SetPrivateState(eStateStopped);
  return Status();
}

Status ProcessSimulator::DoDestroy() {
  m_system.reset();
  m_owned_contexts.clear();
  m_emulator_initialized = false;
  return Status();
}

void ProcessSimulator::RefreshStateAfterStop() {
  // Nothing to do - emulator state is always current
}

bool ProcessSimulator::IsAlive() {
  return m_emulator_initialized && m_system && !m_system->allHalted();
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

  if (m_system)
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

  if (m_system)
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
    tid_t tid = i + 1;

    ThreadSP thread_sp = old_thread_list.FindThreadByID(tid, false);
    if (!thread_sp) {
      thread_sp = std::make_shared<ThreadSimulator>(*this, tid, i);
    }
    new_thread_list.AddThread(thread_sp);
  }
  return true;
}

uint32_t
ProcessSimulator::GetRegisterSizeInBytes(llvm::MCRegister Reg) const {
  if (!m_reg_info_external)
    return 1;

  // Find the smallest register class containing this register
  for (const auto &RC : m_reg_info_external->regclasses()) {
    if (RC.contains(Reg)) {
      unsigned Bits = RC.getSizeInBits();
      if (Bits > 0)
        return (Bits + 7) / 8;
    }
  }
  return 1; // Default to 1 byte
}

std::shared_ptr<DynamicRegisterInfo> ProcessSimulator::GetRegisterInfo() {
  if (m_register_info_sp)
    return m_register_info_sp;

  if (!m_system || m_system->getContextCount() == 0 || !m_reg_info_external)
    return nullptr;

  std::vector<DynamicRegisterInfo::Register> regs;
  unsigned num_regs = m_system->getContext(0)->getNumRegisters();

  for (unsigned dwarf = 0; dwarf < num_regs; ++dwarf) {
    // Map DWARF number to MC register
    auto mc_reg = m_reg_info_external->getLLVMRegNum(dwarf, false);
    if (!mc_reg)
      continue;

    const char *name = m_reg_info_external->getName(*mc_reg);
    if (!name)
      continue;

    DynamicRegisterInfo::Register reg;
    reg.name = ConstString(name);
    reg.byte_size = GetRegisterSizeInBytes(*mc_reg);
    reg.regnum_dwarf = dwarf;
    reg.set_name = ConstString("General Purpose Registers");
    regs.push_back(reg);
  }

  // Let ABI augment with generic register kinds (PC, SP, etc.)
  if (ABISP abi = GetABI())
    abi->AugmentRegisterInfo(regs);

  m_register_info_sp = std::make_shared<DynamicRegisterInfo>();
  m_register_info_sp->SetRegisterInfo(std::move(regs),
                                       GetTarget().GetArchitecture());
  return m_register_info_sp;
}
