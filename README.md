## ESP32 motion sensor MQTT and Homie sketches for Arduino


I have 3 motion sensors. 'family-1' uses a Keystudio PIR.
'office-2' and 'kitchen-2' are alike, They both use an AM312 PIR and a
RCWL-0516 microwave sensor. They use two different ESP32 so they have different
IO pins. Since they are MQTT they all have different MQTT client names.

Each one has a Device.h where those differences are declared. Each one
has a normal Arduino sketch (.ino) and they include a library, MQTT_Motion
to do all the mqtt stuff.

MQTT_Motion.cpp and .h live in libraries/MQTT_Motion/ It is not a C++ object.
It has a C like api.  The library creates and uses a Homie v3 capatible topic
stucture which it creates in the MQTT_setup() call from the passed parameters
which came from the .ino and Device.h. 

Installing the library is Arduino, which is to say a bit non-standard. Just
copy the libraries/MQTT_Motion/ directory to where your sketchbook libraries live.

The library implements the 'motion' property for the 'motionsensor' node. It also
implements an 'active_hold' property for the 'motionsensor' node. This is the
time in seconds to wait for additional motion to appear again after motion is
detected. The default is 45 seconds. This keep the sensor from being too chatty.
This value can be changed while running with mqtt publish to .../active_hold/set
The value is stored in EEPROM so it is sticky between device power on/off cycles.

The mqtt_setup calling sketch has to pass two callback funtions to handle the
get and set of the EEPROM value. 
