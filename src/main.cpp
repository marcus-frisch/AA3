#include <Arduino.h>
#include <esp_camera.h>
#include <WiFi.h>
#include <WiFiClient.h>

#include <Adafruit_I2CDevice.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "Oled.h"

#include <FS.h>               // File system support
#include <SD_MMC.h>           // SD card access over SDMMC
#include <soc/soc.h>          // System-on-chip (SoC) definitions
#include <soc/rtc_cntl_reg.h> // Access to RTC control registers
#include <driver/rtc_io.h>    // Real-time clock input/output control

#include <ESP32_FTPClient.h>

#include <EEPROM.h> // EEPROM read/write support
// Define EEPROM size (1 byte in this case) for storing persistent data
#define EEPROM_SIZE 1

#include "Pindef.h"
#include "Secrets.h"

#define SER_DBG

camera_config_t config;

int pictureNumber = 0; // Counter to keep track of picture numbers
unsigned long lastPhotoMillis = 0;
bool timedOut = false;
#define SYS_IDLE_TIMEOUT 1200000 // system idle timeout in millis 5mins

char ftp_server[] = FTP_ADDR;
uint16_t ftp_port = FTP_PORT;
char ftp_user[] = FTP_USER;
char ftp_pass[] = FTP_PASS;

ESP32_FTPClient ftp(ftp_server, ftp_port, ftp_user, ftp_pass, 5000, 0); // Disable Debug to increase Tx Speed

void camera_init();
void take_photo();
void readAndSendBigBinFile(fs::FS &fs, const char *path, ESP32_FTPClient ftpClient);
bool shutterPressed();

#define shutterTouchThreshold 9 // difference threshold to detect shutter button pressed

#define SD_MMC_CMD 15 // Please do not modify it.
#define SD_MMC_CLK 14 // Please do not modify it.
#define SD_MMC_D0 2   // Please do not modify it.

void setup()
{
  // Disable brownout detector to prevent resets due to low voltage
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

  delay(5000);
  esp_log_level_set("camera", ESP_LOG_VERBOSE);

  // pinMode(13, OUTPUT);
  // digitalWrite(13, HIGH);

#ifdef SER_DBG
  Serial.begin(115200);
  Serial.setDebugOutput(true);
#else
  Serial.setDebugOutput(false);
#endif

  delay(5000);
  initOled();

  pinMode(SHUTTER_BUTTON, INPUT);

  delay(5000);

  WiFi.begin(WIFI_SSID, WIFI_PASS);

  msg("Connecting Wifi...", "Cting Wifi");
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
  }
  // msg("WIFI Connected", "WIFI Cnnted");
  // Serial.print("IP address: ");
  // Serial.println(WiFi.localIP());

  delay(500);

  ftp.OpenConnection();

  delay(500);

  if (ftp.isConnected())
  {
    msg("FTP CONNECTED", "FTP True");
  }
  else
  {
    msg("FTP NOT CONNECTED", "FTP False");
  }

  delay(5000);
  camera_init();

  // camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK)
  {
    // Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  sensor_t *s = esp_camera_sensor_get();
  if (s)
  {
    s->set_framesize(s, FRAMESIZE_UXGA); // Adjust frame size as needed
    s->set_brightness(s, 0);             // Default brightness
    s->set_contrast(s, 0);               // Default contrast
    s->set_saturation(s, 0);             // Default saturation
    s->set_gainceiling(s, GAINCEILING_2X);
    s->set_sharpness(s, 0);
    s->set_whitebal(s, 1); // Enable white balance
    s->set_awb_gain(s, 1); // Enable AWB gain
    s->set_hmirror(s, 0);  // No mirroring
    s->set_vflip(s, 0);    // No flipping
  }

  SD_MMC.setPins(SD_MMC_CLK, SD_MMC_CMD, SD_MMC_D0);

  delay(1000);
  if (!SD_MMC.begin("/sdcard", true, true, SDMMC_FREQ_DEFAULT, 5))
  {
    msg("SD Card Mount Failed", "SD Mount Failed");
    return;
  }
  else
  {
    msg("SD mount success", "SD Mount Success");
  }
  delay(2000);
}

bool flag = false;

void loop()
{

  if (!flag || shutterPressed())
  {
    flag = true;
    if (!timedOut)
    {
      msg("Taking photo");
      take_photo();
    }
    else
    {
      msg("awaking");
      timedOut = false;
      camera_init();
      delay(4000);
      take_photo();
    }
  }

  if (millis() >= lastPhotoMillis + SYS_IDLE_TIMEOUT && !timedOut)
  {
    timedOut = true;
    msg("System idle, shutting down now", "Sys idle");
    esp_err_t err = esp_camera_deinit();
    if (err != ESP_OK)
    {
      // Serial.printf("Camera deinit failed with error 0x%x", err);
      return;
    }

    if (ftp.isConnected())
    {
      ftp.CloseConnection();
      delay(500);
    }

    WiFi.disconnect();
  }
}

void camera_init()
{
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.frame_size = FRAMESIZE_QVGA;
  config.pixel_format = PIXFORMAT_JPEG; // for streaming
                                        // config.pixel_format = PIXFORMAT_RGB565; // for face detection/recognition
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = 10;
  config.fb_count = 2;

  // Configure camera settings based on PSRAM availability
  if (psramFound())
  {
    config.frame_size = FRAMESIZE_UXGA; // Adjust as needed
    config.jpeg_quality = 15;           // Reduce quality to save memory
    config.fb_count = 2;                // Single framebuffer
  }
  else
  {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 20;
    config.fb_count = 1;
  }
  config.grab_mode = CAMERA_GRAB_LATEST;
}

void take_photo()
{
  camera_fb_t *fb = NULL; // Frame buffer pointer

  // Capture an image with the camera
  msg("Getting Frame Buffer", "Capturing img");
  fb = esp_camera_fb_get();
  if (!fb)
  {
    msg("Getting Frame Buffer failed", "Capture Failed");
    return;
  }

  // delay(500);

  // Check if SD card is present
  uint8_t cardType = SD_MMC.cardType();
  if (cardType == CARD_NONE)
  {
    msg("No SD Card attached", "NO SD Found");
    return;
  }

  // Initialize EEPROM and retrieve the picture number
  EEPROM.begin(EEPROM_SIZE);
  pictureNumber = EEPROM.read(0) + 1;

  //  generate a random number for file name in an effort to stop people systematically viewing old images on a browser
  long secureNum = random(1000);

  // Construct the file path for the image
  String path = "/aa2_" + String(secureNum) + "_" + String(pictureNumber) + ".jpg";

  // Save the image to the SD card
  msg("Opening Filesystem", "Opening FS");
  fs::FS &fs = SD_MMC;
  msg("Opening FS File", "OPN FS FILE");
  File file = fs.open(path.c_str(), FILE_WRITE);
  if (!file)
  {

    // Serial.printf("Failed to open file: %s\n", path);
  }
  else
  {
    file.write(fb->buf, fb->len); // Write image data to the file
    // Serial.printf("Saved file to path: %s\n", path.c_str());

    // Update and commit picture number in EEPROM
    EEPROM.write(0, pictureNumber);
    EEPROM.commit();
  }
  file.close(); // Close the file

  // delay(1000);
  msg("File closed");
  esp_camera_fb_return(fb); // Release frame buffer memory
  msg("Frame Buffer cleared", "Capture cleared");

  char the_path[32];
  snprintf(the_path, sizeof(the_path), "/aa2_%d_%d.jpg", secureNum, pictureNumber);

  if (SD_MMC.exists(the_path))
  {
    msg("File exists. Sending to FTP...", "SND FTP File");
    readAndSendBigBinFile(SD_MMC, the_path, ftp);
  }
  else
  {
    msg("File does not exist on SD card.", "File nt Found");
  }

  delay(250);
  msg("Finished Photo process", "Photo done");
  delay(250);
  // delay(1000);
  msg("Ready");
}

void readAndSendBigBinFile(fs::FS &fs, const char *path, ESP32_FTPClient ftpClient)
{
  // Serial.printf("Attempting to read file: %s\n", path);

  ftpClient.InitFile("Type I");
  ftpClient.NewFile(path);

  File file = fs.open(path);
  if (!file)
  {
    // Serial.printf("Failed to open file for reading: %s\n", path);
    return;
  }

  msg("Reading file contents...", "Uploding...");
  while (file.available())
  {
    unsigned char buf[1024];
    int readVal = file.read(buf, sizeof(buf));
    ftpClient.WriteData(buf, readVal); // Use the actual number of bytes read
  }
  ftpClient.CloseFile();
  file.close();
}

bool shutterPressed()
{
  static int initialReading = touchRead(SHUTTER_BUTTON);
  // Serial.println("SHUTTER: " + String(touchRead(SHUTTER_BUTTON)));

  if (touchRead(SHUTTER_BUTTON) <= initialReading - shutterTouchThreshold)
  {
    return true;
  }
  return false;
}
