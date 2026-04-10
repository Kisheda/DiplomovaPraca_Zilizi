#include <lvgl.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <SPI.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <time.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// =========================
// Hardware and display pins
// =========================
#define XPT2046_IRQ 36
#define XPT2046_MOSI 32
#define XPT2046_MISO 39
#define XPT2046_CLK 25
#define XPT2046_CS 33

#define SCREEN_WIDTH 240
#define SCREEN_HEIGHT 320

SPIClass touchscreenSPI = SPIClass(VSPI);
XPT2046_Touchscreen touchscreen(XPT2046_CS, XPT2046_IRQ);

#define DRAW_BUF_SIZE (SCREEN_WIDTH * SCREEN_HEIGHT / 10 * (LV_COLOR_DEPTH / 8))
uint32_t draw_buf[DRAW_BUF_SIZE / 4];

// =========================
// Configuration
// =========================
const char* ssid = "Mark_Wifi";
const char* wifi_password = "Heda90102211";

const char* weather_api_key = "81c94b4135766c09a1632cc9b00bb966";
const char* weather_city = "Nitra";
const char* weather_country = "SK";

const char* mqtt_server   = "b583e186639e4bc2aa7bc95605777610.s1.eu.hivemq.cloud";
const int   mqtt_port     = 8883;
const char* mqtt_user     = "Modul_Display";
const char* mqtt_password = "Asd123456789";

// =========================
// SUPABASE
// =========================
const char* supabase_url = "https://tmwzwhnpllgumuupmryf.supabase.co";
const char* supabase_key = "sb_publishable_6_J4gNe2AiqsO2Zs6QF4Aw_TaPoHM4H";
const char* supabase_log_endpoint = "/rest/v1/Logs";

// =========================
// MQTT Topics
// =========================
const char* topic_meteo_status = "meteostanica/status";
const char* topic_soil         = "meteostanica/soil";
const char* topic_temp         = "meteostanica/temperature";
const char* topic_hum          = "meteostanica/humidity";
const char* topic_light        = "meteostanica/light";
const char* topic_pressure     = "meteostanica/pressure";
const char* topic_pump_set     = "meteostanica/pump/set";
const char* topic_pump_state   = "meteostanica/pump";

const char* TOPIC_STATUS       = "modul_cosensor/status";
const char* TOPIC_DATA         = "modul_cosensor/airquality";
const char* TOPIC_ALERT        = "modul_cosensor/siren";
const char* TOPIC_RELAY_SET    = "modul_cosensor/relay/set";
const char* TOPIC_RELAY_STATE  = "modul_cosensor/relay/state";
const char* TOPIC_MODE_SET     = "modul_cosensor/mode/set";
const char* TOPIC_MODE_STATE   = "modul_cosensor/mode/state";

const char* topic_ws_cmd       = "windowshade/cmd";
const char* topic_ws_state     = "windowshade/state";

const char* TOPIC_DISPLAY_STATUS = "modul_display/status";

WiFiClientSecure secureClient;
PubSubClient mqttClient(secureClient);

// =========================
// System state variables
// =========================
enum AppPage {
  PAGE_HOME = 0,
  PAGE_MEASUREMENTS,
  PAGE_SECURITY,
  PAGE_AIRQUALITY,
  PAGE_WINDOW
};

AppPage currentPage = PAGE_HOME;

String value_time = "--:--";
String value_date = "--";
String value_weather = "--";
String value_outtemp = "--";

String value_meteo_status = "--";
String value_soil = "--";
String value_temp = "--";
String value_hum = "--";
String value_light = "--";
String value_pressure = "--";
String value_pump = "--";

String value_co_status = "--";
String value_airquality = "--";
String value_siren = "--";
String value_relay_state = "--";
String value_mode_state = "--";

String value_ws_state = "--";

bool lastWifiConnectedState = false;
bool lastMqttConnectedState = false;
AppPage lastLoggedPage = PAGE_HOME;

// =========================
// LVGL UI objects
// =========================
lv_obj_t* title_label;
lv_obj_t* home_screen;
lv_obj_t* measurements_screen;
lv_obj_t* security_screen;
lv_obj_t* airquality_screen;
lv_obj_t* window_screen;

lv_obj_t *home_time, *home_date, *home_weather, *home_temp;

lv_obj_t *meas_ta_state;
lv_obj_t *meas_ta_temp;
lv_obj_t *meas_ta_hum;
lv_obj_t *meas_ta_light;
lv_obj_t *meas_ta_pressure;
lv_obj_t *meas_ta_soil;
lv_obj_t *meas_ta_pump;
lv_obj_t *meas_btn_pump_label;

lv_obj_t *sec_ta_state, *sec_ta_alarm;

lv_obj_t *aq_ta_state, *aq_btn_mode_label, *aq_btn_relay_label, *aq_airquality_label;

lv_obj_t *ws_label_state;

// =========================
// Formatting helpers
// =========================
String normalizeUpper(String val) {
  val.trim();
  val.toUpperCase();
  return val;
}

String getOnOffText(String val) {
  val = normalizeUpper(val);
  if (val == "ON") return "ON";
  if (val == "OFF") return "OFF";
  return "--";
}

String getModeText(String val) {
  val = normalizeUpper(val);
  if (val == "AUTO") return "AUTO";
  if (val == "MANUAL") return "MANUAL";
  return "--";
}

String getStatusText(String val) {
  val = normalizeUpper(val);
  if (val == "ONLINE" || val == "1") return "ONLINE";
  if (val == "OFFLINE" || val == "0") return "OFFLINE";
  return "--";
}

const char* pageName(AppPage page) {
  switch (page) {
    case PAGE_HOME: return "Home";
    case PAGE_MEASUREMENTS: return "Measurements";
    case PAGE_SECURITY: return "Security";
    case PAGE_AIRQUALITY: return "Air Quality";
    case PAGE_WINDOW: return "Window";
    default: return "Unknown";
  }
}

// =========================
// Supabase logging
// =========================
void logToSupabase(const String& modul, const String& eventType, const String& message) {
  if (WiFi.status() != WL_CONNECTED) return;

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  String url = String(supabase_url) + String(supabase_log_endpoint);

  if (!http.begin(client, url)) return;

  http.addHeader("Content-Type", "application/json");
  http.addHeader("apikey", supabase_key);
  http.addHeader("Authorization", "Bearer " + String(supabase_key));
  http.addHeader("Prefer", "return=minimal");

  DynamicJsonDocument body(512);
  body["modul"] = modul;

  JsonObject logObj = body.createNestedObject("log");
  logObj["event"] = eventType;
  logObj["message"] = message;
  logObj["uptime_ms"] = millis();
  logObj["free_heap"] = ESP.getFreeHeap();

  String payload;
  serializeJson(body, payload);

  http.POST(payload);
  http.end();
}

// =========================
// Touch
// =========================
void touchscreen_read(lv_indev_t * indev, lv_indev_data_t * data) {
  if (touchscreen.tirqTouched() && touchscreen.touched()) {
    TS_Point p = touchscreen.getPoint();
    data->state = LV_INDEV_STATE_PRESSED;
    data->point.x = map(p.x, 200, 3700, 1, SCREEN_WIDTH);
    data->point.y = map(p.y, 240, 3800, 1, SCREEN_HEIGHT);
  } else {
    data->state = LV_INDEV_STATE_RELEASED;
  }
}

// =========================
// Time / Weather
// =========================
void updateTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return;

  char t[16], d[32];
  strftime(t, sizeof(t), "%H:%M:%S", &timeinfo);
  strftime(d, sizeof(d), "%d.%m.%Y", &timeinfo);

  value_time = String(t);
  value_date = String(d);
}

void updateWeather() {
  if (WiFi.status() != WL_CONNECTED) return;

  WiFiClientSecure weatherClient;
  weatherClient.setInsecure();
  HTTPClient http;

  String url = "https://api.openweathermap.org/data/2.5/weather?q=" +
               String(weather_city) + "," + String(weather_country) +
               "&appid=" + String(weather_api_key) +
               "&units=metric&lang=en";

  if (http.begin(weatherClient, url)) {
    int code = http.GET();
    if (code == 200) {
      DynamicJsonDocument doc(2048);
      DeserializationError err = deserializeJson(doc, http.getString());

      if (!err) {
        value_outtemp = String((float)doc["main"]["temp"], 1);
        value_weather = doc["weather"][0]["description"].as<String>();
      } else {
        logToSupabase("weather", "json_error", "Failed to parse weather JSON");
      }
    } else {
      logToSupabase("weather", "http_error", "Weather API HTTP " + String(code));
    }
    http.end();
  } else {
    logToSupabase("weather", "begin_failed", "Weather HTTP begin failed");
  }
}

// =========================
// UI refresh
// =========================
void update_all_ui_elements() {
  if (home_time) lv_label_set_text(home_time, value_time.c_str());
  if (home_date) lv_label_set_text(home_date, value_date.c_str());
  if (home_weather) lv_label_set_text(home_weather, (String("Weather: ") + value_weather).c_str());
  if (home_temp) lv_label_set_text(home_temp, (String("Outside: ") + value_outtemp + " C").c_str());

  if (meas_ta_state)    lv_textarea_set_text(meas_ta_state,    (String("State: ") + getStatusText(value_meteo_status)).c_str());
  if (meas_ta_temp)     lv_textarea_set_text(meas_ta_temp,     (String("Temperature: ") + value_temp + " C").c_str());
  if (meas_ta_hum)      lv_textarea_set_text(meas_ta_hum,      (String("Humidity: ") + value_hum + " %").c_str());
  if (meas_ta_light)    lv_textarea_set_text(meas_ta_light,    (String("Light: ") + value_light + " lx").c_str());
  if (meas_ta_pressure) lv_textarea_set_text(meas_ta_pressure, (String("Pressure: ") + value_pressure + " hPa").c_str());
  if (meas_ta_soil)     lv_textarea_set_text(meas_ta_soil,     (String("Soil: ") + value_soil + " %").c_str());
  if (meas_ta_pump)     lv_textarea_set_text(meas_ta_pump,     (String("Pump: ") + getOnOffText(value_pump)).c_str());
  if (meas_btn_pump_label) lv_label_set_text(meas_btn_pump_label, "WATER");

  if (sec_ta_state) lv_textarea_set_text(sec_ta_state, (String("State: ") + getStatusText(value_co_status)).c_str());
  if (sec_ta_alarm) lv_textarea_set_text(sec_ta_alarm, (String("Alarm: ") + getOnOffText(value_siren)).c_str());

  if (aq_ta_state) lv_textarea_set_text(aq_ta_state, (String("State: ") + getStatusText(value_co_status)).c_str());
  if (aq_btn_mode_label) lv_label_set_text(aq_btn_mode_label, getModeText(value_mode_state).c_str());
  if (aq_btn_relay_label) lv_label_set_text(aq_btn_relay_label, getOnOffText(value_relay_state).c_str());
  if (aq_airquality_label) lv_label_set_text(aq_airquality_label, (String("Air quality: ") + value_airquality).c_str());

  if (ws_label_state) lv_label_set_text(ws_label_state, (String("State: ") + value_ws_state).c_str());
}

void show_current_page() {
  lv_obj_add_flag(home_screen, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(measurements_screen, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(security_screen, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(airquality_screen, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(window_screen, LV_OBJ_FLAG_HIDDEN);

  switch (currentPage) {
    case PAGE_HOME:
      lv_obj_clear_flag(home_screen, LV_OBJ_FLAG_HIDDEN);
      lv_label_set_text(title_label, "Home");
      break;
    case PAGE_MEASUREMENTS:
      lv_obj_clear_flag(measurements_screen, LV_OBJ_FLAG_HIDDEN);
      lv_label_set_text(title_label, "Measurements");
      break;
    case PAGE_SECURITY:
      lv_obj_clear_flag(security_screen, LV_OBJ_FLAG_HIDDEN);
      lv_label_set_text(title_label, "Security");
      break;
    case PAGE_AIRQUALITY:
      lv_obj_clear_flag(airquality_screen, LV_OBJ_FLAG_HIDDEN);
      lv_label_set_text(title_label, "Air Quality");
      break;
    case PAGE_WINDOW:
      lv_obj_clear_flag(window_screen, LV_OBJ_FLAG_HIDDEN);
      lv_label_set_text(title_label, "Window");
      break;
  }

  if (currentPage != lastLoggedPage) {
    lastLoggedPage = currentPage;
    logToSupabase("display", "page_change", pageName(currentPage));
  }
}

static void nav_left_event(lv_event_t * e) {
  LV_UNUSED(e);
  currentPage = (AppPage)((currentPage + 4) % 5);
  show_current_page();
}

static void nav_right_event(lv_event_t * e) {
  LV_UNUSED(e);
  currentPage = (AppPage)((currentPage + 1) % 5);
  show_current_page();
}

// =========================
// UI helpers
// =========================
lv_obj_t* create_text_area(lv_obj_t* parent, int y, int h) {
  lv_obj_t* ta = lv_textarea_create(parent);
  lv_obj_set_size(ta, 200, h);
  lv_obj_align(ta, LV_ALIGN_TOP_MID, 0, y);
  lv_obj_add_state(ta, LV_STATE_DISABLED);
  return ta;
}

// =========================
// GUI
// =========================
void create_gui() {
  lv_obj_t * screen = lv_screen_active();

  lv_obj_t* nav = lv_obj_create(screen);
  lv_obj_set_size(nav, 320, 50);
  lv_obj_align(nav, LV_ALIGN_TOP_MID, 0, 0);

  lv_obj_t* btn_l = lv_button_create(nav);
  lv_obj_set_size(btn_l, 40, 30);
  lv_obj_align(btn_l, LV_ALIGN_LEFT_MID, 0, 0);
  lv_obj_add_event_cb(btn_l, nav_left_event, LV_EVENT_CLICKED, NULL);
  lv_obj_t* lbl_l = lv_label_create(btn_l);
  lv_label_set_text(lbl_l, "<");
  lv_obj_center(lbl_l);

  lv_obj_t* btn_r = lv_button_create(nav);
  lv_obj_set_size(btn_r, 40, 30);
  lv_obj_align(btn_r, LV_ALIGN_RIGHT_MID, 0, 0);
  lv_obj_add_event_cb(btn_r, nav_right_event, LV_EVENT_CLICKED, NULL);
  lv_obj_t* lbl_r = lv_label_create(btn_r);
  lv_label_set_text(lbl_r, ">");
  lv_obj_center(lbl_r);

  title_label = lv_label_create(nav);
  lv_obj_center(title_label);

  // HOME
  home_screen = lv_obj_create(screen);
  lv_obj_set_size(home_screen, 320, 240);
  lv_obj_align(home_screen, LV_ALIGN_TOP_MID, 0, 50);

  home_time = lv_label_create(home_screen);
  lv_obj_set_style_text_font(home_time, &lv_font_montserrat_28, 0);
  lv_obj_align(home_time, LV_ALIGN_TOP_MID, 0, 10);

  home_date = lv_label_create(home_screen);
  lv_obj_align(home_date, LV_ALIGN_TOP_MID, 0, 50);

  home_weather = lv_label_create(home_screen);
  lv_obj_align(home_weather, LV_ALIGN_TOP_MID, 0, 90);

  home_temp = lv_label_create(home_screen);
  lv_obj_align(home_temp, LV_ALIGN_TOP_MID, 0, 120);

  // MEASUREMENTS
  measurements_screen = lv_obj_create(screen);
  lv_obj_set_size(measurements_screen, 320, 240);
  lv_obj_align(measurements_screen, LV_ALIGN_TOP_MID, 0, 50);

  meas_ta_state = create_text_area(measurements_screen, 0, 30);
  meas_ta_temp = create_text_area(measurements_screen, 32, 30);
  meas_ta_hum = create_text_area(measurements_screen, 64, 30);
  meas_ta_light = create_text_area(measurements_screen, 96, 30);
  meas_ta_pressure = create_text_area(measurements_screen, 128, 30);
  meas_ta_soil = create_text_area(measurements_screen, 160, 30);
  meas_ta_pump = create_text_area(measurements_screen, 192, 30);

  lv_obj_t* btn_water = lv_button_create(measurements_screen);
  lv_obj_set_size(btn_water, 90, 28);
  lv_obj_align(btn_water, LV_ALIGN_TOP_RIGHT, -10, 224);
  lv_obj_add_event_cb(btn_water, [](lv_event_t* e) {
    LV_UNUSED(e);
    mqttClient.publish(topic_pump_set, "ON", true);
    logToSupabase("measurements", "water_button_pressed", "Pump ON command sent");
  }, LV_EVENT_CLICKED, NULL);
  meas_btn_pump_label = lv_label_create(btn_water);
  lv_obj_center(meas_btn_pump_label);

  // SECURITY
  security_screen = lv_obj_create(screen);
  lv_obj_set_size(security_screen, 320, 240);
  lv_obj_align(security_screen, LV_ALIGN_TOP_MID, 0, 50);

  sec_ta_state = create_text_area(security_screen, 20, 40);
  sec_ta_alarm = create_text_area(security_screen, 70, 40);

  // AIR QUALITY
  airquality_screen = lv_obj_create(screen);
  lv_obj_set_size(airquality_screen, 320, 240);
  lv_obj_align(airquality_screen, LV_ALIGN_TOP_MID, 0, 50);

  aq_ta_state = create_text_area(airquality_screen, 5, 35);

  aq_airquality_label = lv_label_create(airquality_screen);
  lv_obj_align(aq_airquality_label, LV_ALIGN_TOP_MID, 0, 50);

  lv_obj_t* btn_mode = lv_button_create(airquality_screen);
  lv_obj_set_size(btn_mode, 130, 45);
  lv_obj_align(btn_mode, LV_ALIGN_TOP_MID, -70, 100);
  lv_obj_add_event_cb(btn_mode, [](lv_event_t* e) {
    LV_UNUSED(e);
    String current = normalizeUpper(value_mode_state);
    const char* nextMode = (current == "AUTO") ? "MANUAL" : "AUTO";
    mqttClient.publish(TOPIC_MODE_SET, nextMode, true);
    logToSupabase("airquality", "mode_button_pressed", String("Sent ") + nextMode);
  }, LV_EVENT_CLICKED, NULL);

  aq_btn_mode_label = lv_label_create(btn_mode);
  lv_obj_center(aq_btn_mode_label);

  lv_obj_t* btn_vent = lv_button_create(airquality_screen);
  lv_obj_set_size(btn_vent, 130, 45);
  lv_obj_align(btn_vent, LV_ALIGN_TOP_MID, 70, 100);
  lv_obj_add_event_cb(btn_vent, [](lv_event_t* e) {
    LV_UNUSED(e);
    String current = normalizeUpper(value_relay_state);
    const char* nextRelay = (current == "ON") ? "OFF" : "ON";
    mqttClient.publish(TOPIC_RELAY_SET, nextRelay, true);
    logToSupabase("airquality", "ventilation_button_pressed", String("Sent ") + nextRelay);
  }, LV_EVENT_CLICKED, NULL);

  aq_btn_relay_label = lv_label_create(btn_vent);
  lv_obj_center(aq_btn_relay_label);

  // WINDOW
  window_screen = lv_obj_create(screen);
  lv_obj_set_size(window_screen, 320, 240);
  lv_obj_align(window_screen, LV_ALIGN_TOP_MID, 0, 50);

  ws_label_state = lv_label_create(window_screen);
  lv_obj_align(ws_label_state, LV_ALIGN_TOP_MID, 0, 10);

  lv_obj_t* btn_open = lv_button_create(window_screen);
  lv_obj_set_size(btn_open, 80, 40);
  lv_obj_align(btn_open, LV_ALIGN_CENTER, -90, 0);
  lv_obj_add_event_cb(btn_open, [](lv_event_t* e) {
    LV_UNUSED(e);
    mqttClient.publish(topic_ws_cmd, "OPEN");
    logToSupabase("window", "button_pressed", "OPEN command sent");
  }, LV_EVENT_CLICKED, NULL);
  lv_obj_t* lbl_open = lv_label_create(btn_open);
  lv_label_set_text(lbl_open, "OPEN");
  lv_obj_center(lbl_open);

  lv_obj_t* btn_close = lv_button_create(window_screen);
  lv_obj_set_size(btn_close, 80, 40);
  lv_obj_align(btn_close, LV_ALIGN_CENTER, 90, 0);
  lv_obj_add_event_cb(btn_close, [](lv_event_t* e) {
    LV_UNUSED(e);
    mqttClient.publish(topic_ws_cmd, "CLOSE");
    logToSupabase("window", "button_pressed", "CLOSE command sent");
  }, LV_EVENT_CLICKED, NULL);
  lv_obj_t* lbl_close = lv_label_create(btn_close);
  lv_label_set_text(lbl_close, "CLOSE");
  lv_obj_center(lbl_close);

  show_current_page();
}

// =========================
// MQTT callback
// =========================
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String msg = "";
  for (unsigned int i = 0; i < length; i++) msg += (char)payload[i];

  String t = String(topic);
  String m = msg;
  m.trim();

  String mu = m;
  mu.toUpperCase();

  Serial.print("[MQTT] ");
  Serial.print(t);
  Serial.print(" -> ");
  Serial.println(msg);

  if (t == topic_meteo_status) value_meteo_status = mu;
  else if (t == topic_soil) value_soil = m;
  else if (t == topic_temp) value_temp = m;
  else if (t == topic_hum) value_hum = m;
  else if (t == topic_light) value_light = m;
  else if (t == topic_pressure) value_pressure = m;
  else if (t == topic_pump_state) value_pump = mu;
  else if (t == TOPIC_STATUS) value_co_status = mu;
  else if (t == TOPIC_DATA) value_airquality = msg;
  else if (t == TOPIC_ALERT) value_siren = mu;
  else if (t == TOPIC_RELAY_STATE) value_relay_state = mu;
  else if (t == TOPIC_MODE_STATE) value_mode_state = mu;
  else if (t == topic_ws_state) value_ws_state = msg;

  update_all_ui_elements();
}

// =========================
// WiFi / MQTT
// =========================
void connectMQTT() {
  if (mqttClient.connected()) return;

  String clientId = "ESP32-Dash-";
  clientId += String((uint32_t)ESP.getEfuseMac(), HEX);

  bool ok = mqttClient.connect(
    clientId.c_str(),
    mqtt_user,
    mqtt_password,
    TOPIC_DISPLAY_STATUS,
    1,
    true,
    "OFFLINE"
  );

  if (ok) {
    mqttClient.publish(TOPIC_DISPLAY_STATUS, "ONLINE", true);

    mqttClient.subscribe("meteostanica/#");
    mqttClient.subscribe("modul_cosensor/#");
    mqttClient.subscribe(topic_ws_state);

    Serial.println("MQTT connected");
    logToSupabase("mqtt", "connected", "MQTT connected");
  }
}

// =========================
// Setup
// =========================
void setup() {
  Serial.begin(115200);

  lv_init();

  touchscreenSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
  touchscreen.begin(touchscreenSPI);
  touchscreen.setRotation(2);

  lv_display_t * disp = lv_tft_espi_create(SCREEN_WIDTH, SCREEN_HEIGHT, draw_buf, sizeof(draw_buf));
  lv_display_set_rotation(disp, LV_DISPLAY_ROTATION_270);

  lv_indev_t * indev = lv_indev_create();
  lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(indev, touchscreen_read);

  create_gui();

  WiFi.begin(ssid, wifi_password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }

  logToSupabase("wifi", "connected", "WiFi connected");

  configTime(0, 0, "pool.ntp.org");
  setenv("TZ", "CET-1CEST,M3.5.0/2,M10.5.0/3", 1);
  tzset();

  secureClient.setInsecure();
  mqttClient.setServer(mqtt_server, mqtt_port);
  mqttClient.setCallback(mqttCallback);

  connectMQTT();

  updateTime();
  updateWeather();
  update_all_ui_elements();

  logToSupabase("display", "startup", "Display started");
}

// =========================
// Loop
// =========================
void loop() {
  static unsigned long lastT = 0;
  static unsigned long lastW = 0;

  bool wifiNow = (WiFi.status() == WL_CONNECTED);
  bool mqttNow = mqttClient.connected();

  if (wifiNow != lastWifiConnectedState) {
    lastWifiConnectedState = wifiNow;
    logToSupabase("wifi", wifiNow ? "status_connected" : "status_disconnected",
                  wifiNow ? "WiFi connected" : "WiFi disconnected");
  }

  if (mqttNow != lastMqttConnectedState) {
    lastMqttConnectedState = mqttNow;
    logToSupabase("mqtt", mqttNow ? "status_connected" : "status_disconnected",
                  mqttNow ? "MQTT connected" : "MQTT disconnected");
  }

  if (millis() - lastT > 1000) {
    lastT = millis();
    updateTime();
    update_all_ui_elements();
  }

  if (millis() - lastW > 600000) {
    lastW = millis();
    updateWeather();
    update_all_ui_elements();
  }

  if (WiFi.status() != WL_CONNECTED) {
    WiFi.begin(ssid, wifi_password);
    delay(500);
  }

  if (!mqttClient.connected()) {
    connectMQTT();
  }

  mqttClient.loop();

  lv_timer_handler();
  lv_tick_inc(5);
  delay(5);
}
