import paho.mqtt.client as mqtt
import firebase_admin
from firebase_admin import credentials, db
import pandas as pd
from sklearn.linear_model import LinearRegression
import numpy as np
from datetime import datetime

# ================== FIREBASE SETUP ==================
cred = credentials.Certificate("energy.json")

if not firebase_admin._apps:
    firebase_admin.initialize_app(cred, {
        'databaseURL': 'setup you onw database url here'
    })

ref = db.reference("energy_data")

# ================== MQTT ==================
broker = "broker.emqx.io"
topic = "home_auto_3_load_pub"

# ================== DATA STORAGE ==================
data = []
model = LinearRegression()

# ================== TRAIN MODEL ==================
def train_model():
    global model
    if len(data) < 5:
        return

    df = pd.DataFrame(data)
    X = np.array(range(len(df))).reshape(-1, 1)
    y = df["kwh"].values
    model.fit(X, y)

# ================== PREDICTION ==================
def predict_energy():
    if len(data) < 5:
        return 0

    next_x = np.array([[len(data)]])
    prediction = model.predict(next_x)[0]
    print("🔮 Predicted Next kWh:", round(prediction, 4))
    return prediction

# ================== MQTT CALLBACK ==================
def on_message(client, userdata, message):
    global data

    msg = message.payload.decode()
    print("Received:", msg)

    try:
        v, c, w, kwh, bill = msg.split(",")

        v = float(v)
        c = float(c)
        w = float(w)
        kwh = float(kwh)
        bill = float(bill)

        # store locally
        data.append({
            "kwh": kwh,
            "time": datetime.now()
        })

        if len(data) > 50:
            data.pop(0)

        # save to firebase
        firebase_data = {
            "voltage": v,
            "current": c,
            "power": w,
            "kwh": kwh,
            "bill": bill,
            "time": datetime.now().strftime("%Y-%m-%d %H:%M:%S")
        }

        ref.push(firebase_data)
        print("Saved to Firebase ✅")

        # AI
        train_model()
        if len(data) > 5:
            predict_energy()

    except Exception as e:
        print("Error:", e)

# ================== MQTT SETUP ==================
client = mqtt.Client()
client.on_message = on_message

client.connect(broker, 1883)
client.subscribe(topic)

print("🚀 System Running...")
print("📡 Listening for ESP32 data...")

client.loop_forever()
