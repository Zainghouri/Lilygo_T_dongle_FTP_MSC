#include <SPI.h>
#include <Arduino.h>
#include <USB.h>
#include <USBMSC.h>
#include "SD_MMC.h"
#include "FS.h"
#include <WiFi.h>
#include "ESP-FTP-Server-Lib.h"
#include "FTPFilesystem.h"
#include "Arduino_GFX_Library.h"
#include "Arduino_APA102.h"

const char *ssid = "ESP32"; // AP SSID 
const char *pass = "12345"; // AP PASSWORD

#define FTP_USER     "lilygo"
#define FTP_PASSWORD "lilygo"


FTPServer ftp;


WiFiServer server(80);

//TFT PINS
int clk = 12;
int cmd = 16;
int d0 = 14;
int d1 = 17;
int d2 = 21;
int d3 = 18;

//LCD PINS
#define TFT_CS_PIN 4
#define TFT_SDA_PIN 3
#define TFT_SCL_PIN 5
#define TFT_DC_PIN 2
#define TFT_RES_PIN 1
#define TFT_LEDA_PIN 38

/* More dev device declaration: https://github.com/moononournation/Arduino_GFX/wiki/Dev-Device-Declaration */
#if defined(DISPLAY_DEV_KIT)
Arduino_GFX *gfx = create_default_Arduino_GFX();
#else /* !defined(DISPLAY_DEV_KIT) */

/* More data bus class: https://github.com/moononournation/Arduino_GFX/wiki/Data-Bus-Class */
Arduino_DataBus *bus = new Arduino_ESP32SPI(TFT_DC_PIN /* DC */, TFT_CS_PIN /* CS */, TFT_SCL_PIN /* SCK */, TFT_SDA_PIN /* MOSI */);

/* More display class: https://github.com/moononournation/Arduino_GFX/wiki/Display-Class */
Arduino_GFX *gfx = new Arduino_ST7735(
  bus, TFT_RES_PIN /* RST */, 1 /* rotation */, true /* IPS */,
  80 /* width */, 160 /* height */,
  26 /* col offset 1 */, 1 /* row offset 1 */,
  26 /* col offset 2 */, 1 /* row offset 2 */);

#endif
//-----------------------------------------------------------------------------------------------------------------
//LED Diode
int dataPin = 40;
int clockPin = 39;
int totalLEDs = 1;
// Construct object, Arduino_APA102(nLEDS, data line, clock line)
Arduino_APA102 leds(totalLEDs, dataPin, clockPin);

//uint32_t color = random(0, 0xFFFFFF);                    // 0x[R][G][B]
uint32_t color_red = leds.Color( 255 , 0 , 0);  // Save red color
uint32_t color_green = leds.Color( 0 , 255 , 0);  // Save green color
uint32_t color_blue = leds.Color( 0 , 0 , 255);  // Save blue color
uint32_t color_black = leds.Color( 0 , 0 , 0);  // Save black color

int r = 0;
int g = 100;
int b = 250;


USBMSC msc;
bool onebit = false;  // set to false for 4-bit. 1-bit will ignore the d1-d3 pins (but d3 must be pulled high)

bool lastFtpState = false; // Variable to store the last FTP connection state

static int32_t onWrite(uint32_t lba, uint32_t offset, uint8_t *buffer, uint32_t bufsize) {
  uint32_t secSize = SD_MMC.sectorSize();
  if (!secSize) {
    return false;  // disk error
  }
  for (int x = 0; x < bufsize / secSize; x++) {
    uint8_t blkbuffer[secSize];
    memcpy(blkbuffer, (uint8_t *)buffer + secSize * x, secSize);
    if (!SD_MMC.writeRAW(blkbuffer, lba + x)) {
      return false;
    }
  }
  return bufsize;
}

static int32_t onRead(uint32_t lba, uint32_t offset, void *buffer, uint32_t bufsize) {
  uint32_t secSize = SD_MMC.sectorSize();
  if (!secSize) {
    return false;  // disk error
  }
  for (int x = 0; x < bufsize / secSize; x++) {
    if (!SD_MMC.readRAW((uint8_t *)buffer + (x * secSize), lba + x)) {
      return false;  // outside of volume boundary
    }
  }
  return bufsize;
}

static bool onStartStop(uint8_t power_condition, bool start, bool load_eject) {
  return true;
}



void setup() {

  Serial.begin(115200);
  while (!Serial) {};

  SD_MMC.setPins(clk, cmd, d0, d1, d2, d3);
  Serial.println("Mounting SD card");
  delay(3000);
  if (!SD_MMC.begin("/sdcard", onebit)) {
    Serial.println("Mount Failed");
    return;
  }

  Serial.println("Initializing MSC");
  msc.onRead(onRead);
  msc.onWrite(onWrite);
  msc.onStartStop(onStartStop);
  msc.mediaPresent(true);
  msc.isWritable(true);
  msc.begin(SD_MMC.numSectors(), SD_MMC.sectorSize());
  
  Serial.println();
  Serial.println("Configuring access point...");
  leds.begin();
  leds.setPixelColor(0 , color_red);
  leds.setCurrent(0,5);
  leds.show();
  gfx->begin();
  gfx->fillScreen(BLACK);
  gfx->setCursor(0, 0);
  gfx->setTextColor(GREEN);
  gfx->setTextSize(1 /* x scale */, 1 /* y scale */, 3 /* pixel_margin */);
  gfx->println("Starting AP:");
  WiFi.softAP(ssid, pass);
  IPAddress IP = WiFi.softAPIP();
  gfx->println("\nIP address:\n");
  gfx->println(IP);
  gfx->print("\nReady");
  
  server.begin();
  USB.begin();
  delay(1000);
  leds.setPixelColor(0 , color_green);
  leds.show();
 
  ftp.addUser(FTP_USER, FTP_PASSWORD);
  ftp.addFilesystem("SD_MMC", &SD_MMC);

  ftp.begin();
  
  
}


void loop() {
  bool currentFtpState = ftp.countConnections() > 0;

  if (currentFtpState != lastFtpState) { // Only update the screen if the state has changed
    lastFtpState = currentFtpState;      // Update last state

    gfx->fillScreen(BLACK);
    gfx->setTextSize(1 /* x scale */, 1 /* y scale */, 4 /* pixel_margin */);
    gfx->setCursor(0, 0);

    if (currentFtpState) {
    msc.end();
    gfx->println("FTP Connected");
    gfx->println("\nMSC Disconnected");
    leds.setPixelColor(0 , color_blue);
    } else {
    delay(1000);  
    msc.onRead(onRead);
    msc.onWrite(onWrite);
    msc.onStartStop(onStartStop);
    msc.mediaPresent(true);
    msc.isWritable(true);  // true if writable, false if read-only
    msc.begin(SD_MMC.numSectors(), SD_MMC.sectorSize());
    
    gfx->println("FTP DisConnected");
    gfx->println("\nMSC Connected");
    leds.setPixelColor(0 , color_green);
    }
    leds.show();  // Update LED colors
  }

  ftp.handle();  // Handle FTP server operations
}