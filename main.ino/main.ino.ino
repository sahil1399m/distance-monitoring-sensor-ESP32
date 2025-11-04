#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ESP32Servo.h>
#include <WiFi.h>
#include <WebServer.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// Pin definitions
#define trigPin 18
#define echoPin 19
#define greenLED 27
#define yellowLED 26
#define redLED 25
#define buzzerPin 4
#define servoPin 23

// Wi-Fi credentials
const char* ssid = "vivo V30e";
const char* password = "12345678"; // Replace with your hotspot password

WebServer server(80);
Servo myServo;

// Global variables for current measurements
float currentDistanceCM = 0.0;
float currentDistanceIN = 0.0;
int currentAngle = 0;

// Buzzer timing variables
unsigned long lastBuzzerChange = 0;
bool buzzerState = LOW;
bool buzzerActive = false;
unsigned long buzzerOnDuration = 0;
unsigned long buzzerOffDuration = 0;

// For non-blocking timing
unsigned long lastMeasurement = 0;
const unsigned long measurementInterval = 100; // ms

void setup() {
  Serial.begin(115200);

  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  pinMode(greenLED, OUTPUT);
  pinMode(yellowLED, OUTPUT);
  pinMode(redLED, OUTPUT);
  pinMode(buzzerPin, OUTPUT);

  myServo.attach(servoPin);
  myServo.write(0);
  currentAngle = 0;

  // OLED setup
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    while (true);
  }
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.println("Distance");
  display.display();
  delay(2000);

  // WiFi connect
  WiFi.begin(ssid, password);
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
    attempts++;
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("IP: ");
  display.println(WiFi.localIP());
  display.display();
  delay(3000);

  // CSS endpoint
  server.on("/style.css", []() {
    String css = R"rawliteral(
      body {
        font-family: 'Arial', 'Helvetica', sans-serif;
        background: linear-gradient(to right, #1e3c72, #2a5298);
        color: #fff;
        display: flex;
        flex-direction: column;
        align-items: center;
        justify-content: center;
        min-height: 100vh;
        margin: 0;
        padding: 20px;
        box-sizing: border-box;
        transition: all 0.3s ease;
      }
      body.dark-mode {
        background: linear-gradient(to right, #121212, #1f1f1f);
        color: #ddd;
      }
      h1 {
        font-size: 2.5em;
        margin-bottom: 20px;
        text-shadow: 2px 2px 4px rgba(0, 0, 0, 0.5);
      }
      .container {
        background: rgba(255, 255, 255, 0.15);
        padding: 25px;
        border-radius: 20px;
        box-shadow: 0 6px 12px rgba(0, 0, 0, 0.4);
        text-align: center;
        width: 90%;
        max-width: 600px;
        backdrop-filter: blur(5px);
        transition: all 0.3s ease;
      }
      body.dark-mode .container {
        background: rgba(255, 255, 255, 0.05);
      }
      p {
        font-size: 1.5em;
        margin: 10px 0;
        padding: 12px;
        background: rgba(255, 255, 255, 0.2);
        border-radius: 10px;
        box-shadow: 0 2px 4px rgba(0, 0, 0, 0.2);
        transition: all 0.3s ease;
      }
      body.dark-mode p {
        background: rgba(255, 255, 255, 0.1);
      }
      .chart-container {
        position: relative;
        margin-top: 20px;
        background: rgba(255, 255, 255, 0.95);
        border-radius: 15px;
        padding: 15px;
        box-shadow: 0 4px 8px rgba(0, 0, 0, 0.3);
        max-width: 100%;
        overflow: hidden;
        transition: all 0.3s ease;
      }
      body.dark-mode .chart-container {
        background: rgba(0, 0, 0, 0.9);
      }
      canvas {
        max-width: 100%;
        height: auto;
        border-radius: 10px;
      }
      .slider {
        width: 90%;
        margin: 10px 0 24px 0;
        accent-color: #4CAF50;
      }
      .servo-label {
        font-size: 1.2em;
        color: #03A9F4;
        margin-bottom: 20px;
      }
      .button-container {
        margin-top: 20px;
        display: flex;
        flex-wrap: wrap;
        justify-content: center;
        gap: 10px;
      }
      button {
        padding: 10px 20px;
        font-size: 1em;
        border: none;
        border-radius: 8px;
        background-color: #4CAF50;
        color: white;
        cursor: pointer;
        transition: background-color 0.3s ease;
      }
      button:hover {
        background-color: #45a049;
      }
      #pauseButton.paused {
        background-color: #f44336;
      }
      #pauseButton.paused:hover {
        background-color: #e53935;
      }
      #themeButton {
        background-color: #2196F3;
      }
      #themeButton:hover {
        background-color: #1e88e5;
      }
      #resetButton {
        background-color: #ff9800;
      }
      #resetButton:hover {
        background-color: #fb8c00;
      }
      .log-container {
        margin-top: 20px;
        background: rgba(255, 255, 255, 0.15);
        padding: 15px;
        border-radius: 10px;
        max-height: 150px;
        overflow-y: auto;
        transition: all 0.3s ease;
      }
      body.dark-mode .log-container {
        background: rgba(255, 255, 255, 0.05);
      }
      .log-container h3 {
        margin: 0 0 10px 0;
        font-size: 1.2em;
      }
      .log-container p {
        font-size: 1em;
        margin: 5px 0;
        padding: 5px;
      }
    )rawliteral";
    server.send(200, "text/css", css);
  });

  // Main web page
  server.on("/", []() {
    String html = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>ESP32 Distance Monitor</title>
  <link rel="stylesheet" href="/style.css">
  <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
</head>
<body>
  <div class="container">
    <h1>ESP32 Distance Monitor</h1>
    <p id="distanceCM">Distance (cm): --</p>
    <p id="distanceIN">Distance (in): --</p>
    <div>
      <label for="servoSlider" class="servo-label">Servo Angle: <span id="angleVal">0</span>&deg;</label>
      <input type="range" min="0" max="180" value="0" id="servoSlider" class="slider" oninput="updateAngle(this.value)">
    </div>
    <div class="chart-container">
      <canvas id="distanceChart"></canvas>
    </div>
    <div class="button-container">
      <button id="resetButton" onclick="resetChart()">Reset Graph</button>
      <button id="pauseButton" onclick="togglePause()">Pause Updates</button>
      <button id="themeButton" onclick="toggleTheme()">Toggle Dark Mode</button>
    </div>
    <div class="log-container">
      <h3>Data Log</h3>
      <div id="dataLog"></div>
    </div>
  </div>
  <script>
    let distancesCM = [];
    let distancesIN = [];
    let labels = [];
    let maxPoints = 20;
    let isPaused = false;
    let dataLog = [];
    let chart;
    let angle = 0;

    function fetchData() {
      if (isPaused) return;
      fetch('/data')
        .then(response => response.json())
        .then(data => {
          let cm = parseFloat(data.distanceCM);
          let inch = parseFloat(data.distanceIN);
          angle = data.angle;
          document.getElementById('distanceCM').innerText = `Distance (cm): ${cm.toFixed(1)}`;
          document.getElementById('distanceIN').innerText = `Distance (in): ${inch.toFixed(1)}`;
          document.getElementById('servoSlider').value = angle;
          document.getElementById('angleVal').innerText = angle;
          if (labels.length >= maxPoints) { labels.shift(); distancesCM.shift(); distancesIN.shift(); }
          labels.push('');
          distancesCM.push(cm);
          distancesIN.push(inch);
          chart.data.labels = labels;
          chart.data.datasets[0].data = distancesCM;
          chart.data.datasets[1].data = distancesIN;
          chart.update();
          // Data log
          const logEntry = `Angle: ${angle}&deg;, ${cm.toFixed(1)} cm / ${inch.toFixed(1)} in`;
          dataLog.unshift(logEntry);
          if (dataLog.length > 10) dataLog.pop();
          document.getElementById('dataLog').innerHTML = dataLog.map(entry => `<p>${entry}</p>`).join('');
        });
    }
    function updateAngle(val) {
      document.getElementById('angleVal').innerText = val;
      fetch('/set_angle?angle=' + val);
    }
    function resetChart() {
      labels = []; distancesCM = []; distancesIN = [];
      chart.data.labels = labels;
      chart.data.datasets[0].data = distancesCM;
      chart.data.datasets[1].data = distancesIN;
      chart.update();
      dataLog = [];
      document.getElementById('dataLog').innerHTML = '';
    }
    function togglePause() {
      isPaused = !isPaused;
      const pauseButton = document.getElementById('pauseButton');
      pauseButton.innerText = isPaused ? 'Resume Updates' : 'Pause Updates';
      pauseButton.classList.toggle('paused');
    }
    function toggleTheme() { document.body.classList.toggle('dark-mode'); }
    window.onload = function() {
      let ctx = document.getElementById('distanceChart').getContext('2d');
      chart = new Chart(ctx, {
        type: 'line',
        data: {
          labels: labels,
          datasets: [
            { label: 'Distance (cm)', data: distancesCM, borderColor: '#00ff00', backgroundColor: 'rgba(0,255,0,0.3)', fill: true, tension: 0.3, borderWidth: 4, pointRadius: 3 },
            { label: 'Distance (in)', data: distancesIN, borderColor: '#ff00ff', backgroundColor: 'rgba(255,0,255,0.3)', fill: true, tension: 0.3, borderWidth: 4, pointRadius: 3 }
          ]
        },
        options: { animation: false, scales: { y: { beginAtZero: true, max: 200 } }, plugins: { legend: { labels: { color: '#333', font: { size: 14 } } } } }
      });
      setInterval(fetchData, 500);
      fetchData();
    }
  </script>
</body>
</html>
)rawliteral";
    server.send(200, "text/html", html);
  });

  // Endpoint to set servo angle
  server.on("/set_angle", []() {
    if (server.hasArg("angle")) {
      int newAngle = server.arg("angle").toInt();
      if (newAngle >= 0 && newAngle <= 180) {
        currentAngle = newAngle;
        myServo.write(currentAngle);
      }
    }
    server.send(200, "text/plain", "OK");
  });

  // Data endpoint
  server.on("/data", []() {
    String json = "{\"distanceCM\":";
    json += String(currentDistanceCM, 1);
    json += ",\"distanceIN\":";
    json += String(currentDistanceIN, 1);
    json += ",\"angle\":";
    json += String(currentAngle);
    json += "}";
    server.send(200, "application/json", json);
  });

  server.begin();
  Serial.println("Web server started");
}

void loop() {
  server.handleClient();

  unsigned long currentMillis = millis();

  // Measure distance every 100ms
  if (currentMillis - lastMeasurement >= measurementInterval) {
    long duration;
    float distanceCM, distanceIN;

    digitalWrite(trigPin, LOW);
    delayMicroseconds(2);
    digitalWrite(trigPin, HIGH);
    delayMicroseconds(10);
    digitalWrite(trigPin, LOW);

    duration = pulseIn(echoPin, HIGH, 30000);
    distanceCM = duration * 0.0343 / 2;
    distanceIN = duration * 0.0133 / 2;

    if (duration <= 0 || duration > 100000 || isnan(distanceCM) || distanceCM > 1000) {
      distanceCM = 0.0;
      distanceIN = 0.0;
    }

    currentDistanceCM = distanceCM;
    currentDistanceIN = distanceIN;

    // OLED display
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.print("Angle: ");
    display.print(currentAngle);
    display.print((char)223);

    display.setCursor(0, 15);
    display.print("Dist (cm): ");
    display.print(distanceCM, 1);

    display.setCursor(0, 30);
    display.print("Dist (in): ");
    display.print(distanceIN, 1);

    display.display();

    // LED Logic
    if (distanceCM > 40) {
      digitalWrite(greenLED, HIGH);
      digitalWrite(yellowLED, LOW);
      digitalWrite(redLED, LOW);
    } else if (distanceCM > 15 && distanceCM <= 40) {
      digitalWrite(greenLED, LOW);
      digitalWrite(yellowLED, HIGH);
      digitalWrite(redLED, LOW);
    } else {
      digitalWrite(greenLED, LOW);
      digitalWrite(yellowLED, LOW);
      digitalWrite(redLED, HIGH);
    }

    // Buzzer Logic (non-blocking)
    if (distanceCM > 0 && distanceCM <= 15) {
      if (!buzzerActive || buzzerOnDuration != 50) {
        buzzerActive = true;
        buzzerOnDuration = 50;
        buzzerOffDuration = 50;
        lastBuzzerChange = currentMillis;
        buzzerState = HIGH;
        digitalWrite(buzzerPin, buzzerState);
      }
    } else if (distanceCM > 15 && distanceCM <= 40) {
      if (!buzzerActive || buzzerOnDuration != 200) {
        buzzerActive = true;
        buzzerOnDuration = 200;
        buzzerOffDuration = 200;
        lastBuzzerChange = currentMillis;
        buzzerState = HIGH;
        digitalWrite(buzzerPin, buzzerState);
      }
    } else {
      buzzerActive = false;
      digitalWrite(buzzerPin, LOW);
    }

    lastMeasurement = currentMillis;
  }

  // Handle buzzer timing (non-blocking)
  unsigned long currentMillis2 = millis();
  if (buzzerActive && (currentMillis2 - lastBuzzerChange >= (buzzerState ? buzzerOnDuration : buzzerOffDuration))) {
    buzzerState = !buzzerState;
    digitalWrite(buzzerPin, buzzerState);
    lastBuzzerChange = currentMillis2;
  }
}