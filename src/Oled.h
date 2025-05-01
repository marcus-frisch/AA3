#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "Pindef.h"

#define SER_DBG

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

String screenLines[6] = {"",
                         "",
                         "",
                         "",
                         "",
                         ""};

void initOled()
{
    Wire.begin(OLED_SDA, OLED_SCL);

    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C))
    { // Address 0x3D for 128x64
#if defined(SER_DBG)
        Serial.println(F("SSD1306 allocation failed"));
#endif
        for (;;)
            ;
    }
    // display.dim(true); // Lowers brightness
    delay(500);
    display.clearDisplay();
    display.setTextWrap(false);
    display.setTextColor(WHITE);
    display.setTextSize(1);
    display.setCursor(0, 28);
    display.println("AA3");
    display.display();
}

void displayFocusOled()
{
    display.clearDisplay();
    int line = 0;
    for (int i = 0; i < 40; i++) // displays 5 small lines and 1 big line
    {
        display.setCursor(0, i);
        i = i + 8;
        // display.setTextSize(1);
        // display.println(screenLines[line]);
        line++;
    }
    display.setTextSize(1);
    display.setCursor(0, 48);
    display.println(screenLines[line]);
    display.display();
}

void msg(String str_serial, String str_oled = "")
{
    for (int i = 0; i < 5; i++)
    {
        screenLines[i] = screenLines[i + 1];
    }
    if (str_oled != "")
    {
        screenLines[5] = str_oled;
    }
    else
    {
        screenLines[5] = str_serial;
    }

#if defined(SER_DBG)
    Serial.println(str_serial);
#endif

    displayFocusOled();
}