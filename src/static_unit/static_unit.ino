#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <TaskScheduler.h>

const char *ssid = "IOT_TEST";
const char *password = "IOT_TEST";
const char *mqtt_server = "192.168.178.10";

unsigned long last_reconnect_attempt = 0;
#define RECONNECT_DELAY 5000
const char *cmdtopic = "PortableThermostat/cmd/mobile";
const char *infotopic = "PortableThermostat/info/static/currentlyOn";

WiFiClient espClient;
PubSubClient mqtt_client(espClient);
#define MSG_BUFFER_SIZE (50)
char msg[MSG_BUFFER_SIZE];

#define TOPIC_BUFFER_SIZE (150)
char topic_buffer[TOPIC_BUFFER_SIZE];

bool turnOn = false;
short currentlyOn = -1;

#define RELAY_PIN D5
#define NUMBER_OF_MOBILES (5)
short mobiles[NUMBER_OF_MOBILES]; //Memorizza lo stato dei termostati: -1 se unknown, 0 se non richiedono riscaldamento, 1 se lo richiedono
unsigned long last_updates[NUMBER_OF_MOBILES]; //Usato per "invecchiamento" degli stati dei termostati. Se un termostato non comunica per più di un certo numero di ms si resetta il suo stato a unknown
#define AGING_THRESHOLD 300000

Scheduler ts;

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

    if(index < NUMBER_OF_MOBILES){
        mobiles[index] = (payload[0] - '0');
        last_updates[index] = millis();
    }
    delay(50);
    digitalWrite(BUILTIN_LED, HIGH);
}

void mqtt_subscribe_to_mobiles(){
    snprintf(topic_buffer, TOPIC_BUFFER_SIZE, "%s/+/turnOn", cmdtopic);
    Serial.print("Subscribed to topic [");
    Serial.print(topic_buffer);
    Serial.println("]");
    mqtt_client.subscribe(topic_buffer);
}

void mqtt_reconnect() {
    digitalWrite(BUILTIN_LED, LOW);
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (mqtt_client.connect(clientId.c_str())) {
        Serial.println("connected");
        digitalWrite(BUILTIN_LED, HIGH);
        // ... and resubscribe
        mqtt_subscribe_to_mobiles();
    } else {
        Serial.print("failed, rc=");
        Serial.println(mqtt_client.state());
    }
}
Task mqtt_reconnect_task(RECONNECT_DELAY * TASK_MILLISECOND, TASK_FOREVER, mqtt_reconnect);

//Inizializza l'array dei mobiles a -1
void init_mobile_arr(short mobilearr[], int mobilecount){
    for(int i=0; i<mobilecount; i++){
        mobilearr[i]=-1;
    }
}

void check_mobiles_aging(){
    //invecchiamento
    unsigned long now = millis();
    for(int i=0; i < NUMBER_OF_MOBILES; i++){
        if(mobiles[i] != -1 && now - last_updates[i] > AGING_THRESHOLD){
            mobiles[i] = -1;
        }
    }
}
Task check_mobiles_aging_task(TASK_SECOND, TASK_FOREVER, check_mobiles_aging);

void update_turnOn(){
    turnOn = false;
    for(int i=0; i < NUMBER_OF_MOBILES; i++){
        if(mobiles[i] == 1){
            turnOn = true;
            break;
        }
    }
}
Task update_turnOn_task(TASK_IMMEDIATE, TASK_FOREVER, update_turnOn);

void handle_relay(){
    if(turnOn == true){
        digitalWrite(RELAY_PIN, HIGH);
        currentlyOn = 1;
    } else {
        digitalWrite(RELAY_PIN, LOW);
        currentlyOn = 0;
    }
}
Task handle_relay_task(TASK_IMMEDIATE, TASK_FOREVER, handle_relay);

void publish_currentlyOn(){
    itoa(currentlyOn, msg, 10);
    Serial.print("Publish message [" + String(infotopic) + "]: ");
    Serial.println(msg);
    mqtt_client.publish(infotopic, msg);
}
Task publish_currentlyOn_task(10 * TASK_SECOND, TASK_FOREVER, publish_currentlyOn);


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

    ts.addTask(mqtt_reconnect_task);
    ts.addTask(check_mobiles_aging_task);
    ts.addTask(update_turnOn_task);
    ts.addTask(handle_relay_task);
    ts.addTask(publish_currentlyOn_task);
    check_mobiles_aging_task.enable();
    update_turnOn_task.enable();
    handle_relay_task.enable();
}

void loop() {
    unsigned long now = millis();
    if (!mqtt_client.connected()) {
        mqtt_reconnect_task.enableIfNot();
        publish_currentlyOn_task.disable();
    } else {
        mqtt_reconnect_task.disable();
        publish_currentlyOn_task.enableIfNot();
    }
    mqtt_client.loop();

    ts.execute();
}
