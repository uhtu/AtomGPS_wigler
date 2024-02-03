#include <M5Atom.h>
#include <SD.h>
#include <SPI.h>
#include <TinyGPS++.h>
#include <WiFi.h>

// LED
bool ledState = false;

// state machine goes OFF -> BLINKY -> COUNTY -> OFF
#define LED_OFF    0x00
#define LED_BLINKY 0x01
#define LED_COUNTY 0x02
int buttonLedState = LED_BLINKY;

#define RED 0xff0000
#define GREEN 0x00ff00
#define BLUE 0x0000ff
#define YELLOW 0xffff00
#define PURPLE 0x800080
#define CYAN 0x00ffff
#define WHITE 0xffffff
#define OFF 0x000000

// GPS and Filesys
TinyGPSPlus gps;
char fileName[50];
const int maxMACs = 150;  // TESTING: buffer size
char macAddressArray[maxMACs][20];
int macArrayIndex = 0;
int loggedNets=0;
// Network Scanning
const int popularChannels[] = { 1, 6, 11 };
const int standardChannels[] = { 2, 3, 4, 5, 7, 8, 9, 10 };
const int rareChannels[] = { 12, 13, 14 };  // Depending on region
int timePerChannel[14] = { 300, 200, 200, 200, 200, 300, 200, 200, 200, 200, 300, 200, 200, 200 };
// rewritten based on original work from Addison Sears-Collins. thanks! https://automaticaddison.com/how-to-display-a-string-as-morse-code-on-an-led-using-arduino/
const int dot_duration = 200;

// The numbers 0-9 in Morse code
// encode the morse as 0 for ., 1 for -, using the low 5 bits
const uint8_t numbytes[] = {
  0x1f, // -----
  0x0f, // .----
  0x07, // ..---
  0x03, // ...--
  0x01, // ....-
  0x00, // .....
  0x10, // -....
  0x18, // --...
  0x1c, // ---..
  0x1e};// ----.

void setup() {
  Serial.begin(115200);
  Serial.println("Starting...");
  M5.begin(true, false, true);
  SPI.begin(23, 33, 19, -1);              // investigate the -1 assignment and esp32 boards
  while (!SD.begin(-1, SPI, 40000000)) {  // params throw a bunch of gpio warnings, TODO assign ss
    Serial.println("SD Card initialization failed! Retrying...");
    blinkLED(RED, 500);  // will hang here until SD is readable
    delay(1000);
  }
  Serial.println("SD Card initialized.");

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  Serial.println("WiFi initialized.");

  Serial1.begin(9600, SERIAL_8N1, 22, -1);
  Serial.println("GPS Serial initialized.");
  waitForGPSFix();
  initializeFile();
}

void loop() {
  M5.update();
  if (M5.Btn.wasPressed()) {
    switch (buttonLedState){
      case LED_OFF:
        buttonLedState = LED_BLINKY;
        break;
      case LED_BLINKY:
        buttonLedState = LED_COUNTY;
        break;
      case LED_COUNTY:
        buttonLedState = LED_OFF;
        break;
    }

    delay(50);
  }

  while (Serial1.available() > 0) {
    gps.encode(Serial1.read());
  }

  if (gps.location.isValid()) {
    // Use crap
    if (buttonLedState == LED_BLINKY) {
      M5.dis.drawpix(0, GREEN);  // Flash green without a static blink
      delay(80);
      M5.dis.clear();
    } else if (buttonLedState == LED_COUNTY){
      // display loggedNets in morse counter
      flash_counter(loggedNets, BLUE); // should use White for WIFI, blue for Blootoof
    } // else nothing

    float lat = gps.location.lat();
    float lon = gps.location.lng();
    float altitude = gps.altitude.meters();
    float accuracy = gps.hdop.hdop();
    char utc[21];
    sprintf(utc, "%04d-%02d-%02d %02d:%02d:%02d", gps.date.year(), gps.date.month(), gps.date.day(), gps.time.hour(), gps.time.minute(), gps.time.second());
    // Dynamic async per-channel scanning
    for (int channel = 1; channel <= 14; channel++) {
      int numNetworks = WiFi.scanNetworks(false, true, false, timePerChannel[channel - 1], channel);
      for (int i = 0; i < numNetworks; i++) {
        char currentMAC[20];
        strcpy(currentMAC, WiFi.BSSIDstr(i).c_str());
        if (!isMACSeen(currentMAC)) {
          strcpy(macAddressArray[macArrayIndex++], currentMAC);
          if (macArrayIndex >= maxMACs) macArrayIndex = 0;
          char dataString[300];
          snprintf(dataString, sizeof(dataString), "%s,\"%s\",%s,%s,%d,%d,%.6f,%.6f,%.2f,%.2f,WIFI", currentMAC, WiFi.SSID(i).c_str(), getAuthType(WiFi.encryptionType(i)), utc, WiFi.channel(i), WiFi.RSSI(i), lat, lon, altitude, accuracy);
          logData(dataString);
          loggedNets++;
        }
      }
      // Update the scan duration for this channel based on the results
      updateTimePerChannel(channel, numNetworks);
    }
  } else {
    blinkLED(PURPLE, 250);
  }
  delay(75);
}

/**
 * blink out a counter in decimal morse code
 * @param counter the value to emit
 * @param color the color to flash
 */
void flash_counter(int counter, struct CRGB color){
  if (counter <= 0) {
      flash_morse_code(numbytes[0], color);
      return;
  }
  // walk the decimal digits and write them out serially
  // the largest value representable in a 32bit int is 1s of billions
  int mask = 1000000000;
  bool seen = false; // seen any nonzero digits
  while (counter > 0){
    int idx = counter / mask;
    if (idx != 0) { // don't blink out leading zeros
      seen = true;
    }
    if (seen) {
      flash_morse_code(numbytes[idx], color);
    }
    counter -= (mask*idx);
    mask /= 10;
  }
}

/**
  *  Flashes the Morse code for the input bitmask. caller ensures the high three bits are zero.
  *  @param morse_code bitmask value of the morse code digit
  *  @param color the color to flash
  */
void flash_morse_code(const uint8_t morse_code, struct CRGB color) {

  // walk morse_code from mask start to end, shifting right by 1 each time.
  // mask & code == 0 is a dot, mask & code != 0 is a dash
  uint8_t mask = 0x10; // start selecting bit 5

  // Read the dots and dashes and flash accordingly
  while (mask != 0) {
    flash_dot_or_dash( (mask & morse_code) != 0, color );
    mask = mask >> 1;
  }

  // Space between two letters is equal to three dots
  delay(dot_duration * 3);
}

/**
  *  Flashes the dot or dash in the Morse code
  *  @param dash true if dash, false for dot
  *  @param color the color to flash
  */
void flash_dot_or_dash(bool dash, struct CRGB color) {
  M5.dis.drawpix(0, color);

  if (dash) { // If it is a dash...equal to three dots
    delay(dot_duration * 3);
  } else { // Has to be a dot
    delay(dot_duration);
  }

  M5.dis.clear();

  // Give space between parts of the same letter...equal to one dot
  delay(dot_duration);
}

void blinkLED(uint32_t color, unsigned long interval) {
  static unsigned long previousBlinkMillis = 0;
  unsigned long currentMillis = millis();

  if (currentMillis - previousBlinkMillis >= interval) {
    ledState = !ledState;
    M5.dis.drawpix(0, ledState ? color : OFF);
    previousBlinkMillis = currentMillis;
  }
}

void waitForGPSFix() {
  Serial.println("Waiting for GPS fix...");
  while (!gps.location.isValid()) {
    if (Serial1.available() > 0) {
      gps.encode(Serial1.read());
    }
    blinkLED(PURPLE, 250);
  }
  M5.dis.clear();
  Serial.println("GPS fix obtained.");
}

void initializeFile() {
  int fileNumber = 0;
  bool isNewFile = false;
  char fileDateStamp[16];
  sprintf(fileDateStamp, "%04d-%02d-%02d-", gps.date.year(), gps.date.month(), gps.date.day());
  do {
    snprintf(fileName, sizeof(fileName), "/AtomWigler-%s%d.csv", fileDateStamp, fileNumber);
    isNewFile = !SD.exists(fileName);
    fileNumber++;
  } while (!isNewFile);
  if (isNewFile) {
    File dataFile = SD.open(fileName, FILE_WRITE);
    if (dataFile) {
      dataFile.println("WigleWifi-1.4,appRelease=1.300000,model=GPS Kit,release=1.100000F+00,device=M5ATOM,display=NONE,board=ESP32,brand=M5");
      dataFile.println("MAC,SSID,AuthMode,FirstSeen,Channel,RSSI,CurrentLatitude,CurrentLongitude,AltitudeMeters,AccuracyMeters,Type");
      dataFile.close();
      Serial.println("New file created: " + String(fileName));
    }
  } else {
    Serial.println("Using existing file: " + String(fileName));
  }
}

bool isMACSeen(const char* mac) {
  for (int i = 0; i < macArrayIndex; i++) {
    if (strcmp(macAddressArray[i], mac) == 0) {
      return true;
    }
  }
  return false;
}

void logData(const char* data) {
  File dataFile = SD.open(fileName, FILE_APPEND);
  if (dataFile && data) {
    dataFile.println(data);
    dataFile.close();
  } else {
    Serial.println("Error opening " + String(fileName));
    blinkLED(RED, 500);
  }
}

const char* getAuthType(uint8_t wifiAuth) {
  switch (wifiAuth) {
    case WIFI_AUTH_OPEN:
      return "[]";
      break;

    case WIFI_AUTH_WEP:
      return "[WEP]";
      break;

    case WIFI_AUTH_WPA_PSK:
      return "[WPA]";
      break;

    case WIFI_AUTH_WPA2_PSK:
      return "[WPA2]";
      break;

    case WIFI_AUTH_WPA_WPA2_PSK:
      return "[WPA2]";
      break;

    case WIFI_AUTH_WPA2_ENTERPRISE:
      return "[WPA2]";
      break;

    case WIFI_AUTH_WPA3_PSK:
      return "[WPA3]";
      break;

    case WIFI_AUTH_WPA2_WPA3_PSK:
      return "[WPA3]";
      break;

    case WIFI_AUTH_WAPI_PSK:
      return "[WAPI]";
      break;

    default:
      return "[UNDEFINED]";
  }
}


bool findInArray(int value, const int* array, int size) {
  for (int i = 0; i < size; i++) {
    if (array[i] == value) return true;
  }
  return false;
}

// TESTING: algo for timePerChan
void updateTimePerChannel(int channel, int networksFound) {
  const int FEW_NETWORKS_THRESHOLD = 1;
  const int MANY_NETWORKS_THRESHOLD = 8;
  const int POPULAR_TIME_INCREMENT = 75;   // Higher increment for popular channels
  const int STANDARD_TIME_INCREMENT = 50;  // Standard increment
  const int RARE_TIME_INCREMENT = 30;      // Lower increment for rare channels
  const int MAX_TIME = 500;
  const int MIN_TIME = 50;

  int timeIncrement;

  // Determine the time increment based on channel type
  if (findInArray(channel, popularChannels, sizeof(popularChannels) / sizeof(popularChannels[0]))) {
    timeIncrement = POPULAR_TIME_INCREMENT;
  } else if (findInArray(channel, rareChannels, sizeof(rareChannels) / sizeof(rareChannels[0]))) {
    timeIncrement = RARE_TIME_INCREMENT;
  } else {
    timeIncrement = STANDARD_TIME_INCREMENT;
  }

  // Adjust the time per channel based on the number of networks found
  if (networksFound >= MANY_NETWORKS_THRESHOLD) {
    timePerChannel[channel - 1] += timeIncrement;
    if (timePerChannel[channel - 1] > MAX_TIME) {
      timePerChannel[channel - 1] = MAX_TIME;
    }
  } else if (networksFound <= FEW_NETWORKS_THRESHOLD) {
    timePerChannel[channel - 1] -= timeIncrement;
    if (timePerChannel[channel - 1] < MIN_TIME) {
      timePerChannel[channel - 1] = MIN_TIME;
    }
  }
}