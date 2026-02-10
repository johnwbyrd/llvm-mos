//===-- FileDescTable.cpp - FD Pool Implementation -------------*- C++ -*-===//
//
// Part of LLVM-MOS, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/Emulator/Semihost/FileDescTable.h"
#include <cstdio>

using namespace llvm::emu::semihost;

FileDescTable::FileDescTable() {
  Files_.fill(nullptr);
  // FDs 0, 1, 2 are stdin, stdout, stderr
  Files_[0] = stdin;
  Files_[1] = stdout;
  Files_[2] = stderr;
}

FileDescTable::~FileDescTable() { closeAll(); }

FileDescTable::FileDescTable(FileDescTable &&Other) noexcept
    : Files_(Other.Files_) {
  Other.Files_.fill(nullptr);
}

FileDescTable &FileDescTable::operator=(FileDescTable &&Other) noexcept {
  if (this != &Other) {
    closeAll();
    Files_ = Other.Files_;
    Other.Files_.fill(nullptr);
  }
  return *this;
}

int FileDescTable::allocate(FILE *FP) {
  if (!FP)
    return -1;

  // Find first empty slot starting from FirstUserFD
  for (int I = FirstUserFD; I < MaxFiles; ++I) {
    if (Files_[I] == nullptr) {
      Files_[I] = FP;
      return I;
    }
  }
  return -1; // Table full
}

bool FileDescTable::release(int FD) {
  // Don't close stdin/stdout/stderr
  if (FD < FirstUserFD || FD >= MaxFiles)
    return false;

  if (Files_[FD] == nullptr)
    return false;

  std::fclose(Files_[FD]);
  Files_[FD] = nullptr;
  return true;
}

FILE *FileDescTable::get(int FD) const {
  if (FD < 0 || FD >= MaxFiles)
    return nullptr;
  return Files_[FD];
}

bool FileDescTable::isValid(int FD) const {
  if (FD < 0 || FD >= MaxFiles)
    return false;
  return Files_[FD] != nullptr;
}

void FileDescTable::closeAll() {
  for (int I = FirstUserFD; I < MaxFiles; ++I) {
    if (Files_[I] != nullptr) {
      std::fclose(Files_[I]);
      Files_[I] = nullptr;
    }
  }
}
