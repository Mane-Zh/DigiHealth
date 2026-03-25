const express = require('express');
const router = express.Router();
const Device = require('../models/Device');
const auth = require('../middleware/auth');

// User → Server
router.post('/claim', auth, async (req, res) => {
  try {
    const { pairingCode } = req.body;

    if (!pairingCode) {
      return res.status(400).json({ error: "pairingCode required" });
    }

    const now = new Date();

    // find the device with matching active code
    const device = await Device.findOne({
      pairingCode,
      pairingExpiresAt: { $gt: now },
      userId: null
    });

    if (!device) {
      return res.status(400).json({ error: "Invalid or expired code" });
    }

    // attach device to logged-in user
    device.userId = req.userId;
    device.pairingCode = null;
    device.pairingExpiresAt = null;

    await device.save();

    res.json({ ok: true, deviceId: device.deviceId });
  } catch (e) {
    console.error(e);
    res.status(500).json({ error: "server error" });
  }
});

module.exports = router;
