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


class RedHwMode: HwMode<"FeatureRed">;
class YellowHwMode: HwMode<"FeatureYellow">;
class GreenHwMode: HwMode<"FeatureGreen">;
class SWEET16HwMode: HwMode<"FeatureSWEET16">;

// Most real registers on 6502 are 8 bits.
class RealRegInfo: RegInfo< 8, 8, 8>;

// Only the program counter is 16 bits.
class PCRegInfo: RegInfo< 16, 16, 16 >;


// 8-bit General purpose register definition.
class MOSReg<bits<16> num,
             string name,
             list<Register> subregs = [],
             list<string> altNames = []
 >
  : RegisterWithSubRegs<name, subregs>
{
  field bits<16> Num = num;
  
  let HWEncoding = num;
  let Namespace = "MOS";
  let SubRegs = subregs;
  let AltNames = altNames;
  bit isVirtual = 0;
}
 
class MOSVirtReg<bits<16> num,
             string name,
             list<Register> subregs = [],
             list<string> altNames = []> :
             MOSReg< num, name, subregs, altNames >
{
  let isVirtual = 1;
}

class MOSReg8Class< dag RegList, int priority = 32 >: 
  RegisterClass< "MOS", [i8], 8, RegList >
  {
    let AllocationPriority = priority;
  }

class MOSReg16Class< dag RegList, int priority = 33 >: 
  RegisterClass< "MOS", [i16], 16, RegList >
  {
    let AllocationPriority = priority;
  }

def A : MOSReg< 64, "A" >, DwarfRegNum<[64]>;
def AClass : MOSReg8Class<(add A), 0>;

def X : MOSReg< 65, "X" >, DwarfRegNum<[65]>;
def XClass : MOSReg8Class<(add X), 1>;

def Y : MOSReg< 66, "Y" >, DwarfRegNum<[66]>;
def YClass : MOSReg8Class<(add Y), 2>;

// Subregister indices.
let Namespace = "MOS" in
{
  def sub_lo : SubRegIndex<8>;
  def sub_hi : SubRegIndex<8, 8>;
}

let 
  CoveredBySubRegs = 1,
  SubRegIndices = [sub_lo, sub_hi]
 in
{

  def AX : MOSReg< 67, "AX", [A, X]>, DwarfRegNum<[67]>;
  def AY : MOSReg< 68, "AY", [A, Y]>, DwarfRegNum<[68]>;
}

def AXClass : MOSReg16Class<(add AX), 3>;
def AYClass : MOSReg16Class<(add AY), 4>;




let Namespace = "MOS" in {
  def ptr : RegAltNameIndex;
}


//===----------------------------------------------------------------------===//
//  Zero page, 8 bit virtual registers
//===----------------------------------------------------------------------===//

foreach i = 0-31 in 
{
  def R#i : MOSVirtReg< i, "r"#i>, DwarfRegNum<[i]>;
}

def SPL : MOSVirtReg<32, "SPL">, DwarfRegNum<[32]>;
def SPH : MOSVirtReg<33, "SPH">, DwarfRegNum<[33]>;

let SubRegIndices = [sub_lo, sub_hi],
CoveredBySubRegs = 1 in
{
  // 16 bit GPR pairs.
  def SP     : MOSReg<32, "SP",      [SPL, SPH]>, DwarfRegNum<[32]>;

  // The pointer registers (X,Y,Z) are a special case because they
  // are printed as a `high:low` pair when a DREG is expected,
  // but printed using `X`, `Y`, `Z` when a pointer register is expected.
  let RegAltNameIndices = [ptr] in {
      def R31R30 : MOSReg<30, "r31:r30", [R30, R31], ["ZZ"]>, DwarfRegNum<[30]>;
      def R29R28 : MOSReg<28, "r29:r28", [R28, R29], ["YY"]>, DwarfRegNum<[28]>;
      def R27R26 : MOSReg<26, "r27:r26", [R26, R27], ["XX"]>, DwarfRegNum<[26]>;
  }
  def R25R24 : MOSReg<24, "r25:r24", [R24, R25]>, DwarfRegNum<[24]>;
  def R23R22 : MOSReg<22, "r23:r22", [R22, R23]>, DwarfRegNum<[22]>;
  def R21R20 : MOSReg<20, "r21:r20", [R20, R21]>, DwarfRegNum<[20]>;
  def R19R18 : MOSReg<18, "r19:r18", [R18, R19]>, DwarfRegNum<[18]>;
  def R17R16 : MOSReg<16, "r17:r16", [R16, R17]>, DwarfRegNum<[16]>;
  def R15R14 : MOSReg<14, "r15:r14", [R14, R15]>, DwarfRegNum<[14]>;
  def R13R12 : MOSReg<12, "r13:r12", [R12, R13]>, DwarfRegNum<[12]>;
  def R11R10 : MOSReg<10, "r11:r10", [R10, R11]>, DwarfRegNum<[10]>;
  def R9R8   : MOSReg<8,  "r9:r8",   [R8, R9]>,   DwarfRegNum<[8]>;
  def R7R6   : MOSReg<6,  "r7:r6",   [R6, R7]>,   DwarfRegNum<[6]>;
  def R5R4   : MOSReg<4,  "r5:r4",   [R4, R5]>,   DwarfRegNum<[4]>;
  def R3R2   : MOSReg<2,  "r3:r2",   [R2, R3]>,   DwarfRegNum<[2]>;
  def R1R0   : MOSReg<0,  "r1:r0",   [R0, R1]>,   DwarfRegNum<[0]>;
}

//===----------------------------------------------------------------------===//
// Register Classes
//===----------------------------------------------------------------------===//


// Main 8-bit register class.
def GPR8 : RegisterClass<"MOS", [i8], 8,
  (
    // Return value and argument registers.
    add R24, R25, R18, R19, R20, R21, R22, R23,
    // Scratch registers.
    R30, R31, R26, R27,
    // Callee saved registers.
    R28, R29, R17, R16, R15, R14, R13, R12, R11, R10,
    R9, R8, R7, R6, R5, R4, R3, R2, R0, R1
  )>;

// Simple lower registers r0..r15
def GPR8lo : RegisterClass<"MOS", [i8], 8,
  (
    add R15, R14, R13, R12, R11, R10, R9, R8, R7, R6, R5, R4, R3, R2, R0, R1
  )>;

// 8-bit register class for instructions which take immediates.
def LD8 : RegisterClass<"MOS", [i8], 8,
  (
    // Return value and arguments.
    add R24, R25, R18, R19, R20, R21, R22, R23,
    // Scratch registers.
    R30, R31, R26, R27,
    // Callee saved registers.
    R28, R29, R17, R16
  )>;

// Simple lower registers r16..r23
def LD8lo : RegisterClass<"MOS", [i8], 8,
  (
    add R23, R22, R21, R20, R19, R18, R17, R16
  )>;

// Main 16-bit pair register class.
def DREGS : RegisterClass<"MOS", [i16], 8,
  (
    // Return value and arguments.
    add R25R24, R19R18, R21R20, R23R22,
    // Scratch registers.
    R31R30, R27R26,
    // Callee saved registers.
    R29R28, R17R16, R15R14, R13R12, R11R10,
    R9R8, R7R6, R5R4, R3R2, R1R0
  )>;

def DLDREGS : RegisterClass<"MOS", [i16], 8,
  (
    // Return value and arguments.
    add R25R24, R19R18, R21R20, R23R22,
    // Scratch registers.
    R31R30, R27R26,
    // Callee saved registers.
    R29R28, R17R16
  )>;

// Register class used for the stack read pseudo instruction.
def GPRSP: RegisterClass<"MOS", [i16], 8, (add SP)>;

// Status register.
def SREG : MOSReg<14, "FLAGS">, DwarfRegNum<[88]>;
def CCR : RegisterClass<"MOS", [i8], 8, (add SREG)>
{
  let CopyCost = -1;      // Don't allow copying of status registers
}

