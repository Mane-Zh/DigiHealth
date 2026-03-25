// ===== DigiHealth Watch (ESP8266) =====
// OLED + Time + Steps (MPU6050) + MAX30105 HR + On-device ML + Pairing + Send to Backend

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <time.h>
#include <math.h>
#include <MPU6050.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <MAX30105.h>
#include <string.h>
#include "heartRate.h"
#include "secrets.h"
// ======== ON-DEVICE ML HEADER ========
#include "logistic_model.h"   // must be in the same folder

// ======== BACKEND ENDPOINTS  =========
String URL_PAIR_REGISTER = String(SERVER_HOST) + "/api/pairing/register";
String URL_INGEST_STEPS  = String(SERVER_HOST) + "/ingest/steps";
String URL_INGEST_PULSE  = String(SERVER_HOST) + "/ingest/pulse";

// ================== TIME (NTP) ==================
const char* NTP1 = "pool.ntp.org";
const char* NTP2 = "time.google.com";
const char* NTP3 = "time.cloudflare.com";
const long   GMT_OFFSET_SEC = 4 * 3600; // Asia/Yerevan UTC+4
const int    DST_OFFSET_SEC = 0;

// ================== OLED ==================
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDR 0x3C
#define OLED_RESET -1

// ESP8266 I2C pins
const int SDA_PIN = 4;   // D2
const int SCL_PIN = 5;   // D1

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ================== DEVICE ID + PAIRING ==================
String DEVICE_ID;    // MAC
String pairingCode;  // 6 digits

String generatePairingCode() {
  return String(random(100000, 999999));
}

// ================== MPU6050 STEP COUNTER ==================
// Notes for tuning sensitivity and accuracy:
// Acceleration magnitude is ~1.0 at rest (gravity)
// A step is detected when accel rises above HIGH and then drops below LOW
// Lower thresholds -> more sensitive (detects slow walking, more false positives)
// Higher thresholds -> stricter (fewer false positives, may miss gentle steps)
MPU6050 mpu;
const float accelThresholdHigh = 1.06f; 
const float accelThresholdLow  = 0.95f; 
const float gyroThreshold      = 100.0f;
const unsigned long debounceTime    = 300;
const unsigned long minStepDuration = 150;

const int   filterWindow = 4;
float       accelBuffer[filterWindow];
int         bufferIndex = 0;
bool        stepInProgress = false;
unsigned long lastStepTime  = 0;
unsigned long stepStartTime = 0;
unsigned long lastIMURead   = 0;
const unsigned long IMU_DT_MS = 20; // 50 Hz

long stepCount = 0;


// ================== HEART / ML STATE + HELPERS ==================
MAX30105 particleSensor;

uint32_t lastBeatMs = 0;
float bpm_smooth = 0.0f;
float last_bpm = 0.0f;

int   heartRateBPM = 0;
float ml_p = 0.0f;
bool  ml_alert = false;

volatile bool beatJustDetected = false;

// ML helper
static inline float sigmoidf(float x){ return 1.0f/(1.0f+expf(-x)); }
float predict_prob(const float feat[], int n) {
  float z = B;
  for (int i = 0; i < n; i++) {
    float xs = (feat[i] - CENTER[i]) / (SCALE[i] + 1e-6f);
    z += W[i] * xs;
  }
  return sigmoidf(z);
}

// ================== UI STATE ==================
uint8_t currentScreen = 0; // 0 time, 1 steps, 2 heart
bool needsRedraw = true;

char timeHHMM[6]  = "00:00";
char dateLine[16] = "";
unsigned long lastBlinkMs = 0;
bool blinkColon = true;

unsigned long lastPoll = 0;
const unsigned long POLL_MS = 500;

// ================== SENDING TIMERS ==================
unsigned long lastSendSteps = 0;
unsigned long lastSendPulse = 0;
const unsigned long SEND_STEPS_MS = 5000;
const unsigned long SEND_PULSE_MS = 2000;

long lastSentSteps = -1;
int  lastSentBPM   = -1;

// ================== UI HELPERS ==================
void textCenter(const char* s, int y, uint8_t sz=1) {
  display.setTextSize(sz);
  int16_t x1, y1; uint16_t w, h;
  display.getTextBounds(String(s), 0, y, &x1, &y1, &w, &h);
  int x = (SCREEN_WIDTH - w)/2;
  display.setCursor(x, y);
  display.print(s);
}

void splash(const char* line1, const char* line2=nullptr) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  textCenter(line1, 18, 1);
  if (line2) textCenter(line2, 36, 1);
  display.display();
}

// ================== TIME HELPERS ==================
bool timeIsSet() {
  time_t now = time(nullptr);
  return now > 8 * 3600 * 2;
}

bool getLocalTimeCompat(struct tm* info) {
  if (!timeIsSet()) return false;
  time_t now = time(nullptr);
  *info = *localtime(&now);
  return true;
}

void formatTimeAndDate() {
  struct tm t;
  if (!getLocalTimeCompat(&t)) return;
  snprintf(timeHHMM, sizeof(timeHHMM), "%02d:%02d", t.tm_hour, t.tm_min);
  static const char* wdays[7] = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
  static const char* months[12]={"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
  snprintf(dateLine, sizeof(dateLine), "%s %02d %s", wdays[t.tm_wday], t.tm_mday, months[t.tm_mon]);
}

// ================== WIFI + NTP ==================
void connectWiFi(uint32_t timeout_ms=20000) {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  splash("Connecting Wi-Fi...");
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < timeout_ms) {
    delay(250);
    yield();
  }
  if (WiFi.status() == WL_CONNECTED) {
    splash("Wi-Fi connected", WiFi.localIP().toString().c_str());
    delay(800);
  } else {
    splash("Wi-Fi FAILED");
    delay(1200);
  }
}

bool syncTimeOnce(const char* server, uint32_t wait_ms=12000) {
  configTime(GMT_OFFSET_SEC, DST_OFFSET_SEC, server);
  uint32_t t0 = millis();
  while (!timeIsSet() && millis() - t0 < wait_ms) {
    delay(250);
    yield();
  }
  if (timeIsSet()) { formatTimeAndDate(); return true; }
  return false;
}

void ensureTimeWithRetries() {
  if (WiFi.status() != WL_CONNECTED) return;
  if (syncTimeOnce(NTP1)) return;
  if (syncTimeOnce(NTP2)) return;
  if (syncTimeOnce(NTP3)) return;
}

// ================== HTTP HELPERS ==================
bool httpPostJson(const String& url, const String& json, bool addApiKey) {
  if (WiFi.status() != WL_CONNECTED) return false;

  WiFiClient client;
  HTTPClient http;

  if (!http.begin(client, url)) return false;
  http.addHeader("Content-Type", "application/json");
  if (addApiKey) http.addHeader("x-api-key", API_KEY);

  int code = http.POST(json);
  http.end();
  return (code >= 200 && code < 300);
}

// ================== PAIRING ==================
void doPairingRegister() {
  pairingCode = generatePairingCode();

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0,0);
  display.println("DigiHealth Watch");
  display.println("----------------");
  display.println("Pairing Code:");
  display.setTextSize(2);
  display.println(pairingCode);
  display.setTextSize(1);
  display.display();

  String body = "{\"deviceId\":\"" + DEVICE_ID + "\",\"pairingCode\":\"" + pairingCode + "\"}";
  bool ok = httpPostJson(URL_PAIR_REGISTER, body, false);

  Serial.print("Pair register: ");
  Serial.println(ok ? "OK" : "FAIL");
  delay(10000); // show pairing code for 10 seconds
}

// ================== STEP COUNTER ==================
void initStepCounter() {
  mpu.initialize();
  for (int i = 0; i < filterWindow; i++) accelBuffer[i] = 1.0f;
  stepInProgress = false;
  lastStepTime = stepStartTime = 0;
}

void updateStepCounter() {
  if (millis() - lastIMURead < IMU_DT_MS) return;
  lastIMURead = millis();

  int16_t ax_raw, ay_raw, az_raw;
  int16_t gx_raw, gy_raw, gz_raw;
  mpu.getMotion6(&ax_raw, &ay_raw, &az_raw, &gx_raw, &gy_raw, &gz_raw);

  float ax = ax_raw / 16384.0f;
  float ay = ay_raw / 16384.0f;
  float az = az_raw / 16384.0f;
  float gx = gx_raw / 131.0f;
  float gy = gy_raw / 131.0f;
  float gz = gz_raw / 131.0f;

  float accelMag = sqrtf(ax*ax + ay*ay + az*az);
  float gyroMag  = sqrtf(gx*gx + gy*gy + gz*gz);

  accelBuffer[bufferIndex] = accelMag;
  bufferIndex = (bufferIndex + 1) % filterWindow;

  float accelFiltered = 0.0f;
  for (int i = 0; i < filterWindow; i++) accelFiltered += accelBuffer[i];
  accelFiltered /= filterWindow;

  unsigned long now = millis();

  if (gyroMag < gyroThreshold) {
    if (!stepInProgress && accelFiltered > accelThresholdHigh && (now - lastStepTime) > debounceTime) {
      stepInProgress = true;
      stepStartTime = now;
    }
    if (stepInProgress && accelFiltered < accelThresholdLow) {
      stepInProgress = false;
      if ((now - stepStartTime) >= minStepDuration) {
        lastStepTime = now;
        stepCount++;
        if (currentScreen == 1) needsRedraw = true;
      }
    }
  } else {
    stepInProgress = false;
  }
}


// ================== HEART / ML UPDATE ==================
void updateHeart_RR_ML() {
  long irValue = particleSensor.getIR();

  // No finger / weak signal
  if (irValue < 50000) {
    heartRateBPM = 0;
    bpm_smooth = 0.0f;
    last_bpm = 0.0f;
    lastBeatMs = 0;
    ml_p = 0.0f;
    ml_alert = false;
    beatJustDetected = false;
    return;
  }

  if (!checkForBeat(irValue)) {
    return;
  }

  uint32_t now = millis();
  beatJustDetected = false;

  // Need a previous beat to compute IBI
  if (lastBeatMs == 0) {
    lastBeatMs = now;
    return;
  }

  uint32_t ibi = now - lastBeatMs;
  lastBeatMs = now;

  // Ignore unrealistic intervals
  if (ibi <= 400 || ibi >= 1500) {
    return;
  }

  beatJustDetected = true;

  float bpm_new = 60000.0f / (float)ibi;

  if (bpm_smooth == 0.0f) {
    bpm_smooth = bpm_new;
  } else {
    bpm_smooth = 0.2f * bpm_new + 0.8f * bpm_smooth;
  }

  float diff_bpm = bpm_smooth - last_bpm;
  last_bpm = bpm_smooth;

  heartRateBPM = (int)(bpm_smooth + 0.5f);

  float feat[NFEAT];
  feat[0] = (float)heartRateBPM;
  feat[1] = diff_bpm;

  ml_p = predict_prob(feat, NFEAT);
  ml_alert = (ml_p >= THRESHOLD);

  if (currentScreen == 2) {
    needsRedraw = true;
  }
}
// ================== UI DRAW ==================
void drawScreen(uint8_t s) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0,0);

  if (s == 0) {
    display.println("Time");
    if (timeIsSet()) formatTimeAndDate();
    char buf[6]; strncpy(buf, timeHHMM, sizeof(buf));
    if (!blinkColon) buf[2] = ' ';
    display.setTextSize(3);
    display.setCursor(8, 18);
    display.print(buf);
    display.setTextSize(1);
    display.setCursor(8, 46);
    if (dateLine[0] != '\0') display.print(dateLine); else display.print("-- -- ---");

  } else if (s == 1) {
    display.println("Steps");
    display.setTextSize(3);
    display.setCursor(10, 26);
    display.print(stepCount);
    display.setTextSize(1);
    display.setCursor(0, 56);
    display.print("1: rotate  R: reset  T: resync");

  } else {
    display.println("Heart Rate");
    display.setTextSize(3);
    display.setCursor(0, 16);
    if (heartRateBPM > 0) display.print(heartRateBPM); else display.print("--");
    display.setTextSize(1);
    display.print(" bpm");

    display.setCursor(96, 0);
    display.println(ml_alert ? "ANOM" : "OK");
    display.setCursor(96, 10);
    display.print("p=");
    display.println(ml_p, 2);

    display.setCursor(0, 56);
    display.print("IR ok");
  }

  display.display();
}

// ================== SEND TO BACKEND ==================
void maybeSendSteps() {
  unsigned long now = millis();
  if (now - lastSendSteps < SEND_STEPS_MS) return;

  if (stepCount == lastSentSteps) {
    lastSendSteps = now;
    return;
  }

  String body = "{\"deviceId\":\"" + DEVICE_ID + "\",\"steps\":" + String(stepCount) + "}";
  bool ok = httpPostJson(URL_INGEST_STEPS, body, true);

  if (ok) lastSentSteps = stepCount;
  lastSendSteps = now;
}

void maybeSendPulse() {
  unsigned long now = millis();
  if (now - lastSendPulse < SEND_PULSE_MS) return;

  bool shouldSend = false;
  if (heartRateBPM != lastSentBPM && heartRateBPM > 0) shouldSend = true;
  if (beatJustDetected) shouldSend = true;

  if (!shouldSend) {
    lastSendPulse = now;
    beatJustDetected = false;
    return;
  }

  bool beat = beatJustDetected;
  beatJustDetected = false;

  String body = "{\"deviceId\":\"" + DEVICE_ID + "\",\"bpm\":" + String(heartRateBPM) +
                ",\"beat\":" + String(beat ? "true" : "false") + "}";

  bool ok = httpPostJson(URL_INGEST_PULSE, body, true);

  if (ok && heartRateBPM > 0) lastSentBPM = heartRateBPM;
  lastSendPulse = now;
}

// ================== ARDUINO ==================
void setup() {
  Serial.begin(115200);
  randomSeed(analogRead(A0));

  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(100000); //standard I2C (more stable for multi-device setups)

  // OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("SSD1306 not found");
    while (1) {}
  }
  splash("OLED ready");

  // MAX30105
  if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) {
    splash("MAX30105 FAIL");
    Serial.println("MAX30105 not found");
    while (1);
  }
  particleSensor.setup();                 // default
  particleSensor.setPulseAmplitudeRed(0x3F);   // stronger LED (more reliable)
  particleSensor.setPulseAmplitudeGreen(0);

  // Steps
  initStepCounter();
  splash("Step counter", "ready");
  delay(400);

  // WiFi
  connectWiFi();

  DEVICE_ID = WiFi.macAddress();
  Serial.print("DEVICE_ID: ");
  Serial.println(DEVICE_ID);

  ensureTimeWithRetries();
  if (timeIsSet()) formatTimeAndDate();

  doPairingRegister();

  needsRedraw = true;
}

void loop() {
  // Serial commands
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '1') {
      currentScreen = (currentScreen + 1) % 3;
      needsRedraw = true;
    } else if (c == 'T' || c == 't') {
      if (WiFi.status() == WL_CONNECTED) {
        ensureTimeWithRetries();
        if (timeIsSet()) { formatTimeAndDate(); needsRedraw = true; }
      }
    } else if (c == 'R' || c == 'r') {
      stepCount = 0;
      stepInProgress = false;
      lastStepTime = stepStartTime = 0;
      lastSentSteps = -1;
      if (currentScreen == 1) needsRedraw = true;
    }
  }

  updateStepCounter();
  updateHeart_RR_ML();

  // Time screen blink
  if (currentScreen == 0 && millis() - lastBlinkMs > 1000) {
    lastBlinkMs = millis();
    blinkColon = !blinkColon;
    needsRedraw = true;
  }

  // Minute refresh
  if (millis() - lastPoll > POLL_MS && timeIsSet()) {
    lastPoll = millis();
    static int lastMin = -1;
    struct tm t;
    if (getLocalTimeCompat(&t)) {
      if (t.tm_min != lastMin) {
        lastMin = t.tm_min;
        formatTimeAndDate();
        if (currentScreen == 0) needsRedraw = true;
      }
    }
  }

  // Retry NTP
  static unsigned long lastRetry = 0;
  if (!timeIsSet() && WiFi.status() == WL_CONNECTED) {
    if (millis() - lastRetry > 15000) {
      lastRetry = millis();
      ensureTimeWithRetries();
      if (timeIsSet()) { formatTimeAndDate(); needsRedraw = true; }
    }
  }

  maybeSendSteps();
  maybeSendPulse();

  if (needsRedraw) {
    drawScreen(currentScreen);
    needsRedraw = false;
  }

  yield();
}
