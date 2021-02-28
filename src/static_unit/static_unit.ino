#include <ESP8266WiFi.h>
#include <PubSubClient.h>

const char *ssid = "IOT_TEST";
const char *password = "IOT_TEST";
const char *mqtt_server = "192.168.178.10";

const char *cmdtopic = "PortableThermostat/cmd/mobile";
const char *infotopic = "PortableThermostat/info/static/isOn";

WiFiClient espClient;
PubSubClient mqtt_client(espClient);
unsigned long lastMsg = 0;
unsigned long lastCheck = 0;
#define MSG_BUFFER_SIZE (50)
char msg[MSG_BUFFER_SIZE];

#define TOPIC_BUFFER_SIZE (150)
char topic_buffer[TOPIC_BUFFER_SIZE];

int value = 0;

short currentlyOn = -1;

#define RELAY_PIN D5
#define NUMBER_OF_MOBILES (5)
short mobiles[NUMBER_OF_MOBILES]; //Memorizza lo stato dei termostati: -1 se unknown, 0 se non richiedono riscaldamento, 1 se lo richiedono
unsigned long last_updates[NUMBER_OF_MOBILES]; //Usato per "invecchiamento" degli stati dei termostati. Se un termostato non comunica per più di un certo numero di ms si resetta il suo stato a unknown
#define AGING_THRESHOLD 300000

void setup_wifi() {
    digitalWrite(BUILTIN_LED, LOW);
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
    digitalWrite(BUILTIN_LED, HIGH);
}

void mqtt_callback(char *topic, byte *payload, unsigned int length) {
    Serial.print("Message arrived [");
    Serial.print(topic);
    Serial.print("] ");
    for (int i = 0; i < length; i++) {
        Serial.print((char)payload[i]);
    }
    Serial.println();
    
    digitalWrite(BUILTIN_LED, LOW);
    int index = atoi((char*)&topic[(strlen(cmdtopic) + 1)]); //Estrae il numero del termostato mobile dal percorso del topic

    if(index < NUMBER_OF_MOBILES){ //Non dovrebbe essere necessario perché non sottoscrive nessun topic diverso da quelli con numeri compatibili
        mobiles[index] = (payload[0] - '0');
        last_updates[index] = millis();
    }
    delay(50);
    digitalWrite(BUILTIN_LED, HIGH);
}

//Sottoscrive iterativamente ai topic relativi a ciascun termostato mobile in base al numero specificato in mobilecount
void mqtt_subscribe_to_mobiles(PubSubClient client, int mobilecount, const char* cmdtopic){
    for(int i=0; i<NUMBER_OF_MOBILES; i++){
        snprintf(topic_buffer, TOPIC_BUFFER_SIZE, "%s/%d/turnOn", cmdtopic, i);
        Serial.print("Subscribed to topic [");
        Serial.print(topic_buffer);
        Serial.println("]");
        mqtt_client.subscribe(topic_buffer);
    }
}

void mqtt_reconnect() {
    digitalWrite(BUILTIN_LED, LOW);
    // Loop until we're reconnected
    while (!mqtt_client.connected()) {
        if(WiFi.status() != WL_CONNECTED){
            Serial.println("Wifi non connesso: evito connessione MQTT");
            delay(5000);
        } else {
        
            Serial.print("Attempting MQTT connection...");
            // Create a random client ID
            String clientId = "ESP8266Client-";
            clientId += String(random(0xffff), HEX);
            // Attempt to connect
            if (mqtt_client.connect(clientId.c_str())) {
                Serial.println("connected");
                // Once connected, publish an announcement...
                mqtt_client.publish("PortableThermostat/info/static/hello", "hello world");
                // ... and resubscribe
                mqtt_subscribe_to_mobiles(mqtt_client, NUMBER_OF_MOBILES, cmdtopic);
            } else {
                Serial.print("failed, rc=");
                Serial.print(mqtt_client.state());
                Serial.println(" try again in 5 seconds");
                // Wait 5 seconds before retrying
                delay(5000);
            }
        }
    }
    digitalWrite(BUILTIN_LED, HIGH);
}

//Inizializza l'array dei mobiles a -1
void init_mobile_arr(short mobilearr[], int mobilecount){
    for(int i=0; i<mobilecount; i++){
        mobilearr[i]=-1;
    }
}


void setup() {
    Serial.begin(115200);
    pinMode(BUILTIN_LED, OUTPUT);
    digitalWrite(BUILTIN_LED, HIGH);
    pinMode(RELAY_PIN, OUTPUT);
    digitalWrite(RELAY_PIN, LOW);
    currentlyOn = 0;
    setup_wifi();
    mqtt_client.setServer(mqtt_server, 1883);
    mqtt_client.setCallback(mqtt_callback);
    init_mobile_arr(mobiles, NUMBER_OF_MOBILES);
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
        mqtt_client.publish("PortableThermostat/info/static/hello", msg);
    }

    if(now - lastCheck > 2000){
        lastCheck=now;
        
        //invecchiamento
        for(int i=0; i < NUMBER_OF_MOBILES; i++){
            if(mobiles[i] != -1 && now - last_updates[i] > AGING_THRESHOLD){
                mobiles[i] = -1;
            }
        }
        
        bool turnOn = false;
        for(int i=0; i < NUMBER_OF_MOBILES; i++){
            if(mobiles[i] == 1){
                turnOn = true;
                break;
            }
        }
        if(turnOn == true){
            digitalWrite(RELAY_PIN, HIGH);
            currentlyOn = 1;
        } else {
            digitalWrite(RELAY_PIN, LOW);
            currentlyOn = 0;
        }
        char buf[10];
        mqtt_client.publish(infotopic, itoa(currentlyOn, buf, 10));
    }









    
}
