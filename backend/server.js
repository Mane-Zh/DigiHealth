// server.js
const express   = require("express");
const mongoose  = require("mongoose");
const bcrypt    = require("bcryptjs");
const jwt       = require("jsonwebtoken");
const cors      = require("cors");
const path      = require("path");
const { spawn } = require("child_process");
require("dotenv").config();

const app = express();

// ====== MIDDLEWARE ======
app.use(express.json());
app.use(cors());

// Serve frontend from ./public
app.use(express.static(path.join(__dirname, "public")));

// ====== CONFIG ======
const MONGO_URI  = process.env.MONGO_URI  || "mongodb://127.0.0.1:27017/digihealth";
const JWT_SECRET = process.env.JWT_SECRET || "SECRET123";

// ====== AUTH MIDDLEWARE ======
const authMiddleware = require("./middleware/auth");

// ====== CONNECT TO MONGODB ======
mongoose
  .connect(MONGO_URI, { useNewUrlParser: true, useUnifiedTopology: true })
  .then(() => console.log("✅ MongoDB connected"))
  .catch((err) => console.error("Mongo connect error:", err));

// ====== USER MODEL ======
const userSchema = new mongoose.Schema({
  username: String,
  email: { type: String, unique: true, required: true },
  password: String,

  // Profile fields for BMI
  weight: { type: Number, default: null }, // kg
  height: { type: Number, default: null }, // cm
  bmi:    { type: Number, default: null }, // computed
});

const User = mongoose.model("User", userSchema);

// ====== REGISTER ======
app.post("/register", async (req, res) => {
  try {
    const { username, email, password } = req.body;

    if (!email || !password)
      return res.status(400).json({ ok: false, message: "Email and password required" });

    const exists = await User.findOne({ email });
    if (exists) return res.status(409).json({ ok: false, message: "User already exists" });

    const hashedPassword = await bcrypt.hash(password, 10);
    await User.create({ username, email, password: hashedPassword });

    return res.json({ ok: true, message: "User registered" });
  } catch (err) {
    console.error("Register error:", err);
    return res.status(500).json({ ok: false, message: "Server error" });
  }
});

// ====== LOGIN ======
app.post("/login", async (req, res) => {
  try {
    const { email, password } = req.body;

    if (!email || !password)
      return res.status(400).json({ ok: false, message: "Email and password required" });

    const user = await User.findOne({ email });
    if (!user)
      return res.status(404).json({ ok: false, message: "User not found" });

    const valid = await bcrypt.compare(password, user.password);
    if (!valid)
      return res.status(401).json({ ok: false, message: "Invalid password" });

    const token = jwt.sign({ userId: user._id }, JWT_SECRET, { expiresIn: "6h" });

    return res.json({
      ok: true,
      token,
      user: { id: user._id, username: user.username, email: user.email }
    });
  } catch (err) {
    console.error("Login error:", err);
    return res.status(500).json({ ok: false, message: "Server error" });
  }
});

// ====== GET CURRENT USER ======
app.get("/me", authMiddleware, async (req, res) => {
  try {
    const user = await User.findById(req.userId).select("username email");
    if (!user)
      return res.status(404).json({ ok: false, message: "User not found" });

    return res.json({ ok: true, user });
  } catch (e) {
    return res.status(500).json({ ok: false, message: "Server error" });
  }
});

// ====== PROFILE GET ======
app.get("/api/profile", authMiddleware, async (req, res) => {
  try {
    const user = await User.findById(req.userId).select("weight height bmi");
    if (!user)
      return res.status(404).json({ ok: false, message: "User not found" });

    return res.json({
      ok: true,
      profile: {
        weight: user.weight,
        height: user.height,
        bmi: user.bmi
      }
    });
  } catch (err) {
    console.error("Profile GET error:", err);
    return res.status(500).json({ ok: false, message: "Server error" });
  }
});

// ====== PROFILE UPDATE (Compute BMI) ======
app.post("/api/profile", authMiddleware, async (req, res) => {
  try {
    let { weight, height } = req.body;

    if (!weight || !height)
      return res.status(400).json({ ok: false, message: "weight and height required" });

    weight = Number(weight);
    height = Number(height);

    if (weight <= 0 || height <= 0)
      return res.status(400).json({ ok: false, message: "Invalid measurements" });

    const heightM = height / 100;
    const bmi = Number((weight / (heightM * heightM)).toFixed(1));

    const user = await User.findByIdAndUpdate(
      req.userId,
      { weight, height, bmi },
      { new: true, select: "weight height bmi" }
    );

    if (!user)
      return res.status(404).json({ ok: false, message: "User not found" });

    return res.json({
      ok: true,
      profile: {
        weight: user.weight,
        height: user.height,
        bmi: user.bmi,
      }
    });
  } catch (err) {
    console.error("Profile POST error:", err);
    return res.status(500).json({ ok: false, message: "Server error" });
  }
});

// ===== DEVICE MODEL ======
const Device = require("./models/Device");

// ===== API KEY FOR ESP =====
const API_KEY = process.env.DEVICE_API_KEY;

function checkKey(req, res) {
  const key = req.headers["x-api-key"];
  if (key !== API_KEY) {
    res.status(403).json({ ok: false, message: "Invalid API key" });
    return false;
  }
  return true;
}

// ===== INTAKE: STEPS =====
app.post("/ingest/steps", async (req, res) => {
  if (!checkKey(req, res)) return;

  const { deviceId, steps } = req.body;

  if (!deviceId || steps === undefined)
    return res.status(400).json({ ok: false, message: "deviceId and steps required" });

  const device = await Device.findOne({ deviceId });
  if (!device)
    return res.status(404).json({ ok: false, message: "Device not found" });

  device.latestSteps = steps;
  device.lastUpdate = new Date();
  await device.save();

  console.log(`🟢 Steps updated for ${deviceId}: ${steps}`);

  res.json({ ok: true });
});

// ===== INTAKE: PULSE =====
app.post("/ingest/pulse", async (req, res) => {
  if (!checkKey(req, res)) return;

  const { deviceId, bpm, beat } = req.body;

  if (!deviceId || bpm === undefined)
    return res.status(400).json({ ok: false, message: "deviceId and bpm required" });

  const device = await Device.findOne({ deviceId });
  if (!device)
    return res.status(404).json({ ok: false, message: "Device not found" });

  device.latestBpm = bpm;
  device.latestBeat = !!beat;
  device.lastUpdate = new Date();
  await device.save();

  console.log(`❤️ BPM updated for ${deviceId}: ${bpm}`);

  res.json({ ok: true });
});

// ===== FRONTEND: GET LATEST DATA =====
app.get("/api/latest", authMiddleware, async (req, res) => {
  const userId = req.userId;

  const device = await Device.findOne({ userId });

  if (!device) {
    return res.json({
      steps: 0,
      bpm: 0,
      beat: false,
      timestamp: null
    });
  }

  res.json({
    steps: device.latestSteps,
    bpm: device.latestBpm,
    beat: device.latestBeat,
    timestamp: device.lastUpdate
  });
});

// ===== ML WELLNESS SCORE (REAL-TIME) =====
app.get("/api/wellness/current", authMiddleware, async (req, res) => {
  try {
    // Get user (for BMI)
    const user = await User.findById(req.userId);
    if (!user || user.bmi == null) {
      return res.json({ wellnessScore: null });
    }

    // Get device data
    const device = await Device.findOne({ userId: req.userId });
    if (!device) {
      return res.json({ wellnessScore: null });
    }

    // Call Python ML model
    const py = spawn("python", [path.join(__dirname, "ML/predict_wellness.py")]);


    py.stdin.write(JSON.stringify({
      bmi: user.bmi,
      steps: device.latestSteps || 0,
      heart_rate: device.latestBpm || 0,
      temperature: 36.7 // placeholder (until temp sensor added)
    }));
    py.stdin.end();

    py.stdout.on("data", (data) => {
      const result = JSON.parse(data.toString());
      res.json(result);
    });

    py.stderr.on("data", (err) => {
      console.error("Python ML error:", err.toString());
    });

  } catch (err) {
    console.error("Wellness ML route error:", err);
    res.status(500).json({ wellnessScore: null });
  }
});

// ===== PAIRING ROUTES =====
const pairingRegister = require("./routes/pairingRegister");
app.use("/api/pairing", pairingRegister);

const pairingClaim = require("./routes/pairingClaim");
app.use("/api/pairing", pairingClaim);

// ===== SPA FALLBACK ======
app.use((req, res) => {
  res.sendFile(path.join(__dirname, "public", "index.html"));
});

// ===== START SERVER =====
const PORT = process.env.PORT || 8080;
app.listen(PORT, () => {
  console.log(`🚀 Server running at http://localhost:${PORT}`);
});
