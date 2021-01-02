#ifndef MQTT_RANGER_H
#define MQTT_RANGER_H
#include "Device.h"
// Functions in MQTT_Ranger.cpp 
// Public functions:
extern void mqtt_setup(char *wusr, char *wpw,
    char *mqsrv, int mqport, char* mqdev,
    char *hdev, char *hnm, 
    void (*ccb)(String jstr));
extern void mqtt_loop();
extern void mqtt_ranger_set_dist(int);

#endif
