# Il progetto
Alla base del progetto c'è un microcontrollore **STM32G030F6P6**, compatto e dal costo di qualche decina di centesimi, che si interfaccia con un modulo **ESP-01S** per la connessione a Internet. Il microcontrollore può pilotare quattro elettrovalvole assieme a quattro sensori di flusso dell'acqua, uno per ogni tubo. I sensori di flusso permettono di rilevare la presenza di problemi lungo le linee e le quattro valvole consentono di irrigare fino a quattro aree alla volta.

Il tutto è **controllabile da remoto tramite un'app Android**, con la quale si possono aprire o chiudere le valvole manualmente ma si può anche impostare una programmazione per l'apertura e chiusura automatica in base all'orario o al tempo atmosferico. Infatti, il microcontrollore effettua ogni 15 minuti una richiesta all'API Open-Meteo per **ottenere le previsioni meteo della giornata**: se è prevista pioggia, è inutile irrigare, quindi le valvole resteranno chiuse.
# Costruzione
## Hardware
### Circuito stampato
Ho iniziato con l'hardware. Ho sviluppato un circuito elettronico in KiCad in modo che l'STM potesse controllare le valvole e i sensori. Le valvole sono a 12 V e consumano ~600 mA di corrente, quindi ho usato MOSFET AO3400A per poterle controllare con l'STM (i cui pin hanno un output massimo di ~15 mA a 3,3 V). Pure i sensori di flusso operano a 12 V, quindi ho usato gli stessi MOSFET per poterli interfacciare col microcontrollore.

Ho poi stampato in 3D uno stencil per poter spalmare la pasta saldante sui vari pad dei componenti a montaggio superficiale (SMD). Questo perché ordinarlo assieme alle PCB sarebbe costato quasi 20 € in più, mentre così è gratis e il risultato è comunque accettabile. Quindi perché no?

Schema:

![schema](https://github.com/Kikkiu17/AutoIrrigator/blob/main/Immagini/autoirrigator_schema.png)

PCB:

![pcb](https://github.com/Kikkiu17/AutoIrrigator/blob/main/Immagini/autoirrigator_pcb.png)

Stencil fatto in casa

![stencil](https://github.com/Kikkiu17/AutoIrrigator/blob/main/Immagini/stencil.jpg)
![pcb](https://github.com/Kikkiu17/AutoIrrigator/blob/main/Immagini/pcb.jpg)

### Modello 3D
Mentre aspettavo l'arrivo delle PCB, ho iniziato a creare il modello 3D del progetto, con dei raccordi per le valvole e i sensori. Ho creato un design abbastanza compatto ma comunque semplice da assemblare, con il circuito stampato al centro in modo da poterci collegare tutti i fili comodamente.
![3d](https://github.com/Kikkiu17/AutoIrrigator/blob/main/Immagini/3d.png)
![valvole](https://github.com/Kikkiu17/AutoIrrigator/blob/main/Immagini/valvole.jpg)

## Software
### Firmware embedded
Dopo aver fatto i raccordi e i supporti, sono passato al software. Ho iniziato a scrivere il [mio driver](https://github.com/Kikkiu17/ESP-AT-STM32) per l'ESP-01S dato che non ne ho trovato nessuno online abbastanza semplice da usare compatibile con l'STM32. Il driver sfrutta il DMA dell'STM32 per poter ricevere i dati dell'ESP dalla porta seriale senza usare la CPU, aumentando quindi le prestazioni (in questo caso le prestazioni non sono critiche, ma fa comunque comodo). L'ESP viene connesso alla rete WiFi; viene impostato l'hostname `ESPDEVICE002` per poter essere riconosciuto nella rete; viene impostato il nome `Hub irrigazione`; viene connesso al server NTP in modo da poter ottenere la data e l'ora e infine l'ESP viene impostato in modalità `server` in modo da accettare connessioni remote sulla porta `23`. Poi, nel ciclo while principale, il firmware attende nuove richieste tramite la funzione `WIFI_ReceiveRequest` che dopo verranno gestite.

Grazie ai numerosi timer dell'STM32G030F6P6, ho dedicato tre timer ai quattro sensori di flusso (chiamati FLOW): TIM14 per FLOW1, TIM1 per FLOW2, TIM3 per FLOW3 e FLOW4. In questo modo ho potuto usare quattro pin fisici per ciascun sensore, invece che fare multiplexing coi quattro sensori, rendendo quindi tutto molto più semplice. Imposto i canali timer dei sensori in modalità `Input compare` e abilito gli interrupt per i timer, in modo da ottenere il periodo del segnale PWM dei sensori. Facendo `1/T`, dove T è il periodo, si ottiene la frequenza e dividendola per 7,5 si ottiene il flusso d'acqua in `litri/min`.

Infine ho scritto del codice per connettermi all'API di Open-meteo per ottenere le [previsioni per il giorno corrente](http://api.open-meteo.com/v1/forecast?latitude=45.5582&longitude=9.2149&hourly=precipitation,precipitation_probability&current=precipitation&timezone=auto&forecast_days=1) tramite la funzione `WEATHER_GetForecast`. Le previsioni vengono immagazzinate in degli array in modo da poter accedere facilmente ad esse.

### App Android
Per controllare il tutto si usa un'app Android, ESPValve. Volevo utilizzare l'app [ESPIOT](https://github.com/Kikkiu17/espiot) di un mio vecchio progetto, ma il modo in cui l'ho scritta rende difficile l'integrazione di nuovi dispositivi ad essa, quindi ho deciso di riscriverla da zero. La vecchia app richiedeva delle caratteristiche specifiche da chiedere ai dispositivi connessi, quindi per aggiungere, ad esempio, un pulsante, bisognava modificare in modo pesante sia l'applicazione che il dispositivo (in questo caso l'STM32 e l'ESP). La nuova app invece chiede ogni 250 ms un pacchetto di `feature` dal dispositivo con questa struttura: `tipoX:Nome,caratteristica_aggiuntiva:Nome:dato` oppure `tipoX:Nome:dato`. `tipo` è il tipo di caratteristica, quindi interruttore, bottone, sensore o timestamp e `caratteristica_aggiuntiva` può essere ad esempio `sensore`. In questo modo, posso creare il pacchetto di caratteristiche che mi interessano e l'app Android crea ogni bottone o sensore o altro col nome specificato e col dato specificato, in base alla struttura data. L'applicazione è un po' instabile a causa del modo in cui aggiorno i dati, data la mia inesperienza riguardo Android, ma è utilizzabile. 
