#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include "MAX30105.h"
#include "heartRate.h"
#include <OneWire.h>
#include <DallasTemperature.h>
#include <HardwareSerial.h>

HardwareSerial SerialPort(0);
// WiFi info
const char* ssid = "Ademm";
const char* password = "25676360";

// Capteurs
MAX30105 particleSensor;
#define ONE_WIRE_BUS 4
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

// Web server
WebServer server(80);

// Données patient
String nom = "Ali Ben Salah";
int age = 27;
String coordonnees = "Monastir, Tunisie";
int bp1=0;
int i=0;
// Mesures
float bpm = 0;
float spo2 = 0;
float temperature = 0;
unsigned long lastBeat = 0;
unsigned long lastUpdate = 0;

// Image intégrée (petit cœur)
const char* imgBase64 = "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAoAAAAKCAYAAACNMs+9AAAAIUlEQVQYV2NkYGD4z0AEYBxVSFGBgYH4z8DAwMiAIIIEAACGiBN/P+Z95wAAAABJRU5ErkJggg==";

String getHTML() {
  String html = R"rawliteral(
<!DOCTYPE html><html><head><meta charset='UTF-8'>
<title>Patient Monitor</title>
<meta name="viewport" content="width=device-width, initial-scale=1">
<meta http-equiv='refresh' content='5'>
<style>
  body {
    font-family: Arial;
    background: #f0f8ff;
    color: #333;
    text-align: center;
    transition: all 0.3s ease;
  }
  @media (prefers-color-scheme: dark) {
    body { background: #121212; color: #eee; }
    .card { background: #1e1e1e; }
  }
  .card {
    background: white;
    border-radius: 15px;
    box-shadow: 0 4px 10px rgba(0,0,0,0.2);
    padding: 20px;
    max-width: 500px;
    margin: 50px auto;
    transition: background 0.3s;
  }
  img { width: 100px; }
  h1 { color: #007acc; }
  .data { font-size: 1.2em; margin: 10px 0; }
  button {
    background-color: #007acc;
    color: white;
    border: none;
    padding: 10px 15px;
    border-radius: 10px;
    cursor: pointer;
    margin-top: 10px;
  }
</style>
</head><body>
<div class='card' id="card">
  <div class='data'>🖼️<br><img src=')rawliteral" + String(imgBase64) + R"rawliteral(' alt='heart'/></div>
  <h1>Surveillance Patient</h1>
  <div class='data'>👤 <strong>Nom:</strong> )rawliteral" + nom + R"rawliteral(</div>
  <div class='data'>🎂 <strong>Âge:</strong> )rawliteral" + String(age) + R"rawliteral(</div>
  <div class='data'>💓 <strong>BPM:</strong> <span id="bpm">)rawliteral" + String(bpm, 1) + R"rawliteral(</span></div>
  <div class='data'>🩸 <strong>SpO₂:</strong> <span id="spo2">)rawliteral" + String(spo2, 1) + R"rawliteral(</span> %</div>
  <div class='data'>🌡️ <strong>Température:</strong> <span id="temp">)rawliteral" + String(temperature, 1) + R"rawliteral(</span> °C</div>
  <div class='data'>📍 <strong>Coordonnées:</strong> )rawliteral" + coordonnees + R"rawliteral(</div>
  <div class='data' style='font-size:0.9em;color:gray;'>📅 Dernière mise à jour: )rawliteral" + String(millis() / 1000) + R"rawliteral( s</div>
  <button onclick="exportPDF()">📄 Télécharger PDF</button>
  <canvas id="chart" width="400" height="200"></canvas>
</div>

<script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
<script>
  let bpmList = [], spo2List = [], tempList = [], labels = [];

  function updateChart() {
    if (bpmList.length > 20) {
      bpmList.shift(); spo2List.shift(); tempList.shift(); labels.shift();
    }
    bpmList.push(parseFloat(document.getElementById("bpm").innerText));
    spo2List.push(parseFloat(document.getElementById("spo2").innerText));
    tempList.push(parseFloat(document.getElementById("temp").innerText));
    labels.push(new Date().toLocaleTimeString());

    chart.data.labels = labels;
    chart.data.datasets[0].data = bpmList;
    chart.data.datasets[1].data = spo2List;
    chart.data.datasets[2].data = tempList;
    chart.update();
  }

  function exportPDF() {
    window.print();
  }

  const ctx = document.getElementById('chart').getContext('2d');
  const chart = new Chart(ctx, {
    type: 'line',
    data: {
      labels: [],
      datasets: [
        { label: '💓 BPM', data: [], borderColor: 'red', fill: false },
        { label: '🩸 SpO₂ %', data: [], borderColor: 'blue', fill: false },
        { label: '🌡️ °C', data: [], borderColor: 'green', fill: false }
      ]
    },
    options: {
      responsive: true,
      plugins: { legend: { position: 'top' } },
      scales: {
        y: { beginAtZero: false }
      }
    }
  });

  setInterval(updateChart, 5000); // update every refresh
</script>
</body></html>
)rawliteral";

  return html;
}
int read_temp(void){
    if (Serial.available() >= 2) 
    {  // Attendre 2 octets
        uint8_t highByte = Serial.read();
        uint8_t lowByte = Serial.read();
        int16_t valeur = (highByte << 8) | lowByte;  // Reconstituer l'entier

        return valeur;
    }
}
void setup() {
    // Initialisation de l'UART
  Serial.begin(115200);  // Correspond au baudrate du STM32
    while (!Serial) {
    delay(10);
    }

  // Connexion WiFi
  Serial.println("Connexion au WiFi...");
  WiFi.begin(ssid, password);

  int tries = 0;
  while (WiFi.status() != WL_CONNECTED && tries < 20) {
    delay(500);
    Serial.print(".");
    tries++;
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("\nImpossible de se connecter au WiFi !");
    return;
  }

  Serial.println("\nConnecté !");
  Serial.print("IP locale : ");
  Serial.println(WiFi.localIP());

  // Capteur MAX30102
  if (!particleSensor.begin(Wire, I2C_SPEED_STANDARD)) {
    Serial.println("MAX30102 non détecté !");
    while (1);
  }

  particleSensor.setup(60, 4, 2, 100, 411, 4096);
  particleSensor.setPulseAmplitudeRed(0x0A);
  particleSensor.setPulseAmplitudeIR(0x0A);

  // Temp capteur
  sensors.begin();

  // Web route
  server.on("/", []() {
    server.send(200, "text/html", getHTML());
  });

  server.begin();
}


void loop() {
  server.handleClient();

  long irValue = particleSensor.getIR();
  long redValue = particleSensor.getRed();

  // Calcul BPM
  if (irValue > 15000) {
        i=0;
      int irValue1 =irValue;
    lastBeat = millis();
    delay(20);
    while(i<100){
    irValue = particleSensor.getIR();

     if (irValue > 15000 && irValue<irValue1){
          long delta = millis() - lastBeat;


          float beatsPerMinute = 60 / (delta / 1000.0);
              if (beatsPerMinute > 20 && beatsPerMinute < 220) {
      bpm = beatsPerMinute;
    }
          break;
     }
     delay(10);
     i++;
    }
    
    

  }
       bp1 =bpm;




  // SpO2 estimation (simplifiée)

  if (redValue > 10000 && irValue > 10000) {
    spo2 = 100.0 - abs(redValue - irValue) * 0.001;
  }

  int bp2 = bp1 + 2000;
  Serial.write((uint8_t*)&bp2, sizeof(bp2)); // Envoyer les 4 octets de l'entier
  delay(500); // Attendre 1 seconde avant de renvoyer



  int spo = spo2 + 1000;
  Serial.write((uint8_t*)&spo, sizeof(spo)); // Envoyer les 4 octets de l'entier
  delay(100); // Attendre 1 seconde avant de renvoyer

  // Température



  temperature = read_temp() ;

  lastUpdate = millis();
  delay(1000);
}
