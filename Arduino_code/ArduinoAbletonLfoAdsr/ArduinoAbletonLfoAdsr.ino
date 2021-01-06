// --------------------------------------------------
//
// Arduino LFO + ADSR, controlled from Ableton. 
// Code optimized for Arduino Due as it has a built-in DAC
// For use on other boards just reduce the DACSIZE, 
// remove the analogWriteResolution(12) statement and write
// to a different port for all analogWrite statements. 
// Further notes:
//     -> Max for Live plug-in required in Ableton
//     -> Make sure that lfo and adsr Arduino libraries are installed
//     -> see readme on https://github.com/mo-thunderz/ArduinoAbletonLfoAdsr for installation instructions
//
// Four bytes interface is used between Ableton and Arduino:
// Byte 1: 255 (sync byte)
// Byte 2: <control> - [0:255]
// Byte 3: <value MSB> - [0:255]
// Byte 4: <value LSB> - [0:255]
//
// Written by mo thunderz (last update: 2.1.2021)
//
// --------------------------------------------------

#include <lfo.h>          // required for function generation
#include <adsr.h>         // required for function generation

#define DACSIZE 4096             // vertical resolution of the DACs
#define CONNECTED_TIMEOUT 500000 // LED_BUILTIN goes on for CONNECTED_TIMEOUT micro seconds whenever a serial command is received

//
// NOTE: all ID_xxxx variables below are CONTROL numbers for interfacing with Ableton
//       for instance: if Ableton sends over 255 - 210 - 0 - 100 that will mean that the attack of the ADSR is going to be set to 100µs
//       (Because of this definition -> #define ID_ADSR_ATTACK 210)
// NOTE2: you can change these definitions as you like, but dont forget to change the according numbers in Max for Live as well.
//
#define ID_SONG_MEASURE 251   // defines the start of a measure
#define ID_SONG_BPM 250       // defines the BPM

// for ADSR
#define ID_ADSR_ATTACK 210
#define ID_ADSR_DECAY 211
#define ID_ADSR_SUSTAIN 212
#define ID_ADSR_RELEASE 213
#define ID_ADSR_TRIGGER 214      // 0 trigger off, 1 trigger on

// for sync output
#define ID_SYNC_MODE 220
#define ID_SYNC_MODE0_FREQ 221
#define ID_SYNC_MODE1_RATE 222
#define ID_SYNC_PHASE 226

// for DACs
#define ID_DAC0_MODE 230
#define ID_DAC0_MODE0_FREQ 231
#define ID_DAC0_MODE1_RATE 232
#define ID_DAC0_WAVEFORM 233
#define ID_DAC0_AMPL 234
#define ID_DAC0_AMPL_OFFSET 235
#define ID_DAC0_PHASE 236
#define ID_DAC0_ADSR_ENABLE 237

#define ID_DAC1_MODE 240
#define ID_DAC1_MODE0_FREQ 241
#define ID_DAC1_MODE1_RATE 242
#define ID_DAC1_WAVEFORM 243
#define ID_DAC1_AMPL 244
#define ID_DAC1_AMPL_OFFSET 245
#define ID_DAC1_PHASE 246
#define ID_DAC1_ADSR_ENABLE 247

// pin assignments
#define SYNC_OUTPUT 43      // output port for sync signal (used in the LFO+ADSR Youtube demo to sync the Schippmann filter
#define LED_DAC0 28         // LED that blinks in the rate of the DAC0 speed
#define LED_DAC1 32         // LED that blinks in the rate of the DAC1 speed
#define LED_SYNC 36         // LED that blinks in the rate of the SYNC speed
#define LED_MEASURE 40      // LED that with every measure changes state (when Ableton is playing a track)

// variables 
float           bpm = 120;
int             dac0_ampl = 4095;
bool            dac0_adsr_enable = 0;
int             dac1_ampl = 4095;
bool            dac1_adsr_enable = 0;

// variables for the sync port
float           sync_mode0_freq = 10;                 // Hz
int             sync_mode1_rate = 15;
int             sync_mode = 0;                        // 0: Manual sync, 1: Ableton synced
float           sync_phase = 0;

// internal variables
int rx_state = 0;
byte cc_sync;
byte cc_control;
byte cc_val1;
byte cc_val2;
unsigned long   t = 0;
unsigned long   sync_t0 = 0;
unsigned long   connected_t0 = 0;

float _freqArray[24] = {64, 48, 32, 24, 16, 12, 8, 6, 5.3333, 4, 3.2, 3, 2.667, 2, 1.333, 1, 0.667, 0.5, 0.333, 0.25, 0.167, 0.125, 0.0625, 0.03125};

// internal classes
lfo         dac0_lfo(DACSIZE);
lfo         dac1_lfo(DACSIZE);
adsr        adsr(DACSIZE);

int getInt(int l_highByte, int l_lowByte) {
  return ((unsigned int)l_highByte << 8) + l_lowByte;
}

void setup() {
  delay(100);
  analogWriteResolution(12);  // set the analog output resolution to 12 bit (4096 levels) 

  dac0_lfo.setAmpl(dac0_ampl); // init amplitude
  dac1_lfo.setAmpl(dac1_ampl); // init amplitude

  sync_t0 = t;
  connected_t0 = t;

  pinMode(SYNC_OUTPUT, OUTPUT);
  pinMode(LED_DAC0, OUTPUT);
  pinMode(LED_DAC1, OUTPUT);
  pinMode(LED_SYNC, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);

  digitalWrite(SYNC_OUTPUT, false);
  digitalWrite(LED_DAC0, false);
  digitalWrite(LED_DAC1, false);
  digitalWrite(LED_SYNC, false);
  digitalWrite(LED_BUILTIN, false);
  
  Serial.begin(115200);
}

void loop() {
  t = micros();           // take timestamp

  //-------------------------------write to DAC0---------------------------------//
  if(dac0_adsr_enable == false) {
    dac0_lfo.setAmpl(dac0_ampl);
    analogWrite(DAC0, dac0_lfo.getWave(t));
  }
  else {
    if (dac0_lfo.getWaveForm() == 0) {    // DC waveform -> dac0_lfo.getWave(t) will give constant "_ampl_offset" as return
      analogWrite(DAC0, map(adsr.getWave(t), 0, DACSIZE-1, dac0_lfo.getWave(t), DACSIZE-1));
    }
    else {                                // update amplitude of LFO with ADSR
      dac0_lfo.setAmpl(map(adsr.getWave(t), 0, DACSIZE-1, 0, dac0_ampl));
      analogWrite(DAC0, dac0_lfo.getWave(t));
    }
  }
  digitalWrite(LED_DAC0, round(dac0_lfo.getPhase() * 2) % 2);     // phase goes from 0 to 1 -> multiply w 2 and modulus for 0 and 1

  //-------------------------------write to DAC1---------------------------------//
  if(dac1_adsr_enable == false) {
    dac1_lfo.setAmpl(dac1_ampl);
    analogWrite(DAC1, dac1_lfo.getWave(t));
  }
  else {
    if (dac1_lfo.getWaveForm() == 0) {    // DC waveform -> dac1_lfo.getWave(t) will give constant "_ampl_offset" as return
      analogWrite(DAC1, map(adsr.getWave(t), 0, DACSIZE-1, dac1_lfo.getWave(t), DACSIZE-1));
    }
    else {                                // update amplitude of LFO with ADSR
      dac1_lfo.setAmpl(map(adsr.getWave(t), 0, DACSIZE-1, 0, dac1_ampl));
      analogWrite(DAC1, dac1_lfo.getWave(t));
    }
  }
  digitalWrite(LED_DAC1, round(dac1_lfo.getPhase() * 2) % 2);     // phase goes from 0 to 1 -> multiply w 2 and modulus for 0 and 1

  //-----------------------------write to Sync port-----------------------------//
  double phase = 0;
  if (sync_mode == 0) {                    
    phase = (double)(t) * sync_mode0_freq / 1000000; 
    digitalWrite(SYNC_OUTPUT, round(phase * 2) % 2);   
  } // LFO synced
  else if(sync_mode == 1) {   
    phase = (double)(t - sync_t0) * _freqArray[sync_mode1_rate] * bpm / 60000000 + sync_phase;      // if midiclk is synced     
    digitalWrite(SYNC_OUTPUT, round(phase * 2) % 2);
  }
  digitalWrite(LED_SYNC, round(phase * 2) % 2);

  //-------------------------------timeout led---------------------------------//
  if (digitalRead(LED_BUILTIN))
    if((t - connected_t0) > CONNECTED_TIMEOUT)
      digitalWrite(LED_BUILTIN, 0);

  //----------Check if control commands have been received from Ableton------------//
  if (Serial.available()) {
    connected_t0 = t;
    if (digitalRead(LED_BUILTIN) == 0)
      digitalWrite(LED_BUILTIN, 1);
      
    rx_state++;
    switch (rx_state) {
      case 1:                     // first byte is always 255 for sync
        cc_sync = Serial.read();
        if(cc_sync != 255) {     // reset if first is not 255 sync byte
          rx_state = 0;
        }
        break;
      case 2:                     // second is the control byte
        cc_control = Serial.read();
        break;        
      case 3:                     // third is the most significant byte of the value
        cc_val1 = Serial.read();     
        break;
      case 4:                     // fourth is the least significant byte of the value
        cc_val2 = Serial.read();
        rx_state = 0;

        // re-compile value from its two bytes (cc_val1 is the MSB and cc_val2 the LSB)
        int value = getInt(cc_val1, cc_val2);
        
        // Track specific IDs
        if (cc_control == ID_SONG_BPM) {
          bpm = ((float)value)/10;
          dac0_lfo.setMode1Bpm(bpm);
          dac1_lfo.setMode1Bpm(bpm);
        }
        else if (cc_control == ID_SONG_MEASURE) {
            if(value !=0) {       // when track stops measure = 0
              if(dac0_lfo.getMode() and (int)(10*(value-1)/dac0_lfo.getMode1Rate())%10 == 0)
                dac0_lfo.sync(t);     // reset SYNC
              if(dac1_lfo.getMode() and (int)(10*(value-1)/dac1_lfo.getMode1Rate())%10 == 0)
                dac1_lfo.sync(t);     // reset SYNC
              if((int)(10*(value-1)/_freqArray[sync_mode1_rate])%10 == 0)
                sync_t0 = t;          // reset SYNC            
              digitalWrite(LED_MEASURE, !digitalRead(LED_MEASURE));
            }
        }
              
        // LFO - DAC0
        else if (cc_control == ID_DAC0_MODE)
          dac0_lfo.setMode(value);
        else if (cc_control == ID_DAC0_MODE0_FREQ)
          dac0_lfo.setMode0Freq(((float)value)/100, t);
        else if (cc_control == ID_DAC0_MODE1_RATE)
          dac0_lfo.setMode1Rate(_freqArray[(int)value]);
        else if(cc_control == ID_DAC0_AMPL) {
          dac0_ampl = value;
          dac0_lfo.setAmpl(dac0_ampl);
        }
        else if(cc_control == ID_DAC0_AMPL_OFFSET) 
          dac0_lfo.setAmplOffset(value);
        else if(cc_control == ID_DAC0_WAVEFORM) 
          dac0_lfo.setWaveForm(value);
        else if(cc_control == ID_DAC0_PHASE) 
          dac0_lfo.setMode1Phase((360 - (float)value) / 360); 
        else if(cc_control == ID_DAC0_ADSR_ENABLE) 
          dac0_adsr_enable = value; 
          
        // LFO - DAC1                                          
        else if (cc_control == ID_DAC1_MODE) 
          dac1_lfo.setMode(value);
        else if (cc_control == ID_DAC1_MODE0_FREQ)
          dac1_lfo.setMode0Freq(((float)value)/100, t);                         // freq is tuned in steps of 0.01 -> thus divide INT received by 100         
        else if (cc_control == ID_DAC1_MODE1_RATE)    
          dac1_lfo.setMode1Rate(_freqArray[(int)value]);
        else if(cc_control == ID_DAC1_AMPL) {
          dac1_ampl = value;
          dac1_lfo.setAmpl(dac1_ampl);
        }
        else if(cc_control == ID_DAC1_AMPL_OFFSET) 
          dac1_lfo.setAmplOffset(value);
        else if(cc_control == ID_DAC1_WAVEFORM) 
          dac1_lfo.setWaveForm(value);
        else if(cc_control == ID_DAC1_PHASE) 
          dac1_lfo.setMode1Phase((360 - (float)value) / 360); 
        else if(cc_control == ID_DAC1_ADSR_ENABLE) 
          dac1_adsr_enable = value; 
        
        // ADSR
        else if (cc_control == ID_ADSR_ATTACK)
          adsr.setAttack(1000*value);                                          // times 1000 -> conversion from ms to µs  
        else if (cc_control == ID_ADSR_DECAY)
          adsr.setDecay(1000*value);                                           // times 1000 -> conversion from ms to µs  
        else if (cc_control == ID_ADSR_SUSTAIN)
          adsr.setSustain(pow(10, -1*(float)value/1000)*(DACSIZE - 1));        // parameter is logarithmic from 0 to -70dB -> in MaxForLive it is multiplied with -100 for transportation to the Arduino. The value sent to the Arduino is thus an INT between 0 and 7000. To convert back -> time -1 and divide by 100. Then we need to convet from log to lin, which is done with 10^x/10 -> therefore we divide by 1000 here.
        else if (cc_control == ID_ADSR_RELEASE)
          adsr.setRelease(1000*value);                                         // times 1000 -> conversion from ms to µs  
        else if (cc_control == ID_ADSR_TRIGGER) {
          if(value == 1)
            adsr.noteOn(t);
          else if (value == 0)
            adsr.noteOff(t);
        }

        // Sync
        else if (cc_control == ID_SYNC_MODE)
          sync_mode = value;
        else if (cc_control == ID_SYNC_MODE0_FREQ)
          sync_mode0_freq = ((float)value)/100;                               // freq is tuned in steps of 0.01 -> thus divide INT received by 100
        else if (cc_control == ID_SYNC_MODE1_RATE)
          sync_mode1_rate = (int)value; 
        else if (cc_control == ID_SYNC_PHASE)
          sync_phase = (360 - (float)value) / 360;                          // phase is received in 360 steps -> convert to float from 0 to 1 
        break;
    }
  }
}
