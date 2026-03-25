import numpy as np
from sklearn.linear_model import LogisticRegression
from sklearn.preprocessing import StandardScaler
from sklearn.model_selection import train_test_split
from sklearn.metrics import classification_report

# =========================================
# 1. DATA GENERATION
# =========================================
def generate_data(seed=42):
    rng = np.random.default_rng(seed)

    # Normal samples
    bpm_n = rng.normal(72, 5, 600)
    diff_n = rng.normal(0, 1, 600)
    y_n = np.zeros(600, dtype=int)

    # Abnormal samples
    bpm_a = rng.normal(110, 15, 200)
    diff_a = rng.normal(10, 4, 200)
    y_a = np.ones(200, dtype=int)

    bpm = np.concatenate([bpm_n, bpm_a])
    diff = np.concatenate([diff_n, diff_a])
    y = np.concatenate([y_n, y_a])

    # Two features only: BPM and BPM change
    X = np.column_stack([bpm, diff])

    return X, y


# =========================================
# 2. TRAIN MODEL
# =========================================
def train_model(X, y):
    X_train, X_test, y_train, y_test = train_test_split(
        X,
        y,
        test_size=0.2,
        stratify=y,
        random_state=42
    )

    scaler = StandardScaler()
    X_train_s = scaler.fit_transform(X_train)
    X_test_s = scaler.transform(X_test)

    model = LogisticRegression(
        class_weight="balanced",
        max_iter=1000,
        random_state=42
    )
    model.fit(X_train_s, y_train)

    return model, scaler, X_test_s, y_test


# =========================================
# 3. EVALUATION
# =========================================
def evaluate(model, X_test, y_test, threshold=0.6):
    y_prob = model.predict_proba(X_test)[:, 1]
    y_pred = (y_prob >= threshold).astype(int)

    print("Threshold:", threshold)
    print(classification_report(y_test, y_pred, digits=4))


# =========================================
# 4. EXPORT
# =========================================
def to_c_array(arr):
    return "{" + ", ".join(f"{float(x):.8f}f" for x in arr) + "}"

def export_to_header(model, scaler, threshold=0.6, filename="logistic_model.h"):
    W = model.coef_[0]
    B = model.intercept_[0]

    with open(filename, "w", encoding="utf-8") as f:
        f.write("#ifndef LOGISTIC_MODEL_H\n")
        f.write("#define LOGISTIC_MODEL_H\n\n")

        f.write(f"const int NFEAT = {len(W)};\n")
        f.write(f"const float W[{len(W)}] = {to_c_array(W)};\n")
        f.write(f"const float B = {float(B):.8f}f;\n")
        f.write(f"const float CENTER[{len(W)}] = {to_c_array(scaler.mean_)};\n")
        f.write(f"const float SCALE[{len(W)}] = {to_c_array(scaler.scale_)};\n")
        f.write(f"const float THRESHOLD = {float(threshold):.8f}f;\n\n")

        f.write("#endif\n")

    print(f"Header exported to: {filename}")


# =========================================
# MAIN
# =========================================
if __name__ == "__main__":
    X, y = generate_data()
    model, scaler, X_test, y_test = train_model(X, y)
    evaluate(model, X_test, y_test, threshold=0.6)
    export_to_header(model, scaler, threshold=0.6)