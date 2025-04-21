# 📷 ESP32 Discord Compact Camera [Aperature Archive]

🔗 [Project Webpage](https://marcusfrisch.netlify.app/projects/espcam/)

This project uses an [ESP32-CAM module](https://randomnerdtutorials.com/esp32-cam-video-streaming-web-server-camera-home-assistant/) powered by a single **18650 cell** to snap photos and instantly upload them to a **private Discord server** via a webhook.

The goal was to keep it **pocketable**, fun, and fast — from shutter press to image-in-chat in just a few seconds.

> ⚠️ This project was rapidly prototyped using dev modules (ESP32 dev board, OLED breakout, etc.). A custom PCB is in the works to bring everything together into a compact final form.

---

## 🔧 How it works

1. Press the shutter button (touch-based or physical).
2. The ESP32 captures a JPEG image using the OV2640 camera.
3. Image is saved to the SD card.
4. File is uploaded via **FTP** to a local server.
5. The server:
   - stores the image,
   - generates a public URL,
   - sends it to a Discord webhook.
6. Discord auto-embeds the image directly in chat 🎉

---

## 📦 Tech Stack

| Component         | Purpose                        |
| ----------------- | ------------------------------ |
| `ESP32-CAM`       | Microcontroller + Camera       |
| `Adafruit SSD1306`| OLED display (status messages) |
| `SD_MMC`          | Image storage                  |
| `ESP32_FTPClient` | FTP image transfer             |
| `ExpressJS`       | Public web server for images   |
| `Python (pyftpdlib)` | FTP server with webhook callback |
| `Discord Webhook` | Instant image delivery         |

---

## 📋 Features

- OLED status display: FTP success, errors, image saved, etc.
- Randomized filenames to avoid URL guessing
- System idles after a few minutes to save power
- Image-to-Discord in **~3 seconds**
- Compact wiring layout — designed to be pocket-sized
- Easily swappable camera modules (supports OV2640/OV5640)
- Firmware in C++, server in Python + JS

---

## 🚧 To-Do

- [x] FTP upload working
- [x] Discord webhook integration
- [x] OLED feedback
- [x] Sleep mode when idle
- [ ] Finalize custom PCB

---

## 🧪 Setup

You'll need:

- An ESP32-CAM module (Wrover-E)
- An 18650 Li-ion cell (with a protection circuit!)
- Adafruit 0.96" OLED display (I2C)
- microSD card (formatted FAT32)
- Local WiFi connection
- A computer/server running:
  - `pyftpdlib` (for FTP server)
  - `ExpressJS` (for public image links)

📁 Put your WiFi and FTP credentials into a `Secrets.h` file:

```cpp
#define WIFI_SSID "your_wifi"
#define WIFI_PASS "your_password"

#define FTP_ADDR "192.168.x.x"
#define FTP_PORT 2121
#define FTP_USER "cam_client"
#define FTP_PASS "camera"
