#define CHARGER_PIN A1      // Пин подключения зарядки
#define BATTERY_PIN A0      // Пин измерения напряжения аккумулятора
#define LOAD_PIN A2         // Пин подачи питания на нагрузку
#define VREF 4.5            // Опорное напряжение = 4.5 т.к. питание идет с платы через диод, поэтому падение напряжения до 4.5

const int redLed = 6;       // Красный светодиод
const int greenLed = 5;     // Зеленый светодиод

// Константы для напряжений
const float CHARGER_THRESHOLD = 2.3;    // Порог определения подключения зарядки
const float BATTERY_MIN = 5.3;          // Минимальное напряжение батареи (через делитель)
const float BATTERY_MAX = 7.4;            // Максимальное напряжение батареи (через делитель)
const float BATTERY_LOW = 6;            // Порог низкого заряда (моргание)
const float LOAD_ENABLE_VOLTAGE = 7.3;  // Напряжение для включения нагрузки
const float LOAD_DISABLE_VOLTAGE = 7.1; // Напряжение для выключения нагрузки (гистерезис)

// Флаги состояния
bool isCharging = false;
bool loadEnabled = false;
bool redLedBlinkState = false;
unsigned long lastBlinkTime = 0;
const unsigned long BLINK_INTERVAL = 2000; 

void setup() {
  Serial.begin(9600);
  
  pinMode(redLed, OUTPUT);
  pinMode(greenLed, OUTPUT);
  pinMode(LOAD_PIN, OUTPUT);
  
  digitalWrite(redLed, LOW);
  digitalWrite(greenLed, LOW);
  digitalWrite(LOAD_PIN, LOW);
  
  Serial.println("Система контроля заряда запущена");
}

void loop() {
  float chargerVoltage = analogRead(CHARGER_PIN) * VREF / 1023.0;
  float batteryVoltage = analogRead(BATTERY_PIN) * VREF / 1023.0;
  
  // Корректировка напряжения батареи (делитель 0.5)
  float actualBatteryVoltage = batteryVoltage * 2.0 ;
  
  // Определение состояния зарядки
  bool chargerConnected = (chargerVoltage > CHARGER_THRESHOLD);
  
  // Логика управления
  if (chargerConnected) {
    // Зарядка подключена
    isCharging = true;
    redLedBlinkState = false;
    
    if (actualBatteryVoltage < BATTERY_MAX) {
      // Батарея заряжается, но еще не полностью
      digitalWrite(greenLed, LOW);
      digitalWrite(redLed, HIGH);  // Красный горит постоянно - идет зарядка
      
      // Управление нагрузкой с гистерезисом
      if (!loadEnabled && actualBatteryVoltage >= LOAD_ENABLE_VOLTAGE) {
        // Отключение зарядки через n-транзистор, только при достижении верхнего порога
        digitalWrite(LOAD_PIN, HIGH);
        loadEnabled = true;
        Serial.println("Нагрузка включена (достигнут порог " + String(LOAD_ENABLE_VOLTAGE) + "В)");
        //digitalWrite(greenLed, HIGH);
        //digitalWrite(redLed, LOW); 
      } 
      else if (loadEnabled && actualBatteryVoltage <= LOAD_DISABLE_VOLTAGE) {
        // Выключение зарядки через n-транзистор, только при падении ниже нижнего порога
        digitalWrite(LOAD_PIN, LOW);
        //digitalWrite(greenLed, LOW);
        //digitalWrite(redLed, HIGH);
        loadEnabled = false;
        Serial.println("Нагрузка отключена (напряжение ниже " + String(LOAD_DISABLE_VOLTAGE) + "В)");
      }
      
    } 
    else {
      // Батарея полностью заряжена (>= BATTERY_MAX)
      digitalWrite(greenLed, HIGH);
      digitalWrite(redLed, LOW);
      
      // При полном заряде всегда отключаем зарядку
      if (!loadEnabled) {
        digitalWrite(LOAD_PIN, HIGH);
        loadEnabled = true;
        Serial.println("Нагрузка включена (батарея полностью заряжена)");
      }
    }
  } 
  else {
    // Зарядка отключена
    isCharging = false;
    
    // Выключаем зеленый светодиод (только при зарядке)
    digitalWrite(greenLed, LOW);
    
    // Проверка состояния батареи
    if (actualBatteryVoltage <= BATTERY_LOW) {
      // Батарея разряжена (меньше или равно 6В) - моргание раз в 2 секунды
      unsigned long currentTime = millis();
      if (currentTime - lastBlinkTime >= BLINK_INTERVAL) {
        redLedBlinkState = !redLedBlinkState;
        digitalWrite(redLed, redLedBlinkState);
        lastBlinkTime = currentTime;
      }
      
      // Отключение нагрузки при низком заряде
      if (loadEnabled) {
        digitalWrite(LOAD_PIN, LOW);
        loadEnabled = false;
        Serial.println("Нагрузка отключена из-за низкого заряда");
      }
    } 
    else {
      // Батарея заряжена (> 6В) - светодиод выключен
      digitalWrite(redLed, LOW);
      redLedBlinkState = false;
      
      // Управление нагрузкой с гистерезисом при работе от батареи
      //if (!loadEnabled && actualBatteryVoltage >= LOAD_ENABLE_VOLTAGE) {
      //  digitalWrite(LOAD_PIN, HIGH);
      //  loadEnabled = true;
      //  Serial.println("нагрузка включена (работа от батареи)");
      //} 
      //else if (loadEnabled && actualBatteryVoltage <= LOAD_DISABLE_VOLTAGE) {
      //  digitalWrite(LOAD_PIN, LOW);
      //  loadEnabled = false;
      //  Serial.println("Нагрузка отключена (напряжение батареи упало)");
      //}
    }
  }
  
  // Вывод информации в монитор порта
  static unsigned long lastPrintTime = 0;
  if (millis() - lastPrintTime >= 1000) {
    Serial.print("Зарядка: ");
    Serial.print(chargerConnected ? "ПОДКЛ" : "ОТКЛ");
    Serial.print(" | Батарея: ");
    Serial.print(actualBatteryVoltage, 2);
    Serial.print("В | Нагрузка: ");
    Serial.println(loadEnabled ? "ВКЛ" : "ВЫКЛ");
    
    if (actualBatteryVoltage <= BATTERY_LOW && !isCharging) {
      Serial.println("ВНИМАНИЕ: Низкий заряд батареи! Моргание красным");
    }
    
    lastPrintTime = millis();
  }
  
  delay(50); // Небольшая задержка для стабильности
}
