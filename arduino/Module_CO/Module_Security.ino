#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <SPI.h>
#include <MFRC522.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// =========================
// WIFI
// =========================
const char* ssid = "Mark_Wifi";
const char* wifi_password = "Heda90102211";

// =========================
// MQTT
// =========================
const char* mqtt_server   = "b583e186639e4bc2aa7bc95605777610.s1.eu.hivemq.cloud";
const int   mqtt_port     = 8883;
const char* mqtt_user     = "Modul_Security";
const char* mqtt_password = "Asd123456789";

const char* TOPIC_ALERT         = "modul_cosensor/siren";
const char* TOPIC_STATE         = "modul_security/state";
const char* TOPIC_ENROLL_SET    = "modul_security/enroll/set";
const char* TOPIC_ENROLL_STATE  = "modul_security/enroll/state";
const char* TOPIC_ALARM         = "modul_security/alarm";

// =========================
// SUPABASE
// =========================
const char* supabase_url = "https://tmwzwhnpllgumuupmryf.supabase.co";
const char* supabase_key = "sb_publishable_6_J4gNe2AiqsO2Zs6QF4Aw_TaPoHM4H";
const char* supabase_log_endpoint = "/rest/v1/Logs";
const char* supabase_cards_select_endpoint = "/rest/v1/Authorized_cards?select=uid,active&active=eq.true";
const char* supabase_cards_insert_endpoint = "/rest/v1/Authorized_cards";

// =========================
// PINS
// =========================
#define SS_PIN     17
#define RST_PIN    22
#define PIR_PIN    27
#define REED_PIN   25

// =========================
// OBJECTS
// =========================
MFRC522 rfid(SS_PIN, RST_PIN);
WiFiClientSecure espClient;
PubSubClient client(espClient);

// =========================
// STATE
// =========================
bool systemArmed = false;
bool sirenState = false;
bool enrollMode = false;

unsigned long lastScanTime = 0;
const unsigned long scanCooldown = 1500;

unsigned long lastCardsRefresh = 0;
const unsigned long cardsRefreshInterval = 60000;

unsigned long lastStatusLog = 0;
const unsigned long statusLogInterval = 300000; // 5 perc

// =========================
// CARD CACHE
// =========================
const int MAX_CARDS = 50;
String authorizedCards[MAX_CARDS];
int authorizedCardCount = 0;

// =========================
// HELPERS
// =========================
String uidToString(const MFRC522::Uid &uid) {
  String out = "";
  for (byte i = 0; i < uid.size; i++) {
    if (uid.uidByte[i] < 0x10) out += "0";
    out += String(uid.uidByte[i], HEX);
    if (i < uid.size - 1) out += ":";
  }
  out.toUpperCase();
  return out;
}

void printUid(const MFRC522::Uid &uid) {
  Serial.print("UID: ");
  for (byte i = 0; i < uid.size; i++) {
    if (uid.uidByte[i] < 0x10) Serial.print("0");
    Serial.print(uid.uidByte[i], HEX);
    Serial.print(" ");
  }
  Serial.println();
}

bool isAuthorizedUid(const String& uid) {
  for (int i = 0; i < authorizedCardCount; i++) {
    if (authorizedCards[i] == uid) return true;
  }
  return false;
}

// =========================
// SUPABASE LOGGING
// =========================
void sendSupabaseLog(
  const String& eventName,
  const String& detail,
  const String& uid = "",
  bool includeSensors = true
) {
  if (WiFi.status() != WL_CONNECTED) return;

  bool motion = digitalRead(PIR_PIN) == HIGH;
  bool windowOpen = digitalRead(REED_PIN) == HIGH;

  WiFiClientSecure httpsClient;
  httpsClient.setInsecure();

  HTTPClient http;
  String url = String(supabase_url) + supabase_log_endpoint;

  if (!http.begin(httpsClient, url)) {
    Serial.println("Supabase HTTP begin failed");
    return;
  }

  http.addHeader("Content-Type", "application/json");
  http.addHeader("apikey", supabase_key);
  http.addHeader("Authorization", String("Bearer ") + supabase_key);
  http.addHeader("Prefer", "return=minimal");

  String payload = "{";
  payload += "\"modul\":\"security\",";
  payload += "\"log\":{";
  payload += "\"event\":\"" + eventName + "\",";
  payload += "\"detail\":\"" + detail + "\",";
  payload += "\"uid\":\"" + uid + "\",";
  payload += "\"armed\":" + String(systemArmed ? "true" : "false") + ",";
  payload += "\"siren\":" + String(sirenState ? "true" : "false") + ",";
  payload += "\"enroll_mode\":" + String(enrollMode ? "true" : "false") + ",";
  payload += "\"wifi_rssi\":" + String(WiFi.RSSI()) + ",";
  payload += "\"ip\":\"" + WiFi.localIP().toString() + "\",";
  payload += "\"cards_loaded\":" + String(authorizedCardCount) + ",";
  if (includeSensors) {
    payload += "\"pir\":" + String(motion ? "true" : "false") + ",";
    payload += "\"window_open\":" + String(windowOpen ? "true" : "false") + ",";
  }
  payload += "\"free_heap\":" + String(ESP.getFreeHeap());
  payload += "}}";

  int httpCode = http.POST(payload);

  Serial.print("Supabase log -> ");
  Serial.println(httpCode);

  http.end();
}

// =========================
// SUPABASE CARDS
// =========================
bool fetchAuthorizedCards() {
  if (WiFi.status() != WL_CONNECTED) return false;

  WiFiClientSecure httpsClient;
  httpsClient.setInsecure();

  HTTPClient http;
  String url = String(supabase_url) + supabase_cards_select_endpoint;

  if (!http.begin(httpsClient, url)) {
    Serial.println("Cards fetch begin failed");
    return false;
  }

  http.addHeader("apikey", supabase_key);
  http.addHeader("Authorization", String("Bearer ") + supabase_key);

  int httpCode = http.GET();
  Serial.print("Cards fetch HTTP -> ");
  Serial.println(httpCode);

  if (httpCode != 200) {
    http.end();
    sendSupabaseLog("cards_load_failed", "Failed to fetch authorized cards");
    return false;
  }

  String response = http.getString();
  http.end();

  DynamicJsonDocument doc(8192);
  DeserializationError error = deserializeJson(doc, response);

  if (error) {
    Serial.print("JSON parse error: ");
    Serial.println(error.c_str());
    sendSupabaseLog("cards_parse_failed", "Failed to parse authorized cards JSON");
    return false;
  }

  authorizedCardCount = 0;

  for (JsonVariant item : doc.as<JsonArray>()) {
    if (authorizedCardCount >= MAX_CARDS) break;

    String uid = item["uid"] | "";
    uid.toUpperCase();

    if (uid.length() > 0) {
      authorizedCards[authorizedCardCount++] = uid;
    }
  }

  Serial.print("Loaded cards: ");
  Serial.println(authorizedCardCount);

  sendSupabaseLog("cards_loaded", "Authorized cards loaded from database");
  return true;
}

bool addCardToSupabase(const String& uid) {
  if (WiFi.status() != WL_CONNECTED) return false;

  WiFiClientSecure httpsClient;
  httpsClient.setInsecure();

  HTTPClient http;
  String url = String(supabase_url) + supabase_cards_insert_endpoint;

  if (!http.begin(httpsClient, url)) {
    Serial.println("Add card begin failed");
    return false;
  }

  http.addHeader("Content-Type", "application/json");
  http.addHeader("apikey", supabase_key);
  http.addHeader("Authorization", String("Bearer ") + supabase_key);
  http.addHeader("Prefer", "return=minimal,resolution=merge-duplicates");

  String payload = "{";
  payload += "\"uid\":\"" + uid + "\",";
  payload += "\"active\":true";
  payload += "}";

  int httpCode = http.POST(payload);

  Serial.print("Add card HTTP -> ");
  Serial.println(httpCode);

  http.end();

  return (httpCode == 200 || httpCode == 201);
}

// =========================
// MQTT
// =========================
void publishSiren(bool on) {
  if (!client.connected()) return;

  if (on) {
    client.publish(TOPIC_ALERT, "ON", true);
    sirenState = true;
    Serial.println("MQTT -> ON");
    sendSupabaseLog("siren_on", "Siren activated");
  } else {
    client.publish(TOPIC_ALERT, "OFF", true);
    sirenState = false;
    Serial.println("MQTT -> OFF");
    sendSupabaseLog("siren_off", "Siren deactivated");
  }
}

void publishEnrollState() {
  if (!client.connected()) return;
  client.publish(TOPIC_ENROLL_STATE, enrollMode ? "ON" : "OFF", true);
}

void publishAlarmState() {
  if (!client.connected()) return;
  client.publish(TOPIC_ALARM, systemArmed ? "ON" : "OFF", true);
  Serial.print("MQTT -> modul_security/alarm: ");
  Serial.println(systemArmed ? "ON" : "OFF");
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String msg = "";
  for (unsigned int i = 0; i < length; i++) {
    msg += (char)payload[i];
  }
  msg.trim();
  msg.toUpperCase();

  String topicStr = String(topic);

  Serial.print("MQTT RX [");
  Serial.print(topicStr);
  Serial.print("] -> ");
  Serial.println(msg);

  if (topicStr == TOPIC_ENROLL_SET) {
    if (msg == "ON") {
      enrollMode = true;
      publishEnrollState();
      sendSupabaseLog("enroll_mode_on", "Enroll mode enabled from web");
    } else if (msg == "OFF") {
      enrollMode = false;
      publishEnrollState();
      sendSupabaseLog("enroll_mode_off", "Enroll mode disabled from web");
    }
  }
}

// =========================
// WIFI + MQTT
// =========================
void connectWiFi() {
  Serial.print("WiFi connecting...");
  WiFi.begin(ssid, wifi_password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi OK");
  Serial.println(WiFi.localIP());
  sendSupabaseLog("wifi_connected", "ESP32 connected to WiFi");
}

void reconnectMQTT() {
  while (!client.connected()) {
    Serial.print("MQTT connecting...");

    String clientId = "ESP32-Security-";
    clientId += String(random(0xffff), HEX);

    bool ok = client.connect(
      clientId.c_str(),
      mqtt_user,
      mqtt_password,
      TOPIC_STATE,
      1,
      true,
      "OFFLINE"
    );

    if (ok) {
      Serial.println("OK");
      client.publish(TOPIC_STATE, "ONLINE", true);
      client.subscribe(TOPIC_ENROLL_SET);
      publishEnrollState();
      publishAlarmState();
      publishSiren(false);
      sendSupabaseLog("module_online", "Security module connected to MQTT and set ONLINE");
    } else {
      Serial.print("fail ");
      Serial.println(client.state());
      delay(3000);
    }
  }
}

// =========================
// SETUP
// =========================
void setup() {
  Serial.begin(115200);

  pinMode(PIR_PIN, INPUT);
  pinMode(REED_PIN, INPUT_PULLUP);

  SPI.begin(18, 19, 23, SS_PIN);
  rfid.PCD_Init();
  rfid.PCD_SetAntennaGain(rfid.RxGain_max);

  connectWiFi();

  espClient.setInsecure();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(mqttCallback);

  reconnectMQTT();
  fetchAuthorizedCards();

  Serial.println("SYSTEM READY");
  sendSupabaseLog("system_ready", "Security module ready");
}

// =========================
// LOOP
// =========================
void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
  }

  if (!client.connected()) {
    reconnectMQTT();
  }

  client.loop();

  if (millis() - lastCardsRefresh > cardsRefreshInterval) {
    fetchAuthorizedCards();
    lastCardsRefresh = millis();
  }

  if (millis() - lastStatusLog > statusLogInterval) {
    sendSupabaseLog("heartbeat", "Security module heartbeat");
    lastStatusLog = millis();
  }

  // RFID
  if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
    unsigned long now = millis();
    printUid(rfid.uid);

    String uidStr = uidToString(rfid.uid);

    if (now - lastScanTime > scanCooldown) {
      if (enrollMode) {
        if (isAuthorizedUid(uidStr)) {
          Serial.println("CARD ALREADY EXISTS");
          sendSupabaseLog("card_exists", "Card already exists in database", uidStr);
        } else {
          if (addCardToSupabase(uidStr)) {
            Serial.println("CARD ADDED TO DB");
            sendSupabaseLog("card_added", "New card added to database", uidStr);
            fetchAuthorizedCards();
          } else {
            Serial.println("FAILED TO ADD CARD");
            sendSupabaseLog("card_add_failed", "Failed to add new card to database", uidStr);
          }
        }

        enrollMode = false;
        publishEnrollState();
        sendSupabaseLog("enroll_mode_off", "Enroll mode auto-disabled after card scan", uidStr);
        lastScanTime = now;
      } else {
        if (isAuthorizedUid(uidStr)) {
          sendSupabaseLog("rfid_authorized", "Authorized RFID scanned", uidStr);

          systemArmed = !systemArmed;

          if (systemArmed) {
            Serial.println("ALARM ARMED");
            publishAlarmState();
            sendSupabaseLog("alarm_armed", "Alarm armed by RFID", uidStr);
          } else {
            Serial.println("ALARM DISARMED");
            sendSupabaseLog("alarm_disarmed", "Alarm disarmed by RFID", uidStr);
            publishAlarmState();
            publishSiren(false);
          }

          lastScanTime = now;
        } else {
          Serial.println("ACCESS DENIED");
          sendSupabaseLog("rfid_denied", "Unauthorized RFID scanned", uidStr);
        }
      }
    }

    rfid.PICC_HaltA();
    rfid.PCD_StopCrypto1();
  }

  // Alarm logic
  if (systemArmed) {
    int motion = digitalRead(PIR_PIN);
    bool windowOpen = (digitalRead(REED_PIN) == HIGH);

    if (motion == HIGH || windowOpen) {
      if (!sirenState) {
        if (motion == HIGH) {
          Serial.println("MOTION DETECTED");
          sendSupabaseLog("motion_detected", "PIR detected motion");
        }

        if (windowOpen) {
          Serial.println("WINDOW OPENED");
          sendSupabaseLog("window_opened", "Magnetic sensor detected open window");
        }

        Serial.println("ALARM TRIGGERED");
        sendSupabaseLog("alarm_triggered", "Alarm triggered by PIR or reed sensor");

        publishSiren(true);
      }
    } else {
      if (sirenState) {
        publishSiren(false);
      }
    }
  } else {
    if (sirenState) {
      publishSiren(false);
    }
  }

  delay(50);
}
