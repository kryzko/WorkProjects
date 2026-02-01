#include <Arduino.h>
// Constants for determining the approximate temperature from the sensor
const float VOLTS_PER_STEP = 5.0 / 1023.0; 
const float VOLT_AT_100C = 0.65;  
const float VOLT_AT_300C = 1.81;  

const float TEMP_COEFFICIENT = (300.0 - 100.0) / (VOLT_AT_300C - VOLT_AT_100C); // k
const float TEMP_OFFSET = 100.0 - (TEMP_COEFFICIENT * VOLT_AT_100C); // b

const int tempDataPin = A3;   

const int relayPinThird = 6;
const int relayPinSecond = 7;
const int relayPinFirst = 8;

int greenpin = 2; // pin green 
int mainpin = 3; // pin connection
int redpin = 4; // pin red

bool relayState = false;
unsigned long lastSwitchTime = 0;
const unsigned long SWITCH_DELAY = 2000; // Задержка переключения 2 секунды

// Temperature filtering
const int TEMP_READINGS = 5; // Number of measurements to average
float tempReadings[TEMP_READINGS];
int tempReadIndex = 0;
float tempTotal = 0;
float tempAverage = 0;

void setupRelays() {
  digitalWrite(relayPinThird, LOW);
  digitalWrite(relayPinSecond, LOW);
  digitalWrite(relayPinFirst, LOW);
}

void enableRelaySmoothly() {
  if (!relayState && (millis() - lastSwitchTime > SWITCH_DELAY)) {
    // Плавное включение реле с задержкой между ними
    digitalWrite(relayPinFirst, HIGH);
    delay(300);
    digitalWrite(relayPinSecond, HIGH);
    delay(300);
    digitalWrite(relayPinThird, HIGH);
    
    // Плавное изменение цвета светодиода
    for (int i = 0; i <= 255; i += 5) {
      analogWrite(redpin, 255 - i);
      analogWrite(greenpin, i);
      delay(10);
    }
    
    relayState = true;
    lastSwitchTime = millis();
    Serial.println("Реле плавно включены");
  }
}

void disableRelaySmoothly() {
  if (relayState && (millis() - lastSwitchTime > SWITCH_DELAY)) {
    // Плавное изменение цвета светодиода
    for (int i = 0; i <= 255; i += 5) {
      analogWrite(greenpin, 255 - i);
      analogWrite(redpin, i);
      delay(10);
    }
    
    // Плавное выключение реле с задержкой между ними
    digitalWrite(relayPinThird, LOW);
    delay(300);
    digitalWrite(relayPinSecond, LOW);
    delay(300);
    digitalWrite(relayPinFirst, LOW);
    
    relayState = false;
    lastSwitchTime = millis();
    Serial.println("Реле плавно выключены");
  }
}

float readFilteredTemperature() {
  int sensorValue = analogRead(tempDataPin);
  float inputVoltage = sensorValue * VOLTS_PER_STEP;
  float temperature = (inputVoltage * TEMP_COEFFICIENT) + TEMP_OFFSET;
  
  // Фильтрация скользящим средним
  tempTotal = tempTotal - tempReadings[tempReadIndex];
  tempReadings[tempReadIndex] = temperature;
  tempTotal = tempTotal + tempReadings[tempReadIndex];
  tempReadIndex = (tempReadIndex + 1) % TEMP_READINGS;
  
  tempAverage = tempTotal / TEMP_READINGS;
  return tempAverage;
}

bool isTemperatureValid(float voltage, float temperature) {
  // Проверка на корректность показаний датчика
  if (voltage <= 0.15 || voltage >= 1.85) {
    Serial.println("⚠️  Ошибка датчика: напряжение вне диапазона");
    return false;
  }
  
  if (temperature < -50 || temperature > 350) {
    Serial.println("⚠️  Ошибка датчика: температура вне диапазона");
    return false;
  }
  
  return true;
}

void checkTemperatureState() {
  int sensorValue = analogRead(tempDataPin);
  float inputVoltage = sensorValue * VOLTS_PER_STEP;
  float temperature = readFilteredTemperature();
  
  bool sensorValid = isTemperatureValid(inputVoltage, temperature);
  
  // Вывод данных для отладки
  Serial.print("Напряжение: ");
  Serial.print(inputVoltage, 3);
  Serial.print(" В | Температура: ");
  Serial.print(temperature, 1);
  Serial.print(" °C | Усредненная: ");
  Serial.print(tempAverage, 1);
  Serial.print(" °C | Реле: ");
  Serial.println(relayState ? "ВКЛ" : "ВЫКЛ");
  
  if (!sensorValid) {
    // При ошибке датчика выключаем реле
    disableRelaySmoothly();
    return;
  }
  
  // Гистерезис для предотвращения частых переключений
  const float TURN_ON_TEMP = 280.0;  // Включение при 280°C
  const float TURN_OFF_TEMP = 300.0; // Выключение при 300°C
  
  if (temperature >= TURN_OFF_TEMP) {
    disableRelaySmoothly();
  } else if (temperature <= TURN_ON_TEMP) {
    enableRelaySmoothly();
  }
  // Между 280°C и 300°C сохраняем текущее состояние
}

void initTemperatureFilter() {
  // Инициализация фильтра начальными значениями
  for (int i = 0; i < TEMP_READINGS; i++) {
    int sensorValue = analogRead(tempDataPin);
    float inputVoltage = sensorValue * VOLTS_PER_STEP;
    float temperature = (inputVoltage * TEMP_COEFFICIENT) + TEMP_OFFSET;
    tempReadings[i] = temperature;
    tempTotal += temperature;
    delay(50);
  }
  tempAverage = tempTotal / TEMP_READINGS;
}

void setup() {
  Serial.begin(9600);
  
  // Инициализация пинов
  pinMode(relayPinThird, OUTPUT);
  pinMode(relayPinSecond, OUTPUT);
  pinMode(relayPinFirst, OUTPUT);
  
  pinMode(redpin, OUTPUT);
  pinMode(mainpin, OUTPUT);
  pinMode(greenpin, OUTPUT);
  
  // Начальное состояние
  setupRelays();
  analogWrite(redpin, 255); // Красный светодиод при запуске
  analogWrite(greenpin, 0);
  
  // Инициализация фильтра температуры
  initTemperatureFilter();
  
  Serial.println("Система запущена. Мониторинг температуры...");
}

void loop() {
  checkTemperatureState();
  delay(500); // Уменьшенная задержка для более плавного отклика
}
