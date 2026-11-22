# ESP-32Cam-Teligram-bot-based-home-Security-System
Arduino code for an ESP32-CAM based smart home security system with real-time Telegram bot alerts and notifications.
# ESP32-Based Smart Home Security System with Telegram Bot

An IoT-enabled smart home security system built using the **ESP32-CAM**, a **PIR motion sensor**, and a **Telegram Bot**. Whenever unauthorized motion is detected, the system captures a photo and sends an instant alert directly to your Telegram account. It also features a built-in Wi-Fi Captive Portal for easy network configuration without hardcoding credentials.

## Key Features
* **Motion Detection:** Uses a PIR sensor to detect human movement accurately.
* **Instant Photo Alerts:** Captures real-time images using the ESP32-CAM and sends them via Telegram.
* **Wi-Fi Captive Portal:** Configures Wi-Fi credentials and Telegram Bot details dynamically in Access Point (AP) mode.
* **Status Indicator:** Built-in LED feedback for system status and Wi-Fi connection states.
* **Non-Blocking Architecture:** Smooth execution without missing motion triggers.

## Hardware Components
* **Microcontroller:** ESP32-CAM (AI-Thinker)
* **Motion Sensor:** HC-SR501 PIR Motion Sensor
* **Power Supply:** 5V 2A Regulated DC Adapter / Buck Converter
* **Programmer:** FTDI USB-to-TTL Converter (for uploading code)
* **Miscellaneous:** Jumper wires, Status LED, Resistor (220Ω)

## Pin Connections

Component Pin | ESP32-CAM Pin | Notes |
:--- | :--- | :--- |
**PIR VCC** | 5V | Power supply for PIR sensor |
**PIR GND** | GND | Common Ground |
**PIR OUT** | GPIO 13 | Digital signal input |
**Status LED (+)** | GPIO 12 (via 220Ω resistor) | System status indicator |
**Status LED (-)** | GND | Common Ground |

Note: Disconnect GPIO 0 from GND after uploading the code and press the Reset button.


## Required Arduino Libraries
Make sure to install the following libraries in your Arduino IDE before compiling:
1. **UniversalTelegramBot** by Brian Lough
2. **ArduinoJson** (v6.x or higher) by Benoit Blanchon
3. **WiFiManager** by tzapu (for Captive Portal)
