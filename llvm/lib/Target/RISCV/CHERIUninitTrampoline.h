#include "RISCVISelLowering.h"
using namespace llvm;

static SmallVector<uint64_t, 3> ActrecCode = {
  0xfd82a08f020002db,
  0xff82a303fe82a10f,
  0x000080672261045b
};
