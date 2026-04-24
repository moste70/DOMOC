# DomoC — Nodo THERMO_BUNK (Termostato letto a castello)

---

## Descrizione

Il nodo `THERMO_BUNK` è un termostato locale dedicato al letto a castello. Gestisce autonomamente la valvola dell'aria calda in base alla temperatura impostata e rilevata nella zona letto a castello.

Funzioni principali:
- **Controllo valvola aria calda**: attiva/disattiva la valvola in base al confronto tra temperatura letta e temperatura impostata.
- **Rilevamento temperatura locale**: sensore digitale (es. DS18B20, SHT31) posizionato nel letto a castello.
- **Visualizzazione locale**: piccolo display a basso consumo (es. OLED I2C 0.96") che mostra temperatura letta e temperatura impostata.
- **Gestione pulsanti**: 3 pulsanti fisici per:
  - Temp Up (aumenta setpoint)
  - Temp Down (diminuisce setpoint)
  - On/Off (abilita/disabilita gestione locale)
- **Gestione termostato locale/centrale**: se il termostato locale è "Off", la gestione della valvola passa al termostato centralizzato (es. HMI o nodo ROOT).
- **Pubblicazione dati sulla mesh**: stato valvola, temperatura letta, temperatura impostata e stato locale/centrale sono pubblicati periodicamente sulla mesh per visualizzazione e log.

---

## Controllo luci letto a castello

Oltre alla gestione della valvola aria calda, il nodo THERMO_BUNK controlla anche le luci del letto a castello:

- **Due luci indipendenti**: una per il letto alto e una per il letto basso, ciascuna comandata da un'uscita dedicata (relay o MOSFET).
- **Comando locale e remoto**:
  - Le luci possono essere accese/spente sia tramite pulsanti fisici sul nodo (uno per ogni luce) sia tramite comandi inviati dall'HMI.
  - Lo stato delle luci viene pubblicato sulla mesh e visualizzato in tempo reale sull'HMI.
- **Pulsanti aggiuntivi**: il nodo dispone quindi di 5 pulsanti totali:
  - Temp Up
  - Temp Down
  - On/Off termostato locale/centrale
  - Luce letto alto (toggle)
  - Luce letto basso (toggle)
- **Logica**:
  - Ogni pressione dei pulsanti luce commuta lo stato ON/OFF della rispettiva luce.
  - I comandi ricevuti via mesh dall'HMI hanno la precedenza e aggiornano lo stato locale.

> In questo modo il comfort e il controllo locale sono massimi, ma l'utente può sempre intervenire anche da remoto tramite HMI.

---

## Hardware

- **ESP32-C3** (basso consumo, Wi-Fi mesh)
- **Sensore temperatura**: DS18B20 (1-Wire) o SHT31 (I2C)
- **Display**: OLED I2C 0.96" (128x32 o 128x64, consumo <20mA)
- **Pulsanti**: 3 (Temp Up, Temp Down, On/Off)
- **Relay/MOSFET**: per attuazione valvola aria calda
- **Alimentazione**: 12V bus camper → buck 3.3V

---

## Logica di funzionamento

1. **Modalità locale ON**:
   - L'utente imposta la temperatura desiderata tramite i pulsanti.
   - Il nodo legge la temperatura ambiente.
   - Se la temperatura letta < setpoint - hysteresis → attiva la valvola (apre aria calda).
   - Se la temperatura letta > setpoint + hysteresis → disattiva la valvola.
   - Stato e valori sono mostrati sul display e pubblicati sulla mesh.

2. **Modalità locale OFF**:
   - Il nodo ignora i pulsanti Temp Up/Down e non attua la valvola e la apre.
   - Riceve eventuali comandi dal termostato centralizzato (es. HMI o ROOT) via mesh.
   - Stato e valori sono comunque visualizzati sul display.

3. **Gestione pulsanti**:
   - Temp Up/Down: incrementano/decrementano il setpoint (es. step 0.5°C, range 15–30°C).
   - On/Off: commuta tra gestione locale e centralizzata (con feedback visivo su display).

4. **Pubblicazione mesh**:
   - Ogni variazione di stato/setpoint/temperatura viene pubblicata sulla mesh (MSG_STATUS, MSG_ALERT).
   - L'HMI può visualizzare e, se abilitato, modificare il setpoint da remoto.

---

## Display consigliato

Per il nodo THERMO_BUNK è consigliabile utilizzare un display touch screen di piccole dimensioni (max 3"), ad esempio un modulo TFT o OLED touch SPI/I2C. Il touch permette di gestire la regolazione della temperatura, il controllo delle luci e tutte le funzioni direttamente da interfaccia grafica, eliminando la necessità di pulsanti fisici.

- **Vantaggi**:
  - Interazione più intuitiva e moderna
  - Possibilità di visualizzare più informazioni e controlli su un'unica schermata compatta
  - Riduzione del cablaggio e dei componenti meccanici

> Esempi: display TFT touch SPI (es. 2.4"–2.8" ILI9341), OLED touch, moduli Nextion compatti, ecc.

---

## Display consigliato per questo nodo

Per questo nodo si consiglia l'uso di un display compatto ESP32 touch da circa 3 pollici, ad esempio:

- **Modello:** CrowPanel 3.2" ESP32 LCD IPS Touch Display
- **Risoluzione:** 240x320 pixel
- **Touch:** Capacitivo
- **Compatibilità:** Arduino IDE, ESP-IDF, PlatformIO, MicroPython, LVGL
- **Connettività:** WiFi, Bluetooth integrati
- **Alimentazione:** 5V (USB o batteria)

Questa soluzione offre le stesse funzionalità software e di sviluppo della versione 7" HMI, ma in formato ridotto, ideale per installazione in spazi compatti come il letto a castello.

---

## Esempio schermata display

```
T: 19.5°C  [ON]
Set: 21.0°C
Valvola: ON
```

- [ON]/[OFF]: modalità locale attiva/disattiva
- T: temperatura letta
- Set: temperatura impostata
- Valvola: stato attuale attuatore

---

## Sicurezza e fallback

- In caso di perdita mesh, il nodo continua a funzionare in modalità locale con l'ultimo setpoint.
- In caso di errore sensore, la valvola viene disattivata e viene pubblicato un alert sulla mesh.

---

## Vantaggi

- Comfort personalizzato per il letto a castello
- Basso consumo energetico
- Integrazione trasparente con la logica centralizzata del sistema DomoC

---

## Interazione esclusivamente touch

Nel nodo THERMO_BUNK, tutti i controlli (temperatura, valvola, luci) sono gestiti esclusivamente tramite il display touch screen: **non sono previsti pulsanti fisici**. Tutte le funzioni sono accessibili da interfaccia grafica.

- Dopo 1 minuto di inutilizzo, il display si spegne automaticamente per ridurre i consumi.
- Il display si riattiva al primo tocco.

> Questa soluzione semplifica il cablaggio, migliora l'estetica e rende l'interazione più intuitiva.
