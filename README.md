# 🌬️ Smart Fan Controller (ESP8266 + Firebase + OLED)

This project implements a **smart fan controller** using an ESP8266 (NodeMCU/D1 Mini), Firebase Realtime Database, temperature/humidity sensors, and a triac-based dimmer.

It supports both **manual** and **automatic** modes:

* 📱 **Manual Mode** → Control fan speed (Low/Med/High/Max) via Firebase.
* 🤖 **Automatic Mode** → Adjusts fan speed based on **temperature / heat index** for better comfort.
* 🖥️ **OLED Display** → Shows real-time sensor readings and fan status.
* ☁️ **Cloud Integration** → Sensor data and controls sync with Firebase.


---

https://github.com/user-attachments/assets/064e5523-a295-451d-96de-10b960104280

## ✨ Features

* 🔗 Real-time Firebase integration (control + monitoring from phone/web)
* 🌡️ Temperature monitoring via **DS18B20**
* 💧 Humidity monitoring via **DHT11**
* 🖥️ 0.91" OLED display for live data
* ⚡ AC fan control via **RBD Dimmer (triac + zero-cross detection)**
* 📊 Heat Index–based control (perceived temperature instead of raw temperature)
* 🛠️ TaskScheduler for non-blocking multitasking
* 🔒 Master ON/OFF override via Firebase

---

## 🛠️ Hardware Requirements

* ESP8266 (NodeMCU or Wemos D1 Mini recommended)
* DS18B20 temperature sensor
* DHT11 humidity sensor
* 0.91" OLED display (I2C, 128x32)
* Dimmer circuit with zero-cross detection (Opto-triac based)
* Relay (optional for hard cut-off)
* AC Fan

---

## ⚡ Circuit Connections

| Component            | ESP8266 Pin |
| -------------------- | ----------- |
| DS18B20 (Data)       | D6          |
| DHT11 (Data)         | D7          |
| OLED SDA             | D2          |
| OLED SCL             | D1          |
| Dimmer (PWM)         | D8          |
| Dimmer (ZCD)         | D5          |
| Fan Relay (optional) | D0          |

⚠️ **Warning:** The dimmer circuit works with **AC mains (220V/110V)** → Handle with extreme care!

---

## 📦 Software Requirements

* Arduino IDE (or PlatformIO)
* ESP8266 Core installed
* Required libraries:

  * `RBDdimmer`
  * `ESP8266WiFi`
  * `FirebaseESP8266`
  * `OneWire`
  * `DallasTemperature`
  * `DHT`
  * `Adafruit_GFX`
  * `Adafruit_SSD1306`
  * `TaskScheduler`

---

## ⚙️ Setup

1. Clone this repo:

   ```bash
   git clone https://github.com/your-username/smart-fan-controller.git
   cd smart-fan-controller
   ```
2. Open the code in Arduino IDE.
3. Update your **Wi-Fi** and **Firebase** credentials:

   ```cpp
   #define WIFI_SSID "your-ssid"
   #define WIFI_PASSWORD "your-password"
   #define FIREBASE_HOST "your-firebase-url"
   #define FIREBASE_AUTH "your-firebase-secret"
   ```
4. Flash the code to ESP8266.
5. Open **Firebase Realtime Database** and create the following structure:

   ```json
   {
     "FanONOFF": true,
     "on_off": true,
     "controlFanSpeed": 0,
     "temperatureHold": 25
   }
   ```

---

## 📊 Firebase Controls

| Key               | Type  | Description                               |
| ----------------- | ----- | ----------------------------------------- |
| `FanONOFF`        | Bool  | Master ON/OFF switch                      |
| `on_off`          | Bool  | `true` = Auto Mode, `false` = Manual Mode |
| `controlFanSpeed` | Int   | Manual mode: 1=Low, 2=Med, 3=High, 4=Max  |
| `temperatureHold` | Float | Temperature threshold for auto mode       |

---

## 📷 OLED Display Layout

* Temperature (°C)
* Humidity (%)
* Heat Index (°C)
* Fan Speed (%)

---

## 🚀 Future Improvements

* Add **mobile app** for easier control
* Support multiple fan profiles (eco, turbo, comfort)
* Add local web server fallback if Wi-Fi/Firebase unavailable
* Integrate **MQTT** for IoT platforms (Home Assistant, Node-RED)

---

## ⚠️ Safety Disclaimer

This project involves **AC mains wiring**. Only attempt if you are experienced with electronics and take necessary safety precautions.

---

## 📜 License

MIT License – Free to use and modify.

---

👉 Do you want me to also create a **Firebase rules + example mobile UI (Flutter / MIT App Inventor)** section in the README so someone can easily control it from their phone?
