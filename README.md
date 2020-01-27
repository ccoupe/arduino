## ESP32 motion sensor MQTT sketches for Ardunio

I have 2.5 different motion sensors. 'family-1' uses a Keystudio PIR.
office-2 and kitchen-2 are alike, but. They both use an AM312 PIR and a
RCWL-0516 microwave sensor. They use two different ESP32 so different
IO pins.

family-1 uses a homie v3 topic structure be default and must be matched
with a hubitat driver (my MQTT Motion V3 for example, 
[found here](https://github.com/ccoupe/hubitat))
