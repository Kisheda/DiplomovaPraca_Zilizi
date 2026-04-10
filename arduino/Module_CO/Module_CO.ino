// ---------- Libraries ----------
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <HTTPClient.h>

// =====================================================
// ESP32 + MQ135 + Relay + Passive Buzzer + HiveMQ + Supabase
// Measurements: only air_quality_raw
// Logs: all states/events with ON/OFF, ONLINE/OFFLINE, AUTO/MANUAL
// =====================================================

// ---------- WIFI ----------
const char* WIFI_SSID     = "Mark_Wifi";
const char* WIFI_PASSWORD = "Heda90102211";

// ---------- HIVEMQ CLOUD ----------
const char* MQTT_HOST     = "b583e186639e4bc2aa7bc95605777610.s1.eu.hivemq.cloud";
const int   MQTT_PORT     = 8883;
const char* MQTT_USER     = "Modul_COSensor";
const char* MQTT_PASSWORD = "Asd123456789";

// ---------- SUPABASE ----------
const char* supabase_url = "https://tmwzwhnpllgumuupmryf.supabase.co";
const char* supabase_key = "sb_publishable_6_J4gNe2AiqsO2Zs6QF4Aw_TaPoHM4H";

const char* supabase_log_endpoint          = "/rest/v1/Logs";
const char* supabase_measurements_endpoint = "/rest/v1/Measurements";

// ---------- TOPICOK ----------
const char* TOPIC_STATUS      = "modul_cosensor/status";
const char* TOPIC_DATA        = "modul_cosensor/airquality";
const char* TOPIC_ALERT       = "modul_cosensor/siren";
const char* TOPIC_RELAY_SET   = "modul_cosensor/relay/set";
const char* TOPIC_RELAY_STATE = "modul_cosensor/relay/state";
const char* TOPIC_MODE_SET    = "modul_cosensor/mode/set";
const char* TOPIC_MODE_STATE  = "modul_cosensor/mode/state";

// ---------- PINOUT ----------
const int MQ_AO_PIN  = 34;   // MQ135 AOUT -> GPIO34
const int MQ_DO_PIN  = 14;   // MQ135 DOUT -> GPIO14
const int BUZZER_PIN = 23;   // passive buzzer -> GPIO23
const int RELAY_PIN  = 26;   // relay IN -> GPIO26

// HIGH = ON, LOW = OFF
const bool RELAY_ACTIVE_LOW = false;

// ---------- MODE ----------
bool manualMode = false;        // false = AUTO, true = MANUAL
bool manualRelayState = false;  // manual mode relay state
bool relayState = false;        // actual relay state

// ---------- MQ135 / ALARM ----------
const unsigned long WARMUP_MS    = 30UL * 1000UL;
const unsigned long MIN_ALARM_MS = 1500;

int THRESH_ON = 1100;
int HYST      = 200;
int THRESH_OFF() { return THRESH_ON - HYST; }

const int AVG_N = 12;

// ---------- BUZZER PWM ----------
const int BUZ_RES_BITS = 8;
const int BUZ_DUTY_ON  = 128;

const int FREQ_LOW  = 1800;
const int FREQ_HIGH = 2600;
const unsigned long SIREN_STEP_MS = 180;

// ---------- MQTT / WIFI ----------
WiFiClientSecure secureClient;
PubSubClient mqttClient(secureClient);

// ---------- Timers ----------
unsigned long lastMqttReconnectAttempt = 0;
unsigned long lastPublishMs = 0;
const unsigned long PUBLISH_INTERVAL_MS = 60000;   // 1 perc

// ---------- State ----------
bool alarmOn = false;
unsigned long tStart = 0;
unsigned long alarmStart = 0;

int buf[AVG_N];
int idx = 0;
long sum = 0;

unsigned long lastSirenStep = 0;
bool sirenHigh = false;

// =====================================================
// SUPABASE HELPERS
// =====================================================

bool supabasePost(const char* endpoint, const String& jsonPayload) {
  if (WiFi.status() != WL_CONNECTED) return false;

  WiFiClientSecure sbClient;
  sbClient.setInsecure();

  HTTPClient https;
  String url = String(supabase_url) + endpoint;

  if (!https.begin(sbClient, url)) {
    Serial.println("Supabase HTTPS begin failed");
    return false;
  }

  https.addHeader("Content-Type", "application/json");
  https.addHeader("apikey", supabase_key);
  https.addHeader("Authorization", String("Bearer ") + supabase_key);
  https.addHeader("Prefer", "return=minimal");

  int httpCode = https.POST(jsonPayload);

  Serial.print("Supabase POST -> ");
  Serial.print(url);
  Serial.print(" | code: ");
  Serial.println(httpCode);

  https.end();

  return (httpCode >= 200 && httpCode < 300);
}

void logToSupabase(const String& eventName, const String& detailsJson) {
  String payload =
    "{"
      "\"modul\":\"Modul_COSensor\","
      "\"log\":{"
        "\"event\":\"" + eventName + "\","
        "\"details\":" + detailsJson +
      "}"
    "}";

  supabasePost(supabase_log_endpoint, payload);
}

void sendAirQualityRawToSupabase(int ao) {
  String payload =
    "{"
      "\"measurement_type\":\"air_quality_raw\","
      "\"measurement_data\":" + String(ao) +
    "}";

  supabasePost(supabase_measurements_endpoint, payload);
}

// =====================================================
// MQTT HELPERS
// =====================================================

void publishRelayState() {
  if (mqttClient.connected()) {
    mqttClient.publish(TOPIC_RELAY_STATE, relayState ? "ON" : "OFF", true);
  }
}

void publishModeState() {
  if (mqttClient.connected()) {
    mqttClient.publish(TOPIC_MODE_STATE, manualMode ? "MANUAL" : "AUTO", true);
  }
}

// =====================================================
// HARDWARE HELPERS
// =====================================================

void setRelay(bool on) {
  if (relayState == on) {
    return;
  }

  relayState = on;

  if (RELAY_ACTIVE_LOW) {
    digitalWrite(RELAY_PIN, on ? LOW : HIGH);
  } else {
    digitalWrite(RELAY_PIN, on ? HIGH : LOW);
  }

  if (mqttClient.connected()) {
    mqttClient.publish(TOPIC_RELAY_STATE, on ? "ON" : "OFF", true);
  }

  String details =
    "{"
      "\"relay\":\"" + String(on ? "ON" : "OFF") + "\","
      "\"mode\":\"" + String(manualMode ? "MANUAL" : "AUTO") + "\""
    "}";

  logToSupabase("relay_changed", details);
}

void buzzerInit() {
  bool ok = ledcAttach(BUZZER_PIN, FREQ_LOW, BUZ_RES_BITS);
  Serial.println(ok ? "Buzzer PWM attached OK" : "Buzzer PWM attach FAILED");
  ledcWrite(BUZZER_PIN, 0);
}

void buzzerOff() {
  ledcWrite(BUZZER_PIN, 0);
}

void buzzerTone(int freqHz, int duty) {
  ledcChangeFrequency(BUZZER_PIN, freqHz, BUZ_RES_BITS);
  ledcWrite(BUZZER_PIN, duty);
}

int readAnalogAvg() {
  int v = analogRead(MQ_AO_PIN);
  sum -= buf[idx];
  buf[idx] = v;
  sum += buf[idx];
  idx = (idx + 1) % AVG_N;
  return (int)(sum / AVG_N);
}

// =====================================================
// WIFI / MQTT
// =====================================================

void connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;

  Serial.print("Connecting to WiFi");
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("WiFi connected");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  logToSupabase(
    "wifi_status",
    "{"
      "\"state\":\"ONLINE\","
      "\"ip\":\"" + WiFi.localIP().toString() + "\""
    "}"
  );
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String msg;
  for (unsigned int i = 0; i < length; i++) {
    msg += (char)payload[i];
  }

  msg.trim();
  msg.toUpperCase();

  Serial.print("MQTT message [");
  Serial.print(topic);
  Serial.print("] = ");
  Serial.println(msg);

  String topicStr = String(topic);

  if (topicStr == TOPIC_MODE_SET) {
    if (msg == "AUTO") {
      manualMode = false;
      publishModeState();
      Serial.println("Mode changed to AUTO");

      logToSupabase(
        "mode_changed",
        "{"
          "\"mode\":\"AUTO\""
        "}"
      );
    }
    else if (msg == "MANUAL") {
      manualMode = true;
      publishModeState();
      Serial.println("Mode changed to MANUAL");
      setRelay(manualRelayState);

      logToSupabase(
        "mode_changed",
        "{"
          "\"mode\":\"MANUAL\""
        "}"
      );
    }
  }

  else if (topicStr == TOPIC_RELAY_SET) {
    if (msg == "ON") {
      manualRelayState = true;
      Serial.println("Manual relay command: ON");

      logToSupabase(
        "manual_relay_command",
        "{"
          "\"command\":\"ON\""
        "}"
      );

      if (manualMode) {
        setRelay(true);
      }
    }
    else if (msg == "OFF") {
      manualRelayState = false;
      Serial.println("Manual relay command: OFF");

      logToSupabase(
        "manual_relay_command",
        "{"
          "\"command\":\"OFF\""
        "}"
      );

      if (manualMode) {
        setRelay(false);
      }
    }
  }
}

bool connectMQTT() {
  if (mqttClient.connected()) return true;

  Serial.print("Connecting to MQTT... ");

  String clientId = "ESP32-COSensor-";
  clientId += String((uint32_t)ESP.getEfuseMac(), HEX);

  bool ok = mqttClient.connect(
    clientId.c_str(),
    MQTT_USER,
    MQTT_PASSWORD,
    TOPIC_STATUS,
    1,
    true,
    "OFFLINE"
  );

  if (ok) {
    Serial.println("connected");
    mqttClient.publish(TOPIC_STATUS, "ONLINE", true);

    mqttClient.subscribe(TOPIC_RELAY_SET);
    mqttClient.subscribe(TOPIC_MODE_SET);

    publishModeState();
    publishRelayState();

    logToSupabase(
      "mqtt_status",
      "{"
        "\"state\":\"ONLINE\","
        "\"client_id\":\"" + clientId + "\""
      "}"
    );

    return true;
  } else {
    Serial.print("failed, rc=");
    Serial.println(mqttClient.state());
    return false;
  }
}

void publishTelemetry(int ao, bool warmedUp) {
  char payload[320];

  snprintf(
    payload,
    sizeof(payload),
    "{\"ao\":%d,\"alarm\":\"%s\",\"relay\":\"%s\",\"mode\":\"%s\",\"warmedUp\":\"%s\",\"thresholdOn\":%d,\"thresholdOff\":%d}",
    ao,
    alarmOn ? "ON" : "OFF",
    relayState ? "ON" : "OFF",
    manualMode ? "MANUAL" : "AUTO",
    warmedUp ? "ON" : "OFF",
    THRESH_ON,
    THRESH_OFF()
  );

  mqttClient.publish(TOPIC_DATA, payload, true);
}

void publishAlert(const char* state, int ao) {
  char payload[160];

  snprintf(
    payload,
    sizeof(payload),
    "{\"event\":\"%s\",\"ao\":%d,\"thresholdOn\":%d,\"thresholdOff\":%d}",
    state,
    ao,
    THRESH_ON,
    THRESH_OFF()
  );

  mqttClient.publish(TOPIC_ALERT, payload, true);
}

 void publishAirQualityRaw(int ao) {
  if (mqttClient.connected()) {
    char payload[16];
    snprintf(payload, sizeof(payload), "%d", ao);
    mqttClient.publish(TOPIC_DATA, payload, true);
  }
}

// =====================================================
// SETUP
// =====================================================

void setup() {
  Serial.begin(115200);

  pinMode(MQ_DO_PIN, INPUT);
  pinMode(RELAY_PIN, OUTPUT);

  analogSetPinAttenuation(MQ_AO_PIN, ADC_11db);

  for (int i = 0; i < AVG_N; i++) buf[i] = 0;
  sum = 0;
  for (int i = 0; i < AVG_N; i++) readAnalogAvg();

  setRelay(false);
  buzzerInit();
  buzzerOff();

  tStart = millis();

  connectWiFi();

  secureClient.setInsecure();
  mqttClient.setServer(MQTT_HOST, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);

  Serial.println("\n=== MQ135 Alarm + HiveMQ + Supabase started ===");
  Serial.print("THRESH_ON: ");  Serial.println(THRESH_ON);
  Serial.print("THRESH_OFF: "); Serial.println(THRESH_OFF());

  logToSupabase(
    "device_boot",
    "{"
      "\"state\":\"ONLINE\","
      "\"threshold_on\":" + String(THRESH_ON) + ","
      "\"threshold_off\":" + String(THRESH_OFF()) +
    "}"
  );
}

// =====================================================
// LOOP
// =====================================================

void loop() {
  unsigned long now = millis();

  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
  }

  if (!mqttClient.connected()) {
    if (now - lastMqttReconnectAttempt > 3000) {
      lastMqttReconnectAttempt = now;
      connectMQTT();
    }
  } else {
    mqttClient.loop();
  }

  int ao = readAnalogAvg();
  int doRaw = digitalRead(MQ_DO_PIN);
  bool warmedUp = (now - tStart) >= WARMUP_MS;

  // -------- WARMUP --------
  if (!warmedUp) {
    if (!manualMode) {
      setRelay(false);
    }

    buzzerOff();
    alarmOn = false;

    static unsigned long lastWarmPrint = 0;
    if (now - lastWarmPrint > 500) {
      lastWarmPrint = now;
      Serial.print("WARMUP... AO="); Serial.print(ao);
      Serial.print(" | DO="); Serial.println(doRaw);
    }

    if (mqttClient.connected() && now - lastPublishMs >= PUBLISH_INTERVAL_MS) {
      lastPublishMs = now;
      publishTelemetry(ao, warmedUp);
      sendAirQualityRawToSupabase(ao);

      logToSupabase(
        "telemetry",
        "{"
          "\"air_quality_raw\":" + String(ao) + ","
          "\"alarm\":\"OFF\","
          "\"relay\":\"" + String(relayState ? "ON" : "OFF") + "\","
          "\"mode\":\"" + String(manualMode ? "MANUAL" : "AUTO") + "\","
          "\"warmed_up\":\"OFF\""
        "}"
      );
    }

    delay(40);
    return;
  }

  // -------- AUTO MODE --------
  if (!manualMode) {
    if (!alarmOn && ao >= THRESH_ON) {
      alarmOn = true;
      alarmStart = now;
      setRelay(true);

      sirenHigh = false;
      lastSirenStep = 0;

      Serial.println("ALARM ON -> Relay ON + Siren");

      if (mqttClient.connected()) {
        publishAlert("ON", ao);
      }

      logToSupabase(
        "alarm_changed",
        "{"
          "\"alarm\":\"ON\","
          "\"ao\":" + String(ao) + ","
          "\"threshold_on\":" + String(THRESH_ON) +
        "}"
      );
    }

    if (alarmOn && ao <= THRESH_OFF()) {
      if (now - alarmStart >= MIN_ALARM_MS) {
        alarmOn = false;
        setRelay(false);
        buzzerOff();

        Serial.println("ALARM OFF -> Relay OFF + Buzzer OFF");

        if (mqttClient.connected()) {
          publishAlert("OFF", ao);
        }

        logToSupabase(
          "alarm_changed",
          "{"
            "\"alarm\":\"OFF\","
            "\"ao\":" + String(ao) + ","
            "\"threshold_off\":" + String(THRESH_OFF()) +
          "}"
        );
      }
    }

    if (alarmOn) {
      if (now - lastSirenStep >= SIREN_STEP_MS) {
        lastSirenStep = now;
        sirenHigh = !sirenHigh;
        buzzerTone(sirenHigh ? FREQ_HIGH : FREQ_LOW, BUZ_DUTY_ON);
      }
    } else {
      buzzerOff();
    }
  }

  // -------- MANUAL MODE --------
  else {
    setRelay(manualRelayState);
    buzzerOff();
    alarmOn = false;
  }

  // -------- TELEMETRIA --------
  if (mqttClient.connected() && now - lastPublishMs >= PUBLISH_INTERVAL_MS) {
    lastPublishMs = now;
    publishTelemetry(ao, warmedUp);
    publishAirQualityRaw(ao);
    sendAirQualityRawToSupabase(ao);

    logToSupabase(
      "telemetry",
      "{"
        "\"air_quality_raw\":" + String(ao) + ","
        "\"alarm\":\"" + String(alarmOn ? "ON" : "OFF") + "\","
        "\"relay\":\"" + String(relayState ? "ON" : "OFF") + "\","
        "\"mode\":\"" + String(manualMode ? "MANUAL" : "AUTO") + "\","
        "\"warmed_up\":\"" + String(warmedUp ? "ON" : "OFF") + "\""
      "}"
    );
  }

  // -------- DEBUG --------
  static unsigned long lastPrint = 0;
  if (now - lastPrint > 500) {
    lastPrint = now;
    Serial.print("AO="); Serial.print(ao);
    Serial.print(" | DO="); Serial.print(doRaw);
    Serial.print(" | Alarm="); Serial.print(alarmOn ? "ON" : "OFF");
    Serial.print(" | Relay="); Serial.print(relayState ? "ON" : "OFF");
    Serial.print(" | Mode="); Serial.print(manualMode ? "MANUAL" : "AUTO");
    Serial.print(" | MQTT="); Serial.println(mqttClient.connected() ? "ON" : "OFF");
  }

  delay(20);
}
