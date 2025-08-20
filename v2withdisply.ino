#include <RBDdimmer.h>
#include <ESP8266WiFi.h>
#include <FirebaseESP8266.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <DHT.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <TaskScheduler.h>
#include <RBDdimmer.h> // For Optimus dimmer control

// Wi-Fi credentials
#define WIFI_SSID "huawei"
#define WIFI_PASSWORD "12345678"

// Firebase config
#define FIREBASE_HOST "smart-fan-controller-e1721-default-rtdb.asia-southeast1.firebasedatabase.app"
#define FIREBASE_AUTH "VoyfKYAwfanU32G2XWrPhg7sBMC984ZrXsL1Grny"

// Sensor Pins
#define ONE_WIRE_BUS D6  // DS18B20
#define DHTPIN D7        // DHT11
#define DHTTYPE DHT11
#define FAN_RELAY_PIN D0 // Relay control 

// OLED (0.91" 128x32 I2C) - D1 Mini: SDA = D2 (GPIO4), SCL = D1 (GPIO5)
#define SCREEN_WIDTH    128
#define SCREEN_HEIGHT   32
#define OLED_RESET      -1
#define SCREEN_ADDRESS  0x3C
#define OLED_SDA_PIN    D2
#define OLED_SCL_PIN    D1



// Dimmer pins (ESP8266)
#define DIMMER_PIN D8    // Dimmer control pin
#define ZERO_CROSS_PIN D5 // Zero-cross detection pin
dimmerLamp dimmer(DIMMER_PIN, ZERO_CROSS_PIN); 


int fanSpeed = 0; // 0-100% speed
float temperatureHold = 25.0; // Default threshold from Firebase
float heatIndexGlobal = NAN;


Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// --- ICONS ---
static const unsigned char PROGMEM image_arrow_up_bits[] = {0x20,0x70,0xa8,0x20,0x20,0x20,0x20};
static const unsigned char PROGMEM image_download_1_bits[] = {
  0x04,0x00,0x04,0x00,0x0c,0x00,0x0e,0x00,0x1e,0x00,0x1f,0x00,0x3f,0x80,0x3f,0x80,
  0x7e,0xc0,0x7f,0x40,0xff,0x60,0xff,0xe0,0x7f,0xc0,0x7f,0xc0,0x3f,0x80,0x0f,0x00
};
static const unsigned char PROGMEM image_download_bits[] = {
  0x1c,0x00,0x22,0x02,0x2b,0x05,0x2a,0x02,0x2b,0x38,0x2a,0x60,0x2b,0x40,0x2a,0x40,
  0x2a,0x60,0x49,0x38,0x9c,0x80,0xae,0x80,0xbe,0x80,0x9c,0x80,0x41,0x00,0x3e,0x00
};
static const unsigned char PROGMEM image_images_1_bits[] = {
0xff,0xff,0xc0,0x80,0x00,0x40,0x87,0x00,0x40,0x8f,0x80,0x40,0x8f,0x8e,0x40,0x8f,0x1f,
0x40,0x87,0x3f,0x40,0x83,0xff,0x40,0x81,0xe6,0x40,0x99,0xe0,0x40,0xbf,0xf0,0x40,0xbf,
0x38,0x40,0xbe,0x3c,0x40,0x9c,0x7c,0x40,0x80,0x7c,0x40,0x80,0x38,0x40,0x80,0x00,0x40,0xff,0xff,0xc0
};

// Sensor objects
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature ds18b20(&oneWire);
DHT dht(DHTPIN, DHTTYPE);

// Firebase objects
FirebaseData firebaseData;
FirebaseAuth firebaseAuth;
FirebaseConfig firebaseConfig;

// Sensor value storage
float lastValidTemp = NAN;
float lastValidHumidity = NAN;

// OLED Flashing
bool flashBulb = false;
bool fanState = false; // Global for OLED + logic

// TaskScheduler
Scheduler runner;

// Task function declarations
void readTemperature();
void readHumidity();
void fanControlTask();
void oledFlashTask();
void updateOLEDTask();

// Tasks
Task tReadTemp(1000, TASK_FOREVER, &readTemperature, &runner, true);     // every 1s
Task tReadHumidity(2000, TASK_FOREVER, &readHumidity, &runner, true);    // every 2s
Task tFanControl(200, TASK_FOREVER, &fanControlTask, &runner, true);     // fast for immediate relay
Task tOLEDFlash(500, TASK_FOREVER, &oledFlashTask, &runner, true);       // 0.5s for bulb flash
Task tOLEDUpdate(200, TASK_FOREVER, &updateOLEDTask, &runner, true);     // refresh OLED

// Control parameters
bool onOff = false;
float tempHold = 25.0;

// --- OLED Initialization ---
void initOLED() {
  Wire.begin(OLED_SDA_PIN, OLED_SCL_PIN);
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println("SSD1306 allocation failed");
    for(;;);
  }
  display.clearDisplay();
  display.display();
}

// --- OLED Update ---
void drawStatusUI(float temp, float humidity, float heatIndex, int fanSpeed) {
  display.clearDisplay();

  display.drawBitmap(6, 1, image_download_bits, 16, 16, 1);
  display.drawBitmap(39, 1, image_download_1_bits, 11, 16, 1);
  display.drawBitmap(67, 3, image_arrow_up_bits, 5, 7, 1);
  display.drawBitmap(70, 1, image_download_bits, 16, 16, 1);
  display.drawBitmap(100, 0, image_images_1_bits, 18, 18, 1);


  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(1, 25);
  if (!isnan(temp)) display.print(temp, 1); else display.print("--.-");

  display.setCursor(39, 25);
  if (!isnan(humidity)) display.print(humidity, 0); else display.print("--");

  display.setCursor(64, 25);
  if (!isnan(heatIndex)) display.print(heatIndex, 1); else display.print("--");

  display.setCursor(104, 25);
  if (!isnan(fanSpeed)) display.print(fanSpeed, 0); else display.print("--");
  display.print("%");
  
  display.display();
}

// Accurate heat index calculation (Rothfusz regression)
float calculateHeatIndex(float tempC, float humidity) {
  if (tempC < 20.0 || humidity < 40) return NAN;
  float tempF = (tempC * 9.0 / 5.0) + 32.0;
  float hiF = -42.379 + (2.04901523 * tempF)
            + (10.14333127 * humidity)
            - (0.22475541 * tempF * humidity)
            - (0.00683783 * pow(tempF, 2))
            - (0.05481717 * pow(humidity, 2))
            + (0.00122874 * pow(tempF, 2) * humidity)
            + (0.00085282 * tempF * pow(humidity, 2))
            - (0.00000199 * pow(tempF, 2) * pow(humidity, 2));
  if (humidity < 13 && tempF >= 80 && tempF <= 112) {
    hiF -= ((13 - humidity) / 4) * sqrt((17 - abs(tempF - 95)) / 17);
  }
  if (humidity > 85 && tempF >= 80 && tempF <= 87) {
    hiF += ((humidity - 85) / 10) * ((87 - tempF) / 5);
  }
  return (hiF - 32.0) * 5.0 / 9.0;
}

// --- Task Implementations ---

void readTemperature() {
  ds18b20.requestTemperatures();
  float temp = ds18b20.getTempCByIndex(0);
  if (temp != DEVICE_DISCONNECTED_C) {
    lastValidTemp = temp;
    Firebase.setFloat(firebaseData, "/temperature", temp);
    Serial.print("Temperature: ");
    Serial.print(temp);
    Serial.println("°C");
  } else {
    Serial.println("Temperature sensor error!");
  }
}

void readHumidity() {
  float humidity = dht.readHumidity();
  if (!isnan(humidity)) {
    lastValidHumidity = humidity;
    Firebase.setFloat(firebaseData, "/humidity", humidity);
    Serial.print("Humidity: ");
    Serial.print(humidity);
    Serial.print("%");
    if (!isnan(lastValidTemp) && !isnan(lastValidHumidity)) {
      float heatIndex = calculateHeatIndex(lastValidTemp, lastValidHumidity);
      if (!isnan(heatIndex)) {
        Serial.print(" | Heat Index: ");
        Serial.print(heatIndex);
        Serial.println("°C");
        Firebase.setInt(firebaseData, "/heatIndex", heatIndex);
      } else {
        Serial.println(" | Heat Index: Not valid (T <20°C or RH <40%)");
      }
    }
  } else {
    Serial.println("Humidity sensor error!");
  }

    


}

// Add near global variables
float heatIndexThreshold = NAN;  // Stores heat index equivalent of temperatureHold

void fanControlTask() {

    bool fanEnabled = false;
    if (Firebase.getBool(firebaseData, "/FanONOFF")) {
        fanEnabled = firebaseData.to<bool>();
    }

    // MASTER OVERRIDE: If fan is disabled, turn everything off immediately
    if (!fanEnabled) {
        fanSpeed = 0;
        dimmer.setState(OFF);
        dimmer.setPower(fanSpeed);
        Firebase.setInt(firebaseData, "/fanSpeed", fanSpeed);
        Serial.println("Master OFF - Fan disabled");
        return;  // Exit function early
    }
    // Read fan on/off state from Firebase (default to OFF if read fails)
    bool on_off = false;
    if (Firebase.getBool(firebaseData, "/on_off")) {
        on_off = firebaseData.to<bool>();
    }

    // MANUAL CONTROL MODE (on_off = false)
    if (!on_off) {
        int controlValue = 0;
        if (Firebase.getInt(firebaseData, "/controlFanSpeed")) {
            controlValue = firebaseData.to<int>();
        }

        // Map control value to fan speed
        switch (controlValue) {
            case 1: fanSpeed = 60; break;  // Low speed
            case 2: fanSpeed = 75; break;  // Medium speed
            case 3: fanSpeed = 85; break;  // High speed
            case 4: fanSpeed = 100; break; // Max speed
            default: fanSpeed = 0;         // Off for invalid values
        }

        // Set dimmer state based on fan speed
        dimmer.setState(fanSpeed > 0 ? ON : OFF);
        dimmer.setPower(fanSpeed);
        
        // Update Firebase and log
        Firebase.setInt(firebaseData, "/fanSpeed", fanSpeed);
        Serial.printf("MANUAL MODE | Control level: %d | Fan speed: %d%%\n", 
                      controlValue, fanSpeed);
        return;  // Exit after manual control
    }

    // AUTOMATIC CONTROL MODE (on_off = true)
    // -----------------------------------------------------------------
    // Get temperature setpoint from Firebase
    if (Firebase.getFloat(firebaseData, "/temperatureHold")) {
        temperatureHold = firebaseData.to<float>();
    }

    // Only proceed if we have valid sensor readings
    if (!isnan(lastValidTemp) && !isnan(lastValidHumidity)) {
        // Calculate current heat index
        float currentHeatIndex = calculateHeatIndex(lastValidTemp, lastValidHumidity);
        
        // Convert setpoint to heat index equivalent
        heatIndexThreshold = calculateHeatIndex(temperatureHold, lastValidHumidity);
        
        // Store for display
        heatIndexGlobal = currentHeatIndex;

        // Check if heat index calculations are valid
        bool useHeatIndex = !isnan(currentHeatIndex) && !isnan(heatIndexThreshold);
        
        if (useHeatIndex) {
            // Heat-index based control
            if (currentHeatIndex >= heatIndexThreshold) {
                dimmer.setState(ON);
                float perceivedHeat = currentHeatIndex - heatIndexThreshold;
                
                // Dynamic speed adjustment:
                if (perceivedHeat <= 0) {
                    fanSpeed = 0;
                } else if (perceivedHeat < 10) {
                    // Scale from 80-100% based on heat difference
                    fanSpeed = 60 + static_cast<int>(20 * (perceivedHeat / 10));
                } else {
                    fanSpeed = 100;
                }
            } else {
                fanSpeed = 0;
                dimmer.setState(OFF);
            }
        } else {
            // Fallback to temperature-only control
            if (lastValidTemp >= temperatureHold) {
                dimmer.setState(ON);
                float tempAbove = lastValidTemp - temperatureHold;
                if (tempAbove < 10) {
                    fanSpeed = 60
                    + + static_cast<int>(20 * (tempAbove / 10));
                } else {
                    fanSpeed = 100;
                }
            } else {
                fanSpeed = 0;
                dimmer.setState(OFF);
            }
        }
    } else {
        // Sensor failure handling
        fanSpeed = 0;
        dimmer.setState(OFF);
    }

    // Apply settings and update Firebase
    dimmer.setPower(fanSpeed);
    Firebase.setInt(firebaseData, "/fanSpeed", fanSpeed);
    
    // Detailed status logging
    Serial.printf("AUTO MODE | Set: %.1f°C | Current: %.1f°C | Hum: %.1f%%\n", 
                  temperatureHold, lastValidTemp, lastValidHumidity);
    Serial.printf("HI: %.1f°C | Threshold: %.1f°C | Fan: %d%%\n",
                  heatIndexGlobal, heatIndexThreshold, fanSpeed);
}


void oledFlashTask() {
  flashBulb = !flashBulb;
}

void updateOLEDTask() {
  float heatIndex = (!isnan(lastValidTemp) && !isnan(lastValidHumidity)) ? calculateHeatIndex(lastValidTemp, lastValidHumidity) : NAN;

  // Use fanState as relayOn
  drawStatusUI(lastValidTemp, lastValidHumidity, heatIndex, fanSpeed);
}

void setup() {
  Serial.begin(115200);

  // Initialize sensors
  ds18b20.begin();
  dht.begin();
  pinMode(FAN_RELAY_PIN, OUTPUT);
  digitalWrite(FAN_RELAY_PIN, LOW);

  // OLED
  initOLED();

  // Connect to Wi-Fi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWi-Fi connected");

  // Configure Firebase
  firebaseConfig.database_url = FIREBASE_HOST;
  firebaseConfig.signer.tokens.legacy_token = FIREBASE_AUTH;
  Firebase.begin(&firebaseConfig, &firebaseAuth);
  Firebase.reconnectWiFi(true);

  // Initialize dimmer
  
  dimmer.begin(NORMAL_MODE, ON); 
  dimmer.setPower(0); // Start with fan off
}

void loop() {
  runner.execute(); // Run scheduled tasks
}