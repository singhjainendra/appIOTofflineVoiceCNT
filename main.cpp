#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>
#include "AdafruitIO_WiFi.h"

#define WIFI_SSID       "Selfuse"
#define WIFI_PASS       "savedata"

#define IO_USERNAME     "singhjay98"
#define IO_KEY          "aio_rdzi465E75eYnjpnHOcIibSVKO7S"


#define DOOR_PIN        D1
#define LIGHT_PIN       D2
#define FAN_PIN         D5

AdafruitIO_WiFi io(IO_USERNAME, IO_KEY, WIFI_SSID, WIFI_PASS);
ESP8266WebServer server(80);
WebSocketsServer webSocket(81);

AdafruitIO_Feed *doorFeed = io.feed("door");
AdafruitIO_Feed *lightFeed = io.feed("light");
AdafruitIO_Feed *fanFeed = io.feed("fan");

bool doorState = false;
bool lightState = false;
bool fanState = false;
const char webpage[] PROGMEM = R"=====(
<!DOCTYPE html>
<html>
<head>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>ESP Control Panel</title>
    <style>
        body {
            font-family: Arial, sans-serif;
            text-align: center;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            min-height: 100vh;
            margin: 0;
            padding: 20px;
        }
        
        .container {
            background: rgba(255, 255, 255, 0.95);
            max-width: 400px;
            margin: 40px auto;
            padding: 30px;
            border-radius: 20px;
            box-shadow: 0 10px 30px rgba(0,0,0,0.2);
        }
        
        h1 {
            color: #333;
            margin-bottom: 30px;
            font-size: 28px;
        }
        
        .status {
            background: #f8f9fa;
            padding: 15px;
            border-radius: 10px;
            margin: 20px 0;
            font-size: 14px;
            color: #666;
        }
        
        .control-group {
            margin: 25px 0;
        }
        
        .device-name {
            font-size: 18px;
            font-weight: bold;
            margin-bottom: 10px;
            color: #444;
        }
        
        .btn-group {
            display: flex;
            gap: 10px;
        }
        
        .btn {
            flex: 1;
            padding: 15px;
            border: none;
            border-radius: 10px;
            font-size: 16px;
            font-weight: bold;
            cursor: pointer;
            transition: all 0.3s ease;
        }
        
        .btn-on {
            background: #2ecc71;
            color: white;
        }
        
        .btn-off {
            background: #e74c3c;
            color: white;
        }
        
        .btn:hover {
            transform: translateY(-2px);
            box-shadow: 0 5px 15px rgba(0,0,0,0.1);
        }
        
        .btn:active {
            transform: translateY(0);
        }
        
        .btn-active {
            opacity: 1;
            box-shadow: 0 0 0 3px rgba(52, 152, 219, 0.5);
        }
        
        .btn-inactive {
            opacity: 0.7;
        }
        
        .connection-status {
            margin-top: 20px;
            padding: 10px;
            border-radius: 5px;
            font-size: 14px;
        }
        
        .connected {
            background: #d4edda;
            color: #155724;
        }
        
        .disconnected {
            background: #f8d7da;
            color: #721c24;
        }
        
        .loading {
            color: #666;
            animation: pulse 1.5s infinite;
        }
        
        @keyframes pulse {
            0% { opacity: 1; }
            50% { opacity: 0.5; }
            100% { opacity: 1; }
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>Smart Home Control</h1>
        
        <div class="status" id="connectionStatus">
            <span id="wifiStatus">WiFi: Connecting...</span><br>
            <span id="cloudStatus">Cloud: Connecting...</span><br>
            <span id="webSocketStatus">WebSocket: Connecting...</span>
        </div>
        
        <div class="control-group">
            <div class="device-name">Door Control</div>
            <div class="btn-group">
                <button class="btn btn-on btn-inactive" onclick="sendCommand('door', 1)">ON</button>
                <button class="btn btn-off btn-active" onclick="sendCommand('door', 0)">OFF</button>
            </div>
        </div>
        
        <div class="control-group">
            <div class="device-name">Light Control</div>
            <div class="btn-group">
                <button class="btn btn-on btn-inactive" onclick="sendCommand('light', 1)">ON</button>
                <button class="btn btn-off btn-active" onclick="sendCommand('light', 0)">OFF</button>
            </div>
        </div>
        
        <div class="control-group">
            <div class="device-name">Fan Control</div>
            <div class="btn-group">
                <button class="btn btn-on btn-inactive" onclick="sendCommand('fan', 1)">ON</button>
                <button class="btn btn-off btn-active" onclick="sendCommand('fan', 0)">OFF</button>
            </div>
        </div>
        
        <div class="connection-status" id="syncStatus">
            Last sync: Never
        </div>
    </div>

    <script>
        let ws;
        let connectionCheckInterval;
        
        function initWebSocket() {
            const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
            const wsUrl = `${protocol}//${window.location.hostname}:81`;
            
            ws = new WebSocket(wsUrl);
            
            ws.onopen = function() {
                console.log('WebSocket connected');
                updateStatus('webSocketStatus', 'WebSocket: Connected', 'connected');
                updateConnectionStatus();
            };
            
            ws.onclose = function() {
                console.log('WebSocket disconnected');
                updateStatus('webSocketStatus', 'WebSocket: Disconnected', 'disconnected');
                updateConnectionStatus();
                // Try to reconnect after 3 seconds
                setTimeout(initWebSocket, 3000);
            };
            
            ws.onerror = function(error) {
                console.error('WebSocket error:', error);
                updateStatus('webSocketStatus', 'WebSocket: Error', 'disconnected');
            };
            
            ws.onmessage = function(event) {
                try {
                    const data = JSON.parse(event.data);
                    console.log('Received:', data);
                    
                    // Update button states
                    updateButtonState('door', data.door);
                    updateButtonState('light', data.light);
                    updateButtonState('fan', data.fan);
                    
                    // Update sync time
                    const now = new Date();
                    document.getElementById('syncStatus').innerHTML = 
                        `Last sync: ${now.toLocaleTimeString()}`;
                        
                    // Update connection status
                    if (data.cloudConnected !== undefined) {
                        updateStatus('cloudStatus', 
                            data.cloudConnected ? 'Cloud: Connected' : 'Cloud: Disconnected',
                            data.cloudConnected ? 'connected' : 'disconnected');
                    }
                } catch (e) {
                    console.error('Error parsing WebSocket message:', e);
                }
            };
        }
        
        function sendCommand(device, state) {
            if (ws && ws.readyState === WebSocket.OPEN) {
                const command = {
                    type: 'control',
                    device: device,
                    state: state
                };
                ws.send(JSON.stringify(command));
                console.log('Sent:', command);
            } else {
                alert('WebSocket not connected. Please wait...');
            }
        }
        
        function updateButtonState(device, state) {
            const onBtn = document.querySelector(`[onclick="sendCommand('${device}', 1)"]`);
            const offBtn = document.querySelector(`[onclick="sendCommand('${device}', 0)"]`);
            
            if (state == 1) {
                onBtn.classList.remove('btn-inactive');
                onBtn.classList.add('btn-active');
                offBtn.classList.remove('btn-active');
                offBtn.classList.add('btn-inactive');
            } else {
                offBtn.classList.remove('btn-inactive');
                offBtn.classList.add('btn-active');
                onBtn.classList.remove('btn-active');
                onBtn.classList.add('btn-inactive');
            }
        }
        
        function updateStatus(elementId, text, className) {
            const element = document.getElementById(elementId);
            element.textContent = text;
            element.className = className;
        }
        
        function updateConnectionStatus() {
            const wifiConnected = navigator.onLine;
            updateStatus('wifiStatus', 
                wifiConnected ? 'WiFi: Connected' : 'WiFi: Disconnected',
                wifiConnected ? 'connected' : 'disconnected');
        }
        
        // Check WiFi connection every 5 seconds
        function startConnectionCheck() {
            updateConnectionStatus();
            connectionCheckInterval = setInterval(updateConnectionStatus, 5000);
        }
        
        // Initialize everything when page loads
        window.addEventListener('load', function() {
            initWebSocket();
            startConnectionCheck();
            
            // Listen for WiFi status changes
            window.addEventListener('online', updateConnectionStatus);
            window.addEventListener('offline', updateConnectionStatus);
        });
    </script>
</body>
</html>
)=====";
void handleRoot() {
  server.send_P(200, "text/html", webpage);
}

void handleStatus() {
  StaticJsonDocument<200> doc;
  doc["door"] = doorState;
  doc["light"] = lightState;
  doc["fan"] = fanState;
  doc["cloudConnected"] = (io.status() == AIO_CONNECTED);
  
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}
void webSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length) {
  switch(type) {
    case WStype_DISCONNECTED:
      Serial.printf("[%u] Disconnected!\n", num);
      break;
      
    case WStype_CONNECTED:
      {
        IPAddress ip = webSocket.remoteIP(num);
        Serial.printf("[%u] Connected from %d.%d.%d.%d\n", num, ip[0], ip[1], ip[2], ip[3]);
        
        // Send current state to new client
        StaticJsonDocument<200> doc;
        doc["door"] = doorState;
        doc["light"] = lightState;
        doc["fan"] = fanState;
        doc["cloudConnected"] = (io.status() == AIO_CONNECTED);
        
        String response;
        serializeJson(doc, response);
        webSocket.sendTXT(num, response);
      }
      break;
      
    case WStype_TEXT:
      {
        Serial.printf("[%u] Received: %s\n", num, payload);
        
        StaticJsonDocument<200> doc;
        DeserializationError error = deserializeJson(doc, payload);
        
        if (!error) {
          String type = doc["type"];
          String device = doc["device"];
          int state = doc["state"];
          
          if (type == "control") {
            // Handle control command from browser
            if (device == "door") {
              doorState = state;
              digitalWrite(DOOR_PIN, doorState);
              doorFeed->save(doorState);
            } 
            else if (device == "light") {
              lightState = state;
              digitalWrite(LIGHT_PIN, lightState);
              lightFeed->save(lightState);
            } 
            else if (device == "fan") {
              fanState = state;
              digitalWrite(FAN_PIN, fanState);
              fanFeed->save(fanState);
            }
            
            // Broadcast update to all connected clients
            StaticJsonDocument<200> updateDoc;
            updateDoc["door"] = doorState;
            updateDoc["light"] = lightState;
            updateDoc["fan"] = fanState;
            updateDoc["cloudConnected"] = (io.status() == AIO_CONNECTED);
            
            String updateResponse;
            serializeJson(updateDoc, updateResponse);
            webSocket.broadcastTXT(updateResponse);
          }
        }
      }
      break;
  }
}
void handleDoorMessage(AdafruitIO_Data *data) {
  doorState = data->toInt();
  digitalWrite(DOOR_PIN, doorState);
  Serial.printf("Cloud → Door: %d\n", doorState);
  
  // Broadcast to all web clients
  StaticJsonDocument<200> doc;
  doc["door"] = doorState;
  doc["light"] = lightState;
  doc["fan"] = fanState;
  doc["cloudConnected"] = true;
  
  String response;
  serializeJson(doc, response);
  webSocket.broadcastTXT(response);
}

void handleLightMessage(AdafruitIO_Data *data) {
  lightState = data->toInt();
  digitalWrite(LIGHT_PIN, lightState);
  Serial.printf("Cloud → Light: %d\n", lightState);
  
  StaticJsonDocument<200> doc;
  doc["door"] = doorState;
  doc["light"] = lightState;
  doc["fan"] = fanState;
  doc["cloudConnected"] = true;
  
  String response;
  serializeJson(doc, response);
  webSocket.broadcastTXT(response);
}

void handleFanMessage(AdafruitIO_Data *data) {
  fanState = data->toInt();
  digitalWrite(FAN_PIN, fanState);
  Serial.printf("Cloud → Fan: %d\n", fanState);
  
  StaticJsonDocument<200> doc;
  doc["door"] = doorState;
  doc["light"] = lightState;
  doc["fan"] = fanState;
  doc["cloudConnected"] = true;
  
  String response;
  serializeJson(doc, response);
  webSocket.broadcastTXT(response);
}
void setup() {
  Serial.begin(115200);
  delay(100);
  
  Serial.println("\n\n=== Smart Home Control System ===");
  
  // Initialize pins
  pinMode(DOOR_PIN, OUTPUT);
  pinMode(LIGHT_PIN, OUTPUT);
  pinMode(FAN_PIN, OUTPUT);
  
  // Set initial states
  digitalWrite(DOOR_PIN, LOW);
  digitalWrite(LIGHT_PIN, LOW);
  digitalWrite(FAN_PIN, LOW);
  
  // Connect to WiFi
  Serial.print("Connecting to WiFi: ");
  Serial.println(WIFI_SSID);
  
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println("\nWiFi connected!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  
  // Connect to Adafruit IO
  Serial.print("Connecting to Adafruit IO...");
  io.connect();
  
  // Set up message handlers for Adafruit IO
  doorFeed->onMessage(handleDoorMessage);
  lightFeed->onMessage(handleLightMessage);
  fanFeed->onMessage(handleFanMessage);
  
  // Wait for Adafruit IO connection
  while(io.status() < AIO_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  
  Serial.println("\nConnected to Adafruit IO!");
  
  // Initialize WebSocket server
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
  
  // Setup Web Server routes
  server.on("/", handleRoot);
  server.on("/status", handleStatus);
  
  // Start Web Server
  server.begin();
  
  Serial.println("HTTP server started on port 80");
  Serial.println("WebSocket server started on port 81");
  Serial.println("System ready!");
}
void loop() {
  // Handle Adafruit IO (Cloud communication)
  io.run();
  
  // Handle HTTP requests
  server.handleClient();
  
  // Handle WebSocket events
  webSocket.loop();
  
  // Small delay to prevent watchdog timer issues
  delay(10);
}