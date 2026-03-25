import pandas as pd
from sklearn.model_selection import train_test_split
from sklearn.ensemble import RandomForestRegressor
from sklearn.metrics import mean_absolute_error
import joblib

# 1. Load dataset
df = pd.read_csv("synthetic_wellness.csv")

X = df[["bmi", "steps", "heart_rate", "temperature"]]
y = df["wellness_score"]

# 2. Train / test split
X_train, X_test, y_train, y_test = train_test_split(
    X, y, test_size=0.2, random_state=42
)

# 3. Train model
model = RandomForestRegressor(
    n_estimators=300,
    max_depth=12,
    random_state=42
)

model.fit(X_train, y_train)

# 4. Evaluate
preds = model.predict(X_test)
mae = mean_absolute_error(y_test, preds)

print(f"Mean Absolute Error: {mae:.2f} points")

# 5. Save model
joblib.dump(model, "wellness_model.pkl")

print("Model saved as wellness_model.pkl")

