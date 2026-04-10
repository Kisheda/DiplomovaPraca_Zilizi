#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <AccelStepper.h>
#include <HTTPClient.h>

// ===================== WIFI + MQTT =====================
const char* ssid = "Mark_Wifi";
const char* wifi_password = "Heda90102211";

const char* mqtt_server   = "b583e186639e4bc2aa7bc95605777610.s1.eu.hivemq.cloud";
const int   mqtt_port     = 8883;
const char* mqtt_user     = "Modul_WindowShade";
const char* mqtt_password = "Asd123456789";

// MQTT topics
const char* topic_cmd      = "windowshade/cmd";
const char* topic_state    = "windowshade/state";
const char* topic_position = "windowshade/position";
const char* topic_status   = "windowshade/status";

// ===================== SUPABASE =====================
const char* supabase_url = "https://tmwzwhnpllgumuupmryf.supabase.co";
const char* supabase_key = "sb_publishable_6_J4gNe2AiqsO2Zs6QF4Aw_TaPoHM4H";
const char* supabase_log_endpoint = "/rest/v1/Logs";

const char* module_name = "Modul_WindowShade";

// ===================== STEPPER =====================
const int STEPS_PER_REV = 4096;
const int OPEN_POSITION = 0;
const int CLOSED_POSITION = 2 * STEPS_PER_REV;

// ULN2003 inputs
#define IN1 18
#define IN2 17
#define IN3 16
#define IN4 15

AccelStepper stepper(AccelStepper::FULL4WIRE, IN1, IN3, IN2, IN4);

WiFiClientSecure secureClient;
PubSubClient client(secureClient);

// State
long targetPosition = OPEN_POSITION;
String currentState = "OPEN";

// ===================== JSON ESCAPE =====================
String escapeJson(String s) {
  s.replace("\\", "\\\\");
  s.replace("\"", "\\\"");
  s.replace("\n", "\\n");
  s.replace("\r", "\\r");
  s.replace("\t", "\\t");
  return s;
}

// ===================== SUPABASE LOGGING =====================
bool sendLogToSupabaseRaw(const String& jsonBody) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Supabase log skipped: no WiFi");
    return false;
  }

  WiFiClientSecure logClient;
  logClient.setInsecure();

  HTTPClient https;
  String url = String(supabase_url) + String(supabase_log_endpoint);

  if (!https.begin(logClient, url)) {
    Serial.println("Supabase HTTP begin failed");
    return false;
  }

  https.addHeader("Content-Type", "application/json");
  https.addHeader("apikey", supabase_key);
  https.addHeader("Authorization", "Bearer " + String(supabase_key));
  https.addHeader("Prefer", "return=minimal");

  int httpCode = https.POST(jsonBody);

  Serial.print("Supabase HTTP code: ");
  Serial.println(httpCode);

  if (httpCode <= 0) {
    Serial.print("Supabase POST error: ");
    Serial.println(https.errorToString(httpCode));
    https.end();
    return false;
  }

  String response = https.getString();
  if (response.length() > 0) {
    Serial.println("Supabase response:");
    Serial.println(response);
  }

  https.end();
  return (httpCode >= 200 && httpCode < 300);
}

bool logEvent(const String& eventName) {
  String jsonBody = "{";
  jsonBody += "\"modul\":\"" + String(module_name) + "\",";
  jsonBody += "\"log\":{";
  jsonBody += "\"event\":\"" + escapeJson(eventName) + "\",";
  jsonBody += "\"state\":\"" + escapeJson(currentState) + "\",";
  jsonBody += "\"position\":" + String(stepper.currentPosition()) + ",";
  jsonBody += "\"target_position\":" + String(targetPosition);
  jsonBody += "}";
  jsonBody += "}";

  return sendLogToSupabaseRaw(jsonBody);
}

bool logCommand(const String& cmd) {
  String jsonBody = "{";
  jsonBody += "\"modul\":\"" + String(module_name) + "\",";
  jsonBody += "\"log\":{";
  jsonBody += "\"event\":\"mqtt_command\",";
  jsonBody += "\"command\":\"" + escapeJson(cmd) + "\",";
  jsonBody += "\"state\":\"" + escapeJson(currentState) + "\",";
  jsonBody += "\"position\":" + String(stepper.currentPosition()) + ",";
  jsonBody += "\"target_position\":" + String(targetPosition);
  jsonBody += "}";
  jsonBody += "}";

  return sendLogToSupabaseRaw(jsonBody);
}

bool logStateChange(const String& newState) {
  String jsonBody = "{";
  jsonBody += "\"modul\":\"" + String(module_name) + "\",";
  jsonBody += "\"log\":{";
  jsonBody += "\"event\":\"state_changed\",";
  jsonBody += "\"state\":\"" + escapeJson(newState) + "\",";
  jsonBody += "\"position\":" + String(stepper.currentPosition()) + ",";
  jsonBody += "\"target_position\":" + String(targetPosition);
  jsonBody += "}";
  jsonBody += "}";

  return sendLogToSupabaseRaw(jsonBody);
}

// ===================== WIFI =====================
void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, wifi_password);

  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  logEvent("wifi_connected");
}

// ===================== MQTT =====================
void publishState(const char* state) {
  client.publish(topic_state, state, true);
}

void publishPosition() {
  char buf[20];
  snprintf(buf, sizeof(buf), "%ld", stepper.currentPosition());
  client.publish(topic_position, buf, true);
}

void publishOnline() {
  client.publish(topic_status, "ONLINE", true);
}

void setTargetOpen() {
  targetPosition = OPEN_POSITION;
  stepper.moveTo(targetPosition);
  currentState = "MOVING";
  publishState("MOVING");
  logEvent("opening_started");
}

void setTargetClosed() {
  targetPosition = CLOSED_POSITION;
  stepper.moveTo(targetPosition);
  currentState = "MOVING";
  publishState("MOVING");
  logEvent("closing_started");
}

void handleCommand(String cmd) {
  cmd.trim();
  cmd.toUpperCase();

  Serial.print("Received command: ");
  Serial.println(cmd);

  logCommand(cmd);

  if (cmd == "OPEN" || cmd == "0") {
    setTargetOpen();
  } 
  else if (cmd == "CLOSE" || cmd == "1") {
    setTargetClosed();
  } 
  else if (cmd == "TOGGLE") {
    if (stepper.currentPosition() == OPEN_POSITION) {
      setTargetClosed();
    } else {
      setTargetOpen();
    }
  }
  else if (cmd == "STATE") {
    publishPosition();

    if (stepper.distanceToGo() != 0) {
      publishState("MOVING");
      logEvent("state_requested_moving");
    } else if (stepper.currentPosition() == OPEN_POSITION) {
      publishState("OPEN");
      logEvent("state_requested_open");
    } else if (stepper.currentPosition() == CLOSED_POSITION) {
      publishState("CLOSED");
      logEvent("state_requested_closed");
    } else {
      publishState("PARTIAL");
      logEvent("state_requested_partial");
    }
  }
  else {
    logEvent("unknown_command");
  }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String msg;
  for (unsigned int i = 0; i < length; i++) {
    msg += (char)payload[i];
  }

  Serial.print("MQTT [");
  Serial.print(topic);
  Serial.print("]: ");
  Serial.println(msg);

  handleCommand(msg);
}

void reconnectMQTT() {
  while (!client.connected()) {
    Serial.print("MQTT connecting...");

    String clientId = "ESP32_WindowShade_";
    clientId += String((uint32_t)ESP.getEfuseMac(), HEX);

    if (client.connect(
          clientId.c_str(),
          mqtt_user,
          mqtt_password,
          topic_status,
          1,
          true,
          "OFFLINE")) {

      Serial.println("Successful");
      client.subscribe(topic_cmd);
      publishOnline();
      publishPosition();

      if (stepper.currentPosition() == OPEN_POSITION) {
        publishState("OPEN");
        currentState = "OPEN";
      }
      else if (stepper.currentPosition() == CLOSED_POSITION) {
        publishState("CLOSED");
        currentState = "CLOSED";
      }
      else {
        publishState("PARTIAL");
        currentState = "PARTIAL";
      }

      logEvent("mqtt_connected");

    } else {
      Serial.print("Error, rc=");
      Serial.print(client.state());
      Serial.println(" Retry in 5 seconds...");
      delay(5000);
    }
  }
}

// ===================== SETUP =====================
void setup() {
  Serial.begin(115200);

  stepper.setMaxSpeed(900.0);
  stepper.setAcceleration(400.0);
  stepper.setCurrentPosition(OPEN_POSITION);

  connectWiFi();

  secureClient.setInsecure();

  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(mqttCallback);

  logEvent("boot");
  reconnectMQTT();
}

// ===================== LOOP =====================
void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
  }

  if (!client.connected()) {
    reconnectMQTT();
  }

  client.loop();

  if (stepper.distanceToGo() != 0) {
    stepper.run();
  } else {
    static String lastStableState = "";

    if (stepper.currentPosition() == OPEN_POSITION) {
      currentState = "OPEN";
    } else if (stepper.currentPosition() == CLOSED_POSITION) {
      currentState = "CLOSED";
    } else {
      currentState = "PARTIAL";
    }

    if (currentState != lastStableState) {
      publishState(currentState.c_str());
      publishPosition();
      logStateChange(currentState);
      lastStableState = currentState;
    }
  }
}
