#include <ESP8266WiFi.h>
#include <Firebase_ESP_Client.h>

// ================= WIFI =================
#define WIFI_SSID       "User Wifi name"
#define WIFI_PASSWORD   "Password of wifi"

// ================= FIREBASE =================
  #define API_KEY       "AIzaSyDbrhzVfacbb4dV3x2gQDmM8AE37l-knxQ"
  #define DATABASE_URL  "https://vagdevi-major-projects-default-rtdb.firebaseio.com/"

// ================= PINS =================
#define ACS_PIN   A0
#define RELAY_PIN D1

// ================= ACS712 SETTINGS =================
// 5A version  = 185 mV/A
// 20A version = 100 mV/A
// 30A version =  66 mV/A
float sensitivity = 0.100;      // 20A ACS712 = 0.100 V/A
float vRef = 1.0;               // Bare ESP8266 ADC reference ~1.0V
float zeroCurrentVoltage = 0.5; // Will be auto-calibrated
float supplyVoltage = 5.0;      // Voltage supplied to load

// ================= FIREBASE OBJECTS =================
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

bool signupOK = false;

// ================= VARIABLES =================
float current = 0.0;
float power = 0.0;
String loadStatus = "No meaningful current";

unsigned long lastFirebaseSend = 0;
const unsigned long firebaseInterval = 5000; // send every 5 sec

// ================= FUNCTIONS =================
void connectWiFi();
void initFirebase();
float calibrateACS712();
float readCurrentACS712();
void sendToFirebase();

void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(RELAY_PIN, OUTPUT);

  // Change this if your relay is ACTIVE LOW
  digitalWrite(RELAY_PIN, HIGH);

  Serial.println();
  Serial.println("=== ESP8266 ACS712 CURRENT MONITOR WITH FIREBASE ===");

  connectWiFi();
  initFirebase();

  Serial.println("Calibrating zero current voltage...");
  zeroCurrentVoltage = calibrateACS712();
  Serial.print("Zero Current Voltage: ");
  Serial.println(zeroCurrentVoltage, 4);

  Serial.println("System started.");
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
  }

  current = readCurrentACS712();
  power = supplyVoltage * current;

  if (current > 0.05) {
    loadStatus = "Wireless TX drawing current";
  } else {
    loadStatus = "No meaningful current";
  }

  Serial.println("------ CURRENT MONITOR ------");
  Serial.print("Current: ");
  Serial.print(current, 3);
  Serial.println(" A");

  Serial.print("Power: ");
  Serial.print(power, 3);
  Serial.println(" W");

  Serial.print("Load Status: ");
  Serial.println(loadStatus);
  Serial.println("-----------------------------");

  if (Firebase.ready() && signupOK && millis() - lastFirebaseSend > firebaseInterval) {
    lastFirebaseSend = millis();
    sendToFirebase();
  }

  delay(2000);
}

void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }

  Serial.println();
  Serial.print("WiFi Connected. IP: ");
  Serial.println(WiFi.localIP());
}

void initFirebase() {
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;

  Serial.println("Signing up anonymously...");

  if (Firebase.signUp(&config, &auth, "", "")) {
    Serial.println("Firebase anonymous auth OK");
    signupOK = true;
  } else {
    Serial.print("Firebase signup FAILED: ");
    Serial.println(config.signer.signupError.message.c_str());
    signupOK = false;
  }

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
}

float calibrateACS712() {
  const int samples = 500;
  long total = 0;

  for (int i = 0; i < samples; i++) {
    total += analogRead(ACS_PIN);
    delay(2);
  }

  float avgRaw = total / (float)samples;
  float voltage = (avgRaw / 1023.0) * vRef;
  return voltage;
}

float readCurrentACS712() {
  const int samples = 200;
  long total = 0;

  for (int i = 0; i < samples; i++) {
    total += analogRead(ACS_PIN);
    delay(1);
  }

  float avgRaw = total / (float)samples;
  float sensorVoltage = (avgRaw / 1023.0) * vRef;

  float currentValue = (sensorVoltage - zeroCurrentVoltage) / sensitivity;

  // absolute value
  if (currentValue < 0) currentValue = -currentValue;

  // remove small noise
  if (currentValue < 0.03) currentValue = 0.0;
 
  return currentValue;
}

void sendToFirebase() {
  String basePath = "/ACS712_Monitor";

  bool ok = true;

  if (!Firebase.RTDB.setFloat(&fbdo, basePath + "/Current_A", current)) {
    Serial.print("Failed to send Current: ");
    Serial.println(fbdo.errorReason());
    ok = false;
  }

  if (!Firebase.RTDB.setFloat(&fbdo, basePath + "/Power_W", power)) {
    Serial.print("Failed to send Power: ");
    Serial.println(fbdo.errorReason());
    ok = false;
  }

  if (!Firebase.RTDB.setString(&fbdo, basePath + "/Load_Status", loadStatus)) {
    Serial.print("Failed to send Load Status: ");
    Serial.println(fbdo.errorReason());
    ok = false;
  }

  if (!Firebase.RTDB.setFloat(&fbdo, basePath + "/Zero_Current_Voltage", zeroCurrentVoltage)) {
    Serial.print("Failed to send Zero Voltage: ");
    Serial.println(fbdo.errorReason());
    ok = false;
  }

  if (ok) {
    Serial.println("Data sent to Firebase successfully.");
  }
}