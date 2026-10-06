#include <Arduino.h>

// Виводи (Піни)
const int BUTTON_PIN = 0;    // Кнопка BOOT
const int RELAY_PIN = 21;    // Керування обмоткою реле (через модуль / транзистор)
const int CONTACT_PIN = 17;  // Сухий контакт реле: COM -> GND, NO -> CONTACT_PIN

const uint32_t BUTTON_DEBOUNCE_US = 10000;   // 10 мс
const uint32_t CONTACT_TIMEOUT_US = 200000;  // 200 мс

// Стан, який змінюється в перериваннях
volatile bool buttonPressed = false;
volatile uint32_t lastButtonUs = 0;

volatile bool waitingForContact = false;  // Чекаємо першого фронту контакту після команди
volatile bool contactChanged = false;     // Контакт спрацював, результат готовий
volatile uint32_t contactTimeUs = 0;

bool relayOn = false;
uint32_t commandTimeUs = 0;


// Кнопка: приймаємо лише реальне натискання (LOW) з антибрязкотом
void IRAM_ATTR onButton() {
  uint32_t now = micros();

  if (digitalRead(BUTTON_PIN) == LOW && now - lastButtonUs >= BUTTON_DEBOUNCE_US) {
    lastButtonUs = now;
    buttonPressed = true;
  }
}

// Сухий контакт: фіксуємо лише ПЕРШИЙ фронт після команди, брязкіт ігнорується
void IRAM_ATTR onContact() {
  if (waitingForContact) {
    contactTimeUs = micros();
    waitingForContact = false;
    contactChanged = true;
  }
}

// Перемикаємо реле і запам'ятовуємо момент команди
void toggleRelay() {
  relayOn = !relayOn;

  waitingForContact = true;
  commandTimeUs = micros();
  digitalWrite(RELAY_PIN, relayOn ? HIGH : LOW);

  Serial.printf("Реле %s\n", relayOn ? "ON" : "OFF");
}

// Виводимо затримку або повідомлення про тайм-аут
void checkMeasurement() {
  if (contactChanged) {
    contactChanged = false;
    uint32_t delayUs = contactTimeUs - commandTimeUs;

    Serial.printf("%s: затримка %lu мкс (%.2f мс)\n",
                  relayOn ? "Увімкнення" : "Вимкнення",
                  (unsigned long)delayUs, delayUs / 1000.0);
  } else if (waitingForContact && micros() - commandTimeUs > CONTACT_TIMEOUT_US) {
    waitingForContact = false;
    Serial.println("Контакт не спрацював - перевірте підключення");
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);
  pinMode(CONTACT_PIN, INPUT_PULLUP);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), onButton, FALLING);
  attachInterrupt(digitalPinToInterrupt(CONTACT_PIN), onContact, CHANGE);

  // Скидаємо хибне спрацювання, яке ESP32 може зловити під час старту
  delay(50);
  buttonPressed = false;
}

void loop() {
  if (buttonPressed && !waitingForContact) {
    buttonPressed = false;
    toggleRelay();
  }

  checkMeasurement();
}
