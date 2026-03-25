# DigiHealth Firmware

Firmware for the **DigiHealth** wearable health monitoring system. Runs on an ESP8266 and handles sensor data acquisition, step detection, on-device anomaly detection using a logistic regression model, and real-time communication with the DigiHealth backend.

---

## Contents

1. [Hardware Components](#1-hardware-components)
2. [Project Structure](#2-project-structure)
3. [Setup & Installation](#3-setup--installation)
4. [Core Logic](#4-core-logic)
5. [On-Device Machine Learning](#5-on-device-machine-learning)
6. [Limitations](#6-limitations)

---

## 1. Hardware Components

| Component | Role |
|---|---|
| ESP8266 | Microcontroller / Wi-Fi |
| MAX30105 | Heart rate sensor (IR) |
| MPU6050 | Accelerometer + gyroscope |
| SSD1306 | OLED display |

---

## 2. Project Structure

```
firmware/
├── firmware.ino          # Main sketch
├── logistic_model.h      # Trained model parameters (C++ header)
├── secrets.h             # Wi-Fi and API credentials (gitignored)
├── secrets_example.h     # Credentials template
├── train_logreg.py       # Python script to retrain and export the model
└── README.md
```

---

## 3. Setup & Installation

### Wiring

All sensors share the same I2C bus.

| ESP8266 Pin | Connects To |
|---|---|
| 3.3V | VCC (all modules) |
| GND | GND (all modules) |
| D2 (GPIO4) | SDA |
| D1 (GPIO5) | SCL |

> All devices are connected in parallel on SDA/SCL. Ensure all components support 3.3V and keep wiring short to avoid I2C instability.

### Arduino IDE Setup

Install the ESP8266 board support by adding these URLs under **File → Preferences → Additional Boards Manager URLs**:


https://arduino.esp8266.com/stable/package_esp8266com_index.json

https://dl.espressif.com/dl/package_esp32_index.json


Then install a board from **Tools → Board → Boards Manager**. If unsure, use **NodeMCU 1.0 (ESP-12E)**.

### Required Libraries

Install the following via **Library Manager**:

- Adafruit SSD1306
- Adafruit GFX
- SparkFun MAX3010x Pulse and Proximity Sensor 
- MPU6050 (by Electronic Cats or Jeff Rowberg)
- ESP8266WiFi
- ESP8266HTTPClient

### Credentials

1. Copy `secrets_example.h` and rename it to `secrets.h`
2. Fill in your values:

```cpp
const char* WIFI_SSID     = "your_wifi_name";
const char* WIFI_PASSWORD = "your_password";
const char* SERVER_HOST   = "http://your-server:8080";
const char* API_KEY       = "your_api_key";
```

> `secrets.h` is gitignored and will never be committed.

### Model File

The firmware requires `logistic_model.h` to be present in the same folder. You have two options:

**Option 1 (recommended):** Use the pre-trained `logistic_model.h` already included in this repository.

**Option 2 (retrain):** Run the Python training script to generate your own model parameters:

```bash
pip install numpy scikit-learn
python train_logreg.py
```

This will overwrite `logistic_model.h` with freshly trained parameters.

### Upload

Select your board, then compile and upload the sketch. On first boot the device will:

1. Connect to Wi-Fi
2. Display a pairing code on the OLED
3. Begin sending sensor data to the backend

---

## 4. Core Logic

### System Overview

```
Sensors (IR + Accelerometer)
        │
        ▼
Signal Processing
  ├── Heart rate → BPM
  └── Motion     → Steps
        │
        ▼
Feature Extraction
  ├── BPM
  └── ΔBPM
        │
        ▼
Normalization → Logistic Regression → Anomaly Flag
        │
        ▼
  ┌─────┴─────┐
OLED Display  Backend (Wi-Fi)
```

### Main Loop

Each cycle performs:

| Step | Description |
|---|---|
| Sensor update | Reads accelerometer/gyro and IR signal |
| Signal processing | Extracts BPM and step count |
| ML inference | Computes anomaly probability |
| Display update | Refreshes OLED with current stats |
| Data transmission | Pushes steps and BPM to backend periodically |
| Time sync | NTP sync every minute |

Serial commands are also available: `1` to switch screen, `R` to reset steps, `T` to resync time.

### Step Detection (MPU6050)

Steps are detected using acceleration magnitude, making the algorithm orientation-independent:

$$accelMag = \sqrt{x^2 + y^2 + z^2}$$

At rest this value is approximately 1.0 g (gravity). During walking it oscillates above and below 1.0. A step is counted when:

1. Magnitude rises above the high threshold
2. Then drops below the low threshold
3. Motion duration and timing constraints are satisfied
4. Gyroscope rotation is within limits

**Tunable parameters:**

```cpp
accelThresholdHigh = 1.06
accelThresholdLow  = 0.95
gyroThreshold      = 100
debounceTime       = 300 ms
minStepDuration    = 150 ms
```

Lower thresholds increase sensitivity but may introduce false positives. Higher thresholds are stricter but may miss gentle steps.

### Heart Rate Processing (MAX30105)

Heart rate is extracted from the IR signal using beat detection:

$$IBI = t_{current} - t_{previous}$$

$$BPM = \frac{60000}{IBI}$$

An exponential moving average (EMA) is applied to smooth the signal:

$$BPM_{smooth} = 0.2 \times BPM_{new} + 0.8 \times BPM_{smooth}$$

Where α = 0.2 — lower values produce a smoother but slower response.

Two features are then extracted for ML inference:

- **BPM** — current smoothed heart rate
- **ΔBPM** — change from the previous value: `diff_bpm = bpm_smooth - last_bpm`

---

## 5. On-Device Machine Learning

The firmware runs a logistic regression model entirely on the ESP8266 — no server-side inference required.

### Purpose

The model estimates whether the current heart-rate pattern is anomalous. Normal heart rate changes are gradual; sudden spikes or irregular patterns increase the anomaly probability.

### Model Input

| Feature | Description |
|---|---|
| x₁ = BPM | Absolute heart rate level |
| x₂ = ΔBPM | Magnitude of sudden change |

Together they detect both sustained abnormal rates and sudden irregular events.

### Inference Pipeline

**1. Normalize features:**

$$x_{scaled} = \frac{x - \mu}{\sigma}$$

**2. Compute logistic regression:**

$$z = b + w_1 x_1 + w_2 x_2$$

$$p = \frac{1}{1 + e^{-z}}$$

**3. Apply decision threshold:**

```cpp
ml_alert = (ml_p >= THRESHOLD);
```

If p ≥ threshold → anomaly detected. Otherwise → normal.

### Embedded Model (`logistic_model.h`)

The trained model is exported from Python as a C++ header containing all parameters needed for inference:

| Constant | Description |
|---|---|
| `W[]` | Model weights |
| `B` | Bias term |
| `CENTER[]` | Feature means (normalization) |
| `SCALE[]` | Feature standard deviations |
| `THRESHOLD` | Decision cutoff (default: 0.6) |

---

## 6. Limitations

- Model is trained on synthetic data and has not been validated on real users
- Temperature sensing not yet implemented
- Step detection accuracy may vary across users and movement styles
