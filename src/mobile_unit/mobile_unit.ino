#include <ESP8266WiFi.h>
#include <PubSubClient.h>

#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>

#include <SSD1306Wire.h>

#define MOBILE_ID 0

const char *ssid = "IOT_TEST";
const char *password = "IOT_TEST";
const char *mqtt_server = "192.168.178.100";

WiFiClient espClient;
PubSubClient mqtt_client(espClient);
unsigned long lastMsg = 0;
#define MSG_BUFFER_SIZE (50)
char msg[MSG_BUFFER_SIZE];
int value = 0;

unsigned long lastCmd = 0;
String cmdtopic_base = "PortableThermostat/cmd/mobile";
String infotopic_base = "PortableThermostat/info/mobile";
const char *turnOn_topic;
const char *hello_topic;
String turnOn_string = cmdtopic_base + "/" + String(MOBILE_ID) + "/turnOn";
String hello_string = infotopic_base + "/" + String(MOBILE_ID) + "/hello";

#define SDA D2
#define SCL D1
SSD1306Wire display(0x3c, SDA, SCL, GEOMETRY_128_32);  // ADDRESS, SDA, SCL, OLEDDISPLAY_GEOMETRY  -  Extra param required for 128x32 displays.
#define TEMP_POS 0,0
#define HUMID_POS 0,15
#define TRGT_POS 64,0
#define ONOFF_POS 64,15

#define DHTPIN D4
#define DHTTYPE    DHT11
DHT_Unified dht(DHTPIN, DHTTYPE);
#define SENSOR_DELAY 1000

#define PLUSBTTN D8
#define MINUSBTTN D7
#define ONOFFBTTN D6
#define NUM_BUTTONS 3
#define DEBOUNCE_DELAY 50

int targetTemp=18;
float currentTemp = -1;
int turnOn;
int last_turnOn = 0;
unsigned long last_measurement = 0;

typedef struct {
    int number;
    int current_state;
    int last_state;
    unsigned long last_debounce;
} button_pin;


uint32_t delayMS;

//Bottoni: D8-PLUS-16 D7-MINUS-7 D6-ON/OFF-6
button_pin plusbtn;
button_pin minusbtn;
button_pin onoffbtn;
button_pin *buttons[NUM_BUTTONS];

void setup_buttons(){
    unsigned long now = millis();
    
    pinMode(PLUSBTTN, INPUT);
    plusbtn.number=PLUSBTTN;
    plusbtn.last_state = 0;
    plusbtn.last_debounce = now;
    buttons[0]=&plusbtn;

    pinMode(MINUSBTTN, INPUT);
    minusbtn.number=MINUSBTTN;
    minusbtn.last_state = 0;
    minusbtn.last_debounce = now;
    buttons[1]=&minusbtn;

    pinMode(ONOFFBTTN, INPUT);
    onoffbtn.number=ONOFFBTTN;
    onoffbtn.last_state = 0;
    onoffbtn.last_debounce = now;
    buttons[2]=&onoffbtn;
    
    
}

void handle_buttons(){
    //leggi i bottoni
    for(int i=0; i<NUM_BUTTONS; i++){
      buttons[i]-> current_state = digitalRead(buttons[i]->number);
    }

    //controlla gli stati
    if((plusbtn.current_state == 1) && (plusbtn.current_state != plusbtn.last_state) && (millis() - plusbtn.last_debounce > DEBOUNCE_DELAY)){
      targetTemp++;
    } else if((minusbtn.current_state == 1) && (minusbtn.current_state != minusbtn.last_state) && (millis() - minusbtn.last_debounce > DEBOUNCE_DELAY)){
      targetTemp--;
    }//TODO: on/off

    //aggiorna i last state
    for(int i=0; i<NUM_BUTTONS; i++){
      buttons[i]-> last_state = buttons[i]-> current_state;
    }
}


void setup_wifi() {

    delay(10);
    // We start by connecting to a WiFi network
    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(ssid);

    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    randomSeed(micros());

    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
}

void mqtt_callback(char *topic, byte *payload, unsigned int length) {
    Serial.print("Message arrived [");
    Serial.print(topic);
    Serial.print("] ");
    for (int i = 0; i < length; i++) {
        Serial.print((char)payload[i]);
    }
    Serial.println();
}

void mqtt_reconnect() {
    // Loop until we're reconnected
    while (!mqtt_client.connected()) {
        Serial.print("Attempting MQTT connection...");
        // Create a random client ID
        String clientId = "ESP8266Client-";
        clientId += String(random(0xffff), HEX);
        // Attempt to connect
        if (mqtt_client.connect(clientId.c_str())) {
            Serial.println("connected");
            // Once connected, publish an announcement...
            mqtt_client.publish(hello_topic, "hello world");
        } else {
            Serial.print("failed, rc=");
            Serial.print(mqtt_client.state());
            Serial.println(" try again in 5 seconds");
            // Wait 5 seconds before retrying
            delay(5000);
        }
    }
}

void publish_turnOn(){
    lastCmd = millis();
    char cmd[5];
    itoa(turnOn, cmd, 10);
    Serial.print("Publish message: ");
    Serial.println(cmd);
    mqtt_client.publish(turnOn_topic, cmd);
}

void handle_temp(){
    sensors_event_t event;
    // Get temperature event
    dht.temperature().getEvent(&event);
    if (isnan(event.temperature)) {
        Serial.println("Errore temperatura");
    }
    else {
        currentTemp = event.temperature;
    }
    // Get humidity event and print its value.
    /*
    dht.humidity().getEvent(&event);
    if (isnan(event.relative_humidity)) {
        display.drawString(HUMID_POS, "H:E");
    }
    else {
        display.drawString(HUMID_POS, "H: " + String(event.relative_humidity) + "%");
    }
    */
}

void draw_display(){
    display.clear();

    display.setFont(ArialMT_Plain_24);
    display.drawString(TEMP_POS, String(currentTemp, 1) + "°");
    display.setFont(ArialMT_Plain_10);
    display.drawString(TRGT_POS, "TRGET:" + String(targetTemp) + "°C");

    String str;
    if(turnOn == 0)
        str = "OFF";
    else
        str = "ON";

    display.drawString(ONOFF_POS, str);
    
    display.display();
}

void setup() {
    Serial.begin(115200);
    pinMode(BUILTIN_LED, OUTPUT);
    pinMode(SDA, OUTPUT);
    pinMode(SCL, OUTPUT);
    pinMode(DHTPIN, OUTPUT);
    
  
    setup_wifi();
    setup_buttons();
    mqtt_client.setServer(mqtt_server, 1883);
    mqtt_client.setCallback(mqtt_callback);

    dht.begin();
    sensor_t sensor;
  
    display.init();
    //display.flipScreenVertically();
    display.setFont(ArialMT_Plain_10);

    
    turnOn_topic = turnOn_string.c_str();
    hello_topic = hello_string.c_str();
}

void loop() {
    if (!mqtt_client.connected()) {
        mqtt_reconnect();
    }
    mqtt_client.loop();

    unsigned long now = millis();
    if (now - lastMsg > 20000) {
        lastMsg = now;
        ++value;
        snprintf(msg, MSG_BUFFER_SIZE, "hello world #%ld", value);
        Serial.print("Publish message: ");
        Serial.println(msg);
        mqtt_client.publish(hello_topic, msg);
    }

    if (now - last_measurement > SENSOR_DELAY){
        last_measurement = now;
        handle_temp();
    }

    handle_buttons();

    //TODO: PID
    if(targetTemp > currentTemp){
        turnOn = 1;
    } else {
        turnOn = 0;
    }

    if (last_turnOn != turnOn || now - lastCmd > 10000) {
        publish_turnOn();
    }
    last_turnOn = turnOn;

    draw_display();
}
