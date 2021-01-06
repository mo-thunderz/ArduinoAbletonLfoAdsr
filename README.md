# ArduinoAbletonLfoAdsr
Ableton and Arduino source code of an LFO+ADSR, generated in an Arduino Due and controlled through Ableton with Max for Live (Cycling74).

Youtube video will follow shortly

# Installation
1) first install the LFO and ADSR libraries (https://github.com/mo-thunderz/lfo and https://github.com/mo-thunderz/adsr)
2) Copy file in the "MaxForLive_code" directory to Documents\Ableton\User Library\Presets\MIDI Effects\Max MIDI Effect
2) Copy folder in the "Arduino_code" directory to Documents\Arduino
3) Open Arduino IDE and program Arduino/Teensy/ESP32 with ArduinoAbletonLfoAdsr.ino (and LEAVE the device connected to the computer via USB). NOTE that the current implementation is done for the Arduino Due.
4) Open Ableton and on the left go to Catagories -> Max for Live -> Max MIDI Effect and move ArduinoAbletonLfoAdsr to a midi track

# Updates
4.1.2021: 
1) fixed a small issue that whenever you start a track it does not send out a trigger to the Arduino if the last time the track was played it ended in the same measure count. 
2) Reduced the stepsize for "phase" from 4096 to 360 (that is precise enough for phase and does not load the serial port as much when sweeping the phase).
3) Cleaned up naming for port mapping in Ableton.

6.1.2021: 
1) added a latency parameter to compensate for latency of external hardware. 

# How to use
Please refer to the following youtube post:
https://youtu.be/TTPP9dcvJH8

Have fun ;-)

mo thunderz
