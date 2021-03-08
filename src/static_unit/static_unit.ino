#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <PubSubClientTools.h>
#include <TaskScheduler.h>

const char *ssid = "IOT_TEST";
const char *password = "IOT_TEST";
const char *mqtt_server = "192.168.178.10";

#define MQTT_RECONNECT_DELAY 5000
#define RELAY_PIN D5
#define NUMBER_OF_MOBILES (5)
#define AGING_THRESHOLD 300000

WiFiClient espClient;
PubSubClient mqtt_client(mqtt_server, 1883, espClient);
PubSubClientTools mqtt_tools(mqtt_client);
String cmdtopic_mobile_base = "PortableThermostat/cmd/mobile";
String infotopic = "PortableThermostat/info/static/currentlyOn";

Scheduler ts;

bool turnOn = false;
short currentlyOn = -1;
int mobiles[NUMBER_OF_MOBILES]; //Memorizza lo stato dei termostati: -1 se unknown, 0 se non richiedono riscaldamento, 1 se lo richiedono
unsigned long last_updates[NUMBER_OF_MOBILES]; //Usato per "invecchiamento" degli stati dei termostati. Se un termostato non comunica per più di un certo numero di ms si resetta il suo stato a unknown

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

void mqtt_callback(String topic, String message) {
    Serial.println("Message arrived [" + topic + "]: " + message);
    digitalWrite(BUILTIN_LED, LOW);
    int index = topic.substring(cmdtopic_mobile_base.length()+1, topic.indexOf('/', cmdtopic_mobile_base.length()+1)).toInt();

    if(index < NUMBER_OF_MOBILES){
        mobiles[index] = message.toInt();
        last_updates[index] = millis();
    }
    digitalWrite(BUILTIN_LED, HIGH);
}

void mqtt_subscribe_to_mobiles(){
    String subTo = cmdtopic_mobile_base + "/+/turnOn";
    Serial.println("Subscribed to topic [" + subTo + "]");
    mqtt_tools.subscribe(subTo, mqtt_callback);
}

void mqtt_reconnect() {
    digitalWrite(BUILTIN_LED, LOW);
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (mqtt_client.connect("ESP8266Client-PT-static")) {
        Serial.println("connected");
        digitalWrite(BUILTIN_LED, HIGH);
        // ... and resubscribe
        mqtt_subscribe_to_mobiles();
    } else {
        Serial.print("failed, rc=");
        Serial.println(mqtt_client.state());
    }
}
Task mqtt_reconnect_task(MQTT_RECONNECT_DELAY * TASK_MILLISECOND, TASK_FOREVER, mqtt_reconnect);

//Inizializza l'array dei mobiles a -1
void init_mobile_arr(int mobilearr[], int mobilecount){
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
    String pub = String(currentlyOn);
    Serial.println("Publish message [" + infotopic + "]: " + pub);
    mqtt_tools.publish(infotopic, pub);
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
