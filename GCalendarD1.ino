const char* Version = "V.007";

#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <GxEPD2_BW.h>
#include <GxEPD2_3C.h>
#include <Fonts/FreeMonoBold9pt7b.h>
#include <Fonts/FreeMono12pt7b.h>

// select the display class and display driver class in the following file (new style):
#include "GxEPD2_display_selection_new_style.h"
#include "DatiPrivati.h"

String events[3] = {"Nessun appuntamento", "Nessun appuntamento", "Nessun appuntamento"};
String dates[3] = {"", "", ""};
String times[3] = {"", "", ""};
int currentEvent = 0;

// Usa WiFiClientSecure per HTTPS
WiFiClientSecure client;

void setup() {
  delay(1000);
  
  // Configura il client HTTPS
  client.setInsecure();
  client.setTimeout(15000);
  
  display.init();
  display.setRotation(0);
  display.setTextColor(GxEPD_BLACK);
  display.setFullWindow();
  display.refresh(false);
  display.fillScreen(GxEPD_WHITE);
  display.firstPage();

  // Display iniziale
  do {
    drawFrame();
    display.setFont(&FreeMonoBold9pt7b);
    display.setCursor(50, 80);
    display.print("Avvio...");
  } while (display.nextPage());
  
  display.powerOff();

  // Connessione WiFi
  connectToWiFi();

  if (WiFi.status() == WL_CONNECTED) {
    fetchGoogleCalendarEvents();
  }

  updateDisplay();
}

void loop() {
  // Cambia evento ogni 5 secondi
  delay(5000);
  
  currentEvent = (currentEvent + 1) % 3;
  updateDisplay();

  // Aggiorna gli eventi ogni 10 minuti
  static unsigned long lastUpdate = 0;
  if (millis() - lastUpdate > 600000) {
    if (WiFi.status() == WL_CONNECTED) {
      fetchGoogleCalendarEvents();
    } else {
      connectToWiFi();
      if (WiFi.status() == WL_CONNECTED) {
        fetchGoogleCalendarEvents();
      }
    }
    lastUpdate = millis();
  }
}

void drawFrame() {
  int margin = 3;
  int width = display.width();
  int height = display.height();
  
  display.drawRect(margin, margin, width - 2 * margin, height - 2 * margin, GxEPD_BLACK);
  display.drawRect(margin + 2, margin + 2, width - 2 * (margin + 2), height - 2 * (margin + 2), GxEPD_BLACK);

  // Versione
  display.setFont(&FreeMonoBold9pt7b);
  display.setCursor(width - 80, 20);
  display.print(Version);
}

void connectToWiFi() {
  WiFi.begin(ssid, password);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(1000);
    attempts++;
  }
}

void fetchGoogleCalendarEvents() {
  if (client.connect("www.googleapis.com", 443)) {
    String url = String(googleCalendarURL) + 
                 "?singleEvents=true" +
                 "&orderBy=startTime" +
                 "&maxResults=3" +  // Richiedi 3 eventi
                 "&timeMin=" + getCurrentTime() +
                 "&key=" + apiKey;
    
    client.println("GET " + url + " HTTP/1.1");
    client.println("Host: www.googleapis.com");
    client.println("User-Agent: ESP8266");
    client.println("Connection: close");
    client.println();
    
    // Attendi risposta
    unsigned long timeout = millis();
    while (client.available() == 0) {
      if (millis() - timeout > 15000) {
        client.stop();
        setErrorMessages("Timeout connessione");
        return;
      }
      delay(100);
    }
    
    // Salta l'header HTTP
    while (client.available()) {
      String line = client.readStringUntil('\n');
      if (line == "\r") {
        break;
      }
    }
    
    // Leggi il corpo della risposta
    String response = "";
    while (client.available()) {
      response += client.readString();
    }
    
    client.stop();
    
    if (response.length() > 0) {
      parseCalendarResponse(response);
    } else {
      setErrorMessages("Risposta vuota");
    }
    
  } else {
    setErrorMessages("Errore connessione");
  }
}

void parseCalendarResponse(String response) {
  // Cerca l'inizio del JSON
  int jsonStart = response.indexOf("{");
  if (jsonStart == -1) {
    setErrorMessages("Errore risposta");
    return;
  }
  
  String json = response.substring(jsonStart);
  
  DynamicJsonDocument doc(4096);
  DeserializationError error = deserializeJson(doc, json);
  
  if (error) {
    setErrorMessages("Errore parsing");
    return;
  }
  
  // Controlla se c'è un errore nella risposta
  if (doc.containsKey("error")) {
    setErrorMessages("Errore API");
    return;
  }
  
  JsonArray items = doc["items"];
  if (items.size() == 0) {
    setNoEvents();
    return;
  }
  
  // Popola gli array con gli eventi
  for (int i = 0; i < 3; i++) {
    if (i < items.size()) {
      JsonObject event = items[i];
      events[i] = event["summary"].as<String>();
      
      // Parsing data/ora
      if (event.containsKey("start")) {
        JsonObject start = event["start"];
        if (start.containsKey("dateTime")) {
          String dateTime = start["dateTime"].as<String>();
          parseDateTime(dateTime, i);
        } else if (start.containsKey("date")) {
          dates[i] = start["date"].as<String>();
          times[i] = "Tutto il giorno";
        }
      }
    } else {
      events[i] = "Nessun appuntamento";
      dates[i] = "";
      times[i] = "";
    }
  }
}

void parseDateTime(String dateTime, int index) {
  // Formato: 2024-01-15T10:00:00+01:00
  dates[index] = dateTime.substring(0, 10);
  times[index] = dateTime.substring(11, 16);
}

void setErrorMessages(String error) {
  for (int i = 0; i < 3; i++) {
    events[i] = error;
    dates[i] = "";
    times[i] = "";
  }
}

void setNoEvents() {
  for (int i = 0; i < 3; i++) {
    events[i] = "Nessun appuntamento";
    dates[i] = "";
    times[i] = "";
  }
}

String getCurrentTime() {
  // Usa la data corrente
  return "2024-01-18T00:00:00Z";
}

void updateDisplay() {
  display.init();
  display.setRotation(0);
  display.setTextColor(GxEPD_BLACK);
  display.setFullWindow();
  display.firstPage();

  do {
    drawFrame();
    
    // Indicatore evento corrente (1/3, 2/3, 3/3)
    display.setFont(&FreeMonoBold9pt7b);
    display.setCursor(20, 40);
    display.print("Evento ");
    display.print(currentEvent + 1);
    display.print("/3");
    
    // Titolo prossimo appuntamento
    display.setFont(&FreeMonoBold9pt7b);
    display.setCursor(20, 70);
    display.print("Prossimo appuntamento:");
    
    // Nome evento
    display.setFont(&FreeMono12pt7b);
    display.setCursor(20, 100);
    
    String displayTitle = events[currentEvent];
    if (displayTitle.length() > 20) {
      displayTitle = displayTitle.substring(0, 20) + "...";
    }
    display.print(displayTitle);
    
    // Data e Ora
    if (dates[currentEvent] != "") {
      display.setFont(&FreeMonoBold9pt7b);
      display.setCursor(20, 130);
      display.print("Data: ");
      display.print(dates[currentEvent]);
      
      display.setCursor(20, 150);
      display.print("Ora: ");
      display.print(times[currentEvent]);
    }
    
  } while (display.nextPage());
  
  display.powerOff();
}