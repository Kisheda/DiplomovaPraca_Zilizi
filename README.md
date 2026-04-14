This project is a full-stack IoT smart home system developed as part of a diploma thesis.
It integrates ESP32-based hardware modules with a modern web application dashboard.  

<h1> 🚀 Features: </h1></br>
🌡️ Environmental monitoring (temperature, humidity, light, pressure, soil) </br>
🌫️ Air quality detection (CO sensor)  </br>
🚿 Automatic irrigation system  </br>
🔒 Security system with RFID authentication </br>
🪟 Automated window shading (stepper motor control) </br>
📟 Local touchscreen display control </br>
🌐 Web dashboard for monitoring and control </br>
📊 Logging system with filtering </br>
🔐 Secure authentication via MQTT (HiveMQ) </br>
🔒 TLS encrypted communication </br>

<h1> The system follows a modular IoT architecture: </h1>
Hardware layer
ESP32 modules (C/C++)
Communication layer
MQTT protocol (HiveMQ Cloud)
Backend layer
Supabase (PostgreSQL + REST API)
Frontend layer
React + TypeScript + Tailwind CSS + Vite

<h1>🧠 Architecture </h1>
The system follows a modular IoT architecture:
Hardware layer
ESP32 modules (C/C++)
Communication layer
MQTT protocol (HiveMQ Cloud)
Backend layer
Supabase (PostgreSQL + REST API)
Frontend layer
React + TypeScript + Tailwind CSS + Vite

<h1> 📡 Modules </h1>
CO Sensor Module -Measures air quality, Triggers alarm when threshold exceeded
Environmental & Irrigation Module - Monitors environment, Automatically controls water pump
Security Module - RFID-based authentication, Alarm system for unauthorized access
Window Shade Module - Stepper motor controlled shading, Manual and scheduled automation
Display Module - Local touchscreen UI

## 📦 Installation

### 1. Clone repository
```bash
git clone https://github.com/Kisheda/DiplomovaPraca_Zilizi.git
cd DiplomovaPraca_Zilizi
npm install
npm run dev
```
<h1> 🌐 Web Application </h1>  </br>
The web dashboard provides:

Real-time data visualization
Device control (relays, pump, shading, ventilation)
System status monitoring (ONLINE / OFFLINE)
Configuration (automation schedules, modes)
Log viewing with filtering options
Secure login (MQTT authentication)

🔐 Security
MQTT communication secured via TLS
Authentication against HiveMQ broker
Access control for system operations
