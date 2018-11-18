#include <Arduino.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>

#define USE_SERIAL Serial
#define BUTTON_PIN 1
#define TABLE_NO 2

WebSocketsServer webSocket = WebSocketsServer(81);

enum state_action{
  wifi_server_as_access_point,
  wifi_server_as_connected,
  wifi_server_resting
};

state_action current_state = wifi_server_resting;
String data_to_be_routed ="";
void serial_trigger();
void action_once_wifi_server_as_access_point();
void action_once_wifi_server_as_connected();
void action_once_wifi_resting();
void do_action_wifi_server_as_access_point();
void do_action_wifi_server_as_connected();
bool do_action_wifi_resting();
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length);
void init();

const char* ssid = "Wrangler";
const char* password =  "$%2102120";

void action_once_wifi_server_as_access_point(){
  //initiate open access point
  //blink LED to declare that you are there
  IPAddress apIP(192, 168, 0, 72);
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  WiFi.softAP("CONNECT-TO-ORDER");
  USE_SERIAL.println();
  USE_SERIAL.println("Connected");
  Serial.println(WiFi.softAPIP());
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);

}

void action_once_wifi_server_as_connected(){
  // try to connect to the wifi network
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    USE_SERIAL.println("Connecting to WiFi..");
  }
  USE_SERIAL.println("Connected to the WiFi network");
  USE_SERIAL.println(WiFi.localIP());
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
}

void action_once_wifi_resting(){
  // Kills all wifi connections and go to sleep
  USE_SERIAL.println("Killing Wifi");
  WiFi.mode(WIFI_OFF);
  btStop();
}


void do_action_wifi_server_as_access_point(){
  // Wait for client to connect and give value
  // Check client response
  // check if value is good
  // change to wifi_server_as_connected
  //USE_SERIAL.println("Access-Point Websocket");
  webSocket.loop();
  serial_trigger();
}

void do_action_wifi_server_as_connected(){
  // Wait for client to connect and read value
  // Check client response
  // shutdown
  //USE_SERIAL.println("Connected Websocket");
  webSocket.loop();
  serial_trigger();
}

bool do_action_wifi_resting(){
  int pin_state = digitalRead(BUTTON_PIN);
  if(pin_state == true){
  // may be some delay here?
    //current_state = wifi_server_as_access_point;
  }
 serial_trigger();
 return (current_state != wifi_server_resting) ? true : false;
}


void hexdump(const void *mem, uint32_t len, uint8_t cols = 16) {
	const uint8_t* src = (const uint8_t*) mem;
	USE_SERIAL.printf("\n[HEXDUMP] Address: 0x%08X len: 0x%X (%d)", (ptrdiff_t)src, len, len);
	for(uint32_t i = 0; i < len; i++) {
		if(i % cols == 0) {
			USE_SERIAL.printf("\n[0x%08X] 0x%08X: ", (ptrdiff_t)src, i);
		}
		USE_SERIAL.printf("%02X ", *src);
		src++;
	}
	USE_SERIAL.printf("\n");
}

void serial_trigger(){
  char state_trigger = USE_SERIAL.read();
  switch (state_trigger) {
    case 'c':
    current_state = wifi_server_as_connected;
    break;
    case 'a':
    current_state = wifi_server_as_access_point;
    break;
    case 'r':
    current_state = wifi_server_resting;
    break;
  }
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {

    switch(type) {
        case WStype_DISCONNECTED:
            USE_SERIAL.printf("[%u] Disconnected!\n", num);
            break;
        case WStype_CONNECTED:
            {
                IPAddress ip = webSocket.remoteIP(num);
                USE_SERIAL.printf("[%u] Connected from %d.%d.%d.%d url: %s\n", num, ip[0], ip[1], ip[2], ip[3], payload);
				        // send message to client
				        webSocket.sendTXT(num, "Connected");
            }
            break;
        case WStype_TEXT:
          {
            USE_SERIAL.printf("[%u] get Text: %s\n", num, payload);
            if(current_state == wifi_server_as_access_point){
              // check if payload is JSON compatible ?
              // yes then send this msg and change state
              // send acknowledgement to client
              DynamicJsonBuffer jsonBuffer;
              char buffer[length + 1];
              for(int i = 0; i < length ; i++){
                buffer[i] = (char)(*payload);
                payload++;
              }
              buffer[length] = '\0';
              data_to_be_routed = String(buffer);
              USE_SERIAL.print("PAYLOAD STRING: ");
              USE_SERIAL.println(data_to_be_routed);
              JsonObject& root = jsonBuffer.parseObject(data_to_be_routed);
              root[String("tableNo")]= TABLE_NO;
              String tempString;
              root.printTo(tempString);
              USE_SERIAL.print("NEW STRING: ");
              USE_SERIAL.println(data_to_be_routed);
              webSocket.sendTXT(num, "OK");
              current_state = wifi_server_as_connected;
            }
            else if (current_state == wifi_server_as_connected){
                USE_SERIAL.print("GOT PING ");
                webSocket.sendTXT(num,data_to_be_routed);
                current_state = wifi_server_resting;
            }
          }
            // send data to all connected clients
            // webSocket.broadcastTXT("message here");
            break;
        case WStype_BIN:
            USE_SERIAL.printf("[%u] get binary length: %u\n", num, length);
            hexdump(payload, length);

            // send message to client
            // webSocket.sendBIN(num, payload, length);
            break;
		case WStype_ERROR:
		case WStype_FRAGMENT_TEXT_START:
		case WStype_FRAGMENT_BIN_START:
		case WStype_FRAGMENT:
		case WStype_FRAGMENT_FIN:
			break;
    }

}

void setup() {
  USE_SERIAL.begin(115200);
  pinMode(BUTTON_PIN,INPUT);
  for(uint8_t t = 4; t > 0; t--) {
      USE_SERIAL.printf("[SETUP] BOOT WAIT %d...\n", t);
      USE_SERIAL.flush();
      delay(1000);
  }
}

void loop() {
  if(current_state == wifi_server_resting){
      action_once_wifi_resting();
      while(!do_action_wifi_resting()){

      }
  }
  if(current_state == wifi_server_as_access_point){
      action_once_wifi_server_as_access_point();
    while(current_state == wifi_server_as_access_point){
      do_action_wifi_server_as_access_point();
    }
  }
  if(current_state == wifi_server_as_connected){
    action_once_wifi_server_as_connected();
    while(current_state == wifi_server_as_connected){
      do_action_wifi_server_as_connected();
    }
  }
}
