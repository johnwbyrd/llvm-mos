//===---------------------------------------------------------------------===//
// MOS Device Definitions
//===---------------------------------------------------------------------===//

// A feature set aggregates features, grouping them. We don't want to create a
// new member in MOSSubtarget (to store a value) for each set because we do not
// care if the set is supported, only the subfeatures inside the set. We fix
// this by simply setting the same dummy member for all feature sets, which is
// then ignored.
class FeatureSet<string name, string desc, list<SubtargetFeature> i>
  : SubtargetFeature<name, "m_FeatureSetDummy", "true", desc, i>;

// A family of microcontrollers, defining a set of supported features.
class Family<string name, list<SubtargetFeature> i>
  : FeatureSet<name, !strconcat("The device is a part of the ",
               name, " family"), i>;

// Features as described by http://nparker.llx.com/a2/opcodes.html
def FeatureGreen          : SubtargetFeature<"green", "m_hasGreenInsns", "true",
                            "The device supports 6502 core instructions">;

def FeatureYellow         : SubtargetFeature<"yellow", "m_hasYellowInsns", "true",
                            "The device supports 65C02 additional instructions">;

def FeatureRed            : SubtargetFeature<"red", "m_hasRedInsns", "true",
                            "The device supports 65816 additional instructions">;

def FeatureSWEET16        : SubtargetFeature<"sweet16", "m_hasSweet16Insns", "true",
                            "The device supports SWEET16 instructions">;

class ELFArch<string name>  : SubtargetFeature<"", "ELFArch",
                                    !strconcat("ELF::",name), "">;

// ELF e_flags architecture values
def ELFArchMOSGeneric : ELFArch<"EF_MOS_ARCH_GENERIC">;
//===---------------------------------------------------------------------===//
// MOS Families
//===---------------------------------------------------------------------===//

def FamilyMOS6502           : Family<"mos6502", [FeatureGreen]>;
def FamilyMOS65C02          : Family<"mos65C02", [FeatureGreen, FeatureYellow]>;
def FamilyMOS65816          : Family<"mos65816", [FeatureGreen, FeatureYellow, FeatureRed]>;
def FamilySWEET16_6502      : Family<"mos6502", [FeatureGreen, FeatureSWEET16]>;

class Device<string Name, Family Fam, ELFArch Arch,
             list<SubtargetFeature> ExtraFeatures = []>
  : Processor<Name, NoItineraries, !listconcat([Fam,Arch],ExtraFeatures)>;
  
// Generic MCUs
// Note that several versions of GCC has strange ELF architecture
// settings for backwards compatibility - see `gas/config/tc-avr.c`
// in MOS binutils. We do not replicate this.
def : Device<"mos-generic",  FamilyMOS6502, ELFArchMOSGeneric>;
