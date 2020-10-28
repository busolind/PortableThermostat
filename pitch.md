# Introduzione
L'idea alla base del progetto deriva da una personale esperienza: la rottura e successiva sostituzione del termostato di casa. Tale apparato si è sempre trovato in salotto e finché sono stati utilizzati esclusivamente i termosifoni per il riscaldamento la posizione non ha mai creato problemi.\
Da quando possediamo una stufa a pellet, anch'essa situata in salotto, la faccenda si è complicata, in quanto la temperatura rilevata dal termostato è improvvisamente diventata insignificante ai fini del riscaldamento di tutto il resto della casa.\
Le camere da letto si trovano al lato opposto dell'appartamento e sono ormai le uniche stanze in cui si utilizzano ancora i termosifoni. Nelle attuali condizioni, il termostato è usato semplicemente come interruttore temporizzato, che alterna tra target di 24°C e di 4°C.
# Obiettivo
L'obiettivo è quello di poter spostare il termostato in qualunque punto della casa si voglia utilizzare come riferimento di temperatura senza stravolgere l'impianto elettrico,
lasciando quindi i fili di controllo al loro posto.\
Vorrei dunque realizzare un sistema composto da un'unità fissa, posizionata dove attualmente c'è il normale termostato, che possa agire sul comando della pompa, e un'unità portatile, attraverso la quale si può accendere o spegnere il riscaldamento e impostare la temperatura desiderata mediante tasti "+" e "-".\
Non è da escludere la possibilità che ci sia più di un'unità portatile, per fare in modo che due o più stanze non comunicanti possano essere contemporaneamente usate come punti di riferimento per avere un riscaldamento più omogeneo.
# Materiali
Per l'unità fissa è sufficiente un microcontrollore con funzionalità di rete e un relé per attivare e disattivare la pompa dei termosifoni.\
Ciascuna delle unità portatili avrà un altro microcontrollore, un termometro, un display per visualizzare la temperatura attuale e/o quella impostata come target, un alloggiamento per le batterie al fine da garantire una maggiore libertà di posizionamento e tre pulsanti: due per incremento/decremento della temperatura target e uno per inserire/disinserire il sistema.\
Tutte le unità saranno connesse alla rete WiFi di casa e comunicheranno tramite MQTT.
Per la gestione della temperatura verrà implementato un controllo PID.
