#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_AHTX0.h>
#include <Adafruit_BMP280.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

namespace {
constexpr uint8_t SDA_PIN = 6;
constexpr uint8_t SCL_PIN = 7;
constexpr int SCREEN_WIDTH = 128;
constexpr int SCREEN_HEIGHT = 64;
constexpr int HEADER_HEIGHT = 16;
constexpr int EYE_CENTER_Y = 41;
constexpr int EYE_WIDTH = 44;
constexpr int EYE_MAX_HEIGHT = 38;
constexpr int LEFT_EYE_X = 12;
constexpr int RIGHT_EYE_X = 72;
constexpr uint32_t FRAME_INTERVAL_MS = 35;
constexpr uint32_t SENSOR_INTERVAL_MS = 2000;

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
Adafruit_AHTX0 aht;
Adafruit_BMP280 bmp;

bool oledReady = false;
bool ahtReady = false;
bool bmpReady = false;
uint8_t oledAddress = 0;
uint8_t bmpAddress = 0;

float temperatureC = 0.0f;
float humidityPercent = 0.0f;
float pressureHpa = 0.0f;
bool temperatureValid = false;
bool humidityValid = false;
bool pressureValid = false;
char headerText[32] = "--.-C --% ----hPa";

struct Animation {
  uint32_t nextBlinkAt = 0;
  uint32_t blinkStartedAt = 0;
  uint32_t blinkDuration = 180;
  uint8_t winkEye = 0;
  bool blinking = false;

  uint32_t nextGazeAt = 0;
  uint32_t gazeStartedAt = 0;
  uint32_t gazeDuration = 400;
  float gazeXFrom = 0.0f;
  float gazeXTo = 0.0f;
  float gazeYFrom = 0.0f;
  float gazeYTo = 0.0f;
  float gazeX = 0.0f;
  float gazeY = 0.0f;

  uint32_t nextExpressionAt = 0;
  uint32_t expressionStartedAt = 0;
  uint32_t expressionDuration = 400;
  float eyeHeightFrom = EYE_MAX_HEIGHT;
  float eyeHeightTo = EYE_MAX_HEIGHT;
  float eyeHeight = EYE_MAX_HEIGHT;
};

Animation animation;

float smoothStep(float progress) {
  return progress * progress * (3.0f - 2.0f * progress);
}

void formatHeader() {
  char temperatureText[10];
  char humidityText[8];
  char pressureText[12];

  if (temperatureValid) {
    snprintf(temperatureText, sizeof(temperatureText), "%.1fC", temperatureC);
  } else {
    strcpy(temperatureText, "--.-C");
  }

  if (humidityValid) {
    snprintf(humidityText, sizeof(humidityText), "%.0f%%", humidityPercent);
  } else {
    strcpy(humidityText, "--%");
  }

  if (pressureValid) {
    snprintf(pressureText, sizeof(pressureText), "%.0fhPa", pressureHpa);
  } else {
    strcpy(pressureText, "----hPa");
  }

  snprintf(headerText, sizeof(headerText), "%s %s %s", temperatureText,
           humidityText, pressureText);
}

void updateSensors(uint32_t now) {
  static uint32_t lastUpdateAt = 0;
  static bool hasUpdated = false;
  if (hasUpdated && now - lastUpdateAt < SENSOR_INTERVAL_MS) {
    return;
  }
  lastUpdateAt = now;
  hasUpdated = true;

  if (ahtReady) {
    sensors_event_t humidityEvent;
    sensors_event_t temperatureEvent;
    aht.getEvent(&humidityEvent, &temperatureEvent);

    temperatureC = temperatureEvent.temperature;
    humidityPercent = humidityEvent.relative_humidity;
    temperatureValid = isfinite(temperatureC) &&
                       temperatureC >= -40.0f && temperatureC <= 85.0f;
    humidityValid = isfinite(humidityPercent) &&
                    humidityPercent >= 0.0f && humidityPercent <= 100.0f;
  }

  if (bmpReady) {
    pressureHpa = bmp.readPressure() / 100.0f;
    pressureValid =
        isfinite(pressureHpa) && pressureHpa >= 300.0f && pressureHpa <= 1100.0f;
  }

  formatHeader();
}

void reportStatus(uint32_t now) {
  static uint32_t lastReportAt = 0;
  if (now - lastReportAt < 5000) {
    return;
  }

  lastReportAt = now;
  Serial.println(headerText);
}

float updateBlink(uint32_t now) {
  if (!animation.blinking &&
      static_cast<int32_t>(now - animation.nextBlinkAt) >= 0) {
    animation.blinking = true;
    animation.blinkStartedAt = now;
    animation.blinkDuration = random(160, 221);
    animation.winkEye = random(0, 5) == 0 ? random(1, 3) : 0;
  }

  if (!animation.blinking) {
    return 1.0f;
  }

  const uint32_t elapsed = now - animation.blinkStartedAt;
  if (elapsed >= animation.blinkDuration) {
    animation.blinking = false;
    animation.winkEye = 0;
    animation.nextBlinkAt =
        now + (random(0, 5) == 0 ? 180 : random(2200, 4501));
    return 1.0f;
  }

  const float progress =
      static_cast<float>(elapsed) / animation.blinkDuration;
  return progress < 0.5f ? 1.0f - progress * 2.0f
                         : (progress - 0.5f) * 2.0f;
}

void updateGaze(uint32_t now) {
  if (static_cast<int32_t>(now - animation.nextGazeAt) >= 0) {
    animation.gazeXFrom = animation.gazeX;
    animation.gazeYFrom = animation.gazeY;
    animation.gazeXTo = random(-5, 6);
    animation.gazeYTo = random(-3, 4);
    animation.gazeStartedAt = now;
    animation.gazeDuration = random(250, 651);
    animation.nextGazeAt = now + random(1200, 3001);
  }

  const uint32_t elapsed = now - animation.gazeStartedAt;
  if (elapsed >= animation.gazeDuration) {
    animation.gazeX = animation.gazeXTo;
    animation.gazeY = animation.gazeYTo;
    return;
  }

  const float progress =
      smoothStep(static_cast<float>(elapsed) / animation.gazeDuration);
  animation.gazeX =
      animation.gazeXFrom + (animation.gazeXTo - animation.gazeXFrom) * progress;
  animation.gazeY =
      animation.gazeYFrom + (animation.gazeYTo - animation.gazeYFrom) * progress;
}

float updateExpression(uint32_t now) {
  if (static_cast<int32_t>(now - animation.nextExpressionAt) >= 0) {
    animation.eyeHeightFrom = animation.eyeHeight;
    switch (random(0, 4)) {
      case 0:
        animation.eyeHeightTo = EYE_MAX_HEIGHT;
        break;
      case 1:
        animation.eyeHeightTo = 19.0f;
        break;
      case 2:
        animation.eyeHeightTo = 38.0f;
        break;
      default:
        animation.eyeHeightTo = 26.0f;
        break;
    }
    animation.expressionStartedAt = now;
    animation.expressionDuration = random(300, 601);
    animation.nextExpressionAt = now + random(1800, 4501);
  }

  const uint32_t elapsed = now - animation.expressionStartedAt;
  if (elapsed >= animation.expressionDuration) {
    animation.eyeHeight = animation.eyeHeightTo;
    return animation.eyeHeight;
  }

  const float progress =
      smoothStep(static_cast<float>(elapsed) / animation.expressionDuration);
  animation.eyeHeight = animation.eyeHeightFrom +
                        (animation.eyeHeightTo - animation.eyeHeightFrom) *
                            progress;
  return animation.eyeHeight;
}

void drawHeader() {
  display.fillRect(0, 0, SCREEN_WIDTH, HEADER_HEIGHT, SSD1306_BLACK);
  display.drawRect(0, 0, SCREEN_WIDTH, HEADER_HEIGHT, SSD1306_WHITE);
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setTextWrap(false);
  const int textWidth = static_cast<int>(strlen(headerText)) * 6;
  display.setCursor((SCREEN_WIDTH - textWidth) / 2, 4);
  display.print(headerText);
}

void drawEye(int x, int height, int gazeX, int gazeY) {
  height = constrain(height, 2, EYE_MAX_HEIGHT);
  const int eyeY = EYE_CENTER_Y - height / 2;
  const int cornerRadius = min(height / 2, 10);
  display.fillRoundRect(x, eyeY, EYE_WIDTH, height, cornerRadius, SSD1306_WHITE);

  if (height < 14) {
    return;
  }

  const int pupilRadius = min(6, height / 4);
  const int pupilX = x + EYE_WIDTH / 2 + gazeX;
  const int pupilY = EYE_CENTER_Y + gazeY;
  display.fillCircle(pupilX, pupilY, pupilRadius, SSD1306_BLACK);
  display.fillCircle(pupilX - 2, pupilY - 2, 2, SSD1306_WHITE);
}

void drawFace(uint32_t now) {
  display.clearDisplay();
  drawHeader();

  const float openness = updateBlink(now);
  const float baseEyeHeight = updateExpression(now);
  updateGaze(now);

  const float leftOpenness = animation.winkEye == 2 ? 1.0f : openness;
  const float rightOpenness = animation.winkEye == 1 ? 1.0f : openness;
  const int leftEyeHeight =
      max(2, static_cast<int>(baseEyeHeight * leftOpenness));
  const int rightEyeHeight =
      max(2, static_cast<int>(baseEyeHeight * rightOpenness));
  const int gazeX = static_cast<int>(animation.gazeX);
  const int gazeY = static_cast<int>(animation.gazeY);

  drawEye(LEFT_EYE_X, leftEyeHeight, gazeX, gazeY);
  drawEye(RIGHT_EYE_X, rightEyeHeight, gazeX, gazeY);
  display.display();
}

void initializeDisplay() {
  constexpr uint8_t addresses[] = {0x3C, 0x3D};
  for (uint8_t address : addresses) {
    Wire.beginTransmission(address);
    if (Wire.endTransmission() != 0) {
      Serial.printf("No I2C response at OLED address 0x%02X\n", address);
      continue;
    }

    if (display.begin(SSD1306_SWITCHCAPVCC, address, true, false)) {
      oledReady = true;
      oledAddress = address;
      Serial.printf("SSD1306 initialized at 0x%02X\n", address);
      return;
    }
  }

  Serial.println("OLED initialization failed; check wiring/address");
}

void initializeSensors() {
  ahtReady = aht.begin(&Wire);
  Serial.println(ahtReady ? "AHT20 ready at 0x38"
                          : "AHT20 not found at 0x38");

  if (bmp.begin(0x76)) {
    bmpAddress = 0x76;
  } else if (bmp.begin(0x77)) {
    bmpAddress = 0x77;
  }

  bmpReady = bmpAddress != 0;
  if (bmpReady) {
    bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,
                    Adafruit_BMP280::SAMPLING_X2,
                    Adafruit_BMP280::SAMPLING_X16,
                    Adafruit_BMP280::FILTER_X16,
                    Adafruit_BMP280::STANDBY_MS_500);
    Serial.printf("BMP280 ready at 0x%02X\n", bmpAddress);
  } else {
    Serial.println("BMP280 not found at 0x76 or 0x77");
  }
}
}

void setup() {
  Serial.begin(115200);
  const uint32_t serialStartedAt = millis();
  while (!Serial && millis() - serialStartedAt < 2000) {
    delay(10);
  }
  Serial.println("ESP32 keychain starting");

  Wire.begin(SDA_PIN, SCL_PIN);
  initializeDisplay();
  initializeSensors();

  randomSeed(micros());
  const uint32_t now = millis();
  animation.nextBlinkAt = now + random(1200, 3001);
  animation.nextGazeAt = now + random(500, 1501);
  animation.nextExpressionAt = now + random(1800, 3501);

  updateSensors(now);
  if (oledReady) {
    drawFace(now);
  }
}

void loop() {
  const uint32_t now = millis();
  updateSensors(now);
  reportStatus(now);

  static uint32_t lastFrameAt = 0;
  if (oledReady && now - lastFrameAt >= FRAME_INTERVAL_MS) {
    lastFrameAt = now;
    drawFace(now);
  }
}
