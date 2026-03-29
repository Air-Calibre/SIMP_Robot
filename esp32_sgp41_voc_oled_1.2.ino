#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <SensirionI2CSgp41.h>
#include <SensirionGasIndexAlgorithm.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define BUZZER_PIN 2
#define BUTTON_PIN 3

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
SensirionI2CSgp41 sgp41;
SensirionGasIndexAlgorithm vocAlgo(SensirionGasIndexAlgorithm::ALGORITHM_TYPE_VOC);

// ===== SYSTEM =====
float vocIndex = 0, prevVoc = 0;
float baseline = 100;
float filteredVOC = 0;

int emotion = 0;
int mouthState = 0;

bool shock = false;
bool blinkNow = false;

unsigned long lastRead = 0;
unsigned long lastBlink = 0;

// ===== GRAPH =====
int graph[30];
int gIndex = 0;

// ===== CTA =====
String getCTA() {
  if (emotion <= 1) return "AIR GREAT";
  if (emotion == 2) return "NORMAL";
  if (emotion == 3) return "VENTILATE";
  if (emotion == 4) return "OPEN WINDOW";
  if (emotion == 5) return "POOR AIR";
  return "LEAVE ROOM!";
}

// ===== BASELINE =====
void updateBaseline() {
  baseline = 0.999 * baseline + 0.001 * vocIndex;
}

// ===== EMOTION =====
void updateEmotion() {
  float rel = vocIndex - baseline;

  if (rel < -20) emotion = 0;
  else if (rel < 20) emotion = 1;
  else if (rel < 50) emotion = 2;
  else if (rel < 100) emotion = 3;
  else if (rel < 150) emotion = 4;
  else if (rel < 250) emotion = 5;
  else emotion = 6;

  // mouth mapping
  if (emotion <= 1) mouthState = 0;
  else if (emotion == 2) mouthState = 1;
  else if (emotion == 3) mouthState = 2;
  else if (emotion == 4) mouthState = 3;
  else if (emotion == 5) mouthState = 4;
  else mouthState = 5;
}

// ===== BUZZER =====
void buzzer() {
  if (emotion <= 2) noTone(BUZZER_PIN);
  else if (emotion == 4) tone(BUZZER_PIN, 2000, 100);
  else if (emotion >= 5) tone(BUZZER_PIN, 3000, 200);
}

// ===== MOUTH =====
void drawMouth(int state) {
  int x = 64;
  int y = 50;

  switch (state) {
    case 0: display.drawLine(x - 10, y, x + 10, y, WHITE); break;
    case 1: display.drawLine(x - 8, y, x + 8, y, WHITE); break;
    case 2: display.drawLine(x - 10, y + 1, x + 10, y + 1, WHITE); break;
    case 3: display.drawLine(x - 10, y + 2, x + 10, y + 2, WHITE); break;

    case 4: // angry teeth
      for (int i = -8; i < 8; i += 3)
        display.drawLine(x + i, y, x + i, y + 4, WHITE);
      break;

    case 5: // open mouth
      display.drawCircle(x, y, 5, WHITE);
      break;
  }
}

// ===== FACE =====
void drawFace() {
  display.clearDisplay();

  int jx = random(-1, 2);
  if (emotion >= 5) jx = random(-4, 4);

  // eyes
  if (emotion == 5) {
    display.fillTriangle(20 + jx, 35, 50 + jx, 20, 50 + jx, 40, WHITE);
    display.fillTriangle(78 + jx, 20, 108 + jx, 35, 78 + jx, 40, WHITE);
  } else if (emotion == 6) {
    display.fillCircle(35 + jx, 30, 15, WHITE);
    display.fillCircle(95 + jx, 30, 15, WHITE);
    display.fillCircle(35 + jx, 30, 5, BLACK);
    display.fillCircle(95 + jx, 30, 5, BLACK);
  } else {
    int h = 20 - emotion * 2;
    if (h < 4) h = 4;

    display.fillRoundRect(20 + jx, 30, 30, h, 8, WHITE);
    display.fillRoundRect(78 + jx, 30, 30, h, 8, WHITE);
  }

  // breathing bar
  int bar = map(vocIndex, 0, 400, 10, 120);
  display.drawRect(4, 58, bar, 4, WHITE);

  // CTA
  display.setCursor(18, 0);
  display.print(getCTA());

  // talking effect
  if (emotion >= 4) {
    mouthState = random(1, 4);
  }

  // shock override
  if (shock) {
    drawMouth(5);
    shock = false;
  } else {
    drawMouth(mouthState);
  }

  display.display();
}

// ===== SENSOR =====
void readSensor() {
  uint16_t srawVoc = 0, srawNox = 0;
  sgp41.measureRawSignals(0x8000, 0x6666, srawVoc, srawNox);

  prevVoc = vocIndex;
  vocIndex = vocAlgo.process(srawVoc);

  filteredVOC = 0.8 * filteredVOC + 0.2 * vocIndex;

  if (vocIndex - prevVoc > 50) shock = true;
}

// ===== SETUP =====
void setup() {
  Serial.begin(115200);
  Wire.begin(8, 9);

  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  sgp41.begin(Wire);

  display.setCursor(10, 25);
  display.print("AIR BUDDY");
  display.display();

  delay(2000);
}

// ===== LOOP =====
void loop() {

  // sensor every 1 sec
  if (millis() - lastRead >= 1000) {
    lastRead = millis();

    readSensor();
    updateBaseline();
    updateEmotion();
    buzzer();

    Serial.println(vocIndex);
  }

  // blink
  if (millis() - lastBlink > random(2000, 5000)) {
    lastBlink = millis();
    blinkNow = true;
  }

  if (blinkNow) {
    display.clearDisplay();
    display.fillRoundRect(20, 30, 30, 2, 5, WHITE);
    display.fillRoundRect(78, 30, 30, 2, 5, WHITE);
    display.display();
    delay(80);
    blinkNow = false;
  }

  drawFace();
}