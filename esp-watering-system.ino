#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <LittleFS.h>
#include <stdlib.h>  // Nécessaire pour strtoul()

#define LED_PIN 12 //Watering valve

#define WATERING_DELAY (unsigned long)(12 * 3600000UL) // 12 heures en millisecondes 0.05 pour 3mn
#define MICRO_REPEAT 3             // Nombre de répétitions d'arrosage par cycle
#define MICRO_DELAY 4000           // Nombre de ms d'arrosage pour chaque repetition du cycle

#define LOOP_DELAY 1000            // En ms

// !!! A LIRE !!! La flash de esp8266 est donnée pour un max de 100 000 réecritures: survie de 1 mois si 2 ecritures toutes les 30s par exemple.
// Certes c'est pas la flash entière mais je vais tuer 1 secteur par mois a ce rythme.
// Je definis donc a 360, ce qui donne un enregistrement toute les 6 minutes, mais surtout, qui correspond a une usure de 1 secteur tout les ans.
// Sauf erreur de ma part la flash de mon esp8266(1Mo) contiens 256 secteur. Donc ca va.
#define SAVE_TIME_DELAY 360 * 1000

#define RESET_FLASH //pour reset la flash. Vous pouvez bien sur utiliser une entrée numerique pour executer cette fonction via un bouton par exemple.
#define SERIAL_DEBUG //pour envoyer des logs sur port serie

#define BAUDS 9600 //interet que si SERIAL_DEBUG est definit...

#define W_SSID ""
#define W_PASSWORD ""
#define W_TIMEOUT 30 * 1000UL //timeout en s
#define PORT 80

#define REFRESH_TIME_HTML_CLIENT 30 * 1000 //s .... attention a pas saturer le serv!!!!! risque de deny de service....

ESP8266WebServer server(PORT); // Serveur Web sur le port 80


unsigned long wateringCompleted = 0;
unsigned long lastWateringTime = 0; // Temps de millis() lors du dernier arrosage (millisecondes)
unsigned long lastSaveTime = 0; // Temps de millis() lors du dernier arrosage (millisecondes)


//storeNow=1 pour forcer
void storeTime(unsigned long elapsedTime, bool storeNow){
  unsigned long currentTime = millis();
  if ((unsigned long)(currentTime - lastSaveTime) >= SAVE_TIME_DELAY || storeNow) {
    if (LittleFS.begin()) {
      File file = LittleFS.open("/data.txt", "w");
      if (file) {
        file.printf("%lu\n%lu\n", wateringCompleted, elapsedTime);
        file.close();
      } else {
#ifdef SERIAL_DEBUG
        Serial.println("Failed to open file for writing.");
#endif
      }
      LittleFS.end();
    } else {
#ifdef SERIAL_DEBUG
      Serial.println("Failed to mount LittleFS.");
#endif
    }
    lastSaveTime = currentTime;
  }
}

void restoreTime(){
  unsigned long elapsedTime = 0;
  if (LittleFS.begin()) {
#ifdef RESET_FLASH
    LittleFS.format();  // Formater la mémoire flash
#endif
#ifndef RESET_FLASH
    File file = LittleFS.open("/data.txt", "r");
    if (file) {
      
      String line = file.readStringUntil('\n');
      wateringCompleted = strtoul(line.c_str(), NULL, 10); // Conversion de la chaîne en unsigned long
      
      line = file.readStringUntil('\n');
      elapsedTime = strtoul(line.c_str(), NULL, 10); // Conversion de la chaîne en unsigned long
      
      file.close();
      lastWateringTime = 0 - elapsedTime; //0=debut, pas millis() pour compter le boot du programme...
    } else {
#ifdef SERIAL_DEBUG
      Serial.println("Failed to open file for reading.");
#endif
    }
#endif
    LittleFS.end();
  } else {
#ifdef SERIAL_DEBUG
    Serial.println("Failed to mount LittleFS.");
#endif
    lastWateringTime=millis();
  }
}



void handleRoot() {
  // Initialisation des valeurs par défaut
  String nextWatering = "Waiting from server...";
  String lastWatering = "Waiting from server...";
  String wateringCycles = "Waiting from server...";

  // Calcul du temps restant avant le prochain arrosage
  unsigned long currentTime = millis();
  unsigned long elapsedTime = currentTime - lastWateringTime;

  unsigned long remainingTime = WATERING_DELAY - elapsedTime;
  unsigned long hours = remainingTime / 3600000;
  unsigned long minutes = (remainingTime % 3600000) / 60000;
  unsigned long seconds = (remainingTime % 60000) / 1000;

  unsigned long MaxH = WATERING_DELAY / 3600000;
  unsigned long MaxM = (WATERING_DELAY % 3600000) / 60000;
  unsigned long MaxS = (WATERING_DELAY % 60000) / 1000;
//  nextWatering = String(hours) + " hours, " + String(minutes) + " minutes, and " + String(seconds) + " seconds.";

  unsigned long remainingHours = elapsedTime / 3600000;
  unsigned long remainingMinutes = (elapsedTime % 3600000) / 60000;
  unsigned long remainingSeconds = elapsedTime % 60000 / 1000;
//  lastWatering = String(remainingHours) + " hours, " + String(remainingMinutes) + " minutes, and " + String(remainingSeconds) + " seconds ago.";

  // Création du HTML
  String html = "<!DOCTYPE html><html><head><meta charset=\"UTF-8\"><title>Esp8266 watering system</title>";  html += "<style>";
  html += "body { background-color: #1d1d1d; color: #b4ff9f; font-family: Arial, sans-serif; padding: 20px; margin: 0; }";
  html += "h1 { color: #00ff00; text-align: center; font-size: 36px; }";
  html += "p { font-size: 18px; line-height: 1.6; margin: 10px 0; }";
  html += "div.container { max-width: 800px; margin: 0 auto; background-color: #262626; padding: 20px; border-radius: 10px; box-shadow: 0px 4px 6px rgba(0, 0, 0, 0.6); }";
  html += "footer { text-align: center; margin-top: 20px; font-size: 14px; color: #888; }";
  html += "</style>";
  html += "<script>";
  
  // Variables locales
  html += "let h = " + String(hours) + ", m = " + String(minutes) + ", s = " + String(seconds) + ";";
  html += "let elapsedH = " + String(remainingHours) + ", elapsedM = " + String(remainingMinutes) + ", elapsedS = " + String(remainingSeconds) + ";";
  html += "let cycles = " + String(wateringCompleted) + ";";
  
  // Mise à jour locale
  html += "function updateCounters() {";
  html += "    elapsedS++; if (elapsedS > 59) { elapsedS = 0; elapsedM++; } if (elapsedM > 59) { elapsedM = 0; elapsedH++; }";
  html += "    s--; if (s < 0) { s = 59; m--; } if (m < 0) { m = 59; h--; }";
  html += "    if (h < 0) {";
  html += "      h = " + String(MaxH) + "; m = " + String(MaxM) + "; s = " + String(MaxS) + ";";
  html += "      cycles++;";
//  html += "      refreshFromServer();";
  html += "      elapsedS = 0; elapsedM = 0; elapsedH=0;";
  html += "      console.log('Reset: h = ' + h + ', m = ' + m + ', s = ' + s);";
  html += "      console.log('Reset: elapsedH = ' + elapsedH + ', elapsedM = ' + elapsedM + ', elapsedS = ' + elapsedS);";
  html += "    }";
  // Mise a jour des elements html
  html += "    document.getElementById('nextWatering').innerHTML = 'Next watering in: ' + h + ' hours, ' + m + ' minutes, and ' + s + ' seconds.';";
  html += "    document.getElementById('wateringCycles').innerHTML = 'Watering cycles completed: ' + cycles;";
  html += "    if (cycles == 0) {";
  html += "      document.getElementById('lastWatering').innerHTML = 'Last watering was: No watering has occurred yet.';";
  html += "    } else {";
  html += "      document.getElementById('lastWatering').innerHTML = 'Last watering was: ' + elapsedH + ' hours, ' + elapsedM + ' minutes, and ' + elapsedS + ' seconds ago.';";
  html += "    }";
  html += "}";
  
  html += "function refreshFromServer() {";
  html += "  fetch('/status')";
  html += "    .then(response => response.json())";
  html += "    .then(data => {";
  html += "      cycles = data.wateringCompleted;";
  html += "      document.getElementById('wateringCycles').innerHTML = 'Watering cycles completed: ' + cycles;";
  html += "      const timeParts = data.nextWatering.match(/(\\d+) hours, (\\d+) minutes, and (\\d+) seconds/);";
  html += "      if (timeParts) {";
  html += "        h = parseInt(timeParts[1]);";
  html += "        m = parseInt(timeParts[2]);";
  html += "        s = parseInt(timeParts[3]);";
  html += "      } else { h = 0; m = 0; s = 0; }";
  html += "      const elapsedParts = data.lastWatering.match(/(\\d+) hours, (\\d+) minutes, and (\\d+) seconds/);";
  html += "      if (elapsedParts) {";
  html += "        elapsedH = parseInt(elapsedParts[1]);";
  html += "        elapsedM = parseInt(elapsedParts[2]);";
  html += "        elapsedS = parseInt(elapsedParts[3]);";
  html += "      } else { elapsedH = 0; elapsedM = 0; elapsedS = 0; }";
  // Mises à jour des éléments HTML après avoir récupéré les données
  html += "      document.getElementById('nextWatering').innerHTML = 'Next watering in: ' + h + ' hours, ' + m + ' minutes, and ' + s + ' seconds.';";
  html += "      document.getElementById('wateringCycles').innerHTML = 'Watering cycles completed: ' + cycles;";
  html += "      if (cycles == 0) {";
  html += "        document.getElementById('lastWatering').innerHTML = 'Last watering was: No watering has occurred yet.';";
  html += "      } else {";
  html += "        document.getElementById('lastWatering').innerHTML = 'Last watering was: ' + elapsedH + ' hours, ' + elapsedM + ' minutes, and ' + elapsedS + ' seconds ago.';";
  html += "      }";
  html += "    })";
  html += "    .catch(error => console.error('Error fetching data:', error));";
  html += "}";
  
  // Intervalles
  html += "setInterval(updateCounters, 1000);";
  html += "setInterval(refreshFromServer, " + String(REFRESH_TIME_HTML_CLIENT) + ");";
  
  html += "</script>";
  html += "</head><body>";
  html += "<div class=\"container\">";
  html += "<h1>🐱 Esp8266 watering system 🐱</h1>";
  html += "<p id=\"wateringCycles\">Watering cycles completed: " + wateringCycles + "</p>";
  html += "<p id=\"nextWatering\">Next watering in: " + nextWatering + "</p>";
  html += "<p id=\"lastWatering\">Last watering was: " + lastWatering + "</p>";
  html += "</div>";
  html += "<footer>ESP8266 Watering System | Created with ❤️</footer>";
  html += "</body></html>";

  server.send(200, "text/html", html);
}





void handleStatus() {
  // Calcul du temps restant avant le prochain arrosage
  unsigned long currentTime = millis();
  unsigned long elapsedTime = currentTime - lastWateringTime;
  
  unsigned long remainingTime = WATERING_DELAY - elapsedTime;
  unsigned long hours = remainingTime / 3600000;
  unsigned long minutes = (remainingTime % 3600000) / 60000;
  unsigned long seconds = (remainingTime % 60000) / 1000;
  String nextWatering = String(hours) + " hours, " + String(minutes) + " minutes, and " + String(seconds) + " seconds.";
  
  // Temps écoulé depuis le dernier arrosage
  unsigned long remainingHours = elapsedTime / 3600000;
  unsigned long remainingMinutes = (elapsedTime % 3600000) / 60000;
  unsigned long remainingSeconds = elapsedTime % 60000 / 1000;
  String lastWatering = String(remainingHours) + " hours, " + String(remainingMinutes) + " minutes, and " + String(remainingSeconds) + " seconds ago.";

  // Création de l'objet JSON
  String json = "{";
  json += "\"wateringCompleted\": " + String(wateringCompleted) + ",";
  json += "\"nextWatering\": \"" + nextWatering + "\",";
  json += "\"lastWatering\": \"" + lastWatering + "\"";
  json += "}";
  server.send(200, "application/json", json);  // Renvoi du JSON
}




void watering() {
  for (int i = 0; i < MICRO_REPEAT; i++) {
#ifdef SERIAL_DEBUG
    Serial.print("Mini loop: ");
    Serial.println(i);
#endif
    digitalWrite(LED_PIN, HIGH);
#ifdef SERIAL_DEBUG
    Serial.println("ON");
#endif
    delay(MICRO_DELAY);
    digitalWrite(LED_PIN, LOW);
#ifdef SERIAL_DEBUG
    Serial.println("OFF");
#endif
    delay(MICRO_DELAY);
  }
}


int setup_wifi(const char* ssid, const char* password) {
  // Connexion WiFi
  WiFi.begin(ssid, password);
  unsigned long startAttemptTime = millis(); // Enregistre le temps de début de la tentative
#ifdef SERIAL_DEBUG
    Serial.print("Wifi searching: ");
#endif
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
#ifdef SERIAL_DEBUG
    Serial.print("#");
#endif
    if (millis() - startAttemptTime > W_TIMEOUT) { // Timeout
#ifdef SERIAL_DEBUG
      Serial.println("\nWiFi connection failed. Timeout reached.");
#endif
      return 1;
    }
  }
#ifdef SERIAL_DEBUG
  // Récupération et affichage de l'adresse IP
  Serial.println("\nWiFi connected.");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  // Récupération et affichage de l'adresse MAC
  Serial.print("MAC Address: ");
  Serial.println(WiFi.macAddress());
#endif
  return 0;
}

void setup_webServer() {
  // Configuration du serveur Web
  server.on("/", handleRoot);
  server.on("/status", handleStatus);
  server.begin();
#ifdef SERIAL_DEBUG
  Serial.println("HTTP server started.");
#endif
}


void setup() {
#ifdef SERIAL_DEBUG
  Serial.begin(BAUDS);
  delay(100);
  Serial.println("Starting...");
#endif
  // Configuration de la broche
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  setup_wifi(W_SSID,W_PASSWORD);
  setup_webServer();
  // Initialisation du temps d'attente
  restoreTime();
}

void loop() {
  server.handleClient(); // Gère les requêtes HTTP  
  unsigned long currentTime = millis();
  storeTime(currentTime-lastWateringTime,0);
  if ((unsigned long)(currentTime - lastWateringTime) >= WATERING_DELAY) {
    watering();
    wateringCompleted++;
    lastWateringTime = currentTime; // Réinitialiser le temps du dernier arrosage
    storeTime(currentTime-lastWateringTime,1);//forcer le refresh
  }
#ifdef SERIAL_DEBUG
  Serial.print("millis: ");
  Serial.print(currentTime);
  Serial.print(", last watering: ");
  Serial.print(lastWateringTime);
  Serial.print(", last save: ");
  Serial.print(lastSaveTime);
  Serial.print(", number of watering: ");
  Serial.println(wateringCompleted);
  
#endif
  delay(LOOP_DELAY);
}
