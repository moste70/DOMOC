# DomoC — Nodo THERMO_LOFT (Termostato mansarda)

---

## Descrizione

Il nodo `THERMO_LOFT` è un termostato locale dedicato alla zona mansarda. Come per il letto a castello, gestisce autonomamente la valvola dell'aria calda in base alla temperatura impostata e rilevata nella zona mansarda, e offre controllo locale tramite display e pulsanti.

Funzioni principali:
- **Controllo valvola aria calda**: attiva/disattiva la valvola in base al confronto tra temperatura letta e temperatura impostata.
- **Rilevamento temperatura locale**: sensore digitale (es. DS18B20, SHT31) posizionato in mansarda.
- **Visualizzazione locale**: piccolo display a basso consumo (es. OLED I2C 0.96") che mostra temperatura letta e temperatura impostata.
- **Gestione pulsanti**: 3 pulsanti fisici per:
  - Temp Up (aumenta setpoint)
  - Temp Down (diminuisce setpoint)
  - On/Off (abilita/disabilita gestione locale)
- **Gestione termostato locale/centrale**: se il termostato locale è "Off", la gestione della valvola passa al termostato centralizzato (es. HMI o nodo ROOT).
- **Pubblicazione dati sulla mesh**: stato valvola, temperatura letta, temperatura impostata e stato locale/centrale sono pubblicati periodicamente sulla mesh per visualizzazione e log.

---

## Controllo telecamere anteriori e posteriori

Il nodo THERMO_LOFT gestisce anche l'attivazione delle telecamere anteriori e posteriori del camper:

- **Uscite dedicate**: due uscite (relay/MOSFET) per alimentare le telecamere anteriore e posteriore.
- **Comando locale e remoto**:
  - Le telecamere possono essere attivate/disattivate sia tramite pulsanti fisici aggiuntivi sul nodo (uno per ogni telecamera) sia tramite comandi inviati dall'HMI.
  - Lo stato delle telecamere viene pubblicato sulla mesh e visualizzato in tempo reale sull'HMI.
- **Pulsanti aggiuntivi**: il nodo dispone quindi di 5 pulsanti totali:
  - Temp Up
  - Temp Down
  - On/Off termostato locale/centrale
  - Telecamera anteriore (toggle)
  - Telecamera posteriore (toggle)
- **Logica**:
  - Ogni pressione dei pulsanti telecamera commuta lo stato ON/OFF della rispettiva telecamera.
  - I comandi ricevuti via mesh dall'HMI hanno la precedenza e aggiornano lo stato locale.

---

## Display consigliato

Per il nodo THERMO_LOFT si consiglia l'uso di un display compatto ESP32 touch da circa 3 pollici, ad esempio:

- **Modello:** CrowPanel 3.2" ESP32 LCD IPS Touch Display
- **Risoluzione:** 240x320 pixel
- **Touch:** Capacitivo
- **Compatibilità:** Arduino IDE, ESP-IDF, PlatformIO, MicroPython, LVGL
- **Connettività:** WiFi, Bluetooth integrati
- **Alimentazione:** 5V (USB o batteria)

Questa soluzione offre le stesse funzionalità software e di sviluppo della versione 7" HMI, ma in formato ridotto, ideale per installazione in spazi compatti come la mansarda.

Per il nodo THERMO_LOFT è consigliabile utilizzare un display touch screen di dimensioni massime 5" (es. 3.5"–5"), preferibilmente con interfaccia SPI o I2C. Il touch permette di gestire sia la regolazione della temperatura che il controllo delle telecamere e delle altre funzioni direttamente da interfaccia grafica, semplificando l'uso e riducendo il numero di pulsanti fisici necessari.

- **Vantaggi**:
  - Interazione più intuitiva e moderna
  - Possibilità di visualizzare più informazioni e controlli su un'unica schermata
  - Riduzione del cablaggio e dei componenti meccanici

> Esempi: display TFT touch SPI (es. ILI9488 480x320), display capacitivo I2C, moduli Nextion, ecc.

---

## Esempio schermata display estesa

```
T: 20.0°C  [ON]
Set: 22.0°C
Valvola: ON
Cam Ant: ON
Cam Post: OFF
```

- I due stati telecamera sono sempre visibili sul display locale e sull'HMI.

---

## Sicurezza e fallback

- In caso di perdita mesh, il nodo continua a funzionare in modalità locale con l'ultimo setpoint e stato telecamere.
- In caso di errore sensore, la valvola viene disattivata e viene pubblicato un alert sulla mesh.

---

## Vantaggi

- Comfort personalizzato per la mansarda
- Controllo diretto e remoto delle telecamere
- Integrazione trasparente con la logica centralizzata del sistema DomoC

---

## Interazione esclusivamente touch

Nel nodo THERMO_LOFT, tutti i controlli (temperatura, valvola, telecamere) sono gestiti esclusivamente tramite il display touch screen: **non sono previsti pulsanti fisici**. Tutte le funzioni sono accessibili da interfaccia grafica.

- Dopo 1 minuto di inutilizzo, il display si spegne automaticamente per ridurre i consumi.
- Il display si riattiva al primo tocco.

> Questa soluzione semplifica il cablaggio, migliora l'estetica e rende l'interazione più intuitiva.
