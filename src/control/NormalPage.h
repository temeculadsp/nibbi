#pragma once
// Native port of CHOMPI TAPE a73d732: original event handlers and state transitions.
// MIT: see licenses/LICENSE. GPIO and boot debounce are omitted; LED calculations output native RGB.
#include "ControlServices.h"
#include "LedFrame.h"
namespace nibbi::control {
class NormalPage : public LedOutput {
public:
        void DrawLights()
        {
            uint32_t now = hw_->Now();

            for(size_t i = 7; i < (25 + 7); i++)
            {
                size_t slot = KeyToSlot(i);
                const float* color = &pink[0];
                if(fx_->GetVoiceSlot() != 15 || fx_->GetVoiceMode() == VoiceMode::CUBBI)
                {
                    if(fx_->GetVoiceBank() == 0) // this shouldn't change if we're not actually on that bank
                        color = &purple[0];
                    else if(fx_->GetVoiceBank() == 1)
                        color = &orange[0];
                    else if(fx_->GetVoiceBank() == 2)
                        color = &teal[0];
                    else if(fx_->GetVoiceBank() == 3)
                        color = &dark_orange[0];
                    else if(fx_->GetVoiceBank() == 4)
                        color = &yellow_green[0];
                }

                if (fx_->IsKeyPlaying(i))
                    SetSmtLedFloat(led_map[i], 1.f, 1.f, 1.f);
                else if(fx_->GetVoiceMode() == VoiceMode::CUBBI 
                        && (switch_state || fx_->GetInputSource() != InputSource::MIC)) // no perm KB leds if we're monitoring the mic
                {
                    if (fx_->GetFileExists(slot - 1) && i == 28)
                        SetSmtLedFloat(led_map[i], pink[0] * .25f, pink[1] * .25f, pink[2] * .25f);
                    else if(fx_->GetFileExists(slot - 1) && slot != kSlotNone)
                        SetSmtLedFloat(led_map[i], color[0] * .25f, color[1] * .25f, color[2] * .25f);
                }
                else if(fx_->GetVoiceMode() == VoiceMode::JAMMI && (i == 15 || i == 18 || i == 28)
                        && (switch_state || fx_->GetInputSource() != InputSource::MIC)) // no perm KB leds if we're monitoring the mic
                    SetSmtLedFloat(led_map[i], color[0] * .25f, color[1] * .25f, color[2] * .25f);
                else
                    SetSmtLed(led_map[i], 0, 0, 0);
            }

            // =========   encoders   =========
            for (int i = 0; i < 6; i++)
            {
                uint8_t page = knob_page[i];
                float value = enc_values[page][i];

                float r = 0.f; 
                float g = 0.f;
                float b = 0.f;
                switch (i)
                {
                case 0: // speed, gain, pan
                {
                    if(!switch_state)
                    {
                        r = g = b = 0.f;
                    }
                    else if(page == 0) // speed
                    {
                        float idx = enc_values[0][0] < .5f ? enc_values[0][0] * 2.f : (1.f - enc_values[0][0]) * 2.f; // 0 - 1 - 0
                        r = color_quad_xfade(med_blue[0], green[0], yellow[0], red[0], idx);
                        g = color_quad_xfade(med_blue[1], green[1], yellow[1], red[1], idx);
                        b = color_quad_xfade(med_blue[2], green[2], yellow[2], red[2], idx);
                    }
                    else if(page == 1) // gain
                    {
                        r = color_triple_xfade(blue[0], pink[0], red[0], value);
                        g = color_triple_xfade(blue[1], pink[1], red[1], value);
                        b = color_triple_xfade(blue[2], pink[2], red[2], value);

                    }

                    SetPthLedFloat(1, r, g, b);
                }
                break;
                case 1: // start point
                {
                    if (page == 0) // start point
                    {
                        r = color_xfade(yellow[0], orange[0], value);
                        g = color_xfade(yellow[1], orange[1], value);
                        b = color_xfade(yellow[2], orange[2], value);

                    }
                    else // env. attack
                    {
                        r = color_xfade(purple[0] * .2f, purple[0], value);
                        g = color_xfade(purple[1] * .2f, purple[1], value);
                        b = color_xfade(purple[2] * .2f, purple[2], value);

                    }

                    if(!switch_state)
                    {
                        r = g = b = 0.f;
                    }   

                    SetPthLedFloat(2, r, g, b);
                    break;
                }
                case 2: // end point
                {
                    if (page == 0) // end point
                    {
                        r = color_xfade(orange[0], red[0], value);
                        g = color_xfade(orange[1], red[1], value);
                        b = color_xfade(orange[2], red[2], value);

                    }
                    else // env. decay
                    {
                        r = color_xfade(purple[0] * .2f, purple[0], value);
                        g = color_xfade(purple[1] * .2f, purple[1], value);
                        b = color_xfade(purple[2] * .2f, purple[2], value);

                    }

                    if(!switch_state)
                    {
                        r = g = b = 0.f;
                    }

                    SetPthLedFloat(3, r, g, b);
                    break;
                }
                case 3: // magic
                {
                    if (page == 0) // reverb / delay
                    {   
                        if (split_delay_) {
                            r = color_triple_xfade(green[0], (green[0] + blue[0]) * .5f, blue[0], value);
                            g = color_triple_xfade(green[1], (green[1] + blue[1]) * .5f, blue[1], value);
                            b = color_triple_xfade(green[2], (green[2] + blue[2]) * .5f, blue[2], value);
                        }
                        else {



                            r = color_triple_xfade(teal[0], med_blue[0], blue[0], value);
                            g = color_triple_xfade(teal[1], med_blue[1], blue[1], value);
                            b = color_triple_xfade(teal[2], med_blue[2], blue[2], value);
                        }
                    }
                    else if (page == 1) // lofi
                    {

                        r = color_triple_xfade(yellow[0], orange[0], red[0], value);
                        g = color_triple_xfade(yellow[1], orange[1], red[1], value);
                        b = color_triple_xfade(yellow[2], orange[2], red[2], value);
                    }
                    else // filter
                    {

                        r = color_triple_xfade(purple[0], pink[0], 1.f, value);
                        g = color_triple_xfade(purple[1], pink[1], 1.f, value);
                        b = color_triple_xfade(purple[2], pink[2], 1.f, value);
                    }

                    if(!switch_state)
                    {
                        r = g = b = 0.f;
                    }

                    SetPthLedFloat(4, r, g, b);

                    break;
                }
                case 4: // transport
                {                    
                    if(fx_->GetLooperIsEmpty())
                    {
                        SetPthLedFloat(5, 0.f, 0.f, 0.f);
                        SetPthLedFloat(6, 0.f, 0.f, 0.f);
                    }
                    else if(fx_->IsLooperPlaying())
                    {
                        float idx = value < .5f ? value * 2.f : (1.f - value) * 2.f; // 0 - 1 - 0
                        int led_on = value > .5f ? 6 : 5;
                        int led_off = value > .5f ? 5 : 6;

                        r = color_quad_xfade(med_blue[0], green[0], yellow[0], red[0], idx);
                        g = color_quad_xfade(med_blue[1], green[1], yellow[1], red[1], idx);
                        b = color_quad_xfade(med_blue[2], green[2], yellow[2], red[2], idx);

                        if(!switch_state)
                        {
                            r *= kRecDim;
                            g *= kRecDim;
                            b *= kRecDim;
                        }

                        SetPthLedFloat(led_on, r, g, b);

                        if (idx > .8f)
                        {
                            float dim = (idx - .8f) * 5.f;

                            r = color_xfade(0.f, red[0], dim);
                            g = color_xfade(0.f, red[1], dim);
                            b = color_xfade(0.f, red[2], dim);

                            if(!switch_state)
                            {
                                r *= kRecDim;
                                g *= kRecDim;
                                b *= kRecDim;
                            }

                            SetPthLedFloat(led_off, r, g, b);
                        }
                        else
                        {
                            SetPthLedFloat(led_off, 0.f, 0.f, 0.f);
                        }

                        value = value * 4.f - 2.f; // -2 - 2
                    }
                    else
                    {
                        float scrub = fx_->GetLooperScrub() * .5f;
                        int led = 5 + (scrub > 0.f);

                        scrub = fabsf(scrub);
                        SetPthLedFloat(led, scrub, scrub, scrub);
                    }

                    break;
                }
                case 5: // gain
                {
                    if (page == 0)
                    {
                        float vu_sample = fx_->GetVUSample(VUTarget::VU_OUTPUT);

                        r = value * color_quad_xfade(.1f, green[0], yellow[0], pink[0], vu_sample);
                        g = value * color_quad_xfade(.1f, green[1], yellow[1], pink[1], vu_sample);
                        b = value * color_quad_xfade(.1f, green[2], yellow[2], pink[2], vu_sample);

                    }
                    else
                    {
                        r = color_xfade(blue[0], red[0], value);
                        g = color_xfade(blue[1], red[1], value);
                        b = color_xfade(blue[2], red[2], value);

                    }
                    SetPthLedFloat(9, r, g, b);
                    break;
                }
                default:
                    break;
                }
            }

            DrawTransportLights();

            float r,g,b;
            // nibbi key

            if (!switch_state)
            {
                if(copier_->IsCopying())
                {
                    if(now - last_record_blink > 300)
                    {
                        record_blink = !record_blink;
                        last_record_blink = now;
                    }

                    if(record_blink)
                    {
                        r = pink[0];
                        g = pink[1];
                        b = pink[2];
                    }
                    else
                    {
                        r = g = b = 0.f;
                    }
                }
                else if (fx_->Recording())
                {
                    r = red[0];
                    g = red[1];
                    b = red[2];
                }
                else
                {
                    float vu_sample = fx_->GetVUSample(VUTarget::VU_INPUT);
                
                    r = color_quad_xfade(.1f, green[0], yellow[0], pink[0], vu_sample);
                    g = color_quad_xfade(.1f, green[1], yellow[1], pink[1], vu_sample);
                    b = color_quad_xfade(.1f, green[2], yellow[2], pink[2], vu_sample);
                }
            }
            else
            {
                if (nibbi_key_pressed)
                {
                    r = .67f;
                    g = 0.f;
                    b = 1.f;
                }
                else
                {
                    r = g = b = 0.f;
                }
            } 

            SetPthLedFloat(led_map[5], r, g, b);

            // ========   send the data   =========

        }
        void DrawTransportLights() {
            const auto now=hw_->Now();
            /** PTH leds */
            float r, g, b;
            // play key
            if(fx_->GetLooperIsEmpty() && !fx_->IsLooperRecordArmed())
            {
                r = g = b = 0.f;
            }
            else if(fx_->IsLooperRecordArmed())
            {
                r = g = b = 1.f;
            }
            else if(fx_->IsLooperFirstRecording() && fx_->IsLooperRecording())
            {
                r = teal[0];
                g = teal[1];
                b = teal[2];
            }
            else if(fx_->IsLooperPlaying())
            {
                float position = 1.f - fx_->GetLooperPosition();
                r = teal[0] * position;
                g = teal[1] * position;
                b = teal[2] * position;
            }
            else // we're paused
            {
                float position = 1.f - fx_->GetLooperPosition();
                r = position;
                g = position;
                b = position;
            }

            if(!switch_state)
            {
                r *= kRecDim;
                g *= kRecDim;
                b *= kRecDim;
            }
            SetPthLedFloat(led_map[33], r, g, b);

            // loop key
            if(fx_->GetLooperIsEmpty() && !fx_->IsLooperRecordArmed())
            {
                r = g = b = 0.f;
            }
            else if(fx_->IsLooperRecordArmed())
            {
                if(now - last_arm_blink > 300)
                {
                    arm_blink = !arm_blink;
                    last_arm_blink = now;
                }

                r = arm_blink ? 1.f : 0.f;
                g = 0.f;
                b = 0.f;
            }
            else if(fx_->IsLooperFirstRecording() && fx_->IsLooperRecording())
            {
                r = red[0];
                g = red[1];
                b = red[2];
            }
            else if(fx_->IsLooperRecording()) // overdub
            {
                float position = fx_->GetLooperPosition();
                r = yellow[0] * position;
                g = yellow[1] * position;
                b = yellow[2] * position;
            }
            else
            {
                float position = fx_->GetLooperPosition();
                r = position;
                g = position;
                b = position;
            }

            if(!switch_state)
            {
                r *= kRecDim;
                g *= kRecDim;
                b *= kRecDim;
            }
            SetPthLedFloat(led_map[34], r, g, b);

        }

 void Init(ControlSurface* hw,ControlEngine* fx,CopyService* copier,float** values,const float** defaults,uint8_t* pages,PresetStore* presets) {
  hw_=hw;fx_=fx;copier_=copier;enc_values=values;enc_defaults=defaults;knob_page=pages;presets_=presets;
 }
 void ApplyParameters() {
  if(fx_->CheckReset()) { enc_values[0][4]=enc_defaults[0][4];fx_->ResetLooperPitchQuant(); }
  if(knob_page[0]==1 && switch_state) fx_->SetGain(enc_values[1][0]);
  if(knob_page[1]==1) fx_->SetAttack(enc_values[1][1]);
  if(knob_page[2]==1) fx_->SetDecay(enc_values[1][2]);
  if(knob_page[3]==0) {
   const float value=enc_values[0][3];
   if(split_delay_) { fx_->SetReverb(value<.5f?0.f:(value-.5f)*2.f);fx_->SetDelayFeedback(value<.5f?(.5f-value)*2.f:0.f); }
   else { fx_->SetReverb(value);fx_->SetDelayFeedback(value); }
  } else if(knob_page[3]==1) fx_->SetSaturate(enc_values[1][3]);
  else fx_->SetFilter(enc_values[2][3]);
  if(knob_page[5]==0) fx_->SetMainGain(enc_values[0][5]); else fx_->SetInputGain(enc_values[1][5]);
  fx_->SetInputMonitor(!switch_state);
 }
 void Configure(bool quantized,bool split,int channel) { quantized_pitch_=quantized;split_delay_=split;midi_channel=uint8_t(channel); }
        bool OnButton(uint16_t buttonID,
                      uint8_t numberOfPresses,
                      bool isRetriggering)
        {
            if (init_ignore || copier_->IsCopying())
                return false;
            bool rising = numberOfPresses == 1;
            switch (buttonID)
            {
            case static_cast<uint16_t>(ControlSurface::Button::NC_1):
            case static_cast<uint16_t>(ControlSurface::Button::NC_2):
            case static_cast<uint16_t>(ControlSurface::Button::NC_3):
            case static_cast<uint16_t>(ControlSurface::Button::NC_4):
            case static_cast<uint16_t>(ControlSurface::Button::NC_5):
                break;
            case static_cast<uint16_t>(ControlSurface::Button::ENC_1_SW):
            case static_cast<uint16_t>(ControlSurface::Button::ENC_2_SW):
            case static_cast<uint16_t>(ControlSurface::Button::ENC_3_SW):
            case static_cast<uint16_t>(ControlSurface::Button::ENC_4_SW):
            {
                if(!rising)
                {
                    uint8_t knob = key_map[buttonID];
                    knob_page[knob]++;
                    knob_page[knob] %= knob_num_pages[knob];
                }
                break;
            }
            case static_cast<uint16_t>(ControlSurface::Button::ENC_6_SW):
            {
                if(!rising && hw_->Now() - batt_hold < 2000)
                {
                    uint8_t knob = key_map[buttonID];
                    knob_page[knob]++;
                    knob_page[knob] %= knob_num_pages[knob];
                }
                batt_hold = hw_->Now();
                batt_display = rising;
                break;
            }
            case ENC_5_SW:
            {
                if (!rising)
                {
                    enc_values[0][4] = enc_defaults[0][4];
                    hw_->SendCC(midi_channel, cc_map[0][4], enc_values[0][4] * 127.f);
                }
                fx_->SetLooperPitch(1.f);
                fx_->ResetLooperPitchQuant();
                break;
            }
            case static_cast<uint16_t>(ControlSurface::Button::SW_TOG):
                break;
            case static_cast<uint16_t>(ControlSurface::Button::KEY_27):
            {
                last_arm_blink = hw_->Now();
                fx_->LooperPlayButton(rising);
                hw_->SendCC(midi_channel, 26, rising ? 127 : 0);
                break;
            }
            case static_cast<uint16_t>(ControlSurface::Button::KEY_28):
            {
                last_arm_blink = hw_->Now();
                if(!fx_->Recording())
                    fx_->LooperRecordButton(rising);
                hw_->SendCC(midi_channel, 27, rising ? 127 : 0);
                break;
            }
            case static_cast<uint16_t>(ControlSurface::Button::KEY_26):
            {
                nibbi_key_pressed = rising;
                if (!switch_state)
                {
                    hw_->SendCC(midi_channel, key_map[buttonID], rising ? 127 : 0);
                }
                else
                {
                    midi_channel = rising;
                }
                if(!switch_state)
                {
                    bool latch = fx_->GetRecordLatch();
                    bool rec = fx_->Recording();
                    if (rising && !rec)
                    {
                        fx_->StartNewRecording(0);
                        last_record_blink = hw_->Now();
                        record_blink = false;
                    }
                    else if(!rising && !latch && rec)
                        StopVoiceRecording();
                    else if(rising && rec && latch)
                        StopVoiceRecording();
                }
                break;
            }
            default:
                if (rising)
                {
                    if(!isRetriggering)
                    {
                        if(fx_->GetVoiceMode() == VoiceMode::CUBBI)
                        {
                            size_t slot = KeyToSlot(buttonID);
                            if(slot == kSlotNone)
                                return true;
                            OpenCubbiSlot(slot);
                        }
                        fx_->request_fifo.PushBack(KeyRequest(KeyRequest::Type::START,
                            key_map[buttonID] - 60, buttonID, 127.f));
                    }
                    if(fx_->GetLooperRecordArm())
                        fx_->ToggleLooperRecord();
                    if(!isRetriggering)
                        hw_->SendNoteOn(midi_channel, key_map[buttonID], 127);
                }
                else
                {
                    if(!isRetriggering)
                    {
                        fx_->request_fifo.PushBack(KeyRequest(KeyRequest::Type::STOP,
                            0, buttonID, 127.f));
                        hw_->SendNoteOff(midi_channel, key_map[buttonID], 127);
                    }
                }
                break;
            }
            return true;
        }
        void OpenCubbiSlot(size_t slot)
        {
            if(!fx_->GetFileExists(slot - 1))
                return;
            size_t mode = static_cast<size_t>(fx_->GetVoiceMode());
            size_t bank = fx_->GetBank();
            bool loop = true;
            bool sustain = true;
            float pan = .5f;
            if(!presets_->IsValid(mode, bank, slot))
            {
                enc_values[0][0] = enc_defaults[0][0];
                enc_values[0][1] = enc_defaults[0][1];
                enc_values[0][2] = enc_defaults[0][2];
                enc_values[1][0] = enc_defaults[1][0];
                enc_values[1][1] = enc_defaults[1][1];
                enc_values[1][2] = enc_defaults[1][2];
            }
            else
            {
                enc_values[0][0] = presets_->GetValue(mode, bank, slot, 0);
                enc_values[0][1] = presets_->GetValue(mode, bank, slot, 1);
                enc_values[0][2] = presets_->GetValue(mode, bank, slot, 2);
                enc_values[1][1] = presets_->GetValue(mode, bank, slot, 3);
                enc_values[1][2] = presets_->GetValue(mode, bank, slot, 4);
                loop = presets_->GetValue(mode, bank, slot, 5);
                sustain = presets_->GetValue(mode, bank, slot, 6);
                enc_values[1][0] = presets_->GetValue(mode, bank, slot, 7);
                pan = presets_->GetValue(mode, bank, slot, 8);
            }
            float val = enc_values[0][0];
            val = val < .5f ? (.5f - val) * -2.f : (val - .5f) * 2.f;
            float inv = val < 0.f ? -1.f : 1.f;
            float pitch;
            if(fabsf(val) < .33f)
                pitch = val * 1.484848f + .01f * inv;
            else if (fabsf(val) < .66f )
                pitch = (val - .33f * inv) * 1.515151 + .5f * inv;
            else
                pitch = (val - .66 * inv) * 2.941176 + 1.f * inv;
            fx_->OpenCubbiSlot(pitch, enc_values[0][1], enc_values[0][2], enc_values[1][1],
            enc_values[1][2], loop, sustain, enc_values[1][0],
            pan);
        }
        bool OnEncoderTurned(uint16_t encoderID,
                             int16_t turns,
                             uint16_t stepsPerRevolution)
        {
            if (init_ignore || copier_->IsCopying())
                return false;
            uint8_t page = knob_page[encoderID];
            float old_val = enc_values[page][encoderID];
            bool update_presets = true;
            if(stepsPerRevolution > 0)
            {
                enc_values[page][encoderID] = turns / 127.f;
            }
            else{
                float inc = turns * kEncoderCoarseStep;
                if((encoderID == 0 && page == 0 && quantized_pitch_)
                    || (encoderID == 4 && quantized_pitch_))
                {
                    inc = 0.f;
                }
                else if ((encoderID == 0 && page == 0 && !quantized_pitch_)
                    || (encoderID == 1 && page == 0)
                    || (encoderID == 2 && page == 0)
                    || (encoderID == 4 && !quantized_pitch_))
                {
                    inc = turns * kEncoderFineStep;
                }
                enc_values[page][encoderID] += inc;
            }
            enc_values[page][encoderID] = fclamp(enc_values[page][encoderID], 0.f, 1.f);
            if (encoderID == 0 && page == 0)
            {
                if(quantized_pitch_)
                    enc_values[0][0] = fx_->SetGlobalPitchQuantized(turns, enc_values[0][0]);
                else
                    fx_->SetGlobalPitchFree(enc_values[0][0]);
            }
            else if (encoderID == 4)
            {
                if (fx_->IsLooperPlaying())
                {
                    if(quantized_pitch_)
                        enc_values[0][4] = fx_->SetLooperPitchQuantized(turns, enc_values[0][4]);
                    else
                        fx_->SetLooperPitchFree(enc_values[0][4]);
                }
                else
                {
                    enc_values[0][4] = old_val;
                    fx_->SetLooperScrub(turns);
                }
            }
            if(page == 0 && (encoderID == 1 || encoderID == 2))
            {
                if ((enc_values[0][1] + .01f) >= enc_values[0][2])
                {
                    enc_values[page][encoderID] = old_val;
                }
                else if(encoderID == 1)
                {
                    if(!fx_->SetStartPoint(enc_values[0][1]) && turns > 0)
                    {
                        update_presets = false;
                        enc_values[page][encoderID] = old_val;
                    }
                }
                else if(encoderID == 2)
                {
                    if(!fx_->SetEndPoint(enc_values[0][2]) && turns < 0)
                    {
                        update_presets = false;
                        enc_values[page][encoderID] = old_val;
                    }
                }
            }
            if (stepsPerRevolution == 0) {
                hw_->SendCC(midi_channel, cc_map[page][encoderID], enc_values[page][encoderID] * 127);
            }
            if(encoderID < 3 && update_presets)
            {
                DumpValuePresets();
            }
            return true;
        }
        void DumpValuePresets()
        {
            size_t mode = static_cast<size_t>(fx_->GetVoiceMode());
            size_t bank = fx_->GetBank();
            size_t slot = fx_->GetVoiceSlot();
            presets_->SetValue(enc_values[0][0], mode, bank, slot, 0);
            presets_->SetValue(enc_values[0][1], mode, bank, slot, 1);
            presets_->SetValue(enc_values[0][2], mode, bank, slot, 2);
            presets_->SetValue(enc_values[1][1], mode, bank, slot, 3);
            presets_->SetValue(enc_values[1][2], mode, bank, slot, 4);
            presets_->SetValue(fx_->GetAutoLoop(), mode, bank, slot, 5);
            presets_->SetValue(fx_->GetSustainActive(), mode, bank, slot, 6);
            presets_->SetValue(enc_values[1][0], mode, bank, slot, 7);
            presets_->SetValue(fx_->GetPan(), mode, bank, slot, 8);
        }
        void SetSwitchState(bool state)
        {
            if(state && !switch_state && fx_->Recording() && !copier_->IsCopying())
            {
                StopVoiceRecording();
            }
            switch_state = state;
        }
        void StopVoiceRecording()
        {
            enc_values[0][0] = enc_defaults[0][0];
            enc_values[0][1] = enc_defaults[0][1];
            enc_values[0][2] = enc_defaults[0][2];
            enc_values[1][0] = enc_defaults[1][0];
            enc_values[1][1] = enc_defaults[1][1];
            enc_values[1][2] = enc_defaults[1][2];
            fx_->StopRecording();
        }
private:
 ControlSurface* hw_=nullptr;ControlEngine* fx_=nullptr;CopyService* copier_=nullptr;PresetStore* presets_=nullptr;
 float** enc_values=nullptr;const float** enc_defaults=nullptr;uint8_t* knob_page=nullptr;
 uint8_t midi_channel=0;
 bool switch_state=false,nibbi_key_pressed=false,quantized_pitch_=false,split_delay_=false;
 bool init_ignore=false,batt_display=false,arm_blink=true,record_blink=false;
 uint32_t batt_hold=0,last_arm_blink=0,last_record_blink=0;
};
}
