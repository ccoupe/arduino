# Ranger Sketch

The purpose of the device is to quide the user to the correct distance
For example to take a camera photo. When told a distance it will display
'Move Forward', 'Move Back' or 'Stop There' depending on the objects distance from
the specified distance. When 'stop' is displayed, the MQTT client is notified
of the distance. 

A MQTT device for measuring distance and displaying it and other text. It
uses the common HC-SR504 ultrasonic ranger and the common LCD-1602 (i2c) attatched
to an ESP32. The MQTT pub/sub is (mostly) compatible with the Homie 3
conventions.

It can be compiled to use an OLED display using the U8x8lib library 
(but only two lines of a large font).
With code modications you could print smaller fonts and more lines on that
kind of device.

Device.h will need to be modified for your wifi network, choice of pins
for the sensor, the display you are using and the toplevel Homie 'device'
name.

## Display Usage 

The display can be used independent of the ranger, if the ranger is inactive.
### Turn off display
1. publish 'off' to the topic 'homie/yourdevicename/display/cmd/set'

### Turn on display 
The display turns on automatically if you send text to it but for completeness
you can turn it on (it will clear the display) by 
1. publish 'on' to the topic 'homie/yourdevicename/display/cmd/set'

### Display text

1. publish 'string of words' to the topic 'homie/yourdevicename/display/text/set

The display will turn on, if needed and be cleared. 

If the string contains two words. They will be placed on different lines, centered.
If the string contains more than two words it will attempt to fit them to the
lines and space available, centered if there is any space remaining. Note:
newlines ('\n') are considered as spaces. 

## Ranger Usage

You need to subscribe to 'homie/yourdevicename/autoranger/distance'. The payload
will be a integer number (centimeters)

### Modes
You can set the ranger mode to 'once', 'continous' or 'free' by publishing to
'homie/yourdevicename/autoranger/mode/set'. In 'once' and 'continous' it won't
start to measure distances until the target distance is set. In 'free' mode
it starts publishing distances immediately. WARNING. In free mode it does not throttle
itself so the MQTT broker and your MQTT client could become very busy. 

The default mode is 'once'

### set distance
Distance is in centimeters (cm). The device will display 'Move Forward',
'Move Back' or 'Stop There' depending on the distance. 

1. publish 'number' to 'homie/yourdevicename/autoranger/distance/set'

In 'once' mode, when the object is within 10 cm (+/-) of the target distance
then the distance is published to 'homie/yourdevicename/autoranger/distance'
and the ranging stops.

'continous' mode will publish like 'once' mode, but will continue to range and
publish when in range. To reduce load on the MQTT broker, it will only publish the
distance if with the target spread every 5 seconds.  To stop, publish '0' to 
'homie/yourdevicename/autoranger/distance/set' or 
'off' to 'homie/yourdevicename/autoranger/mode/set'

