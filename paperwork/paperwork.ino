#include <WiFi.h>
#include <WebServer.h>

const char* ssid = "ESP32_AP";
const char* password = "12345678";

WebServer server(80);

uint8_t uartBuffer[5] = {0}; // Инициализируем нулями
volatile bool newDataReady = false;

const char htmlPage[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Метеостанция</title>
    <style>
        body {
            font-family: Arial, sans-serif;
            background: #1e3c72;
            color: white;
            padding: 20px;
            margin: 0;
        }
        .data-container {
            max-width: 600px;
            margin: 0 auto;
            background: rgba(255,255,255,0.1);
            padding: 20px;
            border-radius: 10px;
        }
        h1 {
            text-align: center;
            margin-bottom: 30px;
        }
        .data-item {
            margin-bottom: 15px;
            font-size: 18px;
        }
        .value {
            font-weight: bold;
            color: #4CAF50;
        }
    </style>
    <script>
    function updateData() {
        fetch('/data')
            .then(response => response.json())
            .then(data => {
                document.getElementById('temp').textContent = data.temp;
                document.getElementById('press').textContent = data.press;
                document.getElementById('humid').textContent = data.humid;
                setTimeout(updateData, 1000);
            });
    }
    window.onload = updateData;
    </script>
</head>
<body>
    <div class="data-container">
        <h1>Данные датчиков</h1>
        <div class="data-item">
            Температура: <span id="temp" class="value">0</span> °C
        </div>
        <div class="data-item">
            Давление: <span id="press" class="value">0</span> Pa
        </div>
        <div class="data-item">
            Влажность: <span id="humid" class="value">0</span> %
        </div>
    </div>
</body>
</html>
)rawliteral";

void handleRoot() {
  server.send_P(200, "text/html", htmlPage);
}

void handleData() {
  String json = "{";
  json += "\"temp\":" + String(uartBuffer[1]);
  json += ",\"press\":" + String(uartBuffer[2]);
  json += ",\"humid\":" + String(uartBuffer[3]);
  json += "}";
  server.send(200, "application/json", json);
}

void setup() {
  Serial.begin(115200); // Для отладки
  Serial1.begin(115200, SERIAL_8N1, 16, 17); // UART2 на GPIO16(RX), GPIO17(TX)
  
  WiFi.softAP(ssid, password);
  
  Serial.println();
  Serial.print("AP IP address: ");
  Serial.println(WiFi.softAPIP());

  server.on("/", handleRoot);
  server.on("/data", handleData);
  server.begin();
  Serial.println("HTTP server started");
}

void loop() {
  server.handleClient();
  
  static uint8_t byteIndex = 0;
  static bool packetStarted = false;
  static uint32_t lastByteTime = 0;
  
  while(Serial1.available() || (millis() - lastByteTime < 50)) {
    if(Serial1.available()) {
      uint8_t incomingByte = Serial1.read();
      lastByteTime = millis();
      
      if(!packetStarted && incomingByte == 160) {
        packetStarted = true;
        byteIndex = 0;
        uartBuffer[byteIndex++] = incomingByte;
      }
      else if(packetStarted) {
        uartBuffer[byteIndex++] = incomingByte;
        
        if(byteIndex >= 5) {
          packetStarted = false;
          if(uartBuffer[4] == 254) {
            newDataReady = true;
            Serial.print("Получено: ");
            Serial.print(uartBuffer[1]); Serial.print(" C, ");
            Serial.print(uartBuffer[2]); Serial.print(" Pa, ");
            Serial.print(uartBuffer[3]); Serial.println(" %");
          }
          byteIndex = 0;
        }
      }
    }
  }
  
  // Сброс состояния, если пакет не получен полностью
  if(packetStarted && (millis() - lastByteTime > 100)) {
    packetStarted = false;
    byteIndex = 0;
    Serial.println("Таймаут, сброс состояния приема");
  }
}
