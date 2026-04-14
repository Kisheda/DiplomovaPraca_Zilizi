#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <HTTPClient.h>
#include <DHT.h>
#include <BH1750.h>
#include <Adafruit_BMP280.h>

#define SOIL_PIN 34
#define RELAY_PIN 26
#define DHT_PIN 27

#define SDA_PIN 17
#define SCL_PIN 16

#define DHTTYPE DHT11

// ---------- WIFI ----------
const char* ssid = "Mark_Wifi";
const char* wifi_password = "Heda90102211";

// ---------- MQTT Topics ----------
const char* topic_status    = "meteostanica/status";
const char* topic_soil      = "meteostanica/soil";
const char* topic_temp      = "meteostanica/temperature";
const char* topic_hum       = "meteostanica/humidity";
const char* topic_light     = "meteostanica/light";
const char* topic_pressure  = "meteostanica/pressure";
const char* topic_pump      = "meteostanica/pump";       // state
const char* topic_pump_set  = "meteostanica/pump/set";   // command

// ---------- MQTT / HiveMQ ----------
const char* mqtt_server   = "b583e186639e4bc2aa7bc95605777610.s1.eu.hivemq.cloud";
const int   mqtt_port     = 8883;
const char* mqtt_user     = "Modul_Meteostanica";
const char* mqtt_password = "Asd123456789";

// ---------- SUPABASE ----------
const char* supabase_url = "https://tmwzwhnpllgumuupmryf.supabase.co";
const char* supabase_key = "sb_publishable_6_J4gNe2AiqsO2Zs6QF4Aw_TaPoHM4H";

const char* supabase_log_endpoint          = "/rest/v1/Logs";
const char* supabase_measurements_endpoint = "/rest/v1/Measurements";

WiFiClientSecure espClient;
PubSubClient client(espClient);

DHT dht(DHT_PIN, DHTTYPE);
BH1750 lightMeter;
Adafruit_BMP280 bmp;

bool bh1750Available = false;
bool bmp280Available = false;

// ---------- Soil calibration ----------
int RAW_WET = 1400;
int RAW_DRY = 3200;

// ---------- Moisture thresholds ----------
int MOISTURE_ON = 35;
int HYSTERESIS = 7;

// ---------- Relay / pump state ----------
bool pumpState = false;
bool timedPumpActive = false;
unsigned long pumpStartedAt = 0;
const unsigned long PUMP_ON_TIME_MS = 3000;

// ---------- Timers ----------
unsigned long lastPublish = 0;
unsigned long lastSupabaseLog = 0;
const unsigned long PUBLISH_INTERVAL_MS = 300000; // 5 minutes

int soilPercent(int raw) {
  int pct = map(raw, RAW_DRY, RAW_WET, 0, 100);
  pct = constrain(pct, 0, 100);
  return pct;
}

void setPump(bool on) {
  pumpState = on;
  digitalWrite(RELAY_PIN, on ? LOW : HIGH);   // LOW trigger relay
}

void publishPumpState() {
  client.publish(topic_pump, pumpState ? "ON" : "OFF", true);
}

String jsonEscape(const String& input) {
  String out;
  out.reserve(input.length() + 8);

  for (size_t i = 0; i < input.length(); i++) {
    char c = input[i];
    if (c == '\"') out += "\\\"";
    else if (c == '\\') out += "\\\\";
    else if (c == '\n') out += "\\n";
    else if (c == '\r') out += "\\r";
    else if (c == '\t') out += "\\t";
    else out += c;
  }
  return out;
}

bool supabasePost(const char* endpoint, const String& jsonBody) {
  if (WiFi.status() != WL_CONNECTED) return false;

  WiFiClientSecure sbClient;
  sbClient.setInsecure();

  HTTPClient http;
  String url = String(supabase_url) + endpoint;

  if (!http.begin(sbClient, url)) {
    Serial.println("Supabase HTTP begin failed");
    return false;
  }

  http.addHeader("Content-Type", "application/json");
  http.addHeader("apikey", supabase_key);
  http.addHeader("Authorization", String("Bearer ") + supabase_key);
  http.addHeader("Prefer", "return=minimal");

  int httpCode = http.POST(jsonBody);
  String response = http.getString();
  http.end();

  Serial.print("Supabase POST ");
  Serial.print(endpoint);
  Serial.print(" -> HTTP ");
  Serial.println(httpCode);

  if (httpCode < 200 || httpCode >= 300) {
    Serial.println("Supabase response:");
    Serial.println(response);
    return false;
  }

  return true;
}

void sendLogToSupabase(
  const String& eventName,
  int soilRaw,
  int soilPct,
  float dhtTemp,
  float dhtHum,
  float lux,
  float bmpTemp,
  float pressure
) {
  String body = "[{";
  body += "\"modul\":\"Modul_Meteostanica\",";
  body += "\"log\":{";
  body += "\"event\":\"" + jsonEscape(eventName) + "\",";
  body += "\"status\":\"ONLINE\",";
  body += "\"pump\":\"" + String(pumpState ? "ON" : "OFF") + "\",";
  body += "\"soil_raw\":" + String(soilRaw) + ",";
  body += "\"soil_pct\":" + String(soilPct) + ",";
  body += "\"dht_temp\":" + (isnan(dhtTemp) ? String("null") : String(dhtTemp, 1)) + ",";
  body += "\"dht_hum\":" + (isnan(dhtHum) ? String("null") : String(dhtHum, 1)) + ",";
  body += "\"light_lux\":" + ((bh1750Available && !isnan(lux) && lux >= 0) ? String(lux, 1) : String("null")) + ",";
  body += "\"bmp_temp\":" + ((bmp280Available && !isnan(bmpTemp)) ? String(bmpTemp, 1) : String("null")) + ",";
  body += "\"pressure_hpa\":" + ((bmp280Available && !isnan(pressure)) ? String(pressure, 1) : String("null"));
  body += "}}]";

  supabasePost(supabase_log_endpoint, body);
}

void sendMeasurementsToSupabase(int soilPct, float dhtTemp, float dhtHum, float lux, float pressure) {
  String body = "[";
  bool first = true;

  auto appendMeasurement = [&](const String& type, float value) {
    if (!first) body += ",";
    body += "{\"measurement_type\":\"" + jsonEscape(type) + "\",\"measurement_data\":" + String(value, 2) + "}";
    first = false;
  };

  appendMeasurement("soil", (float)soilPct);

  if (!isnan(dhtTemp)) appendMeasurement("temperature", dhtTemp);
  if (!isnan(dhtHum))  appendMeasurement("humidity", dhtHum);
  if (bh1750Available && !isnan(lux) && lux >= 0) appendMeasurement("light", lux);
  if (bmp280Available && !isnan(pressure)) appendMeasurement("pressure", pressure);

  body += "]";

  supabasePost(supabase_measurements_endpoint, body);
}

void startTimedPump() {
  setPump(true);
  timedPumpActive = true;
  pumpStartedAt = millis();

  Serial.println("Pump ON for 3 seconds");
  publishPumpState();
}

void stopTimedPump() {
  setPump(false);
  timedPumpActive = false;

  Serial.println("Pump OFF after timed run");
  publishPumpState();
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String message = "";

  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }

  message.trim();

  Serial.print("MQTT message: [");
  Serial.print(topic);
  Serial.print("]: ");
  Serial.println(message);

  if (String(topic) == topic_pump_set) {
    if (message == "ON") {
      startTimedPump();
      sendLogToSupabase("pump_on_mqtt", 0, 0, NAN, NAN, NAN, NAN, NAN);
    } else if (message == "OFF") {
      stopTimedPump();
      sendLogToSupabase("pump_off_mqtt", 0, 0, NAN, NAN, NAN, NAN, NAN);
    }
  }
}

void setup_wifi() {
  Serial.print("WiFi connecting");

  WiFi.begin(ssid, wifi_password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("WiFi connected!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void reconnect_mqtt() {
  while (!client.connected()) {
    Serial.print("MQTT connecting...");

    String clientId = "ESP32_Meteo_";
    clientId += String((uint32_t)(ESP.getEfuseMac() & 0xFFFFFFFF), HEX);

    if (client.connect(clientId.c_str(), mqtt_user, mqtt_password, topic_status, 1, true, "OFFLINE")) {
      Serial.println("Success");

      client.subscribe(topic_pump_set);

      client.publish(topic_status, "ONLINE", true);
      publishPumpState();

      sendLogToSupabase("mqtt_connected", 0, 0, NAN, NAN, NAN, NAN, NAN);
    } else {
      Serial.print("MQTT failed, rc=");
      Serial.print(client.state());
      Serial.println(" retry in 5 seconds...");
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(RELAY_PIN, OUTPUT);
  setPump(false);

  analogReadResolution(12);
  analogSetPinAttenuation(SOIL_PIN, ADC_11db);

  dht.begin();

  Wire.begin(SDA_PIN, SCL_PIN);

  bh1750Available = lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE);
  if (bh1750Available) {
    Serial.println("BH1750 OK");
  } else {
    Serial.println("BH1750 error");
  }

  if (bmp.begin(0x76)) {
    bmp280Available = true;
    Serial.println("BMP280 OK (0x76)");
  } else if (bmp.begin(0x77)) {
    bmp280Available = true;
    Serial.println("BMP280 OK (0x77)");
  } else {
    bmp280Available = false;
    Serial.println("BMP280 error");
  }

  setup_wifi();

  espClient.setInsecure();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(mqttCallback);
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    setup_wifi();
  }

  if (!client.connected()) {
    reconnect_mqtt();
  }

  client.loop();

  // -------- Timed relay handling --------
  if (timedPumpActive && (millis() - pumpStartedAt >= PUMP_ON_TIME_MS)) {
    stopTimedPump();
    sendLogToSupabase("pump_off_timeout", 0, 0, NAN, NAN, NAN, NAN, NAN);
  }

  // -------- Sensor reads --------
  int raw = analogRead(SOIL_PIN);
  int moisture = soilPercent(raw);

  float temp = dht.readTemperature();
  float hum = dht.readHumidity();

  float lux = bh1750Available ? lightMeter.readLightLevel() : NAN;

  float bmpTemp = NAN;
  float pressure = NAN;
  if (bmp280Available) {
    bmpTemp = bmp.readTemperature();
    pressure = bmp.readPressure() / 100.0F;
  }

  // -------- Serial print --------
  Serial.print("Soil RAW: ");
  Serial.print(raw);

  Serial.print("  Moisture: ");
  Serial.print(moisture);
  Serial.print("%");

  Serial.print("  Temp DHT11: ");
  Serial.print(temp);
  Serial.print("C");

  Serial.print("  Humidity: ");
  Serial.print(hum);
  Serial.print("%");

  Serial.print("  Light: ");
  Serial.print(lux);
  Serial.print(" lux");

  Serial.print("  BMP Temp: ");
  Serial.print(bmpTemp);
  Serial.print("C");

  Serial.print("  Pressure: ");
  Serial.print(pressure);
  Serial.print(" hPa");

  Serial.print("  Pump: ");
  Serial.println(pumpState ? "ON" : "OFF");

  // -------- Automatic moisture logic --------
  // Only run auto logic if pump is not currently in timed MQTT mode
  if (!timedPumpActive) {
    if (!pumpState && moisture <= MOISTURE_ON) {
      setPump(true);
      Serial.println("Pump ON (auto)");
      publishPumpState();
      sendLogToSupabase("pump_on_auto", raw, moisture, temp, hum, lux, bmpTemp, pressure);
    }

    if (pumpState && moisture >= (MOISTURE_ON + HYSTERESIS)) {
      setPump(false);
      Serial.println("Pump OFF (auto)");
      publishPumpState();
      sendLogToSupabase("pump_off_auto", raw, moisture, temp, hum, lux, bmpTemp, pressure);
    }
  }

  // -------- MQTT + Supabase publish --------
  if (millis() - lastPublish >= PUBLISH_INTERVAL_MS) {
    lastPublish = millis();

    client.publish(topic_status, "ONLINE", true);
    client.publish(topic_soil, String(moisture).c_str(), true);
    client.publish(topic_temp, String(temp, 1).c_str(), true);
    client.publish(topic_hum, String(hum, 1).c_str(), true);

    if (bh1750Available && !isnan(lux) && lux >= 0) {
      client.publish(topic_light, String(lux, 1).c_str(), true);
    }

    if (bmp280Available && !isnan(pressure)) {
      client.publish(topic_pressure, String(pressure, 1).c_str(), true);
    }

    publishPumpState();

    sendMeasurementsToSupabase(moisture, temp, hum, lux, pressure);

    if (millis() - lastSupabaseLog >= 30000) {
      lastSupabaseLog = millis();
      sendLogToSupabase("periodic_sample", raw, moisture, temp, hum, lux, bmpTemp, pressure);
    }

    Serial.println("MQTT + Supabase message sent successfully");
  }

  delay(200);
}
