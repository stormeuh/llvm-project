// The other libunwind tests don't test internal interfaces, so the include path
// is a little wonky.
#include "../src/config.h"

// Only run this test under supported configurations.

#if defined(_LIBUNWIND_USE_DL_ITERATE_PHDR) &&                                 \
    defined(_LIBUNWIND_SUPPORT_DWARF_INDEX) &&                                 \
    defined(_LIBUNWIND_USE_FRAME_HEADER_CACHE)

#include <link.h>
#include <stdio.h>

// This file defines several of the data structures needed here,
// and includes FrameHeaderCache.hpp as well.
#include "../src/AddressSpace.hpp"

#define kBaseAddr 0xFFF000
#define kDwarfSectionLength 0xFF

using namespace libunwind;

using pc_t = LocalAddressSpace::LocalProgramCounter;

int main() {
  FrameHeaderCache FHC;
  struct dl_phdr_info PInfo;
  memset(&PInfo, 0, sizeof(PInfo));
  // The cache itself should only care about these two fields--they
  // tell the cache to invalidate or not; everything else is handled
  // by AddressSpace.hpp.
  PInfo.dlpi_adds = 6;
  PInfo.dlpi_subs = 7;

  UnwindInfoSections UIS;
  UIS.dso_base = kBaseAddr;
  UIS.dwarf_section_length = kDwarfSectionLength;
  dl_iterate_cb_data CBData;
  // Unused by the cache.
  CBData.addressSpace = nullptr;
  CBData.sects = &UIS;
  CBData.targetAddr = pc_t((uintptr_t)kBaseAddr + 1);

  // Nothing present, shouldn't find.
  if (FHC.find(&PInfo, 0, &CBData))
    abort();
  FHC.add(&UIS);
  // Just added. Should find.
  if (!FHC.find(&PInfo, 0, &CBData))
    abort();
  // Cache is invalid. Shouldn't find.
  PInfo.dlpi_adds++;
  if (FHC.find(&PInfo, 0, &CBData))
    abort();

  FHC.add(&UIS);
  CBData.targetAddr = pc_t(kBaseAddr - 1);
  // Shouldn't find something outside of the addresses.
  if (FHC.find(&PInfo, 0, &CBData))
    abort();
  // Add enough things to the cache that the entry is evicted.
  for (int i = 0; i < 9; i++) {
    UIS.dso_base = kBaseAddr + (kDwarfSectionLength * i);
    FHC.add(&UIS);
  }
  CBData.targetAddr = pc_t(kBaseAddr);
  // Should have been evicted.
  if (FHC.find(&PInfo, 0, &CBData))
    abort();
  return 0;
}

#else
int main() { return 0;}
#endif
