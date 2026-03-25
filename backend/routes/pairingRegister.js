const express = require('express');
const router = express.Router();
const Device = require('../models/Device');

// Device → Server
router.post('/register', async (req, res) => {
  try {
    const { deviceId, pairingCode } = req.body;

    if (!deviceId || !pairingCode) {
      return res.status(400).json({ error: "deviceId and pairingCode required" });
    }

    const expires = new Date(Date.now() + 10 * 60 * 1000); // 10 min validity

    const device = await Device.findOneAndUpdate(
      { deviceId },
      {
        deviceId,
        pairingCode,
        pairingExpiresAt: expires
      },
      { upsert: true, new: true }
    );

    res.json({ ok: true, deviceId: device.deviceId });
  } catch (e) {
    console.error(e);
    res.status(500).json({ error: "server error" });
  }
});

module.exports = router;
