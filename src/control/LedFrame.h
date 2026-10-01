#pragma once
// CHOMPI TAPE a73d732 LED colours/interpolation, MIT: licenses/LICENSE.
// Native RGB output only; no board driver, GPIO, PWM or LED-chain emulation.
#include <array>
#include <cstdint>
#include <algorithm>
namespace nibbi::control {
struct LedFrame {
 using RGB=std::array<float,3>;
 std::array<RGB,10> lamps{};
 std::array<RGB,25> keys{};
};
inline uint32_t packLed(const LedFrame::RGB& c) {
 auto byte=[](float x){return uint32_t(std::clamp(x,0.f,1.f)*255.f);};
 return 0xff000000u | (byte(c[0])<<16) | (byte(c[1])<<8) | byte(c[2]);
}
inline constexpr uint8_t led_map[40]={2,3,4,1,0,0,0,0,23,22,21,20,1,2,3,24,19,18,17,16,15,4,5,6,14,13,12,11,10,7,8,9,9,7,8,0,0,0,0,0};
inline constexpr float kRecDim=.7f;
    static const float white[3] = {1.f, 1.f, 1.f};
    static const float red[3] = {1.f, 0.f, 0.f};
    static const float orange[3] = {1.f, .6f, .24f};
    static const float yellow[3] = {1.f, .95f, 0.05f};
    static const float green[3] = {0.f, 1.f, 0.f};
    static const float teal[3] = {.14f, 1.f, .92f};
    static const float med_blue[3] = {0.f, .84f, 1.f};
    static const float blue[3] = {0.f, 0.f, 1.f};
    static const float purple[3] = {.58f, .05f, 1.f};
    static const float pink[3] = {1.f, .36f, .62f};
    static const float dark_orange[3] = {.77f, .38f, .06f};
    static const float yellow_green[3] = {.706f, 1.f, 0.f};

inline float color_xfade(float start, float end, float idx)
    {
        return (1.f - idx) * start + idx * end;
    }
inline float color_triple_xfade(float start, float mid, float end, float idx)
    {
        if(idx < .5f)
        {
            idx *= 2.f;
            return color_xfade(start, mid, idx);
        }
        else
        {
            idx = (idx - .5f) * 2.f;
            return color_xfade(mid, end, idx);
        }
    }
inline float color_quad_xfade(float start, float mid1, float mid2, float end, float idx)
    {
        if(idx < .33f)
        {
            idx *= 3.f;
            return color_xfade(start, mid1, idx);
        }
        else if(idx < .66f)
        {
            idx = (idx - .33f) * 3.f;
            return color_xfade(mid1, mid2, idx);
        }
        else
        {
            idx = (idx - .66f) * 3.f;
            return color_xfade(mid2, end, idx);
        }
    }
class LedOutput {
public:
 void attachLights(LedFrame& frame) {lights_=&frame;}
protected:
 void SetPthLedFloat(size_t i,float r,float g,float b) {lights_->lamps[i]={r,g,b};}
 void SetSmtLedFloat(size_t i,float r,float g,float b) {lights_->keys[i]={r,g,b};}
 void SetSmtLed(size_t i,float r,float g,float b) {SetSmtLedFloat(i,r,g,b);}
private:
 LedFrame* lights_=nullptr;
};
}
