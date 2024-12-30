
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "SparkFun_SCD4x_Arduino_Library.h"

#define SCREEN_WIDTH 128 // Szerokość ekranu OLED w pikselach
#define SCREEN_HEIGHT 64 // Wysokość ekranu OLED w pikselach
#define OLED_RESET -1    // Reset pin nie jest używany
#define OLED_ADDRESS 0x3C // Adres I2C wyświetlacza OLED
#define BAUD_RATE 9600   // Prędkość transmisji danych przez port szeregowy
#define HISTORY_AVERAGE_SIZE 10 // Liczba pomiarów do obliczenia średniej na wykres

int ppmHistory[SCREEN_WIDTH * HISTORY_AVERAGE_SIZE] = {0}; // Tablica dla historii pomiarów
int historyIndex = 0; // Aktualny indeks w historii

// Deklaracja obiektu wyświetlacza SSD1306
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

SCD4x mySensor;

void setup() {
  Serial.begin(BAUD_RATE); // Rozpoczynanie komunikacji szeregowej z prędkością 9600 bps
  // Inicjalizacja wyświetlacza OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) { // Używanie stałej OLED_ADDRESS
    Serial.println(F("Nie można zainicjować SSD1306"));
    for (;;); // Zatrzymanie działania programu, jeśli nie można zainicjować wyświetlacza
  }

  Wire.begin(D2, D1); // Inicjalizacja I2C z D2 jako SDA i D1 jako SCL

  if (mySensor.begin() == false) {
    Serial.println("Sensor not detected. Please check wiring. Freezing...");
    while (1); // Zawieszenie programu, jeśli czujnik nie zostanie wykryty
  }

  mySensor.startPeriodicMeasurement(); // Rozpoczęcie cyklicznych pomiarów
  Serial.println("SCD41 sensor initialized and measurement started!");
 
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
}

// Funkcja do rysowania wykresu
void drawPpmGraph() {
  int graphHeight = 40; // Wysokość wykresu (dolna część ekranu)
  int graphY = SCREEN_HEIGHT - graphHeight; // Pozycja Y wykresu (dolny margines)

  // Obliczanie zakresu wartości
  int maxPpm = 3000; // Maksymalna wartość CO2 do wizualizacji
  int minPpm = 400;  // Minimalna wartość CO2 do wizualizacji

  for (int i = 0; i < SCREEN_WIDTH; i++) {
    int sum = 0;

    // Obliczanie średniej dla AVERAGE_SIZE próbek
    for (int j = 0; j < HISTORY_AVERAGE_SIZE; j++) {
      sum += ppmHistory[(i * HISTORY_AVERAGE_SIZE + j) % (SCREEN_WIDTH * HISTORY_AVERAGE_SIZE)];
    }
    int avgValue = sum / HISTORY_AVERAGE_SIZE;

    // Mapowanie wartości ppm na wysokość wykresu
    int barHeight = map(avgValue, minPpm, maxPpm, 0, graphHeight);
    barHeight = constrain(barHeight, 0, graphHeight); // Ograniczenie do wysokości wykresu

    // Rysowanie słupka dla danego punktu
    display.drawLine(i, SCREEN_HEIGHT, i, SCREEN_HEIGHT - barHeight, WHITE);
  }
}

// Funkcja do dodawania nowych wartości do historii
void addToPpmHistory(int ppmValue) {
  ppmHistory[historyIndex] = ppmValue;
  historyIndex = (historyIndex + 1) % (SCREEN_WIDTH * HISTORY_AVERAGE_SIZE); // Przesunięcie indeksu z zawijaniem
}

void loop() {
  if (mySensor.readMeasurement()) {
    int currentPpm = mySensor.getCO2();
    float  currentTemperature = mySensor.getTemperature();
    float  currentHumidity = mySensor.getHumidity();

    addToPpmHistory(currentPpm); // Dodaj bieżący ppm do historii
  
    
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
  } else {
    Serial.println("Waiting for new data...");
  }

  delay(5000); // Czekanie 5 sekund przed kolejnym odczytem
}
