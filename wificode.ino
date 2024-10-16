// Importation des bibliotheques
#include <ESPmDNS.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

// definition des informqtions de connection WiFi
const char* ssid = "ARIEL-PC";
const char* password = "azerty1234";

// declaration des objets serveurs et evenements
AsyncWebServer server(80);
AsyncEventSource events("/events");

// declaration des broches
const int relayPins[5] = {2, 4, 16, 17, 18};  // Broches pour les relais
const int buttonPins[5] = {32, 25, 26, 27, 14};  // Broches pour les boutons poussoirs

// declaration de la structure device et tableau des dispositifs
struct Device {
  int id;
  int pin;
  int btnPin;
  int prevBtnState;
  bool status;
};

Device devices[5];
int numDevices = 0;

//fonction de traitement des variables pour le modele html

String processor(const String& var) {
  if (var.startsWith("btn") && var.endsWith("txt")) {
    int index = var[3] - '1';  // Extract the index from the var name
    return devices[index].status ? "OFF" : "ON";
  }
  if (var.startsWith("btn") && var.endsWith("class")) {
    int index = var[3] - '1';
    return devices[index].status ? "button2" : "button";
  }
  return String();
}
// page html pour la configuration initiale

const char setup_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <meta name='viewport' content='width=device-width, initial-scale=1'>
  <title>ESP32 Setup</title>
  <style>
    body { font-family: Arial, sans-serif; background-color: #f0f0f0; display: flex; flex-direction: column; align-items: center; justify-content: center; height: 100vh; margin: 0; }
    h1 { color: #333; }
    form { background: white; padding: 20px; border-radius: 5px; box-shadow: 0 2px 4px rgba(0, 0, 0, 0.1); }
    input[type='number'], input[type='submit'] { padding: 10px; margin: 5px 0; width: 100%; box-sizing: border-box; }
    input[type='submit'] { background-color: #4CAF50; color: white; border: none; cursor: pointer; }
    input[type='submit']:hover { background-color: #45a049; }
  </style>
</head>
<body>
  <h1>ESP32 Setup</h1>
  <form action="/setup" method="get">
    <label for="numRelays">Enter the number of relays (1-5):</label><br>
    <input type="number" id="numRelays" name="numRelays" min="1" max="5"><br>
    <input type="submit" value="Submit">
  </form>
</body>
</html>)rawliteral";


//generation dynamique de la page principal html

String generateIndexHtml() {
  String html = "<!DOCTYPE HTML><html><head><meta name='viewport' content='width=device-width, initial-scale=1'><title>ESP32 Home Automation</title><style>body { font-family: Arial, sans-serif; background-color: #f0f0f0; margin: 0; display: flex; flex-direction: column; align-items: center; justify-content: center; height: 100vh; } h1 { color: #333; } .container { display: flex; flex-wrap: wrap; justify-content: center; } .relay { background: white; padding: 20px; margin: 10px; border-radius: 5px; box-shadow: 0 2px 4px rgba(0, 0, 0, 0.1); text-align: center; } .button { background-color: mediumseagreen; border: none; color: white; padding: 10px 15px; text-decoration: none; font-size: 20px; cursor: pointer; margin: 10px; border-radius: 5px; } .button2 { background-color: gray; border: none; color: white; padding: 10px 15px; text-decoration: none; font-size: 20px; cursor: pointer; margin: 10px; border-radius: 5px; }</style></head><body><h1>ESP32 SMART Home!</h1><div class='container'>";

  for (int i = 0; i < numDevices; i++) {
    html += "<div class='relay'><h3>Relay " + String(i + 1) + " State</h3>";
    html += "<p><a href='/toggle?id=" + String(i) + "'><button id='btn" + String(i + 1) + "' class='%btn" + String(i + 1) + "class%'>%btn" + String(i + 1) + "txt%</button></a></p></div>";
  }

  html += "</div><script>if (!!window.EventSource) { var source = new EventSource('/events'); source.addEventListener('toggleState', function(e) { let jsonData = JSON.parse(e.data); let element = document.getElementById('btn' + (jsonData.id + 1)); if (jsonData.status) { element.innerHTML = 'OFF'; element.className = 'button2'; } else { element.innerHTML = 'ON'; element.className = 'button'; } }, false); }</script></body></html>";
  return html;
}

void setup() {
  Serial.begin(115200); // initiation de la communicarion serie
  
  
  // connexion au reseau wifi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }

  Serial.println("Connected to WiFi");
  
  // configuration des routes du serveur web
  //  La route racine / pour la page de configuration initiale.
  // La route /setup pour configurer le nombre de relais.
  // La route /toggle pour basculer l'état d'un relais donné.

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (numDevices == 0){
      request->send_P(200, "text/html", setup_html);
    }
    else{
      String html = generateIndexHtml();
      AsyncWebServerResponse *response = request->beginResponse(200, "text/html", html);
      response->addHeader("Cache-Control", "no-store");
      request->send(response);
    }
  });  

  server.on("/setup", HTTP_GET, [](AsyncWebServerRequest *request) {
      if(numDevices == 0){
        if (request->hasParam("numRelays")) {
        numDevices = request->getParam("numRelays")->value().toInt();
        for (int i = 0; i < numDevices; i++) {
          devices[i] = { i, relayPins[i], buttonPins[i], HIGH, false };
          pinMode(devices[i].pin, OUTPUT);
          digitalWrite(devices[i].pin, LOW);
          pinMode(devices[i].btnPin, INPUT_PULLUP);
        }
        String html = generateIndexHtml();
        AsyncWebServerResponse *response = request->beginResponse(200, "text/html", html);
        response->addHeader("Cache-Control", "no-store");
        request->send(response);
        } else {
          request->send(400, "text/plain", "Missing numRelays parameter");
        }
      }
    });
  
  // Handle the toggle request
  server.on("/toggle", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (request->hasParam("id")) {
      int id = request->getParam("id")->value().toInt();
      if (id >= 0 && id < numDevices) {
        devices[id].status = !devices[id].status;
        digitalWrite(devices[id].pin, devices[id].status ? HIGH : LOW);
        String data = "{\"id\":" + String(devices[id].id) + ", \"status\":" + String(devices[id].status) + "}";
        events.send(data.c_str(), "toggleState", millis());
      }
    }
    String html = generateIndexHtml();
    AsyncWebServerResponse *response = request->beginResponse(200, "text/html", html);
    response->addHeader("Cache-Control", "no-store");
    request->send(response);
  });

  // configuration des evenements asynchrones 
  events.onConnect([](AsyncEventSourceClient *client) {
    if (client->lastId()) {
      Serial.printf("Client reconnected! Last message ID that it got is: %u\n", client->lastId());
    }
    client->send("hello!", NULL, millis(), 10000);
  });

  server.addHandler(&events);
  
  
  // demarrage du serveur
  server.begin();
  Serial.println("HTTP server started");
}

void loop() {
  for (int i = 0; i < numDevices; i++) {
    int buttonState = digitalRead(devices[i].btnPin);
    int buttonPreviousState = devices[i].prevBtnState;

    if (buttonState == buttonPreviousState) {
    }
    else{
      devices[i].status = !devices[i].status;
      digitalWrite(devices[i].pin, devices[i].status ? HIGH : LOW);
      delay(50);  // Debounce delay
      devices[i].prevBtnState = buttonState;
    }
    String data = "{\"id\":" + String(devices[i].id) + ", \"status\":" + String(devices[i].status) + "}";
    events.send(data.c_str(), "toggleState", millis());
  }
  delay(10);
}
