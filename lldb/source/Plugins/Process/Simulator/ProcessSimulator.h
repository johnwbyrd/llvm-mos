//===-- ProcessSimulator.h --------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLDB_SOURCE_PLUGINS_PROCESS_SIMULATOR_PROCESSSIMULATOR_H
#define LLDB_SOURCE_PLUGINS_PROCESS_SIMULATOR_PROCESSSIMULATOR_H

#include "lldb/Target/DynamicRegisterInfo.h"
#include "lldb/Target/Process.h"
#include "lldb/Utility/Status.h"

#include "llvm/Emulator/Context.h"
#include "llvm/Emulator/System.h"
#include "llvm/MC/MCAsmInfo.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/MC/MCSubtargetInfo.h"

#include <memory>

namespace lldb_private {

class ProcessSimulator : public Process {
public:
  static lldb::ProcessSP CreateInstance(lldb::TargetSP target_sp,
                                        lldb::ListenerSP listener_sp,
                                        const FileSpec *crash_file_path,
                                        bool can_connect);
  static void Initialize();
  static void Terminate();
  static llvm::StringRef GetPluginNameStatic() { return "simulator"; }
  static llvm::StringRef GetPluginDescriptionStatic();

  ProcessSimulator(lldb::TargetSP target_sp, lldb::ListenerSP listener_sp);
  ~ProcessSimulator() override;

  llvm::StringRef GetPluginName() override { return GetPluginNameStatic(); }

  bool CanDebug(lldb::TargetSP target_sp, bool plugin_specified) override;
  Status DoLaunch(Module *exe_module, ProcessLaunchInfo &launch_info) override;
  void DidLaunch() override;
  Status DoResume(lldb::RunDirection direction) override;
  Status DoDestroy() override;
  void RefreshStateAfterStop() override;
  bool IsAlive() override;
  bool SupportsReverseDirection() override { return true; }

  size_t DoReadMemory(lldb::addr_t addr, void *buf, size_t size,
                      Status &error) override;
  size_t DoWriteMemory(lldb::addr_t addr, const void *buf, size_t size,
                       Status &error) override;

  Status EnableBreakpointSite(BreakpointSite *bp_site) override;
  Status DisableBreakpointSite(BreakpointSite *bp_site) override;
  Status EnableWatchpoint(lldb::WatchpointSP wp_sp, bool notify) override;
  Status DisableWatchpoint(lldb::WatchpointSP wp_sp, bool notify) override;

  llvm::emu::System *GetSystem() { return m_system.get(); }
  llvm::emu::Context *GetEmulatorContext() { return m_context.get(); }
  const llvm::MCRegisterInfo *GetMCRegisterInfo() { return m_reg_info_external; }
  std::shared_ptr<DynamicRegisterInfo> GetRegisterInfo();
  bool IsAtHistoryBoundary() const { return m_at_history_boundary; }

protected:
  bool DoUpdateThreadList(ThreadList &old_thread_list,
                          ThreadList &new_thread_list) override;

private:
  bool InitializeEmulator();
  bool LoadSections(ObjectFile *obj_file);
  uint32_t GetRegisterSizeInBytes(llvm::MCRegister Reg) const;

  // MC infrastructure - m_reg_info only used if ABI doesn't provide one
  std::unique_ptr<llvm::MCRegisterInfo> m_reg_info;
  const llvm::MCRegisterInfo *m_reg_info_external = nullptr;
  std::unique_ptr<llvm::MCAsmInfo> m_asm_info;
  std::unique_ptr<llvm::MCSubtargetInfo> m_subtarget_info;
  std::unique_ptr<llvm::MCContext> m_mc_context;

  // Register info built from ABI
  std::shared_ptr<DynamicRegisterInfo> m_register_info_sp;

  // Emulator state
  std::unique_ptr<llvm::emu::System> m_system;
  std::unique_ptr<llvm::emu::Context> m_context;

  bool m_emulator_initialized = false;

  // Reverse execution state
  size_t m_current_checkpoint_idx = 0;
  bool m_at_history_boundary = false;
};

} // namespace lldb_private

#endif // LLDB_SOURCE_PLUGINS_PROCESS_SIMULATOR_PROCESSSIMULATOR_H
