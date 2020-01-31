#ifndef MQTT_MOTION_H
#define MQTT_MOTION_H

// Functions in mqtt_lib.cpp 
// Public functions:
extern void mqtt_setup(char *wusr, char *wpw,
    char *mqsrv, int mqport, char* mqdev,
    char *hdev, char *hnm, 
    int (*gcb)(),
    void (*scb)(int));
extern void mqtt_loop();
extern void mqtt_homie_active(bool state);


#endif
