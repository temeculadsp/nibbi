#pragma once
#include <algorithm>
#include <array>
#include <cmath>

namespace nibbi {
// Desktop listening level only: outside the firmware's tape, FX and resampling.
// The original volume control supplies attenuation before this fixed gain.
class OutputStage {
public:
 static constexpr float gain=15.8489319246f; // +24 dB at full volume.
 static constexpr float ceiling=.8912509381f; // -1 dBFS sample peak.
 void prepare(double sampleRate) {
  release_=float(std::exp(-1.0/(.05*sampleRate)));
  reduction_.fill(1.f);
 }
 void processStereo(float& left,float& right,size_t bus) {
  left=std::isfinite(left)?left*gain:0.f;
  right=std::isfinite(right)?right*gain:0.f;
  const float peak=std::max(std::abs(left),std::abs(right));
  const float needed=peak>ceiling?ceiling/peak:1.f;
  // Immediate attack catches transients; a 50 ms release avoids sample-by-sample
  // clipping. One gain per stereo bus preserves pan and channel balance.
  auto& reduction=reduction_[bus];
  reduction=std::min(needed,1.f-(1.f-reduction)*release_);
  left*=reduction;
  right*=reduction;
 }
private:
 std::array<float,2> reduction_ {1.f,1.f};
 float release_=0.f;
};
}
