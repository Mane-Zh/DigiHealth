import random
import csv
import math

NUM_SAMPLES = 10000

def expected_steps(bmi):
    if bmi < 22:
        return 1200
    elif bmi < 27:
        return 900
    elif bmi < 32:
        return 700
    else:
        return 500

def generate_sample():
    # BMI
    bmi = round(random.uniform(16, 40), 1)

    # Steps depend on BMI
    base_steps = expected_steps(bmi)
    steps = max(0, int(random.gauss(base_steps, base_steps * 0.4)))

    # Heart rate depends on steps
    resting_hr = 65 + (bmi - 22) * 0.6
    activity_hr = steps / 20
    heart_rate = int(random.gauss(resting_hr + activity_hr, 8))

    # Temperature (mostly normal, sometimes elevated)
    if random.random() < 0.08:
        temperature = round(random.uniform(37.5, 39.5), 1)
    else:
        temperature = round(random.uniform(36.2, 37.2), 1)

    # ---- Wellness score logic ----
    score = 100

    # BMI penalty
    if bmi < 18.5 or bmi > 35:
        score -= 15
    elif bmi > 30:
        score -= 10
    elif bmi > 25:
        score -= 5

    # Steps penalty (relative to BMI)
    target = expected_steps(bmi)
    if steps < 0.4 * target:
        score -= 20
    elif steps < 0.7 * target:
        score -= 10

    # Heart rate logic (context-aware)
    if heart_rate > 110 and steps < 0.5 * target:
        score -= 25
    elif heart_rate > 130 and steps > target:
        score -= 5

    # Temperature penalty
    if temperature > 39:
        score -= 40
    elif temperature > 38:
        score -= 25
    elif temperature > 37.5:
        score -= 10

    score = max(1, min(100, score))

    return [bmi, steps, heart_rate, temperature, score]

# Write CSV
with open("synthetic_wellness.csv", "w", newline="") as f:
    writer = csv.writer(f)
    writer.writerow(["bmi", "steps", "heart_rate", "temperature", "wellness_score"])
    for _ in range(NUM_SAMPLES):
        writer.writerow(generate_sample())

print("Dataset generated: synthetic_wellness.csv")
