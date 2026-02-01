#include <Arduino.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>
#include <Encoder.h>

#define TFT_CS   10
#define TFT_RST  8
#define TFT_DC   9
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

// Конфигурация датчиков
const int sensorPins[] = {A1, A2, A3, A4};
const int numSensors = 4;
const float Vref = 5.0;
const float Vmin = 0.374;
const float Vmax = 4.75;
const float Pmin = 0.1;
const float Pmax = 1.2;

// Параметры фильтра
const int numReadings = 10;
const float maxDelta = 0.05;
const float hysteresisThreshold = 0.03;
const int stableCountRequired = 3;

// Энкодер
const int encoderCLK = 2;    // CLK пин энкодера
const int encoderDT = 3;     // DT пин энкодера
const int encoderSW = 4;     // SW пин энкодера (кнопка)
Encoder myEncoder(encoderCLK, encoderDT);
unsigned long updateInterval = 100; // Интервал обновления по умолчанию (мс)
unsigned long buttonPressTime = 0;
bool buttonActive = false;
long oldPosition = 0;
unsigned long lastEncoderAction = 0;

// Структура для хранения данных каждого датчика
struct SensorData {
  float readings[numReadings];
  float filteredPressure;
  float initialPressure;
  float pressureDifference;
  bool isStable;
  int stableCount;
  int index;
  float lastDisplayedPressure;
  bool lastStableState;
};

SensorData sensors[numSensors];

// Координаты и размеры для каждого квадранта
const int quadrantWidth = 160/ 2;
const int quadrantHeight = 128 / 2;
const int quadrantPositions[4][2] = {
  {0, 0},                     // Верхний левый
  {quadrantWidth, 0},         // Верхний правый
  {0, quadrantHeight},        // Нижний левый
  {quadrantWidth, quadrantHeight} // Нижний правый
};

// Позиции для отображения разницы давления в каждом квадранте
const int pressureDiffPositions[4][2] = {
  {5, 25},  // Верхний левый
  {quadrantWidth + 5, 25},  // Верхний правый
  {5, quadrantHeight + 25},  // Нижний левый
  {quadrantWidth + 5, quadrantHeight + 25}  // Нижний правый
};

void drawQuadrant(int sensorNum) {
  int x = quadrantPositions[sensorNum][0];
  int y = quadrantPositions[sensorNum][1];
  
  // Очищаем квадрант
  tft.fillRect(x, y, quadrantWidth, quadrantHeight, ST7735_BLACK);
  
  // Рисуем границу квадранта
  tft.drawRect(x, y, quadrantWidth, quadrantHeight, ST7735_WHITE);
  
  // Устанавливаем большой размер текста
  tft.setTextSize(2);
  
  // Отображаем начальное значение (стандартное)
  tft.setCursor(x + 5, y + 5);
  tft.setTextColor(ST7735_WHITE, ST7735_BLACK);
  tft.print(sensors[sensorNum].initialPressure * 100, 1);
  
  // Отображаем разницу давления
  tft.setCursor(x + 5, y + 25);
  if (sensors[sensorNum].isStable) {
    tft.setTextColor(ST7735_GREEN, ST7735_BLACK);
  } else {
    tft.setTextColor(ST7735_RED, ST7735_BLACK);
  }
  
  // Отображаем разницу со знаком
  float diff = sensors[sensorNum].pressureDifference * 100;
  if (diff >= 0) {
    tft.print("+");
  }
  tft.print(diff, 1);
  
  // Единицы измерения
  tft.setCursor(x + 5, y + 45);
  tft.setTextColor(ST7735_YELLOW, ST7735_BLACK);
  tft.print("kpa");
  
  // Возвращаем обычный размер текста
  tft.setTextSize(1);
}

void updatePressureDifference(int sensorNum) {
  int x = pressureDiffPositions[sensorNum][0];
  int y = pressureDiffPositions[sensorNum][1];
  
  // Очищаем только область разницы давления
  tft.fillRect(x, y, 70, 16, ST7735_BLACK);
  
  // Устанавливаем большой размер текста
  tft.setTextSize(2);
  
  // Устанавливаем цвет в зависимости от стабильности
  if (sensors[sensorNum].isStable) {
    tft.setTextColor(ST7735_GREEN, ST7735_BLACK);
  } else {
    tft.setTextColor(ST7735_RED, ST7735_BLACK);
  }
  
  // Отображаем разницу давления со знаком
  tft.setCursor(x, y);
  float diff = sensors[sensorNum].pressureDifference * 100;
  if (diff >= 0) {
    tft.print("+");
  }
  tft.print(diff, 1);
  
  // Возвращаем обычный размер текста
  tft.setTextSize(1);
}

void displayStatusBar() {
  // Статус бар в центре экрана 
  tft.setCursor(quadrantWidth / 2 + 25, quadrantHeight - 4); // Центрируем текст
  tft.setTextColor(ST7735_CYAN, ST7735_BLACK);
  tft.print(updateInterval);
  tft.print("ms");
}

void resetInitialPressure() {
  for (int i = 0; i < numSensors; i++) {
    sensors[i].initialPressure = sensors[i].filteredPressure;
    sensors[i].pressureDifference = 0;
  }
  
  // Показать сообщение о сбросе
  tft.fillRect(tft.width()/4, tft.height()/3, tft.width()/2, 20, ST7735_BLACK);
  tft.setCursor(tft.width()/4 + 10, tft.height()/3);
  tft.setTextColor(ST7735_CYAN, ST7735_BLACK);
  tft.setTextSize(1);
  tft.print("Values Reset!");
  delay(1000);
  
  // Перерисовать все квадранты
  for (int i = 0; i < numSensors; i++) {
    drawQuadrant(i);
  }
  displayStatusBar();
}

void setup() {
  Serial.begin(9600);
  
  // Инициализация энкодера
  pinMode(encoderSW, INPUT_PULLUP);
  
  // Инициализация дисплея
  tft.initR(INITR_BLACKTAB);
  tft.setRotation(3);
  tft.fillScreen(ST7735_BLACK);
  tft.setTextColor(ST7735_WHITE);
  tft.setTextSize(1);
  
  // Заголовок
  tft.setCursor(20, 2);
  tft.setTextSize(1);
  tft.println("PRESSURE MONITOR");
  tft.drawLine(0, 15, tft.width(), 15, ST7735_WHITE);
  
  // Инициализация датчиков
  for (int i = 0; i < numSensors; i++) {
    pinMode(sensorPins[i], INPUT);
    
    float sum = 0;
    for (int j = 0; j < numReadings; j++) {
      int sensorValue = analogRead(sensorPins[i]);
      float voltage = sensorValue * (Vref / 1023.0);
      sensors[i].readings[j] = Pmin + ( (voltage - Vmin) * (Pmax - Pmin) / (Vmax - Vmin) );
      sum += sensors[i].readings[j];
      delay(10);
    }
    
    sensors[i].filteredPressure = sum / numReadings;
    sensors[i].initialPressure = sensors[i].filteredPressure;
    sensors[i].pressureDifference = 0;
    
    sensors[i].lastDisplayedPressure = sensors[i].filteredPressure;
    sensors[i].isStable = true;
    sensors[i].lastStableState = true;
    sensors[i].stableCount = 0;
    sensors[i].index = 0;
    
    // Рисуем начальные квадранты
    drawQuadrant(i);
  }
  
  displayStatusBar();
}

void updateSensorData(int sensorNum) {
  int pin = sensorPins[sensorNum];
  int sensorValue = analogRead(pin);
  float voltage = sensorValue * (Vref / 1023.0);
  
  float currentPressure = Pmin + ( (voltage - Vmin) * (Pmax - Pmin) / (Vmax - Vmin) );

  if (sensors[sensorNum].isStable) {
    if (abs(currentPressure - sensors[sensorNum].filteredPressure) > maxDelta) {
      sensors[sensorNum].isStable = false;
      sensors[sensorNum].stableCount = 0;
    }
  } else {
    if (abs(currentPressure - sensors[sensorNum].filteredPressure) < hysteresisThreshold) {
      sensors[sensorNum].stableCount++;
      if (sensors[sensorNum].stableCount >= stableCountRequired) {
        sensors[sensorNum].isStable = true;
      }
    } else {
      sensors[sensorNum].stableCount = 0;
    }
  }

  sensors[sensorNum].filteredPressure -= sensors[sensorNum].readings[sensors[sensorNum].index] / numReadings;
  sensors[sensorNum].readings[sensors[sensorNum].index] = currentPressure;
  sensors[sensorNum].filteredPressure += sensors[sensorNum].readings[sensors[sensorNum].index] / numReadings;
  sensors[sensorNum].index = (sensors[sensorNum].index + 1) % numReadings;
  
  // Обновляем разницу давления
  sensors[sensorNum].pressureDifference = sensors[sensorNum].filteredPressure - sensors[sensorNum].initialPressure;
}

void checkEncoder() {
  // Чтение текущей позиции энкодера
  long newPosition = myEncoder.read();
  
  // Обработка вращения энкодера - изменение интервала
  if (newPosition != oldPosition) {
    if (newPosition > oldPosition) {
      if (updateInterval < 5000) updateInterval += 50;
    } else {
      if (updateInterval > 50) updateInterval -= 50;
    }
    oldPosition = newPosition;
    
    // Очищаем и обновляем статусную строку
    tft.fillRect(quadrantWidth / 2 + 20, quadrantHeight - 6, 50, 10, ST7735_BLACK);
    displayStatusBar();
  }
  
  // Проверка нажатия кнопки энкодера
  if (digitalRead(encoderSW) == LOW) {
    if (!buttonActive) {
      buttonActive = true;
      buttonPressTime = millis();
    }
    
    // Удержание кнопки более 3 секунд - сброс начальных значений
    if (millis() - buttonPressTime > 3000) {
      resetInitialPressure();
      buttonPressTime = millis();
    }
  } else {
    if (buttonActive) {
      buttonActive = false;
    }
  }
}

void loop() {
  checkEncoder();
  
  static unsigned long lastUpdate = 0;
  if (millis() - lastUpdate >= updateInterval) {
    lastUpdate = millis();
    
    for (int i = 0; i < numSensors; i++) {
      updateSensorData(i);
      // Обновляем только область разницы давления
      updatePressureDifference(i);
    }
  }
  
  delay(10);
}