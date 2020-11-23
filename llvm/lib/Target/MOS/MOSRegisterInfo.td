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


defvar MaxVirt8Regs = 0x40;
defvar Virt8RegsOffset = 0x10;
defvar MaxVirt16Regs = !sra(MaxVirt8Regs, 1);
defvar Virt16RegsOffset = !add(Virt8RegsOffset, MaxVirt8Regs);
defvar MaxVirt32Regs = !sra(MaxVirt16Regs, 1);
defvar Virt32RegsOffset = !add(Virt16RegsOffset, MaxVirt16Regs);
defvar MaxVirt64Regs = !sra(MaxVirt32Regs, 1);
defvar Virt64RegsOffset = !add(Virt32RegsOffset, MaxVirt32Regs);

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

class MOSVirtReg8<bits<16> num,
            string name,
            list<Register> subregs = [],
            list<string> altNames = []> 
        : MOSReg< num, name, subregs, altNames >;

class MOSVirtReg16<bits<16> num,
            string name,
            list<Register> subregs = [],
            list<string> altNames = []>
            : MOSRegWithSubRegs< num, name, subregs, altNames>
{
  let SubRegIndices = [sub_8lo, sub_8hi];
}

class MOSVirtReg32<bits<16> num,
            string name,
            list<Register> subregs = [],
            list<string> altNames = []>
            : MOSRegWithSubRegs< num, name, subregs, altNames>
{
  let SubRegIndices = [sub_16lo, sub_16hi];
}

class MOSVirtReg64<bits<16> num,
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

foreach I = 0...!add(MaxVirt8Regs, -1) in 
{
  def RC#I: 
    MOSVirtReg8< !add(I, Virt8RegsOffset), "rc"#!cast<string>(I) >;
 }

 foreach I = 0...!add(MaxVirt16Regs, -1) in
 {
   def RS#I:
    MOSVirtReg16< !add(I, Virt16RegsOffset), "rs"#!cast<string>(I), 
      [!cast<Register>("RC"#!shl(I, 1)),
       !cast<Register>("RC"#!add(!shl(I,1),1))]>;
 }

 foreach I = 0...!add(MaxVirt32Regs, -1) in
 {
   def RL#I:
    MOSVirtReg32< !add(I, Virt32RegsOffset), "rl"#!cast<string>(I), 
      [!cast<Register>("RS"#!shl(I, 1)),
       !cast<Register>("RS"#!add(!shl(I,1),1))]>;
 }

foreach I = 0...!add(MaxVirt64Regs, -1) in
 {
   def RQ#I:
    MOSVirtReg64< !add(I, Virt64RegsOffset), "rq"#!cast<string>(I), 
      [!cast<Register>("RL"#!shl(I, 1)),
       !cast<Register>("RL"#!add(!shl(I,1),1))]>;
 }

#ifdef FOO

!cast<string>(!shl(I, 1)))
def RS0 : MOSVirtReg16<0x90, "rs0", [RC0, RC1]>;
def RS1 : MOSVirtReg16<0x91, "rs1", [RC2, RC3]>;
def RS2 : MOSVirtReg16<0x92, "rs2", [RC4, RC5]>;
def RS3 : MOSVirtReg16<0x93, "rs3", [RC6, RC7]>;
def RS4 : MOSVirtReg16<0x94, "rs4", [RC8, RC9]>;
def RS5 : MOSVirtReg16<0x95, "rs5", [RC10, RC11]>;
def RS6 : MOSVirtReg16<0x96, "rs6", [RC12, RC13]>;
def RS7 : MOSVirtReg16<0x97, "rs7", [RC14, RC15]>;
#endif

def RCClass : RegisterClass<"MOS", [i8],  8,
                        (add RC0, RC1, RC2, RC3, RC4, RC5, RC6, RC7,
                        RC8, RC9, RC10, RC11, RC12, RC13, RC14, RC15)> {
}

def RSClass: RegisterClass<"MOS", [i16], 16, 
                        (add RS0, RS1, RS2, RS3, RS4, RS5, RS6, RS7)>;