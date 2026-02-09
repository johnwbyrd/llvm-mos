//===-- ThreadSimulator.cpp -----------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "ThreadSimulator.h"

#include "ProcessSimulator.h"
#include "RegisterContextEmulator.h"

#include "lldb/Breakpoint/Watchpoint.h"
#include "lldb/Target/ABI.h"
#include "lldb/Target/RegisterContext.h"
#include "lldb/Target/StopInfo.h"
#include "lldb/Target/Unwind.h"

using namespace lldb;
using namespace lldb_private;

ThreadSimulator::ThreadSimulator(ProcessSimulator &process, tid_t tid,
                                 llvm::emu::Context *context)
    : Thread(process, tid), m_context(context) {}

ThreadSimulator::~ThreadSimulator() { DestroyThread(); }

void ThreadSimulator::RefreshStateAfterStop() {
  if (m_reg_context_sp)
    m_reg_context_sp->InvalidateAllRegisters();
}

RegisterContextSP ThreadSimulator::GetRegisterContext() {
  if (!m_reg_context_sp)
    m_reg_context_sp = CreateRegisterContextForFrame(nullptr);
  return m_reg_context_sp;
}

RegisterContextSP
ThreadSimulator::CreateRegisterContextForFrame(StackFrame *frame) {
  uint32_t concrete_frame_idx = 0;
  if (frame)
    concrete_frame_idx = frame->GetConcreteFrameIndex();

  if (concrete_frame_idx == 0) {
    if (!m_reg_context_sp) {
      auto *process = static_cast<ProcessSimulator *>(GetProcess().get());
      auto reg_info_sp = process->GetRegisterInfo();
      if (reg_info_sp) {
        m_reg_context_sp = std::make_shared<RegisterContextEmulator>(
            *this, concrete_frame_idx, *reg_info_sp, m_context);
        // Let ABI wrap with platform-specific enhancements (e.g., imaginary registers)
        if (ABISP abi = process->GetABI())
          m_reg_context_sp = abi->WrapRegisterContext(m_reg_context_sp);
      }
    }
    return m_reg_context_sp;
  }

  return GetUnwinder().CreateRegisterContextForFrame(frame);
}

bool ThreadSimulator::CalculateStopInfo() {
  auto *process = static_cast<ProcessSimulator *>(GetProcess().get());
  if (!process)
    return false;

  llvm::emu::System *sys = process->GetSystem();
  if (!sys)
    return false;

  auto reason = sys->getStopReason();
  StopInfoSP stop_info_sp;

  switch (reason) {
  case llvm::emu::System::StopReason::Breakpoint: {
    addr_t stop_addr = sys->getStopAddress();
    BreakpointSiteSP bp_site_sp =
        process->GetBreakpointSiteList().FindByAddress(stop_addr);
    if (bp_site_sp) {
      stop_info_sp = StopInfo::CreateStopReasonWithBreakpointSiteID(
          *this, bp_site_sp->GetID());
    }
    break;
  }

  case llvm::emu::System::StopReason::Watchpoint: {
    addr_t stop_addr = sys->getStopAddress();
    WatchpointSP wp_sp = process->GetTarget().GetWatchpointList().FindByAddress(stop_addr);
    if (wp_sp) {
      stop_info_sp =
          StopInfo::CreateStopReasonWithWatchpointID(*this, wp_sp->GetID());
    }
    break;
  }

  case llvm::emu::System::StopReason::SingleStep:
    stop_info_sp = StopInfo::CreateStopReasonToTrace(*this);
    break;

  case llvm::emu::System::StopReason::Halted:
  case llvm::emu::System::StopReason::Error:
  case llvm::emu::System::StopReason::None:
    break;
  }

  SetStopInfo(stop_info_sp);
  return stop_info_sp != nullptr;
}
