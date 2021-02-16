#include <ESP8266WiFi.h>
#include <PubSubClient.h>

#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>

#include <SSD1306Wire.h>

const char *ssid = "IOT_TEST";
const char *password = "IOT_TEST";
const char *mqtt_server = "192.168.178.100";

WiFiClient espClient;
PubSubClient mqtt_client(espClient);
unsigned long lastMsg = 0;
#define MSG_BUFFER_SIZE (50)
char msg[MSG_BUFFER_SIZE];
int value = 0;


#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels
#define SDA D2
#define SCL D1
SSD1306Wire display(0x3c, SDA, SCL, GEOMETRY_128_32);  // ADDRESS, SDA, SCL, OLEDDISPLAY_GEOMETRY  -  Extra param required for 128x32 displays.

#define DHTPIN D5
#define DHTTYPE    DHT11
DHT_Unified dht(DHTPIN, DHTTYPE);

#define PLUSBTTN D8
int lastPlusRead = 0;
int currentPlusRead;
int targetTemp=18;

uint32_t delayMS;




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

    // Switch on the LED if an 1 was received as first character
    if ((char)payload[0] == '1') {
        digitalWrite(BUILTIN_LED, LOW); // Turn the LED on (Note that LOW is the voltage level
                                        // but actually the LED is on; this is because
                                        // it is active low on the ESP-01)
    } else {
        digitalWrite(BUILTIN_LED, HIGH); // Turn the LED off by making the voltage HIGH
    }
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
            mqtt_client.publish("PortableThermostat/static/hello", "hello world");
            // ... and resubscribe
            mqtt_client.subscribe("PortableThermostat/mobile");
        } else {
            Serial.print("failed, rc=");
            Serial.print(mqtt_client.state());
            Serial.println(" try again in 5 seconds");
            // Wait 5 seconds before retrying
            delay(5000);
        }
    }
}

void setup() {
    Serial.begin(115200);
    pinMode(BUILTIN_LED, OUTPUT);
    pinMode(SDA, OUTPUT);
    pinMode(SCL, OUTPUT);
    pinMode(DHTPIN, OUTPUT);
    pinMode(PLUSBTTN, OUTPUT);
  
    setup_wifi();
    mqtt_client.setServer(mqtt_server, 1883);
    mqtt_client.setCallback(mqtt_callback);

    dht.begin();
    sensor_t sensor;
  
  // Set delay
  delayMS = 50;

  display.init();
  //display.flipScreenVertically();
  display.setFont(ArialMT_Plain_10);
}

void loop() {
    delay(delayMS);
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
        mqtt_client.publish("PortableThermostat/mobile/hello", msg);
    }

    display.clear();

    // Get temperature event and print its value.
    sensors_event_t event;
    dht.temperature().getEvent(&event);
    if (isnan(event.temperature)) {
        Serial.println(F("Error reading temperature!"));
        display.drawString(0, 0, "T:E");
    }
    else {
        Serial.print(F("Temperature: "));
        Serial.print(event.temperature);
        Serial.println(F("°C"));
        display.drawString(0, 0, "T: " + String(event.temperature) + "°C");
    }
    // Get humidity event and print its value.
    dht.humidity().getEvent(&event);
    if (isnan(event.relative_humidity)) {
        Serial.println(F("Error reading humidity!"));
        display.drawString(0, 15, "H:E");
    }
    else {
        Serial.print(F("Humidity: "));
        Serial.print(event.relative_humidity);
        Serial.println(F("%"));
        display.drawString(0, 15, "H: " + String(event.relative_humidity) + "%");
    }
  
  
    //Senza debounce (c'è giusto il delay, per i test funziona)
    currentPlusRead=digitalRead(PLUSBTTN);
    if(currentPlusRead == 1 && (currentPlusRead != lastPlusRead)){
        targetTemp++;
    }
    lastPlusRead=currentPlusRead;
    display.drawString(64, 0, "TRGET:" + String(targetTemp) + "°C");
    
    display.display();
}
