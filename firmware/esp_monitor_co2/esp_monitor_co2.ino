
#include <Wire.h>
#include <math.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "SparkFun_SCD4x_Arduino_Library.h"
#include <WiFiManager.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>



#define SCREEN_WIDTH 128 // Szerokość ekranu OLED w pikselach
#define SCREEN_HEIGHT 64 // Wysokość ekranu OLED w pikselach
#define OLED_RESET -1    // Reset pin nie jest używany
#define OLED_ADDRESS 0x3C // Adres I2C wyświetlacza OLED
#define BAUD_RATE 9600   // Prędkość transmisji danych przez port szeregowy


int ppmHistory[SCREEN_WIDTH] = {0}; // Tablica dla historii pomiarów
int historyIndex = 0; // Aktualny indeks w historii
const byte DNS_PORT = 53;

DNSServer dnsServer;

// Inicjalizacja serwera na porcie 80
ESP8266WebServer server(80);

// Deklaracja obiektu wyświetlacza SSD1306
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

WiFiManager wm;

SCD4x mySensor;

int currentPpm = 0;
float  currentTemperature = 0;
float  currentHumidity = 0;

void setupDisplay()
{
  // Inicjalizacja wyświetlacza OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) { // Używanie stałej OLED_ADDRESS
    Serial.println(F("Nie można zainicjować SSD1306"));
    for (;;); // Zatrzymanie działania programu, jeśli nie można zainicjować wyświetlacza
  }
  
  display.clearDisplay();
  display.setTextSize(1);      // Ustawienie rozmiaru tekstu
  display.setTextColor(WHITE); // Ustawienie koloru tekstu

  // Wyświetlanie wszystkich zdefiniowanych stałych na OLED
  display.setCursor(0, 0);
  display.println("Define:");
  display.print("OLED_SIZE: ");
  display.print(SCREEN_WIDTH);
  display.print("x");
  display.println(SCREEN_HEIGHT);
  display.print("OLED_RESET: ");
  display.println(OLED_RESET);
  display.print("OLED_ADDRESS: ");
  display.println(OLED_ADDRESS);
  display.print("BAUD_RATE: ");
  display.println(BAUD_RATE);
  display.display();           // Odświeżenie wyświetlacza

  // Wyświetlanie wszystkich zdefiniowanych stałych w konsoli COM
  Serial.println("Zdefiniowane stałe:");
  Serial.print("SCREEN_WIDTH: ");
  Serial.println(SCREEN_WIDTH);
  Serial.print("SCREEN_HEIGHT: ");
  Serial.println(SCREEN_HEIGHT);
  Serial.print("OLED_RESET: ");
  Serial.println(OLED_RESET);
  Serial.print("OLED_ADDRESS: ");
  Serial.println(OLED_ADDRESS);
  Serial.print("BAUD_RATE: ");
  Serial.println(BAUD_RATE);

  delay(1000);
}

void setupWiFi()
{

    String SSID = "Sensor Co2";
    wm.setConfigPortalBlocking(false);
    
    bool res = wm.autoConnect(SSID.c_str());

    if (!res) {
        Serial.println("Failed to connect and hit timeout");
        // Możesz zrestartować ESP, jeśli połączenie nie powiedzie się
        // ESP.restart();
    } else {
        // Informacja o pomyślnym połączeniu z WiFi
        Serial.println("connected...yeey :)");
        
        display.clearDisplay();
        display.setCursor(0, 0);
        display.println("Settings:");
        
        // Wyświetlenie szczegółów połączenia
        Serial.print("IP Address: ");
        Serial.println(WiFi.localIP()); // Adres IP przydzielony ESP
        display.print("IP: ");
        display.println(WiFi.localIP());
        

        Serial.print("Subnet Mask: ");
        Serial.println(WiFi.subnetMask()); // Maska podsieci
        display.print("M: ");
        display.println(WiFi.subnetMask());

        Serial.print("Gateway IP: ");
        Serial.println(WiFi.gatewayIP()); // Adres IP bramy domyślnej
        display.print("G: ");
        display.println(WiFi.gatewayIP());

        Serial.print("DNS Server: ");
        Serial.println(WiFi.dnsIP()); // Adres IP serwera DNS
        display.print("D: ");
        display.println(WiFi.dnsIP());

        display.display();           // Odświeżenie wyświetlacza

        delay(5000);
    }

     //Wyswietla informacje o trybie konfiguracji
     if (WiFi.status() != WL_CONNECTED) {
        display.clearDisplay();
        display.setCursor(0, 0);
        
        // Wyświetlenie szczegółów połączenia
        Serial.print("IP Address: ");
        Serial.println(WiFi.softAPIP()); // Adres IP przydzielony ESP
        display.print("IP: ");
        display.println(WiFi.softAPIP());

        display.print("SSID: ");
        display.println(SSID);

        display.display();           // Odświeżenie wyświetlacza

        delay(5000);
     }
}

void setupSCD4x()
{
  Wire.begin(D2, D1); // Inicjalizacja I2C z D2 jako SDA i D1 jako SCL

  if (mySensor.begin() == false) {
    Serial.println("Sensor not detected. Please check wiring. Freezing...");
    while (1); // Zawieszenie programu, jeśli czujnik nie zostanie wykryty
  }

  // Najpierw musimy zatrzymać okresowe pomiary, w przeciwnym razie uruchomienie pomiarów okresowych o niskim zużyciu energii nie powiedzie się
  if (mySensor.stopPeriodicMeasurement() == true)
  {
    Serial.println(F("Okresowe pomiary zostały wyłączone!"));
  }  

  // Teraz możemy włączyć okresowe pomiary o niskim zużyciu energii
  if (mySensor.startLowPowerPeriodicMeasurement() == true)
  {
    Serial.println(F("Włączono tryb niskiego zużycia energii!"));
  }  

  Serial.println("SCD41 sensor initialized and measurement started!");
}

void setup() {
  Serial.begin(BAUD_RATE); // Rozpoczynanie komunikacji szeregowej z prędkością 9600 bps



  setupDisplay();
  setupSCD4x();
  setupWiFi();

  if (WiFi.status() == WL_CONNECTED) {
    // Ustawienie ścieżki dla URI
    server.on("/currentData", handleCurrentData);
    server.on("/factoryReset", handleFactoryReset);
    server.begin();
    Serial.println("HTTP server started.");
  }
}

// Funkcja do rysowania wykresu
void drawPpmGraph() {
  int graphHeight = 40; // Wysokość wykresu (dolna część ekranu)
  int graphY = SCREEN_HEIGHT - graphHeight; // Pozycja Y wykresu (dolny margines)

  // Obliczanie zakresu wartości
  int maxPpm = 2000; // Maksymalna wartość CO2 do wizualizacji
  int minPpm = 400;  // Minimalna wartość CO2 do wizualizacji

  for (int i = 0; i < SCREEN_WIDTH; i++) {
   
    // Mapowanie wartości ppm na wysokość wykresu
    int barHeight = map(ppmHistory[i], minPpm, maxPpm, 0, graphHeight);
    barHeight = constrain(barHeight, 0, graphHeight); // Ograniczenie do wysokości wykresu

    // Rysowanie słupka dla danego punktu
    display.drawLine(i, SCREEN_HEIGHT, i, SCREEN_HEIGHT - barHeight, WHITE);
  }
}

// Funkcja do dodawania nowych wartości do historii
void addToPpmHistory(int ppmValue) {
  // Sprawdź, czy indeks osiągnął koniec tablicy
  if (historyIndex < SCREEN_WIDTH) {
    // Dodaj wartość na obecnym indeksie
    ppmHistory[historyIndex++] = ppmValue;
  } else {
    // Przesunięcie wszystkich elementów o jeden w lewo
    for (int i = 1; i < SCREEN_WIDTH; i++) {
      ppmHistory[i - 1] = ppmHistory[i];
    }
    // Dodaj nową wartość na końcu tablicy
    ppmHistory[SCREEN_WIDTH - 1] = ppmValue;
  }
}


void loop()
{
   if (WiFi.status() == WL_CONNECTED) {
        loopSennsorMode();
    } else {
        loopConfigMode();
    }
}

void displaySensorData()
{
    Serial.println("CO2: " + String(currentPpm) + " ppm");
    Serial.println("temperature: " + String(currentTemperature, 1) + "C");
    Serial.println("humidity: " + String(currentHumidity, 1) + "%");

    // Ustwianie OLED
    display.clearDisplay();
    display.setTextSize(2); // Ustawienie większego rozmiaru tekstu dla kluczowych danych

    // Wyśrodkowanie poziomu CO2
    String co2Text = String(currentPpm) + "ppm";
    int16_t x1, y1;
    uint16_t textWidth, textHeight;
    display.getTextBounds(co2Text, 0, 0, &x1, &y1, &textWidth, &textHeight);
    int co2X = (SCREEN_WIDTH - textWidth) / 2;
    display.setCursor(co2X, 0);
    display.println(co2Text);

    // Wyśrodkowanie temperatury lub wilgotności
    String dataText;
    dataText = String(currentTemperature, 0) + "C " + String(currentHumidity, 0) + "%";
    display.getTextBounds(dataText, 0, 0, &x1, &y1, &textWidth, &textHeight);
    int dataX = (SCREEN_WIDTH - textWidth) / 2;
    display.setCursor(dataX, 18); // Wyśrodkowanie w pionie
    display.print(dataText);

    // Rysowanie wykresu
    drawPpmGraph();

    display.display(); // Odświeżenie wyświetlacza
}

void readSennsor()
{
  if (mySensor.readMeasurement()) {
    currentPpm = mySensor.getCO2();
    currentTemperature = mySensor.getTemperature();
    currentHumidity = mySensor.getHumidity();

    addToPpmHistory(currentPpm); // Dodaj bieżący ppm do historii

    displaySensorData();
  }
}

void loopConfigMode()
{
  readSennsor();
  wm.process();
}

void loopSennsorMode() {
  readSennsor();
  server.handleClient();
}

// Funkcja obsługująca żądanie HTTP
void handleCurrentData() {
  String json = "{\"CO2\": " + String(currentPpm) + 
                ", \"Temperature\": " + String(currentTemperature) + 
                ", \"Humidity\": " + String(currentHumidity) + "}";
  server.send(200, "application/json", json);
}

void handleFactoryReset() {
  WiFi.disconnect(true);  // Usuwa dane sieci Wi-Fi, czyści konfigurację
  ESP.restart();          // Restartuje mikrokontroler
}
