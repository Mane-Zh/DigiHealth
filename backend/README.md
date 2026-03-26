# DigiHealth Backend

Backend server for the **DigiHealth** wearable health monitoring system. It receives real-time biometric data from a paired ESP device, computes wellness scores using a machine learning model, and exposes a REST API consumed by the frontend.

---

## Features

- User registration and login with JWT authentication
- BMI calculation from user profile (weight & height)
- Device pairing system using time-limited codes
- Real-time ingestion of steps and heart rate (BPM) from ESP device
- ML-powered wellness score via a Python model (`predict_wellness.py`)
- Static frontend serving with SPA fallback

---

## Tech Stack

| Layer | Technology |
|---|---|
| Runtime | Node.js |
| Framework | Express 5 |
| Database | MongoDB (Mongoose) |
| Auth | JWT + bcryptjs |
| ML inference | Python 3 + scikit-learn (joblib) |

---

## Project Structure

```
├── ML/
│   ├── generate_dataset.py     # Generates synthetic training data
│   ├── predict_wellness.py     # ML inference script called by the server
│   ├── synthetic_wellness.csv  # Generated dataset (output of above)
│   ├── train_model.py          # Trains and saves the wellness model
│   └── wellness_model.pkl      # Trained model file (gitignored, generate locally)
├── middleware/
│   └── auth.js                 # JWT auth middleware
├── models/
│   └── device.js               # Mongoose Device model
├── node_modules/               # Auto-generated, not committed
├── public/
│   └── index.html              # Frontend static files
├── routes/
│   ├── pairingClaim.js         # User → Server: claim device with code
│   └── pairingRegister.js      # Device → Server: register pairing code
├── .env                        # Local environment variables (gitignored)
├── .env.example                # Environment variable template
├── .gitignore
├── package-lock.json           # Auto-generated lockfile
├── package.json
├── README.md
└── server.js                   # Main Express app and all core routes
```

---

## Getting Started

### Prerequisites

- Node.js v18+
- MongoDB (local or Atlas)
- Python 3 with `scikit-learn`, `joblib`, and `pandas` installed

### Installation

```bash
# 1. Clone the repository
git clone <repo-url>
cd DigiHealth/backend

# 2. Install Node dependencies
npm install

# 3. Install Python dependencies
pip install scikit-learn joblib pandas

# 4. Set up environment variables
cp .env.example .env
# Edit .env with your values

# 5. Generate the ML model
python ML/generate_dataset.py   # creates synthetic_wellness.csv
python ML/train_model.py        # creates wellness_model.pkl

# 6. Start the server
npm start
```

---

## Environment Variables

Create a `.env` file based on `.env.example`:

```env
MONGO_URI=mongodb://127.0.0.1:27017/digihealth
JWT_SECRET=your_jwt_secret_here
DEVICE_API_KEY=your_secret_key_here
PORT=8080
```

---

## API Reference

### Auth

| Method | Endpoint | Auth | Description |
|---|---|---|---|
| POST | `/register` | - | Register a new user |
| POST | `/login` | - | Login and receive JWT |
| GET | `/me` | + | Get current user info |

### Profile

| Method | Endpoint | Auth | Description |
|---|---|---|---|
| GET | `/api/profile` | + | Get weight, height, BMI |
| POST | `/api/profile` | + | Update weight & height (auto-computes BMI) |

### Device Data

| Method | Endpoint | Auth | Description |
|---|---|---|---|
| GET | `/api/latest` | JWT | Get latest steps, BPM, and timestamp |
| POST | `/ingest/steps` | API Key | ESP pushes step count |
| POST | `/ingest/pulse` | API Key | ESP pushes BPM and beat flag |

### Wellness

| Method | Endpoint | Auth | Description |
|---|---|---|---|
| GET | `/api/wellness/current` | + | Get ML wellness score (requires BMI + device data) |

### Device Pairing

| Method | Endpoint | Auth | Description |
|---|---|---|---|
| POST | `/api/pairing/register` | API Key | Device registers a pairing code (valid 10 min) |
| POST | `/api/pairing/claim` | JWT | User claims a device using the pairing code |

---

## Device Pairing Flow

1. The ESP device generates a pairing code and sends it to `POST /api/pairing/register` with its `deviceId`.
2. The user enters that code in the frontend and submits it to `POST /api/pairing/claim`.
3. The server links the device to the authenticated user. The code expires after **10 minutes**.

---

## ML Wellness Score

The wellness score is computed by a Python script (`ML/predict_wellness.py`) that receives a JSON payload via stdin and returns a score via stdout.

**Input features:**

| Feature | Source |
|---|---|
| `bmi` | User profile |
| `steps` | Latest device data |
| `heart_rate` | Latest device data |
| `temperature` | Hardcoded placeholder (`36.7°C`) — future temperature sensor |

The script loads `ML/wellness_model.pkl` and returns:

```json
{ "wellnessScore": 78.4 }
```

> Warning!!! `wellness_model.pkl` is not included in this repository. You must train or supply your own model and place it in the `ML/` directory.

---

## ESP / Arduino Device

The firmware lives in the [`firmware/`](../firmware/README.md) folder of this repository. The device communicates with this backend using a static API key passed in the `x-api-key` request header.

---

## Notes

- The default API key for ESP ingestion is hardcoded in `server.js` as a placeholder. **Replace it with a secure value before deploying.**
- JWT tokens expire after **6 hours**.
- Temperature ingestion from hardware is not yet implemented; the value is currently fixed at `36.7°C`.
