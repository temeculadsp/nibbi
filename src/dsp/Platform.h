#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include "primitives/dsp.h"
#include "primitives/delayline.h"
#include "primitives/dcblock.h"
#include "primitives/adsr.h"
#include "primitives/FIFO.h"
namespace daisy {
// libDaisy's 16-bit conversion, without its ARM hardware headers.
inline float s162f(int32_t x) { return float(x) * 3.051850947599719e-5f; }
inline int32_t f2s16(float x) { return int32_t(std::clamp(x, -.999985f, .999985f) * 32767.f); }
}
