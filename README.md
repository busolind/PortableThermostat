# Portable Thermostat
Sistema di termostati portatili che consente un controllo preciso del riscaldamento domestico, da presentare come progetto del corso di [Sistemi Embedded 2019/2020](https://gitlab.di.unimi.it/sistemiembedded/2019-2020) [[Edizione corrente](https://gitlab.di.unimi.it/sistemiembedded/2020-2021)]

## Introduzione
L'idea alla base del progetto deriva da una personale esperienza: la rottura e successiva sostituzione del termostato di casa. Tale apparato si è sempre trovato in salotto e finché sono stati utilizzati esclusivamente i termosifoni per il riscaldamento la posizione non ha mai creato problemi.\
Da quando possediamo una stufa a pellet, anch'essa situata in salotto, la faccenda si è complicata, in quanto la temperatura rilevata dal termostato è improvvisamente diventata insignificante ai fini del riscaldamento di tutto il resto della casa.\
Le camere da letto si trovano al lato opposto dell'appartamento e sono ormai le uniche stanze in cui si utilizzano ancora i termosifoni. Nelle attuali condizioni, il termostato è usato semplicemente come interruttore temporizzato, che alterna tra target di 24°C e di 4°C.

## Descrizione
Il sistema è composto da un'unica unità fissa, il cui compito è quello di agire sui controlli fisici della pompa del circuito caloriferi tramite un relé, e una o più unità mobili, che svolgono l'effettiva funzione di termostato e possono essere piazzate nei luoghi in cui è effettivamente utile misurare la temperatura.\
Ciascuna unità mobile ha un ID settato all'avvio e tre pulsanti: aumento/diminuzione della temperatura e on/off.

## Componentistica
Per l'unità fissa si ha:

 - Wemos D1 mini (ESP8266)
 - Relé

Per le unità mobili si ha:

- Wemos D1 mini (ESP8266)
- Termometro DHT11 (Sarebbe forse meglio un DHT22)
- Display OLED SSD1306 128x32
- Tre pulsanti
- Tre resistenze pull-down da 10kΩ+

## Funzionamento
Per un corretto funzionamento il sistema richiede uno o più access point WiFi e una rete locale con un server DHCP e un broker MQTT in funzione. Al momento SSID e password della rete wifi e IP del broker sono specificati nel codice e serve un flash per modificarli.\
Per quanto riguarda l'unità fissa è sufficiente collegarla all'alimentazione e farà tutto da sola.
Le unità mobili all'avvio si connettono alla rete WiFi e richiedono di impostare un ID mediante i pulsanti + e -, la conferma avviene mediante il pulsante on/off. È importante che tutte le unità mobili abbiano un ID differente.\
Una volta impostato l'ID, l'unità entrerà nella normale modalità di funzionamento.

Il fatto che le unità siano più di una e il circuito del riscaldamento sia unico implica che è sufficiente che una stanza sia sotto la temperatura target per attivare tutti i caloriferi. Questo può essere mitigato mediante l'utilizzo di valvole termostatiche.

## Features principali
- Aumento/diminuzione temperatura target
- Accensione/spegnimento termostato (continua a mostrare la temperatura)
- Riconnessione automatica alla rete WiFi e al server MQTT
- Se l'unità fissa non riceve alcuna comunicazione da un'unità mobile per più di 5 minuti la "dimentica"
- Le unità mobili pubblicano info MQTT su temperatura e umidità misurate e target
- Le unità mobili permettono di impostare l'ID contestualmente all'accensione
