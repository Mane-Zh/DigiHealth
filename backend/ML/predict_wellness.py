import sys
import json
import joblib
import pandas as pd
import os

# Load trained model
model = joblib.load(os.path.join(os.path.dirname(__file__), "wellness_model.pkl"))

# Read JSON input from Node.js
data = json.loads(sys.stdin.read())

bmi = data["bmi"]
steps = data["steps"]
heart_rate = data["heart_rate"]
temperature = data["temperature"]

# Create DataFrame with feature names
X = pd.DataFrame([{
    "bmi": bmi,
    "steps": steps,
    "heart_rate": heart_rate,
    "temperature": temperature
}])

# Predict wellness score
score = model.predict(X)[0]
score = round(float(score), 1)

# Return result as JSON
print(json.dumps({
    "wellnessScore": score
}))
