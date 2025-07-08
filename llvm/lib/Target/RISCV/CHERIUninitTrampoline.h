#include "RISCVISelLowering.h"
using namespace llvm;

static SmallVector<uint64_t, 3> ActrecCodeFp = {
  0xfd02a40f020002db,
  0xfe02a08fff02a10f,
  0x0000000000008067
};

static SmallVector<uint64_t, 3> ActrecCodeNoFp = {
  0xff02a10f020002db,
  0x00008067fe02a08f
};
