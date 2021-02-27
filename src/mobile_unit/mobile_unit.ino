#include <ESP8266WiFi.h>
#include <PubSubClient.h>

#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>

#include <SSD1306Wire.h>

#define MOBILE_ID 0

const char *ssid = "IOT_TEST";
const char *password = "IOT_TEST";
const char *mqtt_server = "192.168.178.10";

WiFiClient espClient;
PubSubClient mqtt_client(espClient);
unsigned long lastMsg = 0;
#define MSG_BUFFER_SIZE (50)
char msg[MSG_BUFFER_SIZE];
int value = 0;

#define MAX_NUMBER_OF_MOBILES (5)
int thermostat_id = 0;

unsigned long lastCmd = 0;
String cmdtopic_base = "PortableThermostat/cmd/mobile";
String infotopic_base = "PortableThermostat/info/mobile";
const char *turnOn_topic;
const char *hello_topic;
String turnOn_string;
String hello_string;

#define SDA D2
#define SCL D1
SSD1306Wire display(0x3c, SDA, SCL, GEOMETRY_128_32);  // ADDRESS, SDA, SCL, OLEDDISPLAY_GEOMETRY  -  Extra param required for 128x32 displays.
#define TEMP_POS 0,0
#define HUMID_POS 0,15
#define TRGT_POS 64,0
#define ONOFF_POS 64,15
#define ENABLED_POS 96,15

#define DHTPIN D4
#define DHTTYPE    DHT11
DHT_Unified dht(DHTPIN, DHTTYPE);
#define SENSOR_DELAY 1000

#define PLUSBTTN D8
#define MINUSBTTN D7
#define ENABLEBTTN D6
#define NUM_BUTTONS 3
#define DEBOUNCE_DELAY 50

float targetTemp=18;
float currentTemp = -1;
int turnOn;
int last_turnOn = 0;
unsigned long last_measurement = 0;
bool enableThermostat = true;

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
button_pin enablebtn;
button_pin *buttons[NUM_BUTTONS];

#define FLAME_WIDTH 16
#define FLAME_HEIGHT 16
const uint8_t FlameLogo_bits[] PROGMEM = {
    0xC0, 0x00, 0xC0, 0x00, 0xE0, 0x00, 0xE0, 0x01, 0xF0, 0x03, 0xF0, 0x03, 
    0xF8, 0x07, 0xF8, 0x0F, 0xF8, 0x0F, 0xFC, 0x0F, 0xBC, 0x1F, 0x3C, 0x1F, 
    0x3C, 0x1F, 0x38, 0x1E, 0x38, 0x0C, 0x30, 0x0C,
};


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

    pinMode(ENABLEBTTN, INPUT);
    enablebtn.number=ENABLEBTTN;
    enablebtn.last_state = 0;
    enablebtn.last_debounce = now;
    buttons[2]=&enablebtn;
    
    
}

void read_buttons(){
    for(int i=0; i<NUM_BUTTONS; i++){
        buttons[i] -> current_state = digitalRead(buttons[i]->number);
    }
}

void debounce_buttons(){
    //aggiorna i last debounce
    unsigned long now = millis();
    for(int i=0; i<NUM_BUTTONS; i++){
        if(buttons[i] -> current_state != buttons[i] -> last_state){
            buttons[i] -> last_debounce = now;
        }
    }
}

void update_last_buttons_state(){
    //aggiorna i last state
    for(int i=0; i<NUM_BUTTONS; i++){
      buttons[i]-> last_state = buttons[i]-> current_state;
    }
}

void handle_buttons(){
    read_buttons();

    unsigned long now = millis();
    //controlla gli stati
    if((plusbtn.current_state == HIGH) && (plusbtn.current_state != plusbtn.last_state) && (now - plusbtn.last_debounce > DEBOUNCE_DELAY)){
        targetTemp+=0.1;
    } else if((minusbtn.current_state == HIGH) && (minusbtn.current_state != minusbtn.last_state) && (now - minusbtn.last_debounce > DEBOUNCE_DELAY)){
        targetTemp-=0.1;
    } else if((enablebtn.current_state == HIGH) && (enablebtn.current_state != enablebtn.last_state) && (now - enablebtn.last_debounce > DEBOUNCE_DELAY)){
        enableThermostat = !enableThermostat;
    }

    debounce_buttons();
    update_last_buttons_state();
    
}

void setup_id(){
    bool done = false;
    do{
        int id=0;
        display.clear();
        display.setFont(ArialMT_Plain_16);
        display.setTextAlignment(TEXT_ALIGN_CENTER_BOTH);
        read_buttons();
        unsigned long now = millis();
        if((plusbtn.current_state == HIGH) && (plusbtn.current_state != plusbtn.last_state) && (now - plusbtn.last_debounce > DEBOUNCE_DELAY)){
            thermostat_id++;
        } else if((minusbtn.current_state == HIGH) && (minusbtn.current_state != minusbtn.last_state) && (now - minusbtn.last_debounce > DEBOUNCE_DELAY)){
            thermostat_id--;
        } else if((enablebtn.current_state == HIGH) && (enablebtn.current_state != enablebtn.last_state) && (now - enablebtn.last_debounce > DEBOUNCE_DELAY)){
            done=true;
        }
        if(thermostat_id < 0){
            thermostat_id += MAX_NUMBER_OF_MOBILES;
        }
        thermostat_id = thermostat_id % MAX_NUMBER_OF_MOBILES;
        display.drawString(64, 16, "Scegli id: " + String(thermostat_id));
        debounce_buttons();
        update_last_buttons_state();
        display.display();
    } while(!done);
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
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    display.setFont(ArialMT_Plain_24);
    display.drawString(TEMP_POS, String(currentTemp, 1) + "°");
    display.setFont(ArialMT_Plain_10);
    display.drawString(TRGT_POS, "SET:" + String(targetTemp, 1) + "°");

    if(turnOn){
        display.drawXbm(ONOFF_POS, FLAME_WIDTH, FLAME_HEIGHT, FlameLogo_bits);
    }

    if(enableThermostat){
        display.drawString(ENABLED_POS, "EN");
    }
    
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

    setup_id();
    
    turnOn_string = cmdtopic_base + "/" + String(thermostat_id) + "/turnOn";
    turnOn_topic = turnOn_string.c_str();
    hello_string = infotopic_base + "/" + String(thermostat_id) + "/hello";
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
    if(enableThermostat) {
        if(targetTemp > currentTemp){
            turnOn = 1;
        } else {
            turnOn = 0;
        }
    } else {
        turnOn = 0;
    }
    

    if (last_turnOn != turnOn || now - lastCmd > 10000) {
        publish_turnOn();
    }
    last_turnOn = turnOn;

    draw_display();
}
