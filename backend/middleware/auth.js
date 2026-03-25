const jwt = require("jsonwebtoken");
require("dotenv").config();

const JWT_SECRET = process.env.JWT_SECRET || "SECRET123";

module.exports = function (req, res, next) {
  const authHeader = req.headers.authorization;

  if (!authHeader) {
    return res.status(401).json({ ok: false, message: "No token" });
  }

  const token = authHeader.split(" ")[1];
  if (!token) {
    return res.status(401).json({ ok: false, message: "No token" });
  }

  try {
    const decoded = jwt.verify(token, JWT_SECRET);
    req.userId = decoded.userId; // we will use this later
    next();
  } catch (e) {
    return res.status(401).json({ ok: false, message: "Invalid token" });
  }
};
