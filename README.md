# DigiHealth

**DigiHealth** is a wearable health monitoring system that tracks real-time 
biometric data — steps, heart rate, and BMI — performs on-device anomaly 
detection, and computes a personalized wellness score using a machine 
learning model.

The system consists of two components: an ESP-based firmware device that collects and transmits sensor data, and a Node.js backend that processes it, stores it, and serves it to a web frontend.

---

## Demo
[!DigiHealth Demo](https://youtu.be/w0B6E8TBsb0)

## System Overview

```
                           
                         HTTP (API Key)                               
   ESP8266 Firmware    ──────────────────────▶    Node.js Backend    
                         steps, BPM,               + MongoDB          
                         anomaly flag,             + Python ML Model  
                         pairing code                                  
                                                             │
                                                   REST API (JWT)
                                                             │
                                                             ▼
                                                  ┌──────────────────────┐                 
                                                       Web Frontend      
                                                    (served by backend)                                                      
```

---

## Repository Structure

```
DigiHealth/
├── backend/               # Node.js server, REST API, ML inference
│   └── README.md          # Backend setup and API reference
├── firmware/              # Arduino/ESP sketch and sensor logic
│   └── README.md          # Firmware setup and wiring guide
├── LICENSE                # MIT License
└── README.md              # You are here
```

---

## Features

- Real-time step count and heart rate monitoring from a wearable device
- On-device anomaly detection using a logistic regression model (runs on the ESP8266)
- Secure device pairing using time-limited codes
- User accounts with JWT authentication
- BMI calculation from user profile
- ML-powered wellness score computed from biometric data
- Web dashboard served directly by the backend

---

## Getting Started

Each component has its own setup guide:

- **[Backend README](./backend/README.md)** — Node.js server setup, environment variables, API reference, and ML model instructions
- **[Firmware README](./firmware/README.md)** — Arduino/ESP wiring, dependencies, and configuration for sending data to the backend

---

## Tech Stack

| Component | Technology |
|---|---|
| Firmware | ESP8266, MAX30105, MPU6050, SSD1306 |
| Backend | Node.js, Express, MongoDB |
| Auth | JWT, bcryptjs |
| ML Model | Python, scikit-learn, pandas |
| Frontend | HTML/CSS/JS (served statically) |

---

## Contributors

| Name | Contribution |
|---|---|
| Zhamakochyan Mane | Architecture, backend server, ML pipeline, firmware, device pairing system, API integration, and frontend (dashboard page) |
| Danoyan Hakob | Initial HTML and CSS frontend structure |
| Sirunyan Artur | Initial HTML and CSS frontend structure |
| Azoyan Narek | Initial user registration and login authentication — adapted and extended by Zhamakochyan Mane with device model, data ingestion endpoints, and API integration |

> If you have any questions or suggestions, feel free to open an issue or reach out.

