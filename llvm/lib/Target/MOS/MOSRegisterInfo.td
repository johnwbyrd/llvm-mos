//===-- MOSRegisterInfo.td - MOS Register defs -------------*- tablegen -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
//  Declarations that describe the MOS register file
//===----------------------------------------------------------------------===//

// The 65xx series of microprocessors uses an 8-bit accumulator register, A, for
// nearly all math operations.  Index registers X and Y are 8-bits.  On the
// 65816 series of microprocessors, these registers are extended to 16 bits.
// There is an 8-bit stack pointer, SP, that refers to memory starting from 
// 0x1ff and growing downward.
// The 65xx also has a processor status register, P, that holds carry, zero,
// negative, and overflow bits which are set as the result of the most
// recent math operation.
// The 6502's program counter register, PC, is 16 bits in length.

// In addition to these real registers,


//
class MOSSubRegIndex<int size, int offset = 0>: SubRegIndex<size, offset>
{
  let Namespace = "MOS";
}

def sub_8lo       : MOSSubRegIndex<8>;
def sub_8hi       : MOSSubRegIndex<8, 8>;
def sub_16lo      : MOSSubRegIndex<16>;
def sub_16hi      : MOSSubRegIndex<16, 16>;
def sub_32lo      : MOSSubRegIndex<32>;
def sub_32hi      : MOSSubRegIndex<32, 32>;
def sub_mask_0    : MOSSubRegIndex<-1>;
def sub_mask_1    : MOSSubRegIndex<-1, -1>;

// This section describes the dimension of the imaginary registers for the MOS
// targets.  We call them imaginary, because LLVM has a different meaning for
// virtual.  Imaginary registers are a bank of continuous memory in MOS
// zero page, that llvm can treat as physical registers for the purpose of
// register allocation.  This puts llvm in the position of managing a large
// chunk of zero page memory.
defvar MaxImag8Regs = 0x40;
defvar Imag8RegsOffset = 0x10;
defvar MaxImag16Regs = !sra(MaxImag8Regs, 1);
defvar Imag16RegsOffset = !add(Imag8RegsOffset, MaxImag8Regs);
defvar MaxImag32Regs = !sra(MaxImag16Regs, 1);
defvar Imag32RegsOffset = !add(Imag16RegsOffset, MaxImag16Regs);
defvar MaxImag64Regs = !sra(MaxImag32Regs, 1);
defvar Imag64RegsOffset = !add(Imag32RegsOffset, MaxImag32Regs);

class MOSReg<bits<16> num,
             string name,
             list<Register> subregs = [],
             list<string> altNames = []
 >
  : RegisterWithSubRegs<!strconcat("llvm_mos_", name), subregs>,
    DwarfRegNum<[num]>
{
  field bits<16> Num = num;
  let HWEncoding = num;
  let Namespace = "MOS";
  let SubRegs = subregs;
  let AltNames = [name];
}

class MOSRegWithSubRegs<bits<16> num,
             string name,
             list<Register> subregs = [],
             list<string> altNames = [] > 
             : MOSReg< num, name, subregs, altNames >
{
  let CoveredBySubRegs = 1;
}

class MOSImagReg8<bits<16> num,
            string name,
            list<Register> subregs = [],
            list<string> altNames = []> 
        : MOSReg< num, name, subregs, altNames >;

class MOSImagReg16<bits<16> num,
            string name,
            list<Register> subregs = [],
            list<string> altNames = []>
            : MOSRegWithSubRegs< num, name, subregs, altNames>
{
  let SubRegIndices = [sub_8lo, sub_8hi];
}

class MOSImagReg32<bits<16> num,
            string name,
            list<Register> subregs = [],
            list<string> altNames = []>
            : MOSRegWithSubRegs< num, name, subregs, altNames>
{
  let SubRegIndices = [sub_16lo, sub_16hi];
}

class MOSImagReg64<bits<16> num,
            string name,
            list<Register> subregs = [],
            list<string> altNames = []>
            : MOSRegWithSubRegs< num, name, subregs, altNames>
{
  let SubRegIndices = [sub_32lo, sub_32hi];
}

class MOSReg8Class< dag RegList >: RegisterClass< "MOS", [i8], 8, RegList >;
class MOSReg16Class< dag RegList >: RegisterClass< "MOS", [i16], 16, RegList >;

def A : MOSReg< 0, "a" >;
def X : MOSReg< 1, "x" >;
def Y : MOSReg< 2, "y" >;
def P : MOSReg< 3, "p" >;
def SP : MOSReg< 4, "sp">;
def PC : MOSReg< 5, "pc">;

def RealReg8Class : MOSReg8Class<(add A, X, Y, P, SP)>;
def RealReg16Class : MOSReg16Class<(add PC)>;

// Imaginary 8-bit registers, starting with the prefix rc
foreach I = 0...!add(MaxImag8Regs, -1) in 
{
  def RC#I: 
    MOSImagReg8< !add(I, Imag8RegsOffset), "rc"#!cast<string>(I) >;
 }
def MOSImagReg8Class : 
  RegisterClass<"MOS", [i8], 8, (sequence "RC%u", 0, !add(MaxImag8Regs, -1))>;

// Imagual 16-bit registers, starting with the prefix rs
foreach I = 0...!add(MaxImag16Regs, -1) in
{
  def RS#I:
  MOSImagReg16< !add(I, Imag16RegsOffset), "rs"#!cast<string>(I), 
    [!cast<Register>("RC"#!shl(I, 1)),
      !cast<Register>("RC"#!add(!shl(I,1),1))]>;
}
def MOSImagReg16Class : 
  RegisterClass<"MOS", [i16], 16, 
    (sequence "RS%u", 0, !add(MaxImag16Regs, -1))>;

// Imagual 32-bit registers, starting with the prefix rl
foreach I = 0...!add(MaxImag32Regs, -1) in
{
  def RL#I:
  MOSImagReg32< !add(I, Imag32RegsOffset), "rl"#!cast<string>(I), 
    [!cast<Register>("RS"#!shl(I, 1)),
      !cast<Register>("RS"#!add(!shl(I,1),1))]>;
}
def MOSImagReg32Class : 
  RegisterClass<"MOS", [i32], 32, 
    (sequence "RL%u", 0, !add(MaxImag32Regs, -1))>;

// Imagual 64-bit registers, starting with the prefix rq
foreach I = 0...!add(MaxImag64Regs, -1) in
 {
   def RQ#I:
    MOSImagReg64< !add(I, Imag64RegsOffset), "rq"#!cast<string>(I), 
      [!cast<Register>("RL"#!shl(I, 1)),
       !cast<Register>("RL"#!add(!shl(I,1),1))]>;
 }
def MOSImagReg64Class : 
  RegisterClass<"MOS", [i64], 64, 
    (sequence "RQ%u", 0, !add(MaxImag64Regs, -1))>;
