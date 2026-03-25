const mongoose = require('mongoose');

const deviceSchema = new mongoose.Schema({
  deviceId: { type: String, unique: true, required: true },

  // which user owns this device (after pairing)
  userId: { type: mongoose.Schema.Types.ObjectId, ref: 'User', default: null },

  // pairing info (temporary)
  pairingCode: { type: String, default: null },
  pairingExpiresAt: { type: Date, default: null },

  // latest live data from this device
  latestSteps: { type: Number, default: 0 },
  latestBpm:   { type: Number, default: 0 },
  latestBeat:  { type: Boolean, default: false },
  lastUpdate:  { type: Date, default: null },

  createdAt: { type: Date, default: Date.now }
});

module.exports = mongoose.model('Device', deviceSchema);
