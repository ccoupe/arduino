## ESP32 motion sensor MQTT sketches for Ardunio

I have 2.5 different motion sensors. 'family-1' uses a Keystudio PIR.

'office-2' and 'kitchen-2' are alike, but. They both use an AM312 PIR and a
RCWL-0516 microwave sensor. They use two different ESP32 so different
IO pins.

family-1 uses a homie v3 topic structure be default and must be matched
with a hubitat driver (my MQTT Motion V3 for example, 
[found here](https://github.com/ccoupe/hubitat)) Note: if you build the
old version of the driver by #define OLD in Device.h then you should use
the groovy V2 driver. 

A note on git branches
V2 - C like api - no OLD define available. needs matching goovy drivers (v2 or V3)
V3 - C++ api
leif - Uses https://github.com/leifclaesson/LeifHomieLib.git
