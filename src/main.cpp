#include <Arduino.h>
#include <esp_camera.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <Adafruit_I2CDevice.h>
#include <SPI.h>

#include <FS.h>               // File system support
#include <SD_MMC.h>           // SD card access over SDMMC
#include <soc/soc.h>          // System-on-chip (SoC) definitions
#include <soc/rtc_cntl_reg.h> // Access to RTC control registers
#include <driver/rtc_io.h>    // Real-time clock input/output control

#include <ESP32_FTPClient.h>

#include <EEPROM.h> // EEPROM read/write support
// Define EEPROM size (1 byte in this case) for storing persistent data
#define EEPROM_SIZE 1

// pin definitions
#define CAMERA_MODEL_WROVER_KIT // Has PSRAM
#include "camera_pins.h"

#include "Pindef.h"
#include "Secrets.h"

camera_config_t config;

int pictureNumber = 0; // Counter to keep track of picture numbers
unsigned long lastPhotoMillis = 0;
bool timedOut = false;
#define SYS_IDLE_TIMEOUT 300000 // system idle timeout in millis 5mins

char ftp_server[] = FTP_ADDR;
uint16_t ftp_port = FTP_PORT;
char ftp_user[] = FTP_USER;
char ftp_pass[] = FTP_PASS;

ESP32_FTPClient ftp(ftp_server, ftp_port, ftp_user, ftp_pass, 5000, 0); // Disable Debug to increase Tx Speed

// void startCameraServer();
void camera_init();
void take_photo();
void readAndSendBigBinFile(fs::FS &fs, const char *path, ESP32_FTPClient ftpClient);

#define SD_MMC_CMD 15 // Please do not modify it.
#define SD_MMC_CLK 14 // Please do not modify it.
#define SD_MMC_D0 2   // Please do not modify it.

void setup()
{
  // Disable brownout detector to prevent resets due to low voltage
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();

  WiFi.begin(WIFI_ONE, WIFI_ONEP);

  Serial.println("Connecting Wifi...");
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println("WIFI Connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  delay(500);

  ftp.OpenConnection();

  delay(500);

  if (ftp.isConnected())
  {
    Serial.println("FTP CONNECTED");
  }
  else
  {
    Serial.println("FTP NOT CONNECTED");
  }

  camera_init();

  // camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK)
  {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  sensor_t *s = esp_camera_sensor_get();
  s->set_vflip(s, 0);       // 1-Upside down, 0-No operation
  s->set_hmirror(s, 0);     // 1-Reverse left and right, 0-No operation
  s->set_brightness(s, 1);  // up the blightness just a bit
  s->set_saturation(s, -1); // lower the saturation

  SD_MMC.setPins(SD_MMC_CLK, SD_MMC_CMD, SD_MMC_D0);
  Serial.println("Pwr on done!");

  delay(1000);
  if (!SD_MMC.begin("/sdcard", true, true, SDMMC_FREQ_DEFAULT, 5))
  {
    Serial.println("SD Card Mount Failed");
    return;
  }
  else
  {
    Serial.println("SD mount success");
  }
  delay(2000);
}

bool flag = false;

void loop()
{

  if (!flag)
  {
    flag = true;
    take_photo();
  }

  if (millis() >= lastPhotoMillis + SYS_IDLE_TIMEOUT && !timedOut)
  {
    timedOut = true;
    Serial.println("System idle, shutting down now");
    esp_err_t err = esp_camera_deinit();
    if (err != ESP_OK)
    {
      Serial.printf("Camera deinit failed with error 0x%x", err);
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
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = 10;
  config.fb_count = 2;

  // Configure camera settings based on PSRAM availability
  if (psramFound())
  {
    config.frame_size = FRAMESIZE_UXGA; // High resolution (UXGA)
    config.jpeg_quality = 10;           // High quality (lower number = better)
    config.fb_count = 2;                // Double buffer for better performance
  }
  else
  {
    config.frame_size = FRAMESIZE_SVGA; // Lower resolution
    config.jpeg_quality = 12;           // Lower quality
    config.fb_count = 1;                // Single buffer
  }
}

void take_photo()
{
  camera_fb_t *fb = NULL; // Frame buffer pointer

  // Capture an image with the camera
  Serial.println("Getting FB");
  fb = esp_camera_fb_get();
  if (!fb)
  {
    Serial.println("Camera capture failed");
    return;
  }

  delay(500);

  // Check if SD card is present
  uint8_t cardType = SD_MMC.cardType();
  if (cardType == CARD_NONE)
  {
    Serial.println("No SD Card attached");
    return;
  }

  // Initialize EEPROM and retrieve the picture number
  Serial.println("Increasing EEPROM");
  EEPROM.begin(EEPROM_SIZE);
  pictureNumber = EEPROM.read(0) + 1;

  //  generate a random number for file name in an effort to stop people systematically viewing old images on a browser
  long secureNum = random(1000);

  // Construct the file path for the image
  String path = "/aa2_" + String(secureNum) + "_" + String(pictureNumber) + ".jpg";

  // Save the image to the SD card
  Serial.println("FS stuff");
  Serial.println("Opening FS");
  fs::FS &fs = SD_MMC;
  Serial.println("Opening FS File");
  File file = fs.open(path.c_str(), FILE_WRITE);
  if (!file)
  {

    Serial.printf("Failed to open file: %s\n", path);
  }
  else
  {
    file.write(fb->buf, fb->len); // Write image data to the file
    Serial.printf("Saved file to path: %s\n", path.c_str());

    // Update and commit picture number in EEPROM
    EEPROM.write(0, pictureNumber);
    EEPROM.commit();
  }
  file.close(); // Close the file

  delay(1000);
  Serial.println("File closed");
  esp_camera_fb_return(fb); // Release frame buffer memory
  Serial.println("FB cleared");

  char the_path[32];
  snprintf(the_path, sizeof(the_path), "/aa2_%d_%d.jpg", secureNum, pictureNumber);

  if (SD_MMC.exists(the_path))
  {
    Serial.println("File exists. Sending to FTP...");
    readAndSendBigBinFile(SD_MMC, the_path, ftp);
  }
  else
  {
    Serial.println("File does not exist on SD card.");
  }

  delay(250);
  Serial.println("Finished Photo process");
  delay(1000);
  Serial.println("Ready");
}

void readAndSendBigBinFile(fs::FS &fs, const char *path, ESP32_FTPClient ftpClient)
{
  Serial.printf("Attempting to read file: %s\n", path);

  ftpClient.InitFile("Type I");
  ftpClient.NewFile(path);

  File file = fs.open(path);
  if (!file)
  {
    Serial.printf("Failed to open file for reading: %s\n", path);
    return;
  }

  Serial.println("Reading file contents...");
  while (file.available())
  {
    unsigned char buf[1024];
    int readVal = file.read(buf, sizeof(buf));
    ftpClient.WriteData(buf, readVal); // Use the actual number of bytes read
  }
  ftpClient.CloseFile();
  file.close();
}
