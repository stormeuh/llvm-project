//===- Writer.cpp ---------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "Writer.h"
#include "AArch64ErrataFix.h"
#include "ARMErrataFix.h"
#include "CallGraphSort.h"
#include "Config.h"
#include "LinkerScript.h"
#include "MapFile.h"
#include "OutputSections.h"
#include "Relocations.h"
#include "SymbolTable.h"
#include "Symbols.h"
#include "SyntheticSections.h"
#include "Target.h"
#include "Arch/Cheri.h"
#include "lld/Common/Filesystem.h"
#include "lld/Common/Memory.h"
#include "lld/Common/Strings.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/Support/Parallel.h"
#include "llvm/Support/RandomNumberGenerator.h"
#include "llvm/Support/SHA1.h"
#include "llvm/Support/TimeProfiler.h"
#include "llvm/Support/xxhash.h"
#include <climits>

#define DEBUG_TYPE "lld"

using namespace llvm;
using namespace llvm::ELF;
using namespace llvm::object;
using namespace llvm::support;
using namespace llvm::support::endian;
using namespace lld;
using namespace lld::elf;

namespace {
// The writer writes a SymbolTable result to a file.
template <class ELFT> class Writer {
public:
  Writer() : buffer(errorHandler().outputBuffer) {}
  using Elf_Shdr = typename ELFT::Shdr;
  using Elf_Ehdr = typename ELFT::Ehdr;
  using Elf_Phdr = typename ELFT::Phdr;

  void run();

private:
  void copyLocalSymbols();
  void addSectionSymbols();
  void forEachRelSec(llvm::function_ref<void(InputSectionBase &)> fn);
  void sortSections();
  void resolveShfLinkOrder();
  void finalizeAddressDependentContent();
  void optimizeBasicBlockJumps();
  void sortInputSections();
  void finalizeSections();
  void checkExecuteOnly();
  void setReservedSymbolSections();

  std::vector<PhdrEntry *> createPhdrs(Partition &part);
  void addPhdrForSection(Partition &part, unsigned shType, unsigned pType,
                         unsigned pFlags);
  void assignFileOffsets();
  void assignFileOffsetsBinary();
  void setPhdrs(Partition &part);
  void checkSections();
  void fixSectionAlignments();
  void openFile();
  void writeTrapInstr();
  void writeHeader();
  void writeSections();
  void writeSectionsBinary();
  void writeBuildId();

  std::unique_ptr<FileOutputBuffer> &buffer;

  void addRelIpltSymbols();
  void addStartEndSymbols();
  void addStartStopSymbols(OutputSection *sec);

  uint64_t fileSize;
  uint64_t sectionHeaderOff;
};
} // anonymous namespace

static bool isSectionPrefix(StringRef prefix, StringRef name) {
  return name.startswith(prefix) || name == prefix.drop_back();
}

StringRef elf::getOutputSectionName(const InputSectionBase *s) {
  if (config->relocatable)
    return s->name;

  // This is for --emit-relocs. If .text.foo is emitted as .text.bar, we want
  // to emit .rela.text.foo as .rela.text.bar for consistency (this is not
  // technically required, but not doing it is odd). This code guarantees that.
  if (auto *isec = dyn_cast<InputSection>(s)) {
    if (InputSectionBase *rel = isec->getRelocatedSection()) {
      OutputSection *out = rel->getOutputSection();
      if (s->type == SHT_RELA)
        return saver.save(".rela" + out->name);
      return saver.save(".rel" + out->name);
    }
  }

  // A BssSection created for a common symbol is identified as "COMMON" in
  // linker scripts. It should go to .bss section.
  if (s->name == "COMMON")
    return ".bss";

  if (script->hasSectionsCommand)
    return s->name;

  // When no SECTIONS is specified, emulate GNU ld's internal linker scripts
  // by grouping sections with certain prefixes.

  // GNU ld places text sections with prefix ".text.hot.", ".text.unknown.",
  // ".text.unlikely.", ".text.startup." or ".text.exit." before others.
  // We provide an option -z keep-text-section-prefix to group such sections
  // into separate output sections. This is more flexible. See also
  // sortISDBySectionOrder().
  // ".text.unknown" means the hotness of the section is unknown. When
  // SampleFDO is used, if a function doesn't have sample, it could be very
  // cold or it could be a new function never being sampled. Those functions
  // will be kept in the ".text.unknown" section.
  // ".text.split." holds symbols which are split out from functions in other
  // input sections. For example, with -fsplit-machine-functions, placing the
  // cold parts in .text.split instead of .text.unlikely mitigates against poor
  // profile inaccuracy. Techniques such as hugepage remapping can make
  // conservative decisions at the section granularity.
  if (config->zKeepTextSectionPrefix)
    for (StringRef v : {".text.hot.", ".text.unknown.", ".text.unlikely.",
                        ".text.startup.", ".text.exit.", ".text.split."})
      if (isSectionPrefix(v, s->name))
        return v.drop_back();

  for (StringRef v :
       {".text.", ".rodata.", ".data.rel.ro.", ".data.", ".bss.rel.ro.",
        ".bss.", ".init_array.", ".fini_array.", ".ctors.", ".dtors.", ".tbss.",
        ".gcc_except_table.", ".tdata.", ".ARM.exidx.", ".ARM.extab."})
    if (isSectionPrefix(v, s->name))
      return v.drop_back();

  return s->name;
}

static bool needsInterpSection() {
  return !config->relocatable && !config->shared &&
         !config->dynamicLinker.empty() && script->needsInterpSection();
}

bool elf::hasDynamicLinker() {
  return config->shared || config->pie || !sharedFiles.empty();
}

template <class ELFT> void elf::writeResult() {
  llvm::TimeTraceScope timeScope("Write output file");
  Writer<ELFT>().run();
}

static void removeEmptyPTLoad(std::vector<PhdrEntry *> &phdrs) {
  auto it = std::stable_partition(
      phdrs.begin(), phdrs.end(), [&](const PhdrEntry *p) {
        if (p->p_type != PT_LOAD)
          return true;
        if (!p->firstSec)
          return false;
        uint64_t size = p->lastSec->addr + p->lastSec->size - p->firstSec->addr;
        return size != 0;
      });

  // Clear OutputSection::ptLoad for sections contained in removed
  // segments.
  DenseSet<PhdrEntry *> removed(it, phdrs.end());
  for (OutputSection *sec : outputSections)
    if (removed.count(sec->ptLoad))
      sec->ptLoad = nullptr;
  phdrs.erase(it, phdrs.end());
}

void elf::copySectionsIntoPartitions() {
  std::vector<InputSectionBase *> newSections;
  for (unsigned part = 2; part != partitions.size() + 1; ++part) {
    for (InputSectionBase *s : inputSections) {
      if (!(s->flags & SHF_ALLOC) || !s->isLive())
        continue;
      InputSectionBase *copy;
      if (s->type == SHT_NOTE)
        copy = make<InputSection>(cast<InputSection>(*s));
      else if (auto *es = dyn_cast<EhInputSection>(s))
        copy = make<EhInputSection>(*es);
      else
        continue;
      copy->partition = part;
      newSections.push_back(copy);
    }
  }

  inputSections.insert(inputSections.end(), newSections.begin(),
                       newSections.end());
}

void elf::combineEhSections() {
  for (InputSectionBase *&s : inputSections) {
    // Ignore dead sections and the partition end marker (.part.end),
    // whose partition number is out of bounds.
    if (!s->isLive() || s->partition == 255)
      continue;

    Partition &part = s->getPartition();
    if (auto *es = dyn_cast<EhInputSection>(s)) {
      part.ehFrame->addSection(es);
      s = nullptr;
    } else if (s->kind() == SectionBase::Regular && part.armExidx &&
               part.armExidx->addSection(cast<InputSection>(s))) {
      s = nullptr;
    }
  }

  std::vector<InputSectionBase *> &v = inputSections;
  v.erase(std::remove(v.begin(), v.end(), nullptr), v.end());
}

template <class ELFT> void elf::combineCapRelocsSections() {
  for (InputSectionBase *&s : inputSections) {
    if (s->name != "__cap_relocs")
      continue;
    // Factory.addInputSec(S, getOutputSectionName(S->Name), InX<ELFT>::CapRelocs->OutSec);

    // We only gather the sections here and add the cap_relocs during
    // finalizeContents() The reason for this is that we don't know if symbols
    // are preemptible when this function is called.
    InX<ELFT>::capRelocs->addSection(s);
    s = nullptr;
  }
  std::vector<InputSectionBase *> &v = inputSections;
  v.erase(std::remove(v.begin(), v.end(), nullptr), v.end());
}

static Defined *addOptionalRegular(StringRef name, SectionBase *sec,
                                   uint64_t val, uint8_t stOther = STV_HIDDEN,
                                   uint8_t binding = STB_GLOBAL,
                                   bool canBeSectionStart = true) {
  Symbol *s = symtab->find(name);
  if (!s || s->isDefined())
    return nullptr;

  s->resolve(Defined{/*file=*/nullptr, name, binding, stOther, STT_NOTYPE, val,
                     /*size=*/0, sec});
  // If Val == 0 assume this symbol references the start of a section.
  // When targetting CHERI we set the size of that symbol since otherwise
  // an expression like foo = &_DYNAMIC will create a zero-length capability
  // for foo and most likely crash the program.
  // TODO: I would like to do this for all targets but that might cause
  // compatibility issues
  if (val == 0 && canBeSectionStart) {
    log("Treating " + name + " as a section start symbol");
    s->isSectionStartSymbol = true;
  }

  return cast<Defined>(s);
}

static Defined *addAbsolute(StringRef name) {
  Symbol *sym = symtab->addSymbol(Defined{nullptr, name, STB_GLOBAL, STV_HIDDEN,
                                          STT_NOTYPE, 0, 0, nullptr});
  return cast<Defined>(sym);
}

// The linker is expected to define some symbols depending on
// the linking result. This function defines such symbols.
void elf::addReservedSymbols() {
  if (config->emachine == EM_MIPS) {
    // Define _gp for MIPS. st_value of _gp symbol will be updated by Writer
    // so that it points to an absolute address which by default is relative
    // to GOT. Default offset is 0x7ff0.
    // See "Global Data Symbols" in Chapter 6 in the following document:
    // ftp://www.linux-mips.org/pub/linux/mips/doc/ABI/mipsabi.pdf
    ElfSym::mipsGp = addAbsolute("_gp");

    // On MIPS O32 ABI, _gp_disp is a magic symbol designates offset between
    // start of function and 'gp' pointer into GOT.
    if (symtab->find("_gp_disp"))
      ElfSym::mipsGpDisp = addAbsolute("_gp_disp");

    // The __gnu_local_gp is a magic symbol equal to the current value of 'gp'
    // pointer. This symbol is used in the code generated by .cpload pseudo-op
    // in case of using -mno-shared option.
    // https://sourceware.org/ml/binutils/2004-12/msg00094.html
    if (symtab->find("__gnu_local_gp"))
      ElfSym::mipsLocalGp = addAbsolute("__gnu_local_gp");
  } else if (config->emachine == EM_PPC) {
    // glibc *crt1.o has a undefined reference to _SDA_BASE_. Since we don't
    // support Small Data Area, define it arbitrarily as 0.
    addOptionalRegular("_SDA_BASE_", nullptr, 0, STV_HIDDEN);
  } else if (config->emachine == EM_PPC64) {
    addPPC64SaveRestore();
  }

  // The Power Architecture 64-bit v2 ABI defines a TableOfContents (TOC) which
  // combines the typical ELF GOT with the small data sections. It commonly
  // includes .got .toc .sdata .sbss. The .TOC. symbol replaces both
  // _GLOBAL_OFFSET_TABLE_ and _SDA_BASE_ from the 32-bit ABI. It is used to
  // represent the TOC base which is offset by 0x8000 bytes from the start of
  // the .got section.
  // We do not allow _GLOBAL_OFFSET_TABLE_ to be defined by input objects as the
  // correctness of some relocations depends on its value.
  StringRef gotSymName =
      (config->emachine == EM_PPC64) ? ".TOC." : "_GLOBAL_OFFSET_TABLE_";

  if (Symbol *s = symtab->find(gotSymName)) {
    if (s->isDefined()) {
      error(toString(s->file) + " cannot redefine linker defined symbol '" +
            gotSymName + "'");
      return;
    }

    uint64_t gotOff = 0;
    if (config->emachine == EM_PPC64)
      gotOff = 0x8000;

    s->resolve(Defined{/*file=*/nullptr, gotSymName, STB_GLOBAL, STV_HIDDEN,
                       STT_NOTYPE, gotOff, /*size=*/0, Out::elfHeader});
    ElfSym::globalOffsetTable = cast<Defined>(s);
  }

  // __ehdr_start is the location of ELF file headers. Note that we define
  // this symbol unconditionally even when using a linker script, which
  // differs from the behavior implemented by GNU linker which only define
  // this symbol if ELF headers are in the memory mapped segment.
  addOptionalRegular("__ehdr_start", Out::elfHeader, 0, STV_HIDDEN);

  // __executable_start is not documented, but the expectation of at
  // least the Android libc is that it points to the ELF header.
  addOptionalRegular("__executable_start", Out::elfHeader, 0, STV_HIDDEN);

  // __dso_handle symbol is passed to cxa_finalize as a marker to identify
  // each DSO. The address of the symbol doesn't matter as long as they are
  // different in different DSOs, so we chose the start address of the DSO.
  addOptionalRegular("__dso_handle", Out::elfHeader, 0, STV_HIDDEN);

  // If linker script do layout we do not need to create any standard symbols.
  if (script->hasSectionsCommand)
    return;

  auto add = [](StringRef s, int64_t pos) {
    return addOptionalRegular(s, Out::elfHeader, pos, STV_DEFAULT);
  };

  ElfSym::bss = add("__bss_start", 0);
  ElfSym::end1 = add("end", -1);
  ElfSym::end2 = add("_end", -1);
  ElfSym::etext1 = add("etext", -1);
  ElfSym::etext2 = add("_etext", -1);
  ElfSym::edata1 = add("edata", -1);
  ElfSym::edata2 = add("_edata", -1);
}

static OutputSection *findSection(StringRef name, unsigned partition = 1) {
  for (BaseCommand *base : script->sectionCommands)
    if (auto *sec = dyn_cast<OutputSection>(base))
      if (sec->name == name && sec->partition == partition)
        return sec;
  return nullptr;
}

template <class ELFT> void elf::createSyntheticSections() {
  // Initialize all pointers with NULL. This is needed because
  // you can call lld::elf::main more than once as a library.
  memset(&Out::first, 0, sizeof(Out));

  // Add the .interp section first because it is not a SyntheticSection.
  // The removeUnusedSyntheticSections() function relies on the
  // SyntheticSections coming last.
  if (needsInterpSection()) {
    for (size_t i = 1; i <= partitions.size(); ++i) {
      InputSection *sec = createInterpSection();
      sec->partition = i;
      inputSections.push_back(sec);
    }
  }

  auto add = [](SyntheticSection *sec) { inputSections.push_back(sec); };

  in.shStrTab = make<StringTableSection>(".shstrtab", false);

  Out::programHeaders = make<OutputSection>("", 0, SHF_ALLOC);
  Out::programHeaders->alignment = config->wordsize;

  if (config->strip != StripPolicy::All) {
    in.strTab = make<StringTableSection>(".strtab", false);
    in.symTab = make<SymbolTableSection<ELFT>>(*in.strTab);
    in.symTabShndx = make<SymtabShndxSection>();
  }

  in.bss = make<BssSection>(".bss", 0, 1);
  add(in.bss);

  // If there is a SECTIONS command and a .data.rel.ro section name use name
  // .data.rel.ro.bss so that we match in the .data.rel.ro output section.
  // This makes sure our relro is contiguous.
  bool hasDataRelRo =
      script->hasSectionsCommand && findSection(".data.rel.ro", 0);
  in.bssRelRo =
      make<BssSection>(hasDataRelRo ? ".data.rel.ro.bss" : ".bss.rel.ro", 0, 1);
  add(in.bssRelRo);

  if (config->capabilitySize > 0) {
    InX<ELFT>::capRelocs = make<CheriCapRelocsSection<ELFT>>();
    in.cheriCapTable = make<CheriCapTableSection>();
    add(in.cheriCapTable);
    if (config->capTableScope != CapTableScopePolicy::All) {
      in.cheriCapTableMapping = make<CheriCapTableMappingSection>();
      add(in.cheriCapTableMapping);
    }
  }

  // Add MIPS-specific sections.
  if (config->emachine == EM_MIPS) {
    // XXXAR: also add the RLD_MAP dynamic tags to rtld so that we can use
    // gdb with rtld direct exec mode.
    // TODO: should probably try to build rtld as PIE instead?
    if ((!config->shared || config->buildingFreeBSDRtld) && config->hasDynSymTab) {
      in.mipsRldMap = make<MipsRldMapSection>();
      add(in.mipsRldMap);
    }
    InX<ELFT>::mipsAbiFlags = MipsAbiFlagsSection<ELFT>::create();
    if (InX<ELFT>::mipsAbiFlags)
      add(InX<ELFT>::mipsAbiFlags);
    if (auto *sec = MipsOptionsSection<ELFT>::create())
      add(sec);
    if (auto *sec = MipsReginfoSection<ELFT>::create())
      add(sec);
  }

  StringRef relaDynName = config->isRela ? ".rela.dyn" : ".rel.dyn";

  for (Partition &part : partitions) {
    auto add = [&](SyntheticSection *sec) {
      sec->partition = part.getNumber();
      inputSections.push_back(sec);
    };

    if (!part.name.empty()) {
      part.elfHeader = make<PartitionElfHeaderSection<ELFT>>();
      part.elfHeader->name = part.name;
      add(part.elfHeader);

      part.programHeaders = make<PartitionProgramHeadersSection<ELFT>>();
      add(part.programHeaders);
    }

    if (config->buildId != BuildIdKind::None) {
      part.buildId = make<BuildIdSection>();
      add(part.buildId);
    }

    part.dynStrTab = make<StringTableSection>(".dynstr", true);
    part.dynSymTab = make<SymbolTableSection<ELFT>>(*part.dynStrTab);
    part.dynamic = make<DynamicSection<ELFT>>();
    if (config->androidPackDynRelocs)
      part.relaDyn = make<AndroidPackedRelocationSection<ELFT>>(relaDynName);
    else
      part.relaDyn =
          make<RelocationSection<ELFT>>(relaDynName, config->zCombreloc);

    if (config->hasDynSymTab) {
      part.dynSymTab = make<SymbolTableSection<ELFT>>(*part.dynStrTab);
      add(part.dynSymTab);

      part.verSym = make<VersionTableSection>();
      add(part.verSym);

      if (!namedVersionDefs().empty()) {
        part.verDef = make<VersionDefinitionSection>();
        add(part.verDef);
      }

      part.verNeed = make<VersionNeedSection<ELFT>>();
      add(part.verNeed);

      if (config->gnuHash) {
        part.gnuHashTab = make<GnuHashTableSection>();
        add(part.gnuHashTab);
      }

      if (config->sysvHash) {
        part.hashTab = make<HashTableSection>();
        add(part.hashTab);
      }

      add(part.dynamic);
      add(part.dynStrTab);
      add(part.relaDyn);
    }

    if (config->relrPackDynRelocs) {
      part.relrDyn = make<RelrSection<ELFT>>();
      add(part.relrDyn);
    }

    if (!config->relocatable) {
      if (config->ehFrameHdr) {
        part.ehFrameHdr = make<EhFrameHeader>();
        add(part.ehFrameHdr);
      }
      part.ehFrame = make<EhFrameSection>();
      add(part.ehFrame);
    }

    if (config->emachine == EM_ARM && !config->relocatable) {
      // The ARMExidxsyntheticsection replaces all the individual .ARM.exidx
      // InputSections.
      part.armExidx = make<ARMExidxSyntheticSection>();
      add(part.armExidx);
    }
  }

  if (partitions.size() != 1) {
    // Create the partition end marker. This needs to be in partition number 255
    // so that it is sorted after all other partitions. It also has other
    // special handling (see createPhdrs() and combineEhSections()).
    in.partEnd = make<BssSection>(".part.end", config->maxPageSize, 1);
    in.partEnd->partition = 255;
    add(in.partEnd);

    in.partIndex = make<PartitionIndexSection>();
    addOptionalRegular("__part_index_begin", in.partIndex, 0);
    addOptionalRegular("__part_index_end", in.partIndex,
                       in.partIndex->getSize());
    add(in.partIndex);
  }

  // Add .got. MIPS' .got is so different from the other archs,
  // it has its own class.
  if (config->emachine == EM_MIPS) {
    in.mipsGot = make<MipsGotSection>();
    add(in.mipsGot);
  } else {
    in.got = make<GotSection>();
    add(in.got);
  }

  if (config->emachine == EM_PPC) {
    in.ppc32Got2 = make<PPC32Got2Section>();
    add(in.ppc32Got2);
  }

  if (config->emachine == EM_PPC64) {
    in.ppc64LongBranchTarget = make<PPC64LongBranchTargetSection>();
    add(in.ppc64LongBranchTarget);
  }

  in.gotPlt = make<GotPltSection>();
  add(in.gotPlt);
  in.igotPlt = make<IgotPltSection>();
  add(in.igotPlt);

  // _GLOBAL_OFFSET_TABLE_ is defined relative to either .got.plt or .got. Treat
  // it as a relocation and ensure the referenced section is created.
  if (ElfSym::globalOffsetTable && config->emachine != EM_MIPS) {
    if (target->gotBaseSymInGotPlt)
      in.gotPlt->hasGotPltOffRel = true;
    else
      in.got->hasGotOffRel = true;
  }

  if (config->gdbIndex)
    add(GdbIndexSection::create<ELFT>());

  // We always need to add rel[a].plt to output if it has entries.
  // Even for static linking it can contain R_[*]_IRELATIVE relocations.
  in.relaPlt = make<RelocationSection<ELFT>>(
      config->isRela ? ".rela.plt" : ".rel.plt", /*sort=*/false);
  add(in.relaPlt);

  // The relaIplt immediately follows .rel[a].dyn to ensure that the IRelative
  // relocations are processed last by the dynamic loader. We cannot place the
  // iplt section in .rel.dyn when Android relocation packing is enabled because
  // that would cause a section type mismatch. However, because the Android
  // dynamic loader reads .rel.plt after .rel.dyn, we can get the desired
  // behaviour by placing the iplt section in .rel.plt.
  in.relaIplt = make<RelocationSection<ELFT>>(
      config->androidPackDynRelocs ? in.relaPlt->name : relaDynName,
      /*sort=*/false);
  add(in.relaIplt);

  if ((config->emachine == EM_386 || config->emachine == EM_X86_64) &&
      (config->andFeatures & GNU_PROPERTY_X86_FEATURE_1_IBT)) {
    in.ibtPlt = make<IBTPltSection>();
    add(in.ibtPlt);
  }

  in.plt = config->emachine == EM_PPC ? make<PPC32GlinkSection>()
                                      : make<PltSection>();
  add(in.plt);
  in.iplt = make<IpltSection>();
  add(in.iplt);

  if (config->andFeatures)
    add(make<GnuPropertySection>());

  // .note.GNU-stack is always added when we are creating a re-linkable
  // object file. Other linkers are using the presence of this marker
  // section to control the executable-ness of the stack area, but that
  // is irrelevant these days. Stack area should always be non-executable
  // by default. So we emit this section unconditionally.
  if (config->relocatable)
    add(make<GnuStackSection>());

  if (in.symTab)
    add(in.symTab);
  if (in.symTabShndx)
    add(in.symTabShndx);
  add(in.shStrTab);
  if (in.strTab)
    add(in.strTab);
}

// The main function of the writer.
template <class ELFT> void Writer<ELFT>::run() {
  copyLocalSymbols();

  if (config->copyRelocs)
    addSectionSymbols();

  // Now that we have a complete set of output sections. This function
  // completes section contents. For example, we need to add strings
  // to the string table, and add entries to .got and .plt.
  // finalizeSections does that.
  finalizeSections();
  checkExecuteOnly();
  if (errorCount())
    return;

  // If -compressed-debug-sections is specified, we need to compress
  // .debug_* sections. Do it right now because it changes the size of
  // output sections.
  for (OutputSection *sec : outputSections)
    sec->maybeCompress<ELFT>();

  if (script->hasSectionsCommand)
    script->allocateHeaders(mainPart->phdrs);

  // Remove empty PT_LOAD to avoid causing the dynamic linker to try to mmap a
  // 0 sized region. This has to be done late since only after assignAddresses
  // we know the size of the sections.
  for (Partition &part : partitions)
    removeEmptyPTLoad(part.phdrs);

  if (!config->oFormatBinary)
    assignFileOffsets();
  else
    assignFileOffsetsBinary();

  for (Partition &part : partitions)
    setPhdrs(part);

  if (config->relocatable)
    for (OutputSection *sec : outputSections)
      sec->addr = 0;

  // Handle --print-map(-M)/--Map, --cref and --print-archive-stats=. Dump them
  // before checkSections() because the files may be useful in case
  // checkSections() or openFile() fails, for example, due to an erroneous file
  // size.
  writeMapFile();
  writeCrossReferenceTable();
  writeArchiveStats();

  if (config->checkSections)
    checkSections();

  // It does not make sense try to open the file if we have error already.
  if (errorCount())
    return;
  // Write the result down to a file.
  openFile();
  if (errorCount())
    return;

  if (!config->oFormatBinary) {
    if (config->zSeparate != SeparateSegmentKind::None)
      writeTrapInstr();
    writeHeader();
    writeSections();
  } else {
    writeSectionsBinary();
  }

  // Backfill .note.gnu.build-id section content. This is done at last
  // because the content is usually a hash value of the entire output file.
  writeBuildId();
  if (errorCount())
    return;

  if (auto e = buffer->commit())
    error("failed to write to the output file: " + toString(std::move(e)));
}

template <class ELFT, class RelTy>
static void markUsedLocalSymbolsImpl(ObjFile<ELFT> *file,
                                     llvm::ArrayRef<RelTy> rels) {
  for (const RelTy &rel : rels) {
    Symbol &sym = file->getRelocTargetSym(rel);
    if (sym.isLocal())
      sym.used = true;
  }
}

// The function ensures that the "used" field of local symbols reflects the fact
// that the symbol is used in a relocation from a live section.
template <class ELFT> static void markUsedLocalSymbols() {
  // With --gc-sections, the field is already filled.
  // See MarkLive<ELFT>::resolveReloc().
  if (config->gcSections)
    return;
  // Without --gc-sections, the field is initialized with "true".
  // Drop the flag first and then rise for symbols referenced in relocations.
  for (InputFile *file : objectFiles) {
    ObjFile<ELFT> *f = cast<ObjFile<ELFT>>(file);
    for (Symbol *b : f->getLocalSymbols())
      b->used = false;
    for (InputSectionBase *s : f->getSections()) {
      InputSection *isec = dyn_cast_or_null<InputSection>(s);
      if (!isec)
        continue;
      if (isec->type == SHT_REL)
        markUsedLocalSymbolsImpl(f, isec->getDataAs<typename ELFT::Rel>());
      else if (isec->type == SHT_RELA)
        markUsedLocalSymbolsImpl(f, isec->getDataAs<typename ELFT::Rela>());
    }
  }
}

static bool shouldKeepInSymtab(const Defined &sym) {
  if (sym.isSection())
    return false;

  // If --emit-reloc or -r is given, preserve symbols referenced by relocations
  // from live sections.
  if (config->copyRelocs && sym.used)
    return true;

  // Exclude local symbols pointing to .ARM.exidx sections.
  // They are probably mapping symbols "$d", which are optional for these
  // sections. After merging the .ARM.exidx sections, some of these symbols
  // may become dangling. The easiest way to avoid the issue is not to add
  // them to the symbol table from the beginning.
  if (config->emachine == EM_ARM && sym.section &&
      sym.section->type == SHT_ARM_EXIDX)
    return false;

  if (config->discard == DiscardPolicy::None)
    return true;
  if (config->discard == DiscardPolicy::All)
    return false;

  // In ELF assembly .L symbols are normally discarded by the assembler.
  // If the assembler fails to do so, the linker discards them if
  // * --discard-locals is used.
  // * The symbol is in a SHF_MERGE section, which is normally the reason for
  //   the assembler keeping the .L symbol.
  StringRef name = sym.getName();
  bool isLocal = name.startswith(".L") || name.empty();
  if (!isLocal)
    return true;

  if (config->discard == DiscardPolicy::Locals)
    return false;

  SectionBase *sec = sym.section;
  return !sec || !(sec->flags & SHF_MERGE);
}

static bool includeInSymtab(const Symbol &b) {
  if (!b.isLocal() && !b.isUsedInRegularObj)
    return false;

  if (auto *d = dyn_cast<Defined>(&b)) {
    // Always include absolute symbols.
    SectionBase *sec = d->section;
    if (!sec)
      return true;
    sec = sec->repl;

    // Exclude symbols pointing to garbage-collected sections.
    if (isa<InputSectionBase>(sec) && !sec->isLive())
      return false;

    if (auto *s = dyn_cast<MergeInputSection>(sec))
      if (!s->getSectionPiece(d->value)->live)
        return false;
    return true;
  }
  return b.used;
}

// Local symbols are not in the linker's symbol table. This function scans
// each object file's symbol table to copy local symbols to the output.
template <class ELFT> void Writer<ELFT>::copyLocalSymbols() {
  if (!in.symTab)
    return;
  if (config->copyRelocs && config->discard != DiscardPolicy::None)
    markUsedLocalSymbols<ELFT>();
  for (InputFile *file : objectFiles) {
    ObjFile<ELFT> *f = cast<ObjFile<ELFT>>(file);
    for (Symbol *b : f->getLocalSymbols()) {
      assert(b->isLocal() && "should have been caught in initializeSymbols()");
      auto *dr = dyn_cast<Defined>(b);

      // No reason to keep local undefined symbol in symtab.
      if (!dr)
        continue;
      if (!includeInSymtab(*b))
        continue;
      if (!shouldKeepInSymtab(*dr))
        continue;
      in.symTab->addSymbol(b);
    }
  }
}

// Create a section symbol for each output section so that we can represent
// relocations that point to the section. If we know that no relocation is
// referring to a section (that happens if the section is a synthetic one), we
// don't create a section symbol for that section.
template <class ELFT> void Writer<ELFT>::addSectionSymbols() {
  for (BaseCommand *base : script->sectionCommands) {
    auto *sec = dyn_cast<OutputSection>(base);
    if (!sec)
      continue;
    auto i = llvm::find_if(sec->sectionCommands, [](BaseCommand *base) {
      if (auto *isd = dyn_cast<InputSectionDescription>(base))
        return !isd->sections.empty();
      return false;
    });
    if (i == sec->sectionCommands.end())
      continue;
    InputSectionBase *isec = cast<InputSectionDescription>(*i)->sections[0];

    // Relocations are not using REL[A] section symbols.
    if (isec->type == SHT_REL || isec->type == SHT_RELA)
      continue;

    // Unlike other synthetic sections, mergeable output sections contain data
    // copied from input sections, and there may be a relocation pointing to its
    // contents if -r or -emit-reloc are given.
    if (isa<SyntheticSection>(isec) && !(isec->flags & SHF_MERGE))
      continue;

    // Set the symbol to be relative to the output section so that its st_value
    // equals the output section address. Note, there may be a gap between the
    // start of the output section and isec.
    auto *sym =
        make<Defined>(isec->file, "", STB_LOCAL, /*stOther=*/0, STT_SECTION,
                      /*value=*/0, /*size=*/0, isec->getOutputSection());
    sym->isSectionStartSymbol = true;
    in.symTab->addSymbol(sym);
  }
}

// Today's loaders have a feature to make segments read-only after
// processing dynamic relocations to enhance security. PT_GNU_RELRO
// is defined for that.
//
// This function returns true if a section needs to be put into a
// PT_GNU_RELRO segment.
bool elf::isRelroSection(const OutputSection *sec) {
  if (!config->zRelro)
    return false;

  uint64_t flags = sec->flags;

  // Non-allocatable or non-writable sections don't need RELRO because
  // they are not writable or not even mapped to memory in the first place.
  // RELRO is for sections that are essentially read-only but need to
  // be writable only at process startup to allow dynamic linker to
  // apply relocations.
  if (!(flags & SHF_ALLOC) || !(flags & SHF_WRITE))
    return false;

  // Once initialized, TLS data segments are used as data templates
  // for a thread-local storage. For each new thread, runtime
  // allocates memory for a TLS and copy templates there. No thread
  // are supposed to use templates directly. Thus, it can be in RELRO.
  if (flags & SHF_TLS)
    return true;

  // .init_array, .preinit_array and .fini_array contain pointers to
  // functions that are executed on process startup or exit. These
  // pointers are set by the static linker, and they are not expected
  // to change at runtime. But if you are an attacker, you could do
  // interesting things by manipulating pointers in .fini_array, for
  // example. So they are put into RELRO.
  uint32_t type = sec->type;
  if (type == SHT_INIT_ARRAY || type == SHT_FINI_ARRAY ||
      type == SHT_PREINIT_ARRAY)
    return true;

  // .got contains pointers to external symbols. They are resolved by
  // the dynamic linker when a module is loaded into memory, and after
  // that they are not expected to change. So, it can be in RELRO.
  if (in.got && sec == in.got->getParent())
    return true;

  // .toc is a GOT-ish section for PowerPC64. Their contents are accessed
  // through r2 register, which is reserved for that purpose. Since r2 is used
  // for accessing .got as well, .got and .toc need to be close enough in the
  // virtual address space. Usually, .toc comes just after .got. Since we place
  // .got into RELRO, .toc needs to be placed into RELRO too.
  if (sec->name.equals(".toc"))
    return true;

  // .got.plt contains pointers to external function symbols. They are
  // by default resolved lazily, so we usually cannot put it into RELRO.
  // However, if "-z now" is given, the lazy symbol resolution is
  // disabled, which enables us to put it into RELRO.
  if (sec == in.gotPlt->getParent())
    return config->zNow;

  // Similarly the CHERI capability table is also relro since the capabilities
  // in the table need to be initialized at runtime to set the tag bits
  if (in.cheriCapTable && sec == in.cheriCapTable->getParent()) {
    // Without -z now, the PLT stubs can update the captable entries so we
    // can't mark it as relro. It can also be relro for static binaries:
    return config->zNow || !config->isPic;
  }

  // .dynamic section contains data for the dynamic linker, and
  // there's no need to write to it at runtime, so it's better to put
  // it into RELRO.
  if (sec->name == ".dynamic")
    return true;

  // Sections with some special names are put into RELRO. This is a
  // bit unfortunate because section names shouldn't be significant in
  // ELF in spirit. But in reality many linker features depend on
  // magic section names.
  StringRef s = sec->name;
  return s == ".data.rel.ro" || s == ".bss.rel.ro" || s == ".ctors" ||
         s == ".dtors" || s == ".jcr" || s == ".eh_frame" ||
         s == ".fini_array" || s == ".init_array" ||
         s == ".openbsd.randomdata" || s == ".preinit_array" ||
         s == "__cap_relocs" || s == ".gcc_except_table";
}

// We compute a rank for each section. The rank indicates where the
// section should be placed in the file.  Instead of using simple
// numbers (0,1,2...), we use a series of flags. One for each decision
// point when placing the section.
// Using flags has two key properties:
// * It is easy to check if a give branch was taken.
// * It is easy two see how similar two ranks are (see getRankProximity).
enum RankFlags {
  RF_NOT_ADDR_SET = 1 << 27,
  RF_NOT_ALLOC = 1 << 26,
  RF_PARTITION = 1 << 18, // Partition number (8 bits)
  RF_NOT_PART_EHDR = 1 << 17,
  RF_NOT_PART_PHDR = 1 << 16,
  RF_NOT_INTERP = 1 << 15,
  RF_NOT_NOTE = 1 << 14,
  RF_WRITE = 1 << 13,
  RF_EXEC_WRITE = 1 << 12,
  RF_EXEC = 1 << 11,
  RF_RODATA = 1 << 10,
  RF_NOT_RELRO = 1 << 9,
  RF_NOT_TLS = 1 << 8,
  RF_BSS = 1 << 7,
  RF_PPC_NOT_TOCBSS = 1 << 6,
  RF_PPC_TOCL = 1 << 5,
  RF_PPC_TOC = 1 << 4,
  RF_PPC_GOT = 1 << 3,
  RF_PPC_BRANCH_LT = 1 << 2,
  RF_MIPS_GPREL = 1 << 1,
  RF_MIPS_NOT_GOT = 1 << 0
};

static unsigned getSectionRank(const OutputSection *sec) {
  unsigned rank = sec->partition * RF_PARTITION;

  // We want to put section specified by -T option first, so we
  // can start assigning VA starting from them later.
  if (config->sectionStartMap.count(sec->name))
    return rank;
  rank |= RF_NOT_ADDR_SET;

  // Allocatable sections go first to reduce the total PT_LOAD size and
  // so debug info doesn't change addresses in actual code.
  if (!(sec->flags & SHF_ALLOC))
    return rank | RF_NOT_ALLOC;

  if (sec->type == SHT_LLVM_PART_EHDR)
    return rank;
  rank |= RF_NOT_PART_EHDR;

  if (sec->type == SHT_LLVM_PART_PHDR)
    return rank;
  rank |= RF_NOT_PART_PHDR;

  // Put .interp first because some loaders want to see that section
  // on the first page of the executable file when loaded into memory.
  if (sec->name == ".interp")
    return rank;
  rank |= RF_NOT_INTERP;

  // Put .note sections (which make up one PT_NOTE) at the beginning so that
  // they are likely to be included in a core file even if core file size is
  // limited. In particular, we want a .note.gnu.build-id and a .note.tag to be
  // included in a core to match core files with executables.
  if (sec->type == SHT_NOTE)
    return rank;
  rank |= RF_NOT_NOTE;

  // Sort sections based on their access permission in the following
  // order: R, RX, RWX, RW.  This order is based on the following
  // considerations:
  // * Read-only sections come first such that they go in the
  //   PT_LOAD covering the program headers at the start of the file.
  // * Read-only, executable sections come next.
  // * Writable, executable sections follow such that .plt on
  //   architectures where it needs to be writable will be placed
  //   between .text and .data.
  // * Writable sections come last, such that .bss lands at the very
  //   end of the last PT_LOAD.
  bool isExec = sec->flags & SHF_EXECINSTR;
  bool isWrite = sec->flags & SHF_WRITE;

  if (isExec) {
    if (isWrite)
      rank |= RF_EXEC_WRITE;
    else
      rank |= RF_EXEC;
  } else if (isWrite) {
    rank |= RF_WRITE;
  } else if (sec->type == SHT_PROGBITS) {
    // Make non-executable and non-writable PROGBITS sections (e.g .rodata
    // .eh_frame) closer to .text. They likely contain PC or GOT relative
    // relocations and there could be relocation overflow if other huge sections
    // (.dynstr .dynsym) were placed in between.
    rank |= RF_RODATA;
  }

  // Place RelRo sections first. After considering SHT_NOBITS below, the
  // ordering is PT_LOAD(PT_GNU_RELRO(.data.rel.ro .bss.rel.ro) | .data .bss),
  // where | marks where page alignment happens. An alternative ordering is
  // PT_LOAD(.data | PT_GNU_RELRO( .data.rel.ro .bss.rel.ro) | .bss), but it may
  // waste more bytes due to 2 alignment places.
  if (!isRelroSection(sec))
    rank |= RF_NOT_RELRO;

  // If we got here we know that both A and B are in the same PT_LOAD.

  // The TLS initialization block needs to be a single contiguous block in a R/W
  // PT_LOAD, so stick TLS sections directly before the other RelRo R/W
  // sections. Since p_filesz can be less than p_memsz, place NOBITS sections
  // after PROGBITS.
  if (!(sec->flags & SHF_TLS))
    rank |= RF_NOT_TLS;

  // Within TLS sections, or within other RelRo sections, or within non-RelRo
  // sections, place non-NOBITS sections first.
  if (sec->type == SHT_NOBITS)
    rank |= RF_BSS;

  // Some architectures have additional ordering restrictions for sections
  // within the same PT_LOAD.
  if (config->emachine == EM_PPC64) {
    // PPC64 has a number of special SHT_PROGBITS+SHF_ALLOC+SHF_WRITE sections
    // that we would like to make sure appear is a specific order to maximize
    // their coverage by a single signed 16-bit offset from the TOC base
    // pointer. Conversely, the special .tocbss section should be first among
    // all SHT_NOBITS sections. This will put it next to the loaded special
    // PPC64 sections (and, thus, within reach of the TOC base pointer).
    StringRef name = sec->name;
    if (name != ".tocbss")
      rank |= RF_PPC_NOT_TOCBSS;

    if (name == ".toc1")
      rank |= RF_PPC_TOCL;

    if (name == ".toc")
      rank |= RF_PPC_TOC;

    if (name == ".got")
      rank |= RF_PPC_GOT;

    if (name == ".branch_lt")
      rank |= RF_PPC_BRANCH_LT;
  }

  if (config->emachine == EM_MIPS) {
    // All sections with SHF_MIPS_GPREL flag should be grouped together
    // because data in these sections is addressable with a gp relative address.
    if (sec->flags & SHF_MIPS_GPREL)
      rank |= RF_MIPS_GPREL;

    if (sec->name != ".got")
      rank |= RF_MIPS_NOT_GOT;
  }

  return rank;
}

static bool compareSections(const BaseCommand *aCmd, const BaseCommand *bCmd) {
  const OutputSection *a = cast<OutputSection>(aCmd);
  const OutputSection *b = cast<OutputSection>(bCmd);

  if (a->sortRank != b->sortRank)
    return a->sortRank < b->sortRank;

  if (!(a->sortRank & RF_NOT_ADDR_SET))
    return config->sectionStartMap.lookup(a->name) <
           config->sectionStartMap.lookup(b->name);
  return false;
}

void PhdrEntry::add(OutputSection *sec) {
  lastSec = sec;
  if (!firstSec)
    firstSec = sec;
  p_align = std::max(p_align, sec->alignment);
  if (p_type == PT_LOAD)
    sec->ptLoad = this;
}

// The beginning and the ending of .rel[a].plt section are marked
// with __rel[a]_iplt_{start,end} symbols if it is a statically linked
// executable. The runtime needs these symbols in order to resolve
// all IRELATIVE relocs on startup. For dynamic executables, we don't
// need these symbols, since IRELATIVE relocs are resolved through GOT
// and PLT. For details, see http://www.airs.com/blog/archives/403.
template <class ELFT> void Writer<ELFT>::addRelIpltSymbols() {
  if (config->relocatable || needsInterpSection())
    return;

  // By default, __rela_iplt_{start,end} belong to a dummy section 0
  // because .rela.plt might be empty and thus removed from output.
  // We'll override Out::elfHeader with In.relaIplt later when we are
  // sure that .rela.plt exists in output.
  ElfSym::relaIpltStart = addOptionalRegular(
      config->isRela ? "__rela_iplt_start" : "__rel_iplt_start",
      Out::elfHeader, 0, STV_HIDDEN, STB_WEAK);

  ElfSym::relaIpltEnd = addOptionalRegular(
      config->isRela ? "__rela_iplt_end" : "__rel_iplt_end",
      Out::elfHeader, 0, STV_HIDDEN, STB_WEAK);
}

template <class ELFT>
void Writer<ELFT>::forEachRelSec(
    llvm::function_ref<void(InputSectionBase &)> fn) {
  // Scan all relocations. Each relocation goes through a series
  // of tests to determine if it needs special treatment, such as
  // creating GOT, PLT, copy relocations, etc.
  // Note that relocations for non-alloc sections are directly
  // processed by InputSection::relocateNonAlloc.
  for (InputSectionBase *isec : inputSections)
    if (isec->isLive() && isa<InputSection>(isec) && (isec->flags & SHF_ALLOC))
      fn(*isec);
  for (Partition &part : partitions) {
    for (EhInputSection *es : part.ehFrame->sections)
      fn(*es);
    if (part.armExidx && part.armExidx->isLive())
      for (InputSection *ex : part.armExidx->exidxSections)
        fn(*ex);
  }
}

// This function generates assignments for predefined symbols (e.g. _end or
// _etext) and inserts them into the commands sequence to be processed at the
// appropriate time. This ensures that the value is going to be correct by the
// time any references to these symbols are processed and is equivalent to
// defining these symbols explicitly in the linker script.
template <class ELFT> void Writer<ELFT>::setReservedSymbolSections() {
  if (ElfSym::globalOffsetTable) {
    // The _GLOBAL_OFFSET_TABLE_ symbol is defined by target convention usually
    // to the start of the .got or .got.plt section.
    InputSection *gotSection = in.gotPlt;
    if (!target->gotBaseSymInGotPlt)
      gotSection = in.mipsGot ? cast<InputSection>(in.mipsGot)
                              : cast<InputSection>(in.got);
    ElfSym::globalOffsetTable->section = gotSection;
  }

  // .rela_iplt_{start,end} mark the start and the end of in.relaIplt.
  if (ElfSym::relaIpltStart && in.relaIplt->isNeeded()) {
    ElfSym::relaIpltStart->section = in.relaIplt;
    ElfSym::relaIpltEnd->section = in.relaIplt;
    ElfSym::relaIpltEnd->value = in.relaIplt->getSize();
  }

  PhdrEntry *last = nullptr;
  PhdrEntry *lastRO = nullptr;

  for (Partition &part : partitions) {
    for (PhdrEntry *p : part.phdrs) {
      if (p->p_type != PT_LOAD)
        continue;
      last = p;
      if (!(p->p_flags & PF_W))
        lastRO = p;
    }
  }

  if (lastRO) {
    // _etext is the first location after the last read-only loadable segment.
    if (ElfSym::etext1)
      ElfSym::etext1->section = lastRO->lastSec;
    if (ElfSym::etext2)
      ElfSym::etext2->section = lastRO->lastSec;
  }

  if (last) {
    // _edata points to the end of the last mapped initialized section.
    OutputSection *edata = nullptr;
    for (OutputSection *os : outputSections) {
      if (os->type != SHT_NOBITS)
        edata = os;
      if (os == last->lastSec)
        break;
    }

    if (ElfSym::edata1)
      ElfSym::edata1->section = edata;
    if (ElfSym::edata2)
      ElfSym::edata2->section = edata;

    // _end is the first location after the uninitialized data region.
    if (ElfSym::end1)
      ElfSym::end1->section = last->lastSec;
    if (ElfSym::end2)
      ElfSym::end2->section = last->lastSec;
  }

  if (ElfSym::bss)
    ElfSym::bss->section = findSection(".bss");

  // Setup MIPS _gp_disp/__gnu_local_gp symbols which should
  // be equal to the _gp symbol's value.
  if (ElfSym::mipsGp) {
    // Find GP-relative section with the lowest address
    // and use this address to calculate default _gp value.
    for (OutputSection *os : outputSections) {
      if (os->flags & SHF_MIPS_GPREL) {
        ElfSym::mipsGp->section = os;
        ElfSym::mipsGp->value = 0x7ff0;
        break;
      }
    }
  }
}

// We want to find how similar two ranks are.
// The more branches in getSectionRank that match, the more similar they are.
// Since each branch corresponds to a bit flag, we can just use
// countLeadingZeros.
static int getRankProximityAux(OutputSection *a, OutputSection *b) {
  return countLeadingZeros(a->sortRank ^ b->sortRank);
}

static int getRankProximity(OutputSection *a, BaseCommand *b) {
  auto *sec = dyn_cast<OutputSection>(b);
  return (sec && sec->hasInputSections) ? getRankProximityAux(a, sec) : -1;
}

// When placing orphan sections, we want to place them after symbol assignments
// so that an orphan after
//   begin_foo = .;
//   foo : { *(foo) }
//   end_foo = .;
// doesn't break the intended meaning of the begin/end symbols.
// We don't want to go over sections since findOrphanPos is the
// one in charge of deciding the order of the sections.
// We don't want to go over changes to '.', since doing so in
//  rx_sec : { *(rx_sec) }
//  . = ALIGN(0x1000);
//  /* The RW PT_LOAD starts here*/
//  rw_sec : { *(rw_sec) }
// would mean that the RW PT_LOAD would become unaligned.
static bool shouldSkip(BaseCommand *cmd) {
  if (auto *assign = dyn_cast<SymbolAssignment>(cmd))
    return assign->name != ".";
  return false;
}

// We want to place orphan sections so that they share as much
// characteristics with their neighbors as possible. For example, if
// both are rw, or both are tls.
static std::vector<BaseCommand *>::iterator
findOrphanPos(std::vector<BaseCommand *>::iterator b,
              std::vector<BaseCommand *>::iterator e) {
  OutputSection *sec = cast<OutputSection>(*e);

  // Find the first element that has as close a rank as possible.
  auto i = std::max_element(b, e, [=](BaseCommand *a, BaseCommand *b) {
    return getRankProximity(sec, a) < getRankProximity(sec, b);
  });
  if (i == e)
    return e;

  // Consider all existing sections with the same proximity.
  int proximity = getRankProximity(sec, *i);
  for (; i != e; ++i) {
    auto *curSec = dyn_cast<OutputSection>(*i);
    if (!curSec || !curSec->hasInputSections)
      continue;
    if (getRankProximity(sec, curSec) != proximity ||
        sec->sortRank < curSec->sortRank)
      break;
  }

  auto isOutputSecWithInputSections = [](BaseCommand *cmd) {
    auto *os = dyn_cast<OutputSection>(cmd);
    return os && os->hasInputSections;
  };
  auto j = std::find_if(llvm::make_reverse_iterator(i),
                        llvm::make_reverse_iterator(b),
                        isOutputSecWithInputSections);
  i = j.base();

  // As a special case, if the orphan section is the last section, put
  // it at the very end, past any other commands.
  // This matches bfd's behavior and is convenient when the linker script fully
  // specifies the start of the file, but doesn't care about the end (the non
  // alloc sections for example).
  auto nextSec = std::find_if(i, e, isOutputSecWithInputSections);
  if (nextSec == e)
    return e;

  while (i != e && shouldSkip(*i))
    ++i;
  return i;
}

// Adds random priorities to sections not already in the map.
static void maybeShuffle(DenseMap<const InputSectionBase *, int> &order) {
  if (!config->shuffleSectionSeed)
    return;

  std::vector<int> priorities(inputSections.size() - order.size());
  // Existing priorities are < 0, so use priorities >= 0 for the missing
  // sections.
  int curPrio = 0;
  for (int &prio : priorities)
    prio = curPrio++;
  uint32_t seed = *config->shuffleSectionSeed;
  std::mt19937 g(seed ? seed : std::random_device()());
  llvm::shuffle(priorities.begin(), priorities.end(), g);
  int prioIndex = 0;
  for (InputSectionBase *sec : inputSections) {
    if (order.try_emplace(sec, priorities[prioIndex]).second)
      ++prioIndex;
  }
}

// Builds section order for handling --symbol-ordering-file.
static DenseMap<const InputSectionBase *, int> buildSectionOrder() {
  DenseMap<const InputSectionBase *, int> sectionOrder;
  // Use the rarely used option -call-graph-ordering-file to sort sections.
  if (!config->callGraphProfile.empty())
    return computeCallGraphProfileOrder();

  if (config->symbolOrderingFile.empty())
    return sectionOrder;

  struct SymbolOrderEntry {
    int priority;
    bool present;
  };

  // Build a map from symbols to their priorities. Symbols that didn't
  // appear in the symbol ordering file have the lowest priority 0.
  // All explicitly mentioned symbols have negative (higher) priorities.
  DenseMap<StringRef, SymbolOrderEntry> symbolOrder;
  int priority = -config->symbolOrderingFile.size();
  for (StringRef s : config->symbolOrderingFile)
    symbolOrder.insert({s, {priority++, false}});

  // Build a map from sections to their priorities.
  auto addSym = [&](Symbol &sym) {
    auto it = symbolOrder.find(sym.getName());
    if (it == symbolOrder.end())
      return;
    SymbolOrderEntry &ent = it->second;
    ent.present = true;

    maybeWarnUnorderableSymbol(&sym);

    if (auto *d = dyn_cast<Defined>(&sym)) {
      if (auto *sec = dyn_cast_or_null<InputSectionBase>(d->section)) {
        int &priority = sectionOrder[cast<InputSectionBase>(sec->repl)];
        priority = std::min(priority, ent.priority);
      }
    }
  };

  // We want both global and local symbols. We get the global ones from the
  // symbol table and iterate the object files for the local ones.
  for (Symbol *sym : symtab->symbols())
    if (!sym->isLazy())
      addSym(*sym);

  for (InputFile *file : objectFiles)
    for (Symbol *sym : file->getSymbols()) {
      if (!sym->isLocal())
        break;
      addSym(*sym);
    }

  if (config->warnSymbolOrdering)
    for (auto orderEntry : symbolOrder)
      if (!orderEntry.second.present)
        warn("symbol ordering file: no such symbol: " + orderEntry.first);

  return sectionOrder;
}

// Sorts the sections in ISD according to the provided section order.
static void
sortISDBySectionOrder(InputSectionDescription *isd,
                      const DenseMap<const InputSectionBase *, int> &order) {
  std::vector<InputSection *> unorderedSections;
  std::vector<std::pair<InputSection *, int>> orderedSections;
  uint64_t unorderedSize = 0;

  for (InputSection *isec : isd->sections) {
    auto i = order.find(isec);
    if (i == order.end()) {
      unorderedSections.push_back(isec);
      unorderedSize += isec->getSize();
      continue;
    }
    orderedSections.push_back({isec, i->second});
  }
  llvm::sort(orderedSections, llvm::less_second());

  // Find an insertion point for the ordered section list in the unordered
  // section list. On targets with limited-range branches, this is the mid-point
  // of the unordered section list. This decreases the likelihood that a range
  // extension thunk will be needed to enter or exit the ordered region. If the
  // ordered section list is a list of hot functions, we can generally expect
  // the ordered functions to be called more often than the unordered functions,
  // making it more likely that any particular call will be within range, and
  // therefore reducing the number of thunks required.
  //
  // For example, imagine that you have 8MB of hot code and 32MB of cold code.
  // If the layout is:
  //
  // 8MB hot
  // 32MB cold
  //
  // only the first 8-16MB of the cold code (depending on which hot function it
  // is actually calling) can call the hot code without a range extension thunk.
  // However, if we use this layout:
  //
  // 16MB cold
  // 8MB hot
  // 16MB cold
  //
  // both the last 8-16MB of the first block of cold code and the first 8-16MB
  // of the second block of cold code can call the hot code without a thunk. So
  // we effectively double the amount of code that could potentially call into
  // the hot code without a thunk.
  size_t insPt = 0;
  if (target->getThunkSectionSpacing() && !orderedSections.empty()) {
    uint64_t unorderedPos = 0;
    for (; insPt != unorderedSections.size(); ++insPt) {
      unorderedPos += unorderedSections[insPt]->getSize();
      if (unorderedPos > unorderedSize / 2)
        break;
    }
  }

  isd->sections.clear();
  for (InputSection *isec : makeArrayRef(unorderedSections).slice(0, insPt))
    isd->sections.push_back(isec);
  for (std::pair<InputSection *, int> p : orderedSections)
    isd->sections.push_back(p.first);
  for (InputSection *isec : makeArrayRef(unorderedSections).slice(insPt))
    isd->sections.push_back(isec);
}

static void sortSection(OutputSection *sec,
                        const DenseMap<const InputSectionBase *, int> &order) {
  StringRef name = sec->name;

  // Never sort these.
  if (name == ".init" || name == ".fini")
    return;

  // IRelative relocations that usually live in the .rel[a].dyn section should
  // be proccessed last by the dynamic loader. To achieve that we add synthetic
  // sections in the required order from the begining so that the in.relaIplt
  // section is placed last in an output section. Here we just do not apply
  // sorting for an output section which holds the in.relaIplt section.
  if (in.relaIplt->getParent() == sec)
    return;

  // Sort input sections by priority using the list provided by
  // --symbol-ordering-file or --shuffle-sections=. This is a least significant
  // digit radix sort. The sections may be sorted stably again by a more
  // significant key.
  if (!order.empty())
    for (BaseCommand *b : sec->sectionCommands)
      if (auto *isd = dyn_cast<InputSectionDescription>(b))
        sortISDBySectionOrder(isd, order);

  // Sort input sections by section name suffixes for
  // __attribute__((init_priority(N))).
  if (name == ".init_array" || name == ".fini_array") {
    if (!script->hasSectionsCommand)
      sec->sortInitFini();
    return;
  }

  // Sort input sections by the special rule for .ctors and .dtors.
  if (name == ".ctors" || name == ".dtors") {
    if (!script->hasSectionsCommand)
      sec->sortCtorsDtors();
    return;
  }

  // .toc is allocated just after .got and is accessed using GOT-relative
  // relocations. Object files compiled with small code model have an
  // addressable range of [.got, .got + 0xFFFC] for GOT-relative relocations.
  // To reduce the risk of relocation overflow, .toc contents are sorted so that
  // sections having smaller relocation offsets are at beginning of .toc
  if (config->emachine == EM_PPC64 && name == ".toc") {
    if (script->hasSectionsCommand)
      return;
    assert(sec->sectionCommands.size() == 1);
    auto *isd = cast<InputSectionDescription>(sec->sectionCommands[0]);
    llvm::stable_sort(isd->sections,
                      [](const InputSection *a, const InputSection *b) -> bool {
                        return a->file->ppc64SmallCodeModelTocRelocs &&
                               !b->file->ppc64SmallCodeModelTocRelocs;
                      });
    return;
  }
}

// If no layout was provided by linker script, we want to apply default
// sorting for special input sections. This also handles --symbol-ordering-file.
template <class ELFT> void Writer<ELFT>::sortInputSections() {
  // Build the order once since it is expensive.
  DenseMap<const InputSectionBase *, int> order = buildSectionOrder();
  maybeShuffle(order);
  for (BaseCommand *base : script->sectionCommands)
    if (auto *sec = dyn_cast<OutputSection>(base))
      sortSection(sec, order);
}

template <class ELFT> void Writer<ELFT>::sortSections() {
  script->adjustSectionsBeforeSorting();

  // Don't sort if using -r. It is not necessary and we want to preserve the
  // relative order for SHF_LINK_ORDER sections.
  if (config->relocatable)
    return;

  sortInputSections();

  for (BaseCommand *base : script->sectionCommands) {
    auto *os = dyn_cast<OutputSection>(base);
    if (!os)
      continue;
    os->sortRank = getSectionRank(os);

    // We want to assign rude approximation values to outSecOff fields
    // to know the relative order of the input sections. We use it for
    // sorting SHF_LINK_ORDER sections. See resolveShfLinkOrder().
    uint64_t i = 0;
    for (InputSection *sec : getInputSections(os))
      sec->outSecOff = i++;
  }

  if (!script->hasSectionsCommand) {
    // We know that all the OutputSections are contiguous in this case.
    auto isSection = [](BaseCommand *base) { return isa<OutputSection>(base); };
    std::stable_sort(
        llvm::find_if(script->sectionCommands, isSection),
        llvm::find_if(llvm::reverse(script->sectionCommands), isSection).base(),
        compareSections);

    // Process INSERT commands. From this point onwards the order of
    // script->sectionCommands is fixed.
    script->processInsertCommands();
    return;
  }

  script->processInsertCommands();

  // Orphan sections are sections present in the input files which are
  // not explicitly placed into the output file by the linker script.
  //
  // The sections in the linker script are already in the correct
  // order. We have to figuere out where to insert the orphan
  // sections.
  //
  // The order of the sections in the script is arbitrary and may not agree with
  // compareSections. This means that we cannot easily define a strict weak
  // ordering. To see why, consider a comparison of a section in the script and
  // one not in the script. We have a two simple options:
  // * Make them equivalent (a is not less than b, and b is not less than a).
  //   The problem is then that equivalence has to be transitive and we can
  //   have sections a, b and c with only b in a script and a less than c
  //   which breaks this property.
  // * Use compareSectionsNonScript. Given that the script order doesn't have
  //   to match, we can end up with sections a, b, c, d where b and c are in the
  //   script and c is compareSectionsNonScript less than b. In which case d
  //   can be equivalent to c, a to b and d < a. As a concrete example:
  //   .a (rx) # not in script
  //   .b (rx) # in script
  //   .c (ro) # in script
  //   .d (ro) # not in script
  //
  // The way we define an order then is:
  // *  Sort only the orphan sections. They are in the end right now.
  // *  Move each orphan section to its preferred position. We try
  //    to put each section in the last position where it can share
  //    a PT_LOAD.
  //
  // There is some ambiguity as to where exactly a new entry should be
  // inserted, because Commands contains not only output section
  // commands but also other types of commands such as symbol assignment
  // expressions. There's no correct answer here due to the lack of the
  // formal specification of the linker script. We use heuristics to
  // determine whether a new output command should be added before or
  // after another commands. For the details, look at shouldSkip
  // function.

  auto i = script->sectionCommands.begin();
  auto e = script->sectionCommands.end();
  auto nonScriptI = std::find_if(i, e, [](BaseCommand *base) {
    if (auto *sec = dyn_cast<OutputSection>(base))
      return sec->sectionIndex == UINT32_MAX;
    return false;
  });

  // Sort the orphan sections.
  std::stable_sort(nonScriptI, e, compareSections);

  // As a horrible special case, skip the first . assignment if it is before any
  // section. We do this because it is common to set a load address by starting
  // the script with ". = 0xabcd" and the expectation is that every section is
  // after that.
  auto firstSectionOrDotAssignment =
      std::find_if(i, e, [](BaseCommand *cmd) { return !shouldSkip(cmd); });
  if (firstSectionOrDotAssignment != e &&
      isa<SymbolAssignment>(**firstSectionOrDotAssignment))
    ++firstSectionOrDotAssignment;
  i = firstSectionOrDotAssignment;

  while (nonScriptI != e) {
    auto pos = findOrphanPos(i, nonScriptI);
    OutputSection *orphan = cast<OutputSection>(*nonScriptI);

    // As an optimization, find all sections with the same sort rank
    // and insert them with one rotate.
    unsigned rank = orphan->sortRank;
    auto end = std::find_if(nonScriptI + 1, e, [=](BaseCommand *cmd) {
      return cast<OutputSection>(cmd)->sortRank != rank;
    });
    std::rotate(pos, nonScriptI, end);
    nonScriptI = end;
  }

  script->adjustSectionsAfterSorting();
}

static bool compareByFilePosition(InputSection *a, InputSection *b) {
  InputSection *la = a->flags & SHF_LINK_ORDER ? a->getLinkOrderDep() : nullptr;
  InputSection *lb = b->flags & SHF_LINK_ORDER ? b->getLinkOrderDep() : nullptr;
  // SHF_LINK_ORDER sections with non-zero sh_link are ordered before
  // non-SHF_LINK_ORDER sections and SHF_LINK_ORDER sections with zero sh_link.
  if (!la || !lb)
    return la && !lb;
  OutputSection *aOut = la->getParent();
  OutputSection *bOut = lb->getParent();

  if (aOut != bOut)
    return aOut->addr < bOut->addr;
  return la->outSecOff < lb->outSecOff;
}

template <class ELFT> void Writer<ELFT>::resolveShfLinkOrder() {
  for (OutputSection *sec : outputSections) {
    if (!(sec->flags & SHF_LINK_ORDER))
      continue;

    // The ARM.exidx section use SHF_LINK_ORDER, but we have consolidated
    // this processing inside the ARMExidxsyntheticsection::finalizeContents().
    if (!config->relocatable && config->emachine == EM_ARM &&
        sec->type == SHT_ARM_EXIDX)
      continue;

    // Link order may be distributed across several InputSectionDescriptions.
    // Sorting is performed separately.
    std::vector<InputSection **> scriptSections;
    std::vector<InputSection *> sections;
    for (BaseCommand *base : sec->sectionCommands) {
      auto *isd = dyn_cast<InputSectionDescription>(base);
      if (!isd)
        continue;
      bool hasLinkOrder = false;
      scriptSections.clear();
      sections.clear();
      for (InputSection *&isec : isd->sections) {
        if (isec->flags & SHF_LINK_ORDER) {
          InputSection *link = isec->getLinkOrderDep();
          if (link && !link->getParent())
            error(toString(isec) + ": sh_link points to discarded section " +
                  toString(link));
          hasLinkOrder = true;
        }
        scriptSections.push_back(&isec);
        sections.push_back(isec);
      }
      if (hasLinkOrder && errorCount() == 0) {
        llvm::stable_sort(sections, compareByFilePosition);
        for (int i = 0, n = sections.size(); i != n; ++i)
          *scriptSections[i] = sections[i];
      }
    }
  }
}

static void finalizeSynthetic(SyntheticSection *sec) {
  if (sec && sec->isNeeded() && sec->getParent())
    sec->finalizeContents();
}

// We need to generate and finalize the content that depends on the address of
// InputSections. As the generation of the content may also alter InputSection
// addresses we must converge to a fixed point. We do that here. See the comment
// in Writer<ELFT>::finalizeSections().
template <class ELFT> void Writer<ELFT>::finalizeAddressDependentContent() {
  ThunkCreator tc;
  AArch64Err843419Patcher a64p;
  ARMErr657417Patcher a32p;
  script->assignAddresses();
  // .ARM.exidx and SHF_LINK_ORDER do not require precise addresses, but they
  // do require the relative addresses of OutputSections because linker scripts
  // can assign Virtual Addresses to OutputSections that are not monotonically
  // increasing.
  for (Partition &part : partitions)
    finalizeSynthetic(part.armExidx);
  resolveShfLinkOrder();

  // Converts call x@GDPLT to call __tls_get_addr
  if (config->emachine == EM_HEXAGON)
    hexagonTLSSymbolUpdate(outputSections);

  int assignPasses = 0;
  for (;;) {
    bool changed = target->needsThunks && tc.createThunks(outputSections);

    // With Thunk Size much smaller than branch range we expect to
    // converge quickly; if we get to 15 something has gone wrong.
    if (changed && tc.pass >= 15) {
      error("thunk creation not converged");
      break;
    }

    if (config->fixCortexA53Errata843419) {
      if (changed)
        script->assignAddresses();
      changed |= a64p.createFixes();
    }
    if (config->fixCortexA8) {
      if (changed)
        script->assignAddresses();
      changed |= a32p.createFixes();
    }

    if (in.mipsGot)
      in.mipsGot->updateAllocSize();

    for (Partition &part : partitions) {
      changed |= part.relaDyn->updateAllocSize();
      if (part.relrDyn)
        changed |= part.relrDyn->updateAllocSize();
    }

    const Defined *changedSym = script->assignAddresses();
    if (!changed) {
      // Some symbols may be dependent on section addresses. When we break the
      // loop, the symbol values are finalized because a previous
      // assignAddresses() finalized section addresses.
      if (!changedSym)
        break;
      if (++assignPasses == 5) {
        errorOrWarn("assignment to symbol " + toString(*changedSym) +
                    " does not converge");
        break;
      }
    }
  }

  // If addrExpr is set, the address may not be a multiple of the alignment.
  // Warn because this is error-prone.
  for (BaseCommand *cmd : script->sectionCommands)
    if (auto *os = dyn_cast<OutputSection>(cmd))
      if (os->addr % os->alignment != 0)
        warn("address (0x" + Twine::utohexstr(os->addr) + ") of section " +
             os->name + " is not a multiple of alignment (" +
             Twine(os->alignment) + ")");
}

// If Input Sections have been shrinked (basic block sections) then
// update symbol values and sizes associated with these sections.  With basic
// block sections, input sections can shrink when the jump instructions at
// the end of the section are relaxed.
static void fixSymbolsAfterShrinking() {
  for (InputFile *File : objectFiles) {
    parallelForEach(File->getSymbols(), [&](Symbol *Sym) {
      auto *def = dyn_cast<Defined>(Sym);
      if (!def)
        return;

      const SectionBase *sec = def->section;
      if (!sec)
        return;

      const InputSectionBase *inputSec = dyn_cast<InputSectionBase>(sec->repl);
      if (!inputSec || !inputSec->bytesDropped)
        return;

      const size_t OldSize = inputSec->data().size();
      const size_t NewSize = OldSize - inputSec->bytesDropped;

      if (def->value > NewSize && def->value <= OldSize) {
        LLVM_DEBUG(llvm::dbgs()
                   << "Moving symbol " << Sym->getName() << " from "
                   << def->value << " to "
                   << def->value - inputSec->bytesDropped << " bytes\n");
        def->value -= inputSec->bytesDropped;
        return;
      }

      if (def->value + def->size > NewSize && def->value <= OldSize &&
          def->value + def->size <= OldSize) {
        LLVM_DEBUG(llvm::dbgs()
                   << "Shrinking symbol " << Sym->getName() << " from "
                   << def->size << " to " << def->size - inputSec->bytesDropped
                   << " bytes\n");
        def->size -= inputSec->bytesDropped;
      }
    });
  }
}

// If basic block sections exist, there are opportunities to delete fall thru
// jumps and shrink jump instructions after basic block reordering.  This
// relaxation pass does that.  It is only enabled when --optimize-bb-jumps
// option is used.
template <class ELFT> void Writer<ELFT>::optimizeBasicBlockJumps() {
  assert(config->optimizeBBJumps);

  script->assignAddresses();
  // For every output section that has executable input sections, this
  // does the following:
  //   1. Deletes all direct jump instructions in input sections that
  //      jump to the following section as it is not required.
  //   2. If there are two consecutive jump instructions, it checks
  //      if they can be flipped and one can be deleted.
  for (OutputSection *os : outputSections) {
    if (!(os->flags & SHF_EXECINSTR))
      continue;
    std::vector<InputSection *> sections = getInputSections(os);
    std::vector<unsigned> result(sections.size());
    // Delete all fall through jump instructions.  Also, check if two
    // consecutive jump instructions can be flipped so that a fall
    // through jmp instruction can be deleted.
    parallelForEachN(0, sections.size(), [&](size_t i) {
      InputSection *next = i + 1 < sections.size() ? sections[i + 1] : nullptr;
      InputSection &is = *sections[i];
      result[i] =
          target->deleteFallThruJmpInsn(is, is.getFile<ELFT>(), next) ? 1 : 0;
    });
    size_t numDeleted = std::count(result.begin(), result.end(), 1);
    if (numDeleted > 0) {
      script->assignAddresses();
      LLVM_DEBUG(llvm::dbgs()
                 << "Removing " << numDeleted << " fall through jumps\n");
    }
  }

  fixSymbolsAfterShrinking();

  for (OutputSection *os : outputSections) {
    std::vector<InputSection *> sections = getInputSections(os);
    for (InputSection *is : sections)
      is->trim();
  }
}

// In order to allow users to manipulate linker-synthesized sections,
// we had to add synthetic sections to the input section list early,
// even before we make decisions whether they are needed. This allows
// users to write scripts like this: ".mygot : { .got }".
//
// Doing it has an unintended side effects. If it turns out that we
// don't need a .got (for example) at all because there's no
// relocation that needs a .got, we don't want to emit .got.
//
// To deal with the above problem, this function is called after
// scanRelocations is called to remove synthetic sections that turn
// out to be empty.
static void removeUnusedSyntheticSections() {
  // All input synthetic sections that can be empty are placed after
  // all regular ones. We iterate over them all and exit at first
  // non-synthetic.
  for (InputSectionBase *s : llvm::reverse(inputSections)) {
    SyntheticSection *ss = dyn_cast<SyntheticSection>(s);
    if (!ss)
      return;
    OutputSection *os = ss->getParent();
    if (!os || ss->isNeeded())
      continue;

    // If we reach here, then ss is an unused synthetic section and we want to
    // remove it from the corresponding input section description, and
    // orphanSections.
    for (BaseCommand *b : os->sectionCommands)
      if (auto *isd = dyn_cast<InputSectionDescription>(b))
        llvm::erase_if(isd->sections,
                       [=](InputSection *isec) { return isec == ss; });
    llvm::erase_if(script->orphanSections,
                   [=](const InputSectionBase *isec) { return isec == ss; });
  }
}

// Create output section objects and add them to OutputSections.
template <class ELFT> void Writer<ELFT>::finalizeSections() {
  Out::preinitArray = findSection(".preinit_array");
  Out::initArray = findSection(".init_array");
  Out::finiArray = findSection(".fini_array");

  // The linker needs to define SECNAME_start, SECNAME_end and SECNAME_stop
  // symbols for sections, so that the runtime can get the start and end
  // addresses of each section by section name. Add such symbols.
  if (!config->relocatable) {
    addStartEndSymbols();
    for (BaseCommand *base : script->sectionCommands)
      if (auto *sec = dyn_cast<OutputSection>(base))
        addStartStopSymbols(sec);
  }

  // Add _DYNAMIC symbol. Unlike GNU gold, our _DYNAMIC symbol has no type.
  // It should be okay as no one seems to care about the type.
  // Even the author of gold doesn't remember why gold behaves that way.
  // https://sourceware.org/ml/binutils/2002-03/msg00360.html
  bool needsDYNAMIC = (config->isPic || !sharedFiles.empty()); // TODO: --as-needed?
  if (mainPart->dynamic->parent && needsDYNAMIC) {
    // Set _DYNAMIC to null for static binaries without shared libraries
    // Note: This is needed in order to not have a valid _DYNAMIC if
    // --export-dynamic is passed to a static executable. Some programs check
    // if they are dynamically linked using `if (&_DYNAMIC != 0)` so we should
    // keep this check working.

    auto *s = symtab->addSymbol(Defined{/*file=*/nullptr, "_DYNAMIC", STB_WEAK,
                              STV_HIDDEN, STT_NOTYPE,
                              /*value=*/0, /*size=*/0, mainPart->dynamic});
    // In CheriABI we want sensible bounds if we do &_DYNAMIC in C code
    s->isSectionStartSymbol = true;
  }

  // The common check if a program is dynamically linked (&_DYNAMIC != 0)
  // will not work in the early CHERI startup. In PIC code we also can't
  // use dla _DYNAMIC since that needs either $gp or text relocations
  // Instead we just let the linker generate a new symbol _HAS__DYNAMIC
  if (auto *reference = symtab->find("_HAS__DYNAMIC"))
    if (!reference->isDefined()) {
      Defined *hasDynamicSym = addAbsolute("_HAS__DYNAMIC");
      hasDynamicSym->value = needsDYNAMIC ? 1 : 0;
    }

  // Define __rel[a]_iplt_{start,end} symbols if needed.
  addRelIpltSymbols();

  // RISC-V's gp can address +/- 2 KiB, set it to .sdata + 0x800. This symbol
  // should only be defined in an executable. If .sdata does not exist, its
  // value/section does not matter but it has to be relative, so set its
  // st_shndx arbitrarily to 1 (Out::elfHeader).
  if (config->emachine == EM_RISCV && !config->shared) {
    OutputSection *sec = findSection(".sdata");
    ElfSym::riscvGlobalPointer =
        addOptionalRegular("__global_pointer$", sec ? sec : Out::elfHeader,
                           0x800, STV_DEFAULT, STB_GLOBAL);
  }

  if (config->emachine == EM_X86_64) {
    // On targets that support TLSDESC, _TLS_MODULE_BASE_ is defined in such a
    // way that:
    //
    // 1) Without relaxation: it produces a dynamic TLSDESC relocation that
    // computes 0.
    // 2) With LD->LE relaxation: _TLS_MODULE_BASE_@tpoff = 0 (lowest address in
    // the TLS block).
    //
    // 2) is special cased in @tpoff computation. To satisfy 1), we define it as
    // an absolute symbol of zero. This is different from GNU linkers which
    // define _TLS_MODULE_BASE_ relative to the first TLS section.
    Symbol *s = symtab->find("_TLS_MODULE_BASE_");
    if (s && s->isUndefined()) {
      s->resolve(Defined{/*file=*/nullptr, s->getName(), STB_GLOBAL, STV_HIDDEN,
                         STT_TLS, /*value=*/0, 0,
                         /*section=*/nullptr});
      ElfSym::tlsModuleBase = cast<Defined>(s);
    }
  }

  StringRef captableSym = "_CHERI_CAPABILITY_TABLE_";
  if (in.cheriCapTable) {
    // When creating relocatable output we should not define the
    // _CHERI_CAPABILITY_TABLE_ symbol because otherwise we get duplicate
    // symbol errors when linking that into a final executable
    if (!config->relocatable)
      ElfSym::cheriCapabilityTable =
          addOptionalRegular(captableSym, in.cheriCapTable, 0);
  }

  // This responsible for splitting up .eh_frame section into
  // pieces. The relocation scan uses those pieces, so this has to be
  // earlier.
  for (Partition &part : partitions)
    finalizeSynthetic(part.ehFrame);

  for (Symbol *sym : symtab->symbols())
    sym->isPreemptible = computeIsPreemptible(*sym);

  // Change values of linker-script-defined symbols from placeholders (assigned
  // by declareSymbols) to actual definitions.
  script->processSymbolAssignments();

  // Scan relocations. This must be done after every symbol is declared so that
  // we can correctly decide if a dynamic relocation is needed. This is called
  // after processSymbolAssignments() because it needs to know whether a
  // linker-script-defined symbol is absolute.
  ppc64noTocRelax.clear();
  if (!config->relocatable) {
    forEachRelSec(scanRelocations<ELFT>);
    reportUndefinedSymbols<ELFT>();
  }

  // Do the cap table index assignment
  // Must come before CapRelocs->finalizeContents() because it can add
  // __cap_relocs
  if (in.cheriCapTable) {
    // Ensure that we always have a _CHERI_CAPABILITY_TABLE_ symbol if the
    // cap table exists. This makes llvm-objdump more useful since it can now
    // print the target of a cap table load
    if (!ElfSym::cheriCapabilityTable && in.cheriCapTable->isNeeded()) {
      ElfSym::cheriCapabilityTable = cast<Defined>(
          symtab->addSymbol(Defined{nullptr, captableSym, STB_LOCAL,
            STV_HIDDEN, STT_NOTYPE, 0, 0, in.cheriCapTable}));
      ElfSym::cheriCapabilityTable->isSectionStartSymbol = true;
      assert(!ElfSym::cheriCapabilityTable->isPreemptible);
    }
    in.cheriCapTable->assignValuesAndAddCapTableSymbols<ELFT>();
  }

  // Now handle __cap_relocs (must be before RelaDyn because it might
  // result in new dynamic relocations being added)
  if (InX<ELFT>::capRelocs) {
    finalizeSynthetic(InX<ELFT>::capRelocs);
  }

  if (in.plt && in.plt->isNeeded())
    in.plt->addSymbols();
  if (in.iplt && in.iplt->isNeeded())
    in.iplt->addSymbols();

  if (!config->allowShlibUndefined) {
    // Error on undefined symbols in a shared object, if all of its DT_NEEDED
    // entries are seen. These cases would otherwise lead to runtime errors
    // reported by the dynamic linker.
    //
    // ld.bfd traces all DT_NEEDED to emulate the logic of the dynamic linker to
    // catch more cases. That is too much for us. Our approach resembles the one
    // used in ld.gold, achieves a good balance to be useful but not too smart.
    for (SharedFile *file : sharedFiles)
      file->allNeededIsKnown =
          llvm::all_of(file->dtNeeded, [&](StringRef needed) {
            return symtab->soNames.count(needed);
          });

    for (Symbol *sym : symtab->symbols())
      if (sym->isUndefined() && !sym->isWeak())
        if (auto *f = dyn_cast_or_null<SharedFile>(sym->file))
          if (f->allNeededIsKnown)
            errorOrWarn(toString(f) + ": undefined reference to " +
                        toString(*sym) + " [--no-allow-shlib-undefined]");
  }

  // Now that we have defined all possible global symbols including linker-
  // synthesized ones. Visit all symbols to give the finishing touches.
  for (Symbol *sym : symtab->symbols()) {
    if (!includeInSymtab(*sym))
      continue;
    if (in.symTab)
      in.symTab->addSymbol(sym);

    if (sym->includeInDynsym()) {
      partitions[sym->partition - 1].dynSymTab->addSymbol(sym);
      if (auto *file = dyn_cast_or_null<SharedFile>(sym->file))
        if (file->isNeeded && !sym->isUndefined())
          addVerneed(sym);
    }
  }

  // We also need to scan the dynamic relocation tables of the other partitions
  // and add any referenced symbols to the partition's dynsym.
  for (Partition &part : MutableArrayRef<Partition>(partitions).slice(1)) {
    DenseSet<Symbol *> syms;
    for (const SymbolTableEntry &e : part.dynSymTab->getSymbols())
      syms.insert(e.sym);
    for (DynamicReloc &reloc : part.relaDyn->relocs)
      if (reloc.sym && !reloc.useSymVA && syms.insert(reloc.sym).second)
        part.dynSymTab->addSymbol(reloc.sym);
  }

  // Do not proceed if there was an undefined symbol.
  if (errorCount())
    return;

  if (in.mipsGot)
    in.mipsGot->build();

  removeUnusedSyntheticSections();
  script->diagnoseOrphanHandling();

  sortSections();

  // Now that we have the final list, create a list of all the
  // OutputSections for convenience.
  for (BaseCommand *base : script->sectionCommands)
    if (auto *sec = dyn_cast<OutputSection>(base))
      outputSections.push_back(sec);

  // Prefer command line supplied address over other constraints.
  for (OutputSection *sec : outputSections) {
    auto i = config->sectionStartMap.find(sec->name);
    if (i != config->sectionStartMap.end())
      sec->addrExpr = [=] { return i->second; };
  }

  // With the outputSections available check for GDPLT relocations
  // and add __tls_get_addr symbol if needed.
  if (config->emachine == EM_HEXAGON && hexagonNeedsTLSSymbol(outputSections)) {
    Symbol *sym = symtab->addSymbol(Undefined{
        nullptr, "__tls_get_addr", STB_GLOBAL, STV_DEFAULT, STT_NOTYPE});
    sym->isPreemptible = true;
    partitions[0].dynSymTab->addSymbol(sym);
  }

  // This is a bit of a hack. A value of 0 means undef, so we set it
  // to 1 to make __ehdr_start defined. The section number is not
  // particularly relevant.
  Out::elfHeader->sectionIndex = 1;

  for (size_t i = 0, e = outputSections.size(); i != e; ++i) {
    OutputSection *sec = outputSections[i];
    sec->sectionIndex = i + 1;
    sec->shName = in.shStrTab->addString(sec->name);
  }

  // Binary and relocatable output does not have PHDRS.
  // The headers have to be created before finalize as that can influence the
  // image base and the dynamic section on mips includes the image base.
  if (!config->relocatable && !config->oFormatBinary) {
    for (Partition &part : partitions) {
      part.phdrs = script->hasPhdrsCommands() ? script->createPhdrs()
                                              : createPhdrs(part);
      if (config->emachine == EM_ARM) {
        // PT_ARM_EXIDX is the ARM EHABI equivalent of PT_GNU_EH_FRAME
        addPhdrForSection(part, SHT_ARM_EXIDX, PT_ARM_EXIDX, PF_R);
      }
      if (config->emachine == EM_MIPS) {
        // Add separate segments for MIPS-specific sections.
        addPhdrForSection(part, SHT_MIPS_REGINFO, PT_MIPS_REGINFO, PF_R);
        addPhdrForSection(part, SHT_MIPS_OPTIONS, PT_MIPS_OPTIONS, PF_R);
        addPhdrForSection(part, SHT_MIPS_ABIFLAGS, PT_MIPS_ABIFLAGS, PF_R);
      }
    }
    Out::programHeaders->size = sizeof(Elf_Phdr) * mainPart->phdrs.size();

    // Find the TLS segment. This happens before the section layout loop so that
    // Android relocation packing can look up TLS symbol addresses. We only need
    // to care about the main partition here because all TLS symbols were moved
    // to the main partition (see MarkLive.cpp).
    for (PhdrEntry *p : mainPart->phdrs)
      if (p->p_type == PT_TLS)
        Out::tlsPhdr = p;
  }

  // Some symbols are defined in term of program headers. Now that we
  // have the headers, we can find out which sections they point to.
  setReservedSymbolSections();

  finalizeSynthetic(in.bss);
  finalizeSynthetic(in.bssRelRo);
  finalizeSynthetic(in.symTabShndx);
  finalizeSynthetic(in.shStrTab);
  finalizeSynthetic(in.strTab);
  finalizeSynthetic(in.got);
  finalizeSynthetic(in.mipsGot);
  finalizeSynthetic(in.igotPlt);
  finalizeSynthetic(in.gotPlt);
  finalizeSynthetic(in.relaIplt);
  finalizeSynthetic(in.relaPlt);
  finalizeSynthetic(in.plt);
  finalizeSynthetic(in.iplt);
  finalizeSynthetic(in.ppc32Got2);
  finalizeSynthetic(in.partIndex);

  // Dynamic section must be the last one in this list and dynamic
  // symbol table section (dynSymTab) must be the first one.
  for (Partition &part : partitions) {
    finalizeSynthetic(part.dynSymTab);
    finalizeSynthetic(part.gnuHashTab);
    finalizeSynthetic(part.hashTab);
    finalizeSynthetic(part.verDef);
    finalizeSynthetic(part.relaDyn);
    finalizeSynthetic(part.relrDyn);
    finalizeSynthetic(part.ehFrameHdr);
    finalizeSynthetic(part.verSym);
    finalizeSynthetic(part.verNeed);
    finalizeSynthetic(part.dynamic);
  }

  if (!script->hasSectionsCommand && !config->relocatable)
    fixSectionAlignments();

  // This is used to:
  // 1) Create "thunks":
  //    Jump instructions in many ISAs have small displacements, and therefore
  //    they cannot jump to arbitrary addresses in memory. For example, RISC-V
  //    JAL instruction can target only +-1 MiB from PC. It is a linker's
  //    responsibility to create and insert small pieces of code between
  //    sections to extend the ranges if jump targets are out of range. Such
  //    code pieces are called "thunks".
  //
  //    We add thunks at this stage. We couldn't do this before this point
  //    because this is the earliest point where we know sizes of sections and
  //    their layouts (that are needed to determine if jump targets are in
  //    range).
  //
  // 2) Update the sections. We need to generate content that depends on the
  //    address of InputSections. For example, MIPS GOT section content or
  //    android packed relocations sections content.
  //
  // 3) Assign the final values for the linker script symbols. Linker scripts
  //    sometimes using forward symbol declarations. We want to set the correct
  //    values. They also might change after adding the thunks.
  finalizeAddressDependentContent();
  if (errorCount())
    return;

  // finalizeAddressDependentContent may have added local symbols to the static symbol table.
  finalizeSynthetic(in.symTab);
  finalizeSynthetic(in.ppc64LongBranchTarget);

  // Relaxation to delete inter-basic block jumps created by basic block
  // sections. Run after in.symTab is finalized as optimizeBasicBlockJumps
  // can relax jump instructions based on symbol offset.
  if (config->optimizeBBJumps)
    optimizeBasicBlockJumps();

  // Fill other section headers. The dynamic table is finalized
  // at the end because some tags like RELSZ depend on result
  // of finalizing other sections.
  for (OutputSection *sec : outputSections)
    sec->finalize();

  // If a synthetic section was removed from the output we have to manually
  // change the start&stop symbols to be NULL since otherwise we create a
  // corrupted symbol table
  // XXXAR: I think this only affects __cap_relocs since the other potentially
  // empty synthetic sections will not have start stop symbols
  for (Symbol *s : symtab->getSymbols()) {
    auto *reg = dyn_cast<Defined>(s);
    if (!reg)
      continue;
    if (const OutputSection *outSec = reg->getOutputSection())
      // XXXAR: Out::ElfHeader is a special output section and will never be
      // marked as live. We still need keep symbols pointing there since they
      // will then point to the first output section
      // See assignment above:   Out::ElfHeader->SectionIndex = 1;
      // Ditto for LinkerScript's "aether" otherwise we will zero out all
      // symbol assignments outside output sections.

      if (!outSec->isLive() && outSec != Out::elfHeader &&
          !script->isAether(outSec)) {
        // errs() << "Symbol " << toString(*Reg) << " points to garbage collected output section " << OutSec->Name << "\n";
        reg->type = STT_NOTYPE;
        reg->section = nullptr;
        reg->value = 0;
        reg->size = 0;
        /* Avoid crashes when calling getSize() */
        reg->isSectionStartSymbol = false;
      }
  }
}

// Ensure data sections are not mixed with executable sections when
// -execute-only is used. -execute-only is a feature to make pages executable
// but not readable, and the feature is currently supported only on AArch64.
template <class ELFT> void Writer<ELFT>::checkExecuteOnly() {
  if (!config->executeOnly)
    return;

  for (OutputSection *os : outputSections)
    if (os->flags & SHF_EXECINSTR)
      for (InputSection *isec : getInputSections(os))
        if (!(isec->flags & SHF_EXECINSTR))
          error("cannot place " + toString(isec) + " into " + toString(os->name) +
                ": -execute-only does not support intermingling data and code");
}

// The linker is expected to define SECNAME_start and SECNAME_end
// symbols for a few sections. This function defines them.
template <class ELFT> void Writer<ELFT>::addStartEndSymbols() {
  // If a section does not exist, there's ambiguity as to how we
  // define _start and _end symbols for an init/fini section. Since
  // the loader assume that the symbols are always defined, we need to
  // always define them. But what value? The loader iterates over all
  // pointers between _start and _end to run global ctors/dtors, so if
  // the section is empty, their symbol values don't actually matter
  // as long as _start and _end point to the same location.
  //
  // That said, we don't want to set the symbols to 0 (which is
  // probably the simplest value) because that could cause some
  // program to fail to link due to relocation overflow, if their
  // program text is above 2 GiB. We use the address of the .text
  // section instead to prevent that failure.
  //
  // In rare situations, the .text section may not exist. If that's the
  // case, use the image base address as a last resort.
  OutputSection *Default = findSection(".text");
  if (!Default)
    Default = Out::elfHeader;

  auto define = [=](StringRef start, StringRef end, OutputSection *os) {
    if (os) {
      addOptionalRegular(start, os, 0);
      addOptionalRegular(end, os, -1);
    } else {
      // Since this is an empty section we don't want to set canBeSectionStart
      // Iterating over this should terminate immediately so setting the size
      // to zero is fine
      addOptionalRegular(start, Default, 0, STV_HIDDEN, STB_GLOBAL,
                         /*canBeSectionStart=*/false);
      // End is not a section start symbol even though it has value 0:
      addOptionalRegular(end, Default, 0, STV_HIDDEN, STB_GLOBAL,
                         /*canBeSectionStart=*/false);
    }
  };

  define("__preinit_array_start", "__preinit_array_end", Out::preinitArray);
  define("__init_array_start", "__init_array_end", Out::initArray);
  define("__fini_array_start", "__fini_array_end", Out::finiArray);
  define("__ctors_start", "__ctors_end", findSection(".ctors"));
  define("__dtors_start", "__dtors_end", findSection(".dtors"));
  if (in.cheriCapTable)
    define("__cap_table_start", "__cap_table_end",
           in.cheriCapTable->getOutputSection());

  if (OutputSection *sec = findSection(".ARM.exidx"))
    define("__exidx_start", "__exidx_end", sec);
}

// If a section name is valid as a C identifier (which is rare because of
// the leading '.'), linkers are expected to define __start_<secname> and
// __stop_<secname> symbols. They are at beginning and end of the section,
// respectively. This is not requested by the ELF standard, but GNU ld and
// gold provide the feature, and used by many programs.
template <class ELFT>
void Writer<ELFT>::addStartStopSymbols(OutputSection *sec) {
  StringRef s = sec->name;
  if (!isValidCIdentifier(s))
    return;
  addOptionalRegular(saver.save("__start_" + s), sec, 0,
                     config->zStartStopVisibility);
  addOptionalRegular(saver.save("__stop_" + s), sec, -1,
                     config->zStartStopVisibility);
}

static bool needsPtLoad(OutputSection *sec) {
  if (!(sec->flags & SHF_ALLOC) || sec->noload)
    return false;

  // Don't allocate VA space for TLS NOBITS sections. The PT_TLS PHDR is
  // responsible for allocating space for them, not the PT_LOAD that
  // contains the TLS initialization image.
  if ((sec->flags & SHF_TLS) && sec->type == SHT_NOBITS)
    return false;
  return true;
}

// Linker scripts are responsible for aligning addresses. Unfortunately, most
// linker scripts are designed for creating two PT_LOADs only, one RX and one
// RW. This means that there is no alignment in the RO to RX transition and we
// cannot create a PT_LOAD there.
static uint64_t computeFlags(uint64_t flags) {
  if (config->omagic)
    return PF_R | PF_W | PF_X;
  if (config->executeOnly && (flags & PF_X))
    return flags & ~PF_R;
  if (config->singleRoRx && !(flags & PF_W))
    return flags | PF_X;
  return flags;
}

// Decide which program headers to create and which sections to include in each
// one.
template <class ELFT>
std::vector<PhdrEntry *> Writer<ELFT>::createPhdrs(Partition &part) {
  std::vector<PhdrEntry *> ret;
  auto addHdr = [&](unsigned type, unsigned flags) -> PhdrEntry * {
    ret.push_back(make<PhdrEntry>(type, flags));
    return ret.back();
  };

  unsigned partNo = part.getNumber();
  bool isMain = partNo == 1;

  // Add the first PT_LOAD segment for regular output sections.
  uint64_t flags = computeFlags(PF_R);
  PhdrEntry *load = nullptr;

  // nmagic or omagic output does not have PT_PHDR, PT_INTERP, or the readonly
  // PT_LOAD.
  if (!config->nmagic && !config->omagic) {
    // The first phdr entry is PT_PHDR which describes the program header
    // itself.
    if (isMain)
      addHdr(PT_PHDR, PF_R)->add(Out::programHeaders);
    else
      addHdr(PT_PHDR, PF_R)->add(part.programHeaders->getParent());

    // PT_INTERP must be the second entry if exists.
    if (OutputSection *cmd = findSection(".interp", partNo))
      addHdr(PT_INTERP, cmd->getPhdrFlags())->add(cmd);

    // Add the headers. We will remove them if they don't fit.
    // In the other partitions the headers are ordinary sections, so they don't
    // need to be added here.
    if (isMain) {
      load = addHdr(PT_LOAD, flags);
      load->add(Out::elfHeader);
      load->add(Out::programHeaders);
    }
  }

  // PT_GNU_RELRO includes all sections that should be marked as
  // read-only by dynamic linker after processing relocations.
  // Current dynamic loaders only support one PT_GNU_RELRO PHDR, give
  // an error message if more than one PT_GNU_RELRO PHDR is required.
  PhdrEntry *relRo = make<PhdrEntry>(PT_GNU_RELRO, PF_R);
  bool inRelroPhdr = false;
  OutputSection *relroEnd = nullptr;
  for (OutputSection *sec : outputSections) {
    if (sec->partition != partNo || !needsPtLoad(sec))
      continue;
    if (isRelroSection(sec)) {
      inRelroPhdr = true;
      if (!relroEnd)
        relRo->add(sec);
      else
        error("section: " + sec->name + " is not contiguous with other relro" +
              " sections");
    } else if (inRelroPhdr) {
      inRelroPhdr = false;
      relroEnd = sec;
    }
  }

  for (OutputSection *sec : outputSections) {
    if (!needsPtLoad(sec))
      continue;

    // Normally, sections in partitions other than the current partition are
    // ignored. But partition number 255 is a special case: it contains the
    // partition end marker (.part.end). It needs to be added to the main
    // partition so that a segment is created for it in the main partition,
    // which will cause the dynamic loader to reserve space for the other
    // partitions.
    if (sec->partition != partNo) {
      if (isMain && sec->partition == 255)
        addHdr(PT_LOAD, computeFlags(sec->getPhdrFlags()))->add(sec);
      continue;
    }

    // Segments are contiguous memory regions that has the same attributes
    // (e.g. executable or writable). There is one phdr for each segment.
    // Therefore, we need to create a new phdr when the next section has
    // different flags or is loaded at a discontiguous address or memory
    // region using AT or AT> linker script command, respectively. At the same
    // time, we don't want to create a separate load segment for the headers,
    // even if the first output section has an AT or AT> attribute.
    uint64_t newFlags = computeFlags(sec->getPhdrFlags());
    bool sameLMARegion =
        load && !sec->lmaExpr && sec->lmaRegion == load->firstSec->lmaRegion;
    if (!(load && newFlags == flags && sec != relroEnd &&
          sec->memRegion == load->firstSec->memRegion &&
          (sameLMARegion || load->lastSec == Out::programHeaders))) {
      load = addHdr(PT_LOAD, newFlags);
      flags = newFlags;
    }

    load->add(sec);
  }

  // Add a TLS segment if any.
  PhdrEntry *tlsHdr = make<PhdrEntry>(PT_TLS, PF_R);
  for (OutputSection *sec : outputSections)
    if (sec->partition == partNo && sec->flags & SHF_TLS)
      tlsHdr->add(sec);
  if (tlsHdr->firstSec)
    ret.push_back(tlsHdr);

  // Add an entry for .dynamic.
  if (OutputSection *sec = part.dynamic->getParent())
    addHdr(PT_DYNAMIC, sec->getPhdrFlags())->add(sec);

  if (relRo->firstSec)
    ret.push_back(relRo);

  // PT_GNU_EH_FRAME is a special section pointing on .eh_frame_hdr.
  if (part.ehFrame->isNeeded() && part.ehFrameHdr &&
      part.ehFrame->getParent() && part.ehFrameHdr->getParent())
    addHdr(PT_GNU_EH_FRAME, part.ehFrameHdr->getParent()->getPhdrFlags())
        ->add(part.ehFrameHdr->getParent());

  // PT_OPENBSD_RANDOMIZE is an OpenBSD-specific feature. That makes
  // the dynamic linker fill the segment with random data.
  if (OutputSection *cmd = findSection(".openbsd.randomdata", partNo))
    addHdr(PT_OPENBSD_RANDOMIZE, cmd->getPhdrFlags())->add(cmd);

  if (config->zGnustack != GnuStackKind::None) {
    // PT_GNU_STACK is a special section to tell the loader to make the
    // pages for the stack non-executable. If you really want an executable
    // stack, you can pass -z execstack, but that's not recommended for
    // security reasons.
    unsigned perm = PF_R | PF_W;
    if (config->zGnustack == GnuStackKind::Exec)
      perm |= PF_X;
    addHdr(PT_GNU_STACK, perm)->p_memsz = config->zStackSize;
  }

  // PT_OPENBSD_WXNEEDED is a OpenBSD-specific header to mark the executable
  // is expected to perform W^X violations, such as calling mprotect(2) or
  // mmap(2) with PROT_WRITE | PROT_EXEC, which is prohibited by default on
  // OpenBSD.
  if (config->zWxneeded)
    addHdr(PT_OPENBSD_WXNEEDED, PF_X);

  if (OutputSection *cmd = findSection(".note.gnu.property", partNo))
    addHdr(PT_GNU_PROPERTY, PF_R)->add(cmd);

  // Create one PT_NOTE per a group of contiguous SHT_NOTE sections with the
  // same alignment.
  PhdrEntry *note = nullptr;
  for (OutputSection *sec : outputSections) {
    if (sec->partition != partNo)
      continue;
    if (sec->type == SHT_NOTE && (sec->flags & SHF_ALLOC)) {
      if (!note || sec->lmaExpr || note->lastSec->alignment != sec->alignment)
        note = addHdr(PT_NOTE, PF_R);
      note->add(sec);
    } else {
      note = nullptr;
    }
  }
  return ret;
}

template <class ELFT>
void Writer<ELFT>::addPhdrForSection(Partition &part, unsigned shType,
                                     unsigned pType, unsigned pFlags) {
  unsigned partNo = part.getNumber();
  auto i = llvm::find_if(outputSections, [=](OutputSection *cmd) {
    return cmd->partition == partNo && cmd->type == shType;
  });
  if (i == outputSections.end())
    return;

  PhdrEntry *entry = make<PhdrEntry>(pType, pFlags);
  entry->add(*i);
  part.phdrs.push_back(entry);
}

// Place the first section of each PT_LOAD to a different page (of maxPageSize).
// This is achieved by assigning an alignment expression to addrExpr of each
// such section.
template <class ELFT> void Writer<ELFT>::fixSectionAlignments() {
  const PhdrEntry *prev;
  auto pageAlign = [&](const PhdrEntry *p) {
    OutputSection *cmd = p->firstSec;
    if (!cmd)
      return;
    cmd->alignExpr = [align = cmd->alignment]() { return align; };
    if (!cmd->addrExpr) {
      // Prefer advancing to align(dot, maxPageSize) + dot%maxPageSize to avoid
      // padding in the file contents.
      //
      // When -z separate-code is used we must not have any overlap in pages
      // between an executable segment and a non-executable segment. We align to
      // the next maximum page size boundary on transitions between executable
      // and non-executable segments.
      //
      // SHT_LLVM_PART_EHDR marks the start of a partition. The partition
      // sections will be extracted to a separate file. Align to the next
      // maximum page size boundary so that we can find the ELF header at the
      // start. We cannot benefit from overlapping p_offset ranges with the
      // previous segment anyway.
      if (config->zSeparate == SeparateSegmentKind::Loadable ||
          (config->zSeparate == SeparateSegmentKind::Code && prev &&
           (prev->p_flags & PF_X) != (p->p_flags & PF_X)) ||
          cmd->type == SHT_LLVM_PART_EHDR)
        cmd->addrExpr = [] {
          return alignTo(script->getDot(), config->maxPageSize);
        };
      // PT_TLS is at the start of the first RW PT_LOAD. If `p` includes PT_TLS,
      // it must be the RW. Align to p_align(PT_TLS) to make sure
      // p_vaddr(PT_LOAD)%p_align(PT_LOAD) = 0. Otherwise, if
      // sh_addralign(.tdata) < sh_addralign(.tbss), we will set p_align(PT_TLS)
      // to sh_addralign(.tbss), while p_vaddr(PT_TLS)=p_vaddr(PT_LOAD) may not
      // be congruent to 0 modulo p_align(PT_TLS).
      //
      // Technically this is not required, but as of 2019, some dynamic loaders
      // don't handle p_vaddr%p_align != 0 correctly, e.g. glibc (i386 and
      // x86-64) doesn't make runtime address congruent to p_vaddr modulo
      // p_align for dynamic TLS blocks (PR/24606), FreeBSD rtld has the same
      // bug, musl (TLS Variant 1 architectures) before 1.1.23 handled TLS
      // blocks correctly. We need to keep the workaround for a while.
      else if (Out::tlsPhdr && Out::tlsPhdr->firstSec == p->firstSec)
        cmd->addrExpr = [] {
          return alignTo(script->getDot(), config->maxPageSize) +
                 alignTo(script->getDot() % config->maxPageSize,
                         Out::tlsPhdr->p_align);
        };
      else
        cmd->addrExpr = [] {
          return alignTo(script->getDot(), config->maxPageSize) +
                 script->getDot() % config->maxPageSize;
        };
    }
  };

  for (Partition &part : partitions) {
    prev = nullptr;
    for (const PhdrEntry *p : part.phdrs)
      if (p->p_type == PT_LOAD && p->firstSec) {
        pageAlign(p);
        prev = p;
      }
  }
}

// Compute an in-file position for a given section. The file offset must be the
// same with its virtual address modulo the page size, so that the loader can
// load executables without any address adjustment.
static uint64_t computeFileOffset(OutputSection *os, uint64_t off) {
  // The first section in a PT_LOAD has to have congruent offset and address
  // modulo the maximum page size.
  if (os->ptLoad && os->ptLoad->firstSec == os)
    return alignTo(off, os->ptLoad->p_align, os->addr);

  // File offsets are not significant for .bss sections other than the first one
  // in a PT_LOAD. By convention, we keep section offsets monotonically
  // increasing rather than setting to zero.
   if (os->type == SHT_NOBITS)
     return off;

  // If the section is not in a PT_LOAD, we just have to align it.
  if (!os->ptLoad)
    return alignTo(off, os->alignment);

  // If two sections share the same PT_LOAD the file offset is calculated
  // using this formula: Off2 = Off1 + (VA2 - VA1).
  OutputSection *first = os->ptLoad->firstSec;
  return first->offset + os->addr - first->addr;
}

// Set an in-file position to a given section and returns the end position of
// the section.
static uint64_t setFileOffset(OutputSection *os, uint64_t off) {
  off = computeFileOffset(os, off);
  os->offset = off;

  if (os->type == SHT_NOBITS)
    return off;
  return off + os->size;
}

template <class ELFT> void Writer<ELFT>::assignFileOffsetsBinary() {
  // Compute the minimum LMA of all non-empty non-NOBITS sections as minAddr.
  auto needsOffset = [](OutputSection &sec) {
    return sec.type != SHT_NOBITS && (sec.flags & SHF_ALLOC) && sec.size > 0;
  };
  uint64_t minAddr = UINT64_MAX;
  for (OutputSection *sec : outputSections)
    if (needsOffset(*sec)) {
      sec->offset = sec->getLMA();
      minAddr = std::min(minAddr, sec->offset);
    }

  // Sections are laid out at LMA minus minAddr.
  fileSize = 0;
  for (OutputSection *sec : outputSections)
    if (needsOffset(*sec)) {
      sec->offset -= minAddr;
      fileSize = std::max(fileSize, sec->offset + sec->size);
    }
}

static std::string rangeToString(uint64_t addr, uint64_t len) {
  return "[0x" + utohexstr(addr) + ", 0x" + utohexstr(addr + len - 1) + "]";
}

// Assign file offsets to output sections.
template <class ELFT> void Writer<ELFT>::assignFileOffsets() {
  uint64_t off = 0;
  off = setFileOffset(Out::elfHeader, off);
  off = setFileOffset(Out::programHeaders, off);

  PhdrEntry *lastRX = nullptr;
  for (Partition &part : partitions)
    for (PhdrEntry *p : part.phdrs)
      if (p->p_type == PT_LOAD && (p->p_flags & PF_X))
        lastRX = p;

  // Layout SHF_ALLOC sections before non-SHF_ALLOC sections. A non-SHF_ALLOC
  // will not occupy file offsets contained by a PT_LOAD.
  for (OutputSection *sec : outputSections) {
    if (!(sec->flags & SHF_ALLOC))
      continue;
    off = setFileOffset(sec, off);

    // If this is a last section of the last executable segment and that
    // segment is the last loadable segment, align the offset of the
    // following section to avoid loading non-segments parts of the file.
    if (config->zSeparate != SeparateSegmentKind::None && lastRX &&
        lastRX->lastSec == sec)
      off = alignTo(off, config->commonPageSize);
  }
  for (OutputSection *sec : outputSections)
    if (!(sec->flags & SHF_ALLOC))
      off = setFileOffset(sec, off);

  sectionHeaderOff = alignTo(off, config->wordsize);
  fileSize = sectionHeaderOff + (outputSections.size() + 1) * sizeof(Elf_Shdr);

  // Our logic assumes that sections have rising VA within the same segment.
  // With use of linker scripts it is possible to violate this rule and get file
  // offset overlaps or overflows. That should never happen with a valid script
  // which does not move the location counter backwards and usually scripts do
  // not do that. Unfortunately, there are apps in the wild, for example, Linux
  // kernel, which control segment distribution explicitly and move the counter
  // backwards, so we have to allow doing that to support linking them. We
  // perform non-critical checks for overlaps in checkSectionOverlap(), but here
  // we want to prevent file size overflows because it would crash the linker.
  for (OutputSection *sec : outputSections) {
    if (sec->type == SHT_NOBITS)
      continue;
    if ((sec->offset > fileSize) || (sec->offset + sec->size > fileSize))
      error("unable to place section " + sec->name + " at file offset " +
            rangeToString(sec->offset, sec->size) +
            "; check your linker script for overflows");
  }
}

// Finalize the program headers. We call this function after we assign
// file offsets and VAs to all sections.
template <class ELFT> void Writer<ELFT>::setPhdrs(Partition &part) {
  for (PhdrEntry *p : part.phdrs) {
    OutputSection *first = p->firstSec;
    OutputSection *last = p->lastSec;

    if (first) {
      p->p_filesz = last->offset - first->offset;
      if (last->type != SHT_NOBITS)
        p->p_filesz += last->size;

      p->p_memsz = last->addr + last->size - first->addr;
      p->p_offset = first->offset;
      p->p_vaddr = first->addr;

      // File offsets in partitions other than the main partition are relative
      // to the offset of the ELF headers. Perform that adjustment now.
      if (part.elfHeader)
        p->p_offset -= part.elfHeader->getParent()->offset;

      if (!p->hasLMA)
        p->p_paddr = first->getLMA();
    }

    if (p->p_type == PT_GNU_RELRO) {
      p->p_align = 1;
      // musl/glibc ld.so rounds the size down, so we need to round up
      // to protect the last page. This is a no-op on FreeBSD which always
      // rounds up.
      p->p_memsz = alignTo(p->p_offset + p->p_memsz, config->commonPageSize) -
                   p->p_offset;
    }
  }
}

// A helper struct for checkSectionOverlap.
namespace {
struct SectionOffset {
  OutputSection *sec;
  uint64_t offset;
};
} // namespace

// Check whether sections overlap for a specific address range (file offsets,
// load and virtual addresses).
static void checkOverlap(StringRef name, std::vector<SectionOffset> &sections,
                         bool isVirtualAddr) {
  llvm::sort(sections, [=](const SectionOffset &a, const SectionOffset &b) {
    return a.offset < b.offset;
  });

  // Finding overlap is easy given a vector is sorted by start position.
  // If an element starts before the end of the previous element, they overlap.
  for (size_t i = 1, end = sections.size(); i < end; ++i) {
    SectionOffset a = sections[i - 1];
    SectionOffset b = sections[i];
    if (b.offset >= a.offset + a.sec->size)
      continue;

    // If both sections are in OVERLAY we allow the overlapping of virtual
    // addresses, because it is what OVERLAY was designed for.
    if (isVirtualAddr && a.sec->inOverlay && b.sec->inOverlay)
      continue;

    errorOrWarn("section " + a.sec->name + " " + name +
                " range overlaps with " + b.sec->name + "\n>>> " + a.sec->name +
                " range is " + rangeToString(a.offset, a.sec->size) + "\n>>> " +
                b.sec->name + " range is " +
                rangeToString(b.offset, b.sec->size));
  }
}

// Check for overlapping sections and address overflows.
//
// In this function we check that none of the output sections have overlapping
// file offsets. For SHF_ALLOC sections we also check that the load address
// ranges and the virtual address ranges don't overlap
template <class ELFT> void Writer<ELFT>::checkSections() {
  // First, check that section's VAs fit in available address space for target.
  for (OutputSection *os : outputSections)
    if ((os->addr + os->size < os->addr) ||
        (!ELFT::Is64Bits && os->addr + os->size > UINT32_MAX))
      errorOrWarn("section " + os->name + " at 0x" + utohexstr(os->addr) +
                  " of size 0x" + utohexstr(os->size) +
                  " exceeds available address space");

  // Check for overlapping file offsets. In this case we need to skip any
  // section marked as SHT_NOBITS. These sections don't actually occupy space in
  // the file so Sec->Offset + Sec->Size can overlap with others. If --oformat
  // binary is specified only add SHF_ALLOC sections are added to the output
  // file so we skip any non-allocated sections in that case.
  std::vector<SectionOffset> fileOffs;
  for (OutputSection *sec : outputSections)
    if (sec->size > 0 && sec->type != SHT_NOBITS &&
        (!config->oFormatBinary || (sec->flags & SHF_ALLOC)))
      fileOffs.push_back({sec, sec->offset});
  checkOverlap("file", fileOffs, false);

  // When linking with -r there is no need to check for overlapping virtual/load
  // addresses since those addresses will only be assigned when the final
  // executable/shared object is created.
  if (config->relocatable)
    return;

  // Checking for overlapping virtual and load addresses only needs to take
  // into account SHF_ALLOC sections since others will not be loaded.
  // Furthermore, we also need to skip SHF_TLS sections since these will be
  // mapped to other addresses at runtime and can therefore have overlapping
  // ranges in the file.
  std::vector<SectionOffset> vmas;
  for (OutputSection *sec : outputSections)
    if (sec->size > 0 && (sec->flags & SHF_ALLOC) && !(sec->flags & SHF_TLS))
      vmas.push_back({sec, sec->addr});
  checkOverlap("virtual address", vmas, true);

  // Finally, check that the load addresses don't overlap. This will usually be
  // the same as the virtual addresses but can be different when using a linker
  // script with AT().
  std::vector<SectionOffset> lmas;
  for (OutputSection *sec : outputSections)
    if (sec->size > 0 && (sec->flags & SHF_ALLOC) && !(sec->flags & SHF_TLS))
      lmas.push_back({sec, sec->getLMA()});
  checkOverlap("load address", lmas, false);
}

// The entry point address is chosen in the following ways.
//
// 1. the '-e' entry command-line option;
// 2. the ENTRY(symbol) command in a linker control script;
// 3. the value of the symbol _start, if present;
// 4. the number represented by the entry symbol, if it is a number;
// 5. the address of the first byte of the .text section, if present;
// 6. the address 0.
static uint64_t getEntryAddr() {
  // Case 1, 2 or 3
  if (Symbol *b = symtab->find(config->entry))
    return b->getVA();

  // Case 4
  uint64_t addr;
  if (to_integer(config->entry, addr))
    return addr;

  // Case 5
  if (OutputSection *sec = findSection(".text")) {
    if (config->warnMissingEntry)
      warn("cannot find entry symbol " + config->entry + "; defaulting to 0x" +
           utohexstr(sec->addr));
    return sec->addr;
  }

  // Case 6
  if (config->warnMissingEntry)
    warn("cannot find entry symbol " + config->entry +
         "; not setting start address");
  return 0;
}

static uint16_t getELFType() {
  if (config->isPic)
    return ET_DYN;
  if (config->relocatable)
    return ET_REL;
  return ET_EXEC;
}

template <class ELFT> void Writer<ELFT>::writeHeader() {
  writeEhdr<ELFT>(Out::bufferStart, *mainPart);
  writePhdrs<ELFT>(Out::bufferStart + sizeof(Elf_Ehdr), *mainPart);

  auto *eHdr = reinterpret_cast<Elf_Ehdr *>(Out::bufferStart);
  eHdr->e_type = getELFType();
  eHdr->e_entry = getEntryAddr();
  eHdr->e_shoff = sectionHeaderOff;

  // Write the section header table.
  //
  // The ELF header can only store numbers up to SHN_LORESERVE in the e_shnum
  // and e_shstrndx fields. When the value of one of these fields exceeds
  // SHN_LORESERVE ELF requires us to put sentinel values in the ELF header and
  // use fields in the section header at index 0 to store
  // the value. The sentinel values and fields are:
  // e_shnum = 0, SHdrs[0].sh_size = number of sections.
  // e_shstrndx = SHN_XINDEX, SHdrs[0].sh_link = .shstrtab section index.
  auto *sHdrs = reinterpret_cast<Elf_Shdr *>(Out::bufferStart + eHdr->e_shoff);
  size_t num = outputSections.size() + 1;
  if (num >= SHN_LORESERVE)
    sHdrs->sh_size = num;
  else
    eHdr->e_shnum = num;

  uint32_t strTabIndex = in.shStrTab->getParent()->sectionIndex;
  if (strTabIndex >= SHN_LORESERVE) {
    sHdrs->sh_link = strTabIndex;
    eHdr->e_shstrndx = SHN_XINDEX;
  } else {
    eHdr->e_shstrndx = strTabIndex;
  }

  for (OutputSection *sec : outputSections)
    sec->writeHeaderTo<ELFT>(++sHdrs);
}

// Open a result file.
template <class ELFT> void Writer<ELFT>::openFile() {
  uint64_t maxSize = config->is64 ? INT64_MAX : UINT32_MAX;
  if (fileSize != size_t(fileSize) || maxSize < fileSize) {
    error("output file too large: " + Twine(fileSize) + " bytes");
    return;
  }

  unlinkAsync(config->outputFile);
  unsigned flags = 0;
  if (!config->relocatable)
    flags |= FileOutputBuffer::F_executable;
  if (!config->mmapOutputFile)
    flags |= FileOutputBuffer::F_no_mmap;
  Expected<std::unique_ptr<FileOutputBuffer>> bufferOrErr =
      FileOutputBuffer::create(config->outputFile, fileSize, flags);

  if (!bufferOrErr) {
    error("failed to open " + config->outputFile + ": " +
          llvm::toString(bufferOrErr.takeError()));
    return;
  }
  buffer = std::move(*bufferOrErr);
  Out::bufferStart = buffer->getBufferStart();
}

template <class ELFT> void Writer<ELFT>::writeSectionsBinary() {
  for (OutputSection *sec : outputSections)
    if (sec->flags & SHF_ALLOC)
      sec->writeTo<ELFT>(Out::bufferStart + sec->offset);
}

static void fillTrap(uint8_t *i, uint8_t *end) {
  for (; i + 4 <= end; i += 4)
    memcpy(i, &target->trapInstr, 4);
}

// Fill the last page of executable segments with trap instructions
// instead of leaving them as zero. Even though it is not required by any
// standard, it is in general a good thing to do for security reasons.
//
// We'll leave other pages in segments as-is because the rest will be
// overwritten by output sections.
template <class ELFT> void Writer<ELFT>::writeTrapInstr() {
  for (Partition &part : partitions) {
    // Fill the last page.
    for (PhdrEntry *p : part.phdrs)
      if (p->p_type == PT_LOAD && (p->p_flags & PF_X))
        fillTrap(Out::bufferStart + alignDown(p->firstSec->offset + p->p_filesz,
                                              config->commonPageSize),
                 Out::bufferStart + alignTo(p->firstSec->offset + p->p_filesz,
                                            config->commonPageSize));

    // Round up the file size of the last segment to the page boundary iff it is
    // an executable segment to ensure that other tools don't accidentally
    // trim the instruction padding (e.g. when stripping the file).
    PhdrEntry *last = nullptr;
    for (PhdrEntry *p : part.phdrs)
      if (p->p_type == PT_LOAD)
        last = p;

    if (last && (last->p_flags & PF_X))
      last->p_memsz = last->p_filesz =
          alignTo(last->p_filesz, config->commonPageSize);
  }
}

// Write section contents to a mmap'ed file.
template <class ELFT> void Writer<ELFT>::writeSections() {
  // In -r or -emit-relocs mode, write the relocation sections first as in
  // ELf_Rel targets we might find out that we need to modify the relocated
  // section while doing it.
  for (OutputSection *sec : outputSections)
    if (sec->type == SHT_REL || sec->type == SHT_RELA)
      sec->writeTo<ELFT>(Out::bufferStart + sec->offset);

  for (OutputSection *sec : outputSections)
    if (sec->type != SHT_REL && sec->type != SHT_RELA)
      sec->writeTo<ELFT>(Out::bufferStart + sec->offset);
}

// Split one uint8 array into small pieces of uint8 arrays.
static std::vector<ArrayRef<uint8_t>> split(ArrayRef<uint8_t> arr,
                                            size_t chunkSize) {
  std::vector<ArrayRef<uint8_t>> ret;
  while (arr.size() > chunkSize) {
    ret.push_back(arr.take_front(chunkSize));
    arr = arr.drop_front(chunkSize);
  }
  if (!arr.empty())
    ret.push_back(arr);
  return ret;
}

// Computes a hash value of Data using a given hash function.
// In order to utilize multiple cores, we first split data into 1MB
// chunks, compute a hash for each chunk, and then compute a hash value
// of the hash values.
static void
computeHash(llvm::MutableArrayRef<uint8_t> hashBuf,
            llvm::ArrayRef<uint8_t> data,
            std::function<void(uint8_t *dest, ArrayRef<uint8_t> arr)> hashFn) {
  std::vector<ArrayRef<uint8_t>> chunks = split(data, 1024 * 1024);
  std::vector<uint8_t> hashes(chunks.size() * hashBuf.size());

  // Compute hash values.
  parallelForEachN(0, chunks.size(), [&](size_t i) {
    hashFn(hashes.data() + i * hashBuf.size(), chunks[i]);
  });

  // Write to the final output buffer.
  hashFn(hashBuf.data(), hashes);
}

template <class ELFT> void Writer<ELFT>::writeBuildId() {
  if (!mainPart->buildId || !mainPart->buildId->getParent())
    return;

  if (config->buildId == BuildIdKind::Hexstring) {
    for (Partition &part : partitions)
      part.buildId->writeBuildId(config->buildIdVector);
    return;
  }

  // Compute a hash of all sections of the output file.
  size_t hashSize = mainPart->buildId->hashSize;
  std::vector<uint8_t> buildId(hashSize);
  llvm::ArrayRef<uint8_t> buf{Out::bufferStart, size_t(fileSize)};

  switch (config->buildId) {
  case BuildIdKind::Fast:
    computeHash(buildId, buf, [](uint8_t *dest, ArrayRef<uint8_t> arr) {
      write64le(dest, xxHash64(arr));
    });
    break;
  case BuildIdKind::Md5:
    computeHash(buildId, buf, [&](uint8_t *dest, ArrayRef<uint8_t> arr) {
      memcpy(dest, MD5::hash(arr).data(), hashSize);
    });
    break;
  case BuildIdKind::Sha1:
    computeHash(buildId, buf, [&](uint8_t *dest, ArrayRef<uint8_t> arr) {
      memcpy(dest, SHA1::hash(arr).data(), hashSize);
    });
    break;
  case BuildIdKind::Uuid:
    if (auto ec = llvm::getRandomBytes(buildId.data(), hashSize))
      error("entropy source failure: " + ec.message());
    break;
  default:
    llvm_unreachable("unknown BuildIdKind");
  }
  for (Partition &part : partitions)
    part.buildId->writeBuildId(buildId);
}

template void elf::createSyntheticSections<ELF32LE>();
template void elf::createSyntheticSections<ELF32BE>();
template void elf::createSyntheticSections<ELF64LE>();
template void elf::createSyntheticSections<ELF64BE>();

template void elf::writeResult<ELF32LE>();
template void elf::writeResult<ELF32BE>();
template void elf::writeResult<ELF64LE>();
template void elf::writeResult<ELF64BE>();

template void elf::combineCapRelocsSections<ELF32LE>();
template void elf::combineCapRelocsSections<ELF32BE>();
template void elf::combineCapRelocsSections<ELF64LE>();
template void elf::combineCapRelocsSections<ELF64BE>();
