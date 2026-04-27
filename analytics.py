import firebase_admin
from firebase_admin import credentials, db
import pandas as pd
from datetime import datetime
import matplotlib.pyplot as plt

# ================= FIREBASE =================
cred = credentials.Certificate("energy.json")

firebase_admin.initialize_app(cred, {
    'databaseURL': 'setup your own database url here'
})

ref = db.reference("energy_data")
data = ref.get()

# ================= LOAD DATA =================
records = []

for key in data:
    item = data[key]

    # convert time safely
    try:
        time = datetime.strptime(item["time"], "%Y-%m-%d %H:%M:%S")
    except:
        continue

    records.append({
        "date": time.date(),
        "kwh": item["kwh"],
        "bill": item["bill"]
    })

df = pd.DataFrame(records)

# ================= DAILY ANALYTICS =================
daily = df.groupby("date").sum()

print("\n📊 DAILY USAGE:")
print(daily)

# Plot daily usage
plt.figure()
plt.plot(daily.index.astype(str), daily["kwh"], marker="o")
plt.title("Daily Energy Usage (kWh)")
plt.xlabel("Date")
plt.ylabel("kWh")
plt.xticks(rotation=45)
plt.tight_layout()
plt.show()

# ================= WEEKLY ANALYTICS =================
df["week"] = pd.to_datetime(df["date"]).dt.isocalendar().week
weekly = df.groupby("week").sum()

print("\n📆 WEEKLY USAGE:")
print(weekly)

# Plot weekly usage
plt.figure()
plt.bar(weekly.index.astype(str), weekly["kwh"])
plt.title("Weekly Energy Usage (kWh)")
plt.xlabel("Week Number")
plt.ylabel("kWh")
plt.tight_layout()
plt.show()

# ================= COST SUMMARY =================
print("\n💰 TOTAL BILL:")
print("Total kWh:", df["kwh"].sum())
print("Estimated Bill:", df["bill"].sum())
