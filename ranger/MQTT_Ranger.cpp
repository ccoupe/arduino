// One day, this might be a class. For now, it's just C
/*
 * Handles all of the mqtt interactions for ranger (there are many)
 * The topic stucture is Homie V3 semi-compliant
 */
#include <Arduino.h>
#include <stdlib.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <Arduino_JSON.h>
#include "MQTT_Ranger.h"

#define USE_ACTIVE_HOLD

// forward declares 
// i.e Private:
void mqtt_reconnect();
void mqtt_send_config();
void mqtt_callback(char* topic, byte* payload, unsigned int length);
void mqtt_publish(char *topic, char *payload);
void mqtt_homie_pub(char *topic, char *payload, bool retain); 

char *wifi_id;
char *wifi_password;
char *mqtt_server;
int  mqtt_port;
char *mqtt_device;
char *hdevice;
char *hname;
char *hlname;
char *hpub;
char *hsub;
char *hsubq;
char *hpubst;             // ..autoranger/$status <- publish to 
char *hpubDistance;       // ..autoranger/distance <- publish to
char *hsubCmd;            // ..autoranger/cmd/set -> subscribe to
char *hsubDspCmd;         // ../display/cmd/set  -> subscribe to
char *hsubDspTxt;         // ../display/text/set -> subscribe to
void (*rgrCBack)(int newval); // does autorange to near newval
void (*dspCBack)(boolean st, char *str);                              

byte macaddr[6];
char *macAddr;
char *ipAddr;

WiFiClient espClient;
PubSubClient client(espClient);

static void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(wifi_id);

  WiFi.begin(wifi_id, wifi_password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  WiFi.macAddress(macaddr);
  macAddr = (char *)malloc(20);
  itoa(macaddr[5], &macAddr[0], 16);
  macAddr[2] = ':';
  itoa(macaddr[4], &macAddr[3], 16);
  macAddr[5] = ':';
  itoa(macaddr[3], &macAddr[6], 16);
  macAddr[8] = ':';
  itoa(macaddr[2], &macAddr[9], 16);
  macAddr[11] = ':';
  itoa(macaddr[1], &macAddr[12], 16);
  macAddr[13] = ':';
  itoa(macaddr[0], &macAddr[14], 16);
  Serial.print("MAC: ");
  Serial.print(macAddr);
  Serial.print(" IP address: ");
  String ipaddr = WiFi.localIP().toString();
  Serial.println(ipaddr);
  ipAddr = strdup(ipaddr.c_str());
}

void mqtt_setup(char *wid, char *wpw, char *mqsrv, int mqport, char* mqdev,
    char *hdev, char *hnm, void (*ccb)(int), void (*dcb)(boolean, char *) ) {

  rgrCBack = ccb;
  dspCBack = dcb;
  wifi_id = wid;
  wifi_password = wpw;
  mqtt_server = mqsrv;
  mqtt_port = mqport;
  mqtt_device = mqdev;
  hdevice = hdev;
  hname = hnm;

  // Create "homie/"HDEVICE"/autoranger"
  hpub = (char *)malloc(6 + strlen(hdevice) + 22); // wastes a byte or two.
  strcpy(hpub, "homie/");
  strcat(hpub, hdevice);
  strcat(hpub, "/autoranger");
  
  // Create "homie/"HDEVICE"/autoranger/status"  
  hpubst = (char *)malloc(strlen(hpub) + 8);
  strcpy(hpubst, hpub);
  strcat(hpubst, "/status");

  // Create "homie/"HDEVICE"/autoranger/cmd
  hsubq = (char *)malloc(6+strlen(hdevice)+30); // wastes a byte or two.
  strcpy(hsubq, "homie/");
  strcat(hsubq, hdevice);
  strcat(hsubq, "/autoranger/cmd");

  // Create "homie/"HDEVICE"/autoranger/cmd/set
  hsub = (char *)malloc(strlen(hsubq) + 6);
  strcpy(hsub, hsubq);
  strcat(hsub, "/set");
  hsubCmd = strdup(hsub);
  
  // Create "homie/"HDEVICE"/autoranger/distance for publishing
  hpubDistance = (char *)malloc(strlen(hpub) + 12);
  strcpy(hpubDistance, hpub);
  strcat(hpubDistance, "/distance");

  // create the subscribe topics for "homie/"HDEVICE"/display/cmd/set
  hsubDspCmd = (char *)malloc(6+strlen(hdevice)+20); // wastes a byte or two.
  strcpy(hsubDspCmd, "homie/");
  strcat(hsubDspCmd, hdevice);
  strcat(hsubDspCmd, "/display/cmd/set");

  // create the subscribe topics for "homie/"HDEVICE"/display/text/set
  hsubDspTxt = (char *)malloc(6+strlen(hdevice)+20); // wastes a byte or two.
  strcpy(hsubDspTxt, "homie/");
  strcat(hsubDspTxt, hdevice);
  strcat(hsubDspTxt, "/display/text/set");

  // Sanitize hname -> hlname
  hlname = strlwr(strdup(hname));
  int i = 0;
  for (i = 0; i < strlen(hlname); i++) {
    if (hlname[i] == ' ' || hlname[i] == '\t')
      hlname[i] = '_';
  }
  
  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(mqtt_callback);
  mqtt_reconnect();
  
  // Create and Publish the infrastructure topics for Homie v3
  // TODO: not unicode compatible, might work for utf-8 though
  char tmp[100] = {};
  char hpfix[30] = {};
  strcpy(hpfix,"homie/");
  strcpy(hpfix+6, hdevice);
  i = strlen(hpfix);
  char *p = tmp+i;
  strcpy(tmp,hpfix);
  
  // "homie/"HDEVICE"/$homie" -> "3.0.1"
  strcpy(p, "/$homie");
  mqtt_homie_pub(tmp, "3.0.1", true);
  
  //"homie/"HDEVICE"/$name" -> hlname
  strcpy(p, "/$name");
  mqtt_homie_pub(tmp, hlname, true);

  // "homie/"HDEVICE"/$state -> ready
  strcpy(p, "/$state");
  mqtt_homie_pub(tmp, "ready", true);
  
  // "homie/"HDEVICE"/$mac" -> macAddr
  strcpy(p, "/$mac");
  mqtt_homie_pub(tmp, strupr(macAddr), true);
  
  // "homie/"HDEVICE"/$localip" -> ipAddr
  strcpy(p, "/$localip");
  mqtt_homie_pub(tmp, ipAddr, true);
  
  //"homie/"HDEVICE"/$nodes", -> 
  strcpy(p, "/$nodes");
  mqtt_homie_pub(tmp, "autoranger.display", true);

  // end node - autoranger
  // begin node - display
  
  // "homie/"HDEVICE"/display/$name" -> hname (Un sanitized)
  strcpy(p, "/display/$name");
  mqtt_homie_pub(tmp, hname, true);
  
  // "homie/"HDEVICE"/display/$type" ->  "sensor"
  strcpy(p, "/display/$type");
  mqtt_homie_pub(tmp, "sensor", true);
  
  // "homie/"HDEVICE"/display/$properties" -> "cmd, text"
  strcpy(p, "/display/$properties");
  mqtt_homie_pub(tmp, "cmd,text", true);


  // Property 'cmd' of 'display' node
  // "homie/"HDEVICE"/display/cmd/$name ->, Unsanitized hname
  strcpy(p, "/display/cmd/$name");
  mqtt_homie_pub(tmp, hname, true); 

  // "homeie"HDEVICE"/display/cmd/$datatype" -> "string"
  strcpy(p, "/display/cmd/$datatype");
  mqtt_homie_pub(tmp, "string", true);

  // "homie/"HDEVICE"/display/cmd/$settable" -> "false"
  strcpy(p, "/display/cmd/$settable");
  mqtt_homie_pub(tmp, "false", true);

  // "homie/"HDEVICE"/display/cmd/$name" -> Unsantized hname
  strcpy(p, "/display/cmd/$name");
  mqtt_homie_pub(tmp, hname, true);

  // "homie/"HDEVICE"/display/cmd/$retained" -> "true"
  strcpy(p,"/display/cmd/$retained");
  mqtt_homie_pub(tmp, "false", true);
  

  // Property 'text' of 'display' node
  
  // "homie/"HDEVICE"/display/text/$name", -> "active_hold" 
  strcpy(p, "/display/text/$name");
  mqtt_homie_pub(tmp, hname, true);  

  // "homie/"HDEVICE"/display/text/$datatype" ->  "strimg"
  strcpy(p, "/display/text/$datatype");
  mqtt_homie_pub(tmp, "string", true);

  // "homie/"HDEVICE"/display/text/$settable" -> "true"
  strcpy(p, "/display/text/$settable");
  mqtt_homie_pub(tmp, "true", true);

  // "homie/"HDEVICE"/display/text/$retained" -> "true"
  strcpy(p,"/display/text/$retained");
  mqtt_homie_pub(tmp, "false", true);
  
}

void mqtt_callback(char* topic, byte* payl, unsigned int length) {
  char payload[length+1];
  // convert byte[] to char[]
  int i;
  for (i = 0; i < length; i++) {
    payload[i] = (char)payl[i];
  }
  payload[i] = '\0';
  Serial.print("Message arrived on topic: ");
  Serial.print(topic);
  Serial.print(" payload: ");
  Serial.println(payload);

  if (! strcmp(hsubCmd, topic)) { 
    int d = atoi(payload);
    if (d < 0)
      d = 0;
    else if (d > 3600)
      d = 3600;
    rgrCBack(d);
  } else if (! strcmp(hsubDspCmd, topic)) {
    if (!strcmp(payload, "on") || !strcmp(payload, "true")) {
      dspCBack(true, (char*)0);
    } else if ((! strcmp(payload, "off")) || (!strcmp(payload, "false" ))) {
      Serial.println("display set off");
      dspCBack(false, (char*)0);
    }
  } else if (! strcmp(hsubDspTxt, topic)) {
    dspCBack(true, payload);
  }
}

void mqtt_reconnect() {
  // Loop until we're reconnected
  int cnt = 1;
  int len = 5;
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect(mqtt_device)) {
      Serial.println("connected");

      // Subscribe to <dev/node/property>/set
      client.subscribe(hsubCmd);
      Serial.print("listening on topic ");
      Serial.println(hsubCmd);

      // Subscribe to <dev/node/property>/set
      Serial.print("listening on topic ");
      Serial.println(hsubDspTxt);
      client.subscribe(hsubDspTxt);

      // Subscribe to <dev/node/property>/set
      client.subscribe(hsubDspCmd);
      Serial.print("listening on topic ");
      Serial.println(hsubDspCmd);

    } else {
      Serial.print("failed, rc=");
      Serial.println(client.state());
      Serial.print(" try again in ");
      Serial.print(cnt * len);
      Serial.println(" seconds");
      // Wait X seconds before retrying
      delay((cnt * len) * 1000);
      cnt = cnt * 2;
      if (cnt > 256) 
        cnt = 256;
    }
  }
}

void mqtt_homie_pub(char *topic, char *payload, bool retain) {
  int err;
#if 0
  Serial.print("pub ");
  Serial.print(topic);
  Serial.print(" ");
  Serial.println(payload);
#endif
  if (! client.publish(topic, payload, retain)) {
    int rc  = client.state();
    if (rc < 0) {
      Serial.print(rc);
      Serial.println(" dead connection, retrying");
      mqtt_reconnect();
    }
  }
 
}

void mqtt_ranger_set_dist(int d) {
 char t[8];
 Serial.print("mqtt pub");
 itoa(d, t, 10);
 Serial.print(hpubDistance);
 Serial.println(t);
 mqtt_homie_pub(hpubDistance, t, false);
}

// Called from sketche's loop()
void mqtt_loop() {
  if (!client.connected()) {
    mqtt_reconnect();
  }
  client.loop();

}
