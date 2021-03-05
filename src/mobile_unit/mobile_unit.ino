#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <PubSubClientTools.h>

#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>

#include <SSD1306Wire.h>
#include <TaskScheduler.h>
#include <InputDebounce.h>

const char *ssid = "IOT_TEST";
const char *password = "IOT_TEST";
const char *mqtt_server = "192.168.178.10";

WiFiClient espClient;
PubSubClient mqtt_client(mqtt_server, 1883, espClient);
PubSubClientTools mqtt_tools(mqtt_client);

Scheduler ts;

#define MAX_NUMBER_OF_MOBILES (5)
int thermostat_id = 0;

unsigned long last_reconnect_attempt = 0;
#define RECONNECT_DELAY 5000
String cmdtopic_base = "PortableThermostat/cmd/mobile";
String infotopic_base = "PortableThermostat/info/mobile";

#define SDA D2
#define SCL D1
SSD1306Wire display(0x3c, SDA, SCL, GEOMETRY_128_32);  // ADDRESS, SDA, SCL, OLEDDISPLAY_GEOMETRY  -  Extra param required for 128x32 displays.
#define TEMP_POS 0,2
#define TRGT_POS 80,0
#define FLAME_POS 60,8
#define ENABLED_POS 96,15
#define ID_POS 128, 15  //Il numero rimane sulla sinistra del punto indicato

#define DHTPIN D4
#define DHTTYPE DHT11
DHT_Unified dht(DHTPIN, DHTTYPE);
#define SENSOR_DELAY 1000

#define ENABLEBTTN D8
#define PLUSBTTN D7
#define MINUSBTTN D6
#define NUM_BUTTONS 3
#define DEBOUNCE_DELAY 50

#define HYSTERESIS 0.3
float target_temp=20;
float current_temp = -1;
float current_humidity = -1;
int turnOn;
bool enableThermostat = true;
bool setup_id_mode;

InputDebounce plusbtn;
InputDebounce minusbtn;
InputDebounce enablebtn;

#define FLAME_WIDTH 16
#define FLAME_HEIGHT 16
const uint8_t FlameLogo_bits[] PROGMEM = {
    0xC0, 0x00, 0xC0, 0x00, 0xE0, 0x00, 0xE0, 0x01, 0xF0, 0x03, 0xF0, 0x03, 
    0xF8, 0x07, 0xF8, 0x0F, 0xF8, 0x0F, 0xFC, 0x0F, 0xBC, 0x1F, 0x3C, 0x1F, 
    0x3C, 0x1F, 0x38, 0x1E, 0x38, 0x0C, 0x30, 0x0C,
};

void button_pressed_callback(uint8_t pin){
    switch(pin){
        case PLUSBTTN:
            target_temp+=0.1;
            break;
        case MINUSBTTN:
            target_temp-=0.1;
            break;
        case ENABLEBTTN:
            enableThermostat = !enableThermostat;
            break;
    }
}

void setup_id_button_pressed_callback(uint8_t pin){
    switch(pin){
        case PLUSBTTN:
            thermostat_id+=1;
            break;
        case MINUSBTTN:
            thermostat_id-=1;
            break;
        case ENABLEBTTN:
            setup_id_mode = false;
            break;
    }
    if(thermostat_id < 0){
        thermostat_id += MAX_NUMBER_OF_MOBILES;
    } else {
        thermostat_id = thermostat_id % MAX_NUMBER_OF_MOBILES;
    }
}

void setup_buttons(){
    plusbtn.setup(PLUSBTTN, DEFAULT_INPUT_DEBOUNCE_DELAY, InputDebounce::PIM_EXT_PULL_DOWN_RES);
    minusbtn.setup(MINUSBTTN, DEFAULT_INPUT_DEBOUNCE_DELAY, InputDebounce::PIM_EXT_PULL_DOWN_RES);
    enablebtn.setup(ENABLEBTTN, DEFAULT_INPUT_DEBOUNCE_DELAY, InputDebounce::PIM_EXT_PULL_DOWN_RES);
}


void register_button_callbacks(){
    plusbtn.registerCallbacks(button_pressed_callback, NULL, NULL, NULL);
    minusbtn.registerCallbacks(button_pressed_callback, NULL, NULL, NULL);
    enablebtn.registerCallbacks(button_pressed_callback, NULL, NULL, NULL);
}


void process_buttons(){
    unsigned long now = millis();
    plusbtn.process(now);
    minusbtn.process(now);
    enablebtn.process(now);
}
Task process_buttons_task(TASK_IMMEDIATE, TASK_FOREVER, process_buttons);

void setup_id(){
    setup_id_mode = true;
    plusbtn.registerCallbacks(setup_id_button_pressed_callback, NULL, NULL, NULL);
    minusbtn.registerCallbacks(setup_id_button_pressed_callback, NULL, NULL, NULL);
    enablebtn.registerCallbacks(setup_id_button_pressed_callback, NULL, NULL, NULL);
    do{
        display.clear();
        display.setFont(ArialMT_Plain_16);
        display.setTextAlignment(TEXT_ALIGN_CENTER_BOTH);
        unsigned long now = millis();

        plusbtn.process(now);
        minusbtn.process(now);
        enablebtn.process(now);
        
        display.drawString(64, 14, "Scegli id: " + String(thermostat_id));
        display.display();
    } while(setup_id_mode);
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

void mqtt_reconnect() {
    digitalWrite(BUILTIN_LED, LOW);
    Serial.print("Attempting MQTT connection...");
    String clientId = "ESP8266Client-PT-mobile-" + String(thermostat_id);
    // Attempt to connect
    if (mqtt_client.connect(clientId.c_str())) {
        Serial.println("connected");
        digitalWrite(BUILTIN_LED, HIGH);
    } else {
        Serial.print("failed, rc=");
        Serial.println(mqtt_client.state());
    }
}
Task mqtt_reconnect_task(RECONNECT_DELAY * TASK_MILLISECOND, TASK_FOREVER, mqtt_reconnect);

void publish_turnOn(){
    String topic = cmdtopic_base + "/" + String(thermostat_id) + "/turnOn";
    String cmd = String(turnOn);
    Serial.println("Publish message [" + topic + "]: " + cmd);
    mqtt_tools.publish(topic, cmd);
}
Task publish_turnOn_task(5 * TASK_SECOND, TASK_FOREVER, publish_turnOn);

void publish_infos(){
    String topic, msg;
    //pubblica temp
    topic = infotopic_base + "/" + String(thermostat_id) + "/temp";
    msg = String(current_temp, 2);
    Serial.println("Publish message [" + topic + "]: " + msg);
    mqtt_tools.publish(topic, msg);

    //pubblica humid
    topic = infotopic_base + "/" + String(thermostat_id) + "/humid";
    msg = String(current_humidity, 2);
    Serial.println("Publish message [" + topic + "]: " + msg);
    mqtt_tools.publish(topic, msg);

    //pubblica target
    topic = infotopic_base + "/" + String(thermostat_id) + "/target_temp";
    msg = String(target_temp, 2);
    Serial.println("Publish message [" + topic + "]: " + msg);
    mqtt_tools.publish(topic, msg);
}
Task publish_infos_task(10 * TASK_SECOND, TASK_FOREVER, publish_infos);

void get_sensor_data(){
    sensors_event_t event;
    // Get temperature event
    dht.temperature().getEvent(&event);
    if (isnan(event.temperature)) {
        Serial.println("Temperature read error");
    }
    else {
        current_temp = event.temperature;
    }
    // Get humidity event
    dht.humidity().getEvent(&event);
    if (isnan(event.relative_humidity)) {
        Serial.println("Humidity read error");
    }
    else {
        current_humidity = event.relative_humidity;
    }
}
Task get_sensor_data_task(TASK_SECOND, TASK_FOREVER, get_sensor_data);

void draw_display(){
    display.clear();
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    display.setFont(ArialMT_Plain_24);
    display.drawString(TEMP_POS, String(current_temp, 1) + "°");
    display.setFont(ArialMT_Plain_10);
    display.drawString(TRGT_POS, "SET:" + String(target_temp, 1) + "°");

    if(turnOn){
        display.drawXbm(FLAME_POS, FLAME_WIDTH, FLAME_HEIGHT, FlameLogo_bits);
    }

    if(enableThermostat){
        display.drawString(ENABLED_POS, "EN");
    }

    display.setTextAlignment(TEXT_ALIGN_RIGHT);
    display.drawString(ID_POS, String(thermostat_id));
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    
    display.display();
}
Task draw_display_task(TASK_IMMEDIATE, TASK_FOREVER, draw_display);

void update_turnOn(){
    //Se termostato è abilitato, verifica che ci sia bisogno di accendere (con isteresi)
    if(enableThermostat) {
        if(turnOn == 0 && current_temp <= (target_temp - HYSTERESIS)){
            turnOn = 1;
        } else if(turnOn == 1 && current_temp >= (target_temp + HYSTERESIS)) {
            turnOn = 0;
        }
    } else {
        turnOn = 0;
    }
}
Task update_turnOn_task(TASK_IMMEDIATE, TASK_FOREVER, update_turnOn);

void setup() {
    Serial.begin(115200);
    pinMode(BUILTIN_LED, OUTPUT);
    pinMode(SDA, OUTPUT);
    pinMode(SCL, OUTPUT);
    pinMode(DHTPIN, OUTPUT);
    
    setup_wifi();
    setup_buttons();

    dht.begin();
  
    display.init();

    setup_id();
    register_button_callbacks();

    ts.addTask(mqtt_reconnect_task);
    ts.addTask(process_buttons_task);
    ts.addTask(get_sensor_data_task);
    ts.addTask(publish_turnOn_task);
    ts.addTask(publish_infos_task);
    ts.addTask(draw_display_task);
    ts.addTask(update_turnOn_task);
    process_buttons_task.enable();
    get_sensor_data_task.enable();
    update_turnOn_task.enable();
    draw_display_task.enable();
}

void loop() {
    if (!mqtt_client.connected()) {
        mqtt_reconnect_task.enableIfNot();
        publish_turnOn_task.disable();
        publish_infos_task.disable();
    } else {
        mqtt_reconnect_task.disable();
        publish_turnOn_task.enableIfNot();
        publish_infos_task.enableIfNot();
    }
    mqtt_client.loop();
    
    ts.execute();
}
