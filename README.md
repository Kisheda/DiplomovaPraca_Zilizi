This project is a full-stack IoT smart home system developed as part of a diploma thesis.
It integrates ESP32-based hardware modules with a modern web application dashboard.  

🚀 Features
🌡️ Environmental monitoring (temperature, humidity, light, pressure, soil)
🌫️ Air quality detection (CO sensor)
🚿 Automatic irrigation system
🔒 Security system with RFID authentication
🪟 Automated window shading (stepper motor control)
📟 Local touchscreen display control
🌐 Web dashboard for monitoring and control
📊 Logging system with filtering
🔐 Secure authentication via MQTT (HiveMQ)
🔒 TLS encrypted communication

The system follows a modular IoT architecture:

Hardware layer
ESP32 modules (C/C++)
Communication layer
MQTT protocol (HiveMQ Cloud)
Backend layer
Supabase (PostgreSQL + REST API)
Frontend layer
React + TypeScript + Tailwind CSS + Vite

🧠 Architecture
The system follows a modular IoT architecture:

Hardware layer
ESP32 modules (C/C++)
Communication layer
MQTT protocol (HiveMQ Cloud)
Backend layer
Supabase (PostgreSQL + REST API)
Frontend layer
React + TypeScript + Tailwind CSS + Vite

📡 Modules
CO Sensor Module -Measures air quality, Triggers alarm when threshold exceeded
Environmental & Irrigation Module - Monitors environment, Automatically controls water pump
Security Module - RFID-based authentication, Alarm system for unauthorized access
Window Shade Module - Stepper motor controlled shading, Manual and scheduled automation
Display Module - Local touchscreen UI

📦 Installation
1. Clone repository
git clone https://github.com/Kisheda/DiplomovaPraca_Zilizi.git
cd DiplomovaPraca_Zilizi
2. Install frontend dependencies
npm install
3. Run development server
npm run dev

🌐 Web Application
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
