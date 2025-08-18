#include <TFT_eSPI.h>
#include <FS.h>
#include <FFat.h>

// TFT_eSPI object
TFT_eSPI tft = TFT_eSPI();
TFT_eSprite sprite = TFT_eSprite(&tft);

// Animation definition
struct Animation {
  const char* const* frames; // Array of BMP filenames
  uint8_t numFrames;         // Number of frames
  uint16_t frameDelayMs;     // Delay per frame (ms)
};

// Animation configurations
const char* VAPE_IDLE_FRAMES[] = {"/VAPE_IDLE0.bmp", "/VAPE_IDLE1.bmp", "/VAPE_IDLE2.bmp", "/VAPE_IDLE3.bmp"};
const char* VAPE_IDLE_SLEEP_FRAMES[] = {"/VAPE_IDLE_SLEEP0.bmp", "/VAPE_IDLE_SLEEP1.bmp", "/VAPE_IDLE_SLEEP2.bmp", "/VAPE_IDLE_SLEEP3.bmp"};
const char* VAPE_HIT_YES_FRAMES[] = {"/VAPE_HIT_YES00.bmp", "/VAPE_HIT_YES01.bmp", "/VAPE_HIT_YES02.bmp", "/VAPE_HIT_YES03.bmp",
                                     "/VAPE_HIT_YES04.bmp", "/VAPE_HIT_YES05.bmp", "/VAPE_HIT_YES06.bmp", "/VAPE_HIT_YES07.bmp",
                                     "/VAPE_HIT_YES08.bmp", "/VAPE_HIT_YES09.bmp", "/VAPE_HIT_YES10.bmp", "/VAPE_HIT_YES11.bmp"};
const char* VAPE_HIT_NO_FRAMES[] = {"/VAPE_HIT_NO00.bmp", "/VAPE_HIT_NO01.bmp", "/VAPE_HIT_NO02.bmp", "/VAPE_HIT_NO03.bmp",
                                    "/VAPE_HIT_NO04.bmp", "/VAPE_HIT_NO05.bmp", "/VAPE_HIT_NO06.bmp", "/VAPE_HIT_NO07.bmp",
                                    "/VAPE_HIT_NO08.bmp", "/VAPE_HIT_NO09.bmp", "/VAPE_HIT_NO10.bmp", "/VAPE_HIT_NO11.bmp"};
const char* VAPE_DEAD_FRAMES[] = {"/VAPE_DEAD0.bmp", "/VAPE_DEAD1.bmp", "/VAPE_DEAD2.bmp", "/VAPE_DEAD3.bmp",
                                  "/VAPE_DEAD4.bmp", "/VAPE_DEAD5.bmp"};
const char* VAPE_FREE_FRAMES[] = {"/VAPE_FREE00.bmp", "/VAPE_FREE01.bmp", "/VAPE_FREE02.bmp", "/VAPE_FREE03.bmp",
                                  "/VAPE_FREE04.bmp", "/VAPE_FREE05.bmp", "/VAPE_FREE06.bmp", "/VAPE_FREE07.bmp",
                                  "/VAPE_FREE08.bmp", "/VAPE_FREE09.bmp", "/VAPE_FREE10.bmp", "/VAPE_FREE11.bmp",
                                  "/VAPE_FREE12.bmp", "/VAPE_FREE13.bmp"};

const Animation ANIMATIONS[] = {
  {VAPE_IDLE_FRAMES, sizeof(VAPE_IDLE_FRAMES) / sizeof(VAPE_IDLE_FRAMES[0]), 1000}, // 1 FPS
  {VAPE_IDLE_SLEEP_FRAMES, sizeof(VAPE_IDLE_SLEEP_FRAMES) / sizeof(VAPE_IDLE_SLEEP_FRAMES[0]), 333}, // 3 FPS
  {VAPE_HIT_YES_FRAMES, sizeof(VAPE_HIT_YES_FRAMES) / sizeof(VAPE_HIT_YES_FRAMES[0]), 250}, // 4 FPS
  {VAPE_HIT_NO_FRAMES, sizeof(VAPE_HIT_NO_FRAMES) / sizeof(VAPE_HIT_NO_FRAMES[0]), 250}, // 4 FPS
  {VAPE_DEAD_FRAMES, sizeof(VAPE_DEAD_FRAMES) / sizeof(VAPE_DEAD_FRAMES[0]), 500}, // 2 FPS
  {VAPE_FREE_FRAMES, sizeof(VAPE_FREE_FRAMES) / sizeof(VAPE_FREE_FRAMES[0]), 500} // 2 FPS
};

const uint8_t NUM_ANIMATIONS = sizeof(ANIMATIONS) / sizeof(ANIMATIONS[0]);

const int screenWidth = 170;
const int screenHeight = 320;

int puffCount = 0;
int dailyMax = 2;
int offenses = 0;
int dayTracker = 1;
bool isDead = false;
bool isFree = false;

unsigned long lastPuffTime = 0;
const unsigned long sleepThreshold = 10000; // 10 seconds
// String input = ""; // REMOVE FOR SIGNAL

unsigned long dayStartTime = millis(); // Track start of the current day
const unsigned long dayDuration = 120000; // 120 seconds = 1 day

uint8_t i = 0;
unsigned long startTime = 0;
int animIndex = 0;
unsigned long drawTime = 0;

#define VAPE_PIN 12  // GPIO12 = ADC2_CH1
#define THRESHOLD 69  // ADC value threshold (0-4095)

void setup() {
  // Initialize Serial
  Serial.begin(115200);
  while (!Serial) delay(100);
  Serial.println("Starting...");

  analogReadResolution(12); // 12-bit ADC (0-4095)

  // Initialize display
  pinMode(15, OUTPUT); // Backlight
  digitalWrite(15, HIGH);
  tft.begin();
  tft.setRotation(0); // Portrait mode for 170x320
  Serial.println("Display initialized in portrait mode (rotation=0)");

  // Initialize sprite
  if (!sprite.createSprite(screenWidth, screenHeight)) {
    Serial.println("Failed to create sprite!");
    while (1);
  }

  // Initialize FATFS
  if (!FFat.begin(false, "/fatfs", 10)) { // false = no format, 10 = max files
    Serial.println("FATFS mount failed!");
    Serial.printf("Total bytes: %llu, Used bytes: %llu\n", FFat.totalBytes(), FFat.usedBytes());
    while (1);
  }
  Serial.println("FATFS mounted successfully");
  
  // Validate one BMP per animation
  for (uint8_t i = 0; i < NUM_ANIMATIONS; i++) {
    if (!FFat.exists(ANIMATIONS[i].frames[0])) {
      Serial.printf("File %s not found\n", ANIMATIONS[i].frames[0]);
      fs::File root = FFat.open("/");
      Serial.println("Root directory contents:");
      while (fs::File file = root.openNextFile()) {
        Serial.println(file.name());
        file.close();
      }
      root.close();
      while (1);
    }
  }
  Serial.println("All animation sets validated");
  Serial.printf("Free heap: %d, Free PSRAM: %d\n", ESP.getFreeHeap(), ESP.getFreePsram());
}

void loop() {
  const Animation& anim = ANIMATIONS[animIndex];

  while (isDead) { animDEAD(); }
  while (isFree) { animFREE(); }

  // // REMOVE FOR SIGNAL
  // if (Serial.available()) {
  //   input = Serial.readStringUntil('\n');
  //   input.trim();
  // }

  int adcValue = analogRead(VAPE_PIN);

  startTime = millis();
  drawBMP(anim.frames[i], 0, 0);
  drawIdleOverlay(puffCount, dailyMax, offenses);
  sprite.pushSprite(0, 0);

  drawTime = millis() - startTime;
  if (drawTime < anim.frameDelayMs) {
    delay(anim.frameDelayMs - drawTime);
  }

  // IDLE SLEEP
  if (millis() - lastPuffTime >= sleepThreshold && animIndex != 1) {
    animIndex = 1;
    i = 0;
  }

  // Change for Signal
  // if (input == "PUFF") {
  if (adcValue > THRESHOLD) {
    puffCount++;
    animIndex = 0;
    i = 0;
    Serial.print("PUFF ");
    Serial.println(puffCount);

    if (puffCount <= dailyMax) {
      animHIT_YES();
    } else if (puffCount > dailyMax) {
      animHIT_NO();
    }

    if (puffCount >= dailyMax + 5) {
      isDead = true;
      animPRE_DEAD();
    }
    lastPuffTime = millis();
  }


  // === DAY CHECK LOGIC ===
  if (millis() - dayStartTime >= dayDuration) {
    if (puffCount > dailyMax) {
      offenses++;
      if (offenses >= 3) {
        isDead = true;
        animPRE_DEAD();
      }
      else {
        dayTracker++;
        dayStartTime = millis();  // Reset day timer
        lastPuffTime = millis();
        puffCount = 0;
        
        animNEW_DAY();
        i = 0;
        animIndex = 0;
      }
    } 
    else {
      if (dailyMax - 1 <= 0) {
        animFREE();
        isFree = true;
      }
      else {
        dailyMax--;
        dayTracker++;
        dayStartTime = millis();  // Reset day timer
        lastPuffTime = millis();
        puffCount = 0;
        
        animNEW_DAY();
        i = 0;
        animIndex = 0;
      }
    }
  }

  // Remove for Signal
  // input = "";

  i++;
  if (i >= anim.numFrames) { i = 0; }
}

void animIDLE() { playAnimation(0); }
void animIDLE_SLEEP() { playAnimation(1); }
void animHIT_YES() { playAnimation(2); }
void animHIT_NO() { playAnimation(3); }
void animPRE_DEAD() {
  String line1 = "BRUH.";
  String line2 = "WELP";
  String line3 = "o7";

  int y2 = screenHeight / 2;
  int y2_0 = screenHeight / 2 - 15;
  int y2_1 = screenHeight / 2 + 15;

  // BRUH
  sprite.fillSprite(TFT_BLACK);
  sprite.setTextSize(3);
  sprite.setTextColor(TFT_WHITE);
  sprite.setTextDatum(MC_DATUM);
  sprite.drawString(line1, screenWidth / 2, y2);
  sprite.pushSprite(0, 0);

  delay(2000);

  // WELP, SHRUG
  sprite.fillSprite(TFT_BLACK);
  sprite.setTextSize(3);
  sprite.setTextColor(TFT_RED);
  sprite.setTextDatum(MC_DATUM);
  sprite.drawString(line2, screenWidth / 2, y2_0);
  sprite.drawString(line3, screenWidth / 2, y2_1);
  sprite.pushSprite(0, 0);

  delay(1000);
}
void animDEAD() { playAnimation(4); }
void animFREE() {
  int y2 = screenHeight / 2;

  // BIG W
  sprite.fillSprite(TFT_YELLOW);
  sprite.setTextSize(7);
  sprite.setTextColor(TFT_BLACK);
  sprite.setTextDatum(MC_DATUM);
  sprite.drawString("W", screenWidth / 2, y2);
  sprite.pushSprite(0, 0);

  delay(1500);

  playAnimation(5); 
}
void animNEW_DAY() {
  String line1 = "Day " + String(dayTracker);
  String line2 = "Offense " + String(offenses);
  String line3 = "Max Puff " + String(dailyMax);

  int y1 = screenHeight / 2 - 30;
  int y2 = screenHeight / 2;
  int y3 = screenHeight / 2 + 30;

  // DAY INTRODUCE
  sprite.fillSprite(TFT_YELLOW);
  sprite.setTextSize(3);
  sprite.setTextColor(TFT_WHITE);
  sprite.setTextDatum(MC_DATUM);
  sprite.drawString(line1, screenWidth / 2, y2);
  sprite.pushSprite(0, 0);

  delay(3000);

  // DAY, OFFENSE, DAILY MAX
  sprite.fillSprite(TFT_WHITE);
  sprite.setTextSize(1);
  sprite.setTextColor(TFT_BLACK);
  sprite.drawString(line1, screenWidth / 2, y1);
  sprite.setTextSize(2);
  sprite.drawString(line2, screenWidth / 2, y2);
  sprite.drawString(line3, screenWidth / 2, y3);
  sprite.pushSprite(0, 0);

  delay(3000);
}

void playAnimation(uint8_t animIndex) {
  if (animIndex >= NUM_ANIMATIONS) {
    Serial.printf("Invalid animation index: %d\n", animIndex);
    return;
  }
  const Animation& anim = ANIMATIONS[animIndex];
  for (uint8_t i = 0; i < anim.numFrames; i++) {
    unsigned long startTime = millis();
    drawBMP(anim.frames[i], 0, 0);

    if (animIndex == 0 || animIndex == 1) {
      drawIdleOverlay(puffCount, dailyMax, offenses);
    }
    sprite.pushSprite(0, 0);
    Serial.println("Pushed sprite to display");

    unsigned long drawTime = millis() - startTime;
    if (drawTime < anim.frameDelayMs) {
      delay(anim.frameDelayMs - drawTime);
    }
  }
}

void drawIdleOverlay(int input_count, int daily_max, int offenses) {
  sprite.setTextDatum(TL_DATUM);    // Top-left alignment
  sprite.setTextSize(1);            // Small readable text
  sprite.setTextColor(TFT_BLACK);   // Text color

  // Puff counter (top-left)
  String puffText = String(input_count) + " / " + String(daily_max);
  sprite.drawString(puffText, 10, 10);  // Draw on sprite

  // Offense counter (below puff counter)
  String offenseText = String(offenses) + " / 3";
  int lineHeight = 10;  // Approx height of text line
  sprite.drawString(offenseText, 10, 10 + lineHeight + 4);
  Serial.printf("Drew offense text: %s at (10, 24)\n", offenseText.c_str());
}

void drawBMP(const char *filename, int16_t x, int16_t y) {
  fs::File bmpFile = FFat.open(filename, FILE_READ);
  if (!bmpFile) {
    Serial.printf("Failed to open %s\n", filename);
    return;
  }

  // Read BMP header
  uint8_t header[54];
  if (bmpFile.read(header, 54) != 54) {
    Serial.printf("Invalid BMP header: %s\n", filename);
    bmpFile.close();
    return;
  }

  // Parse header
  uint32_t dataOffset = *(uint32_t*)&header[10];
  uint32_t width = *(uint32_t*)&header[18];
  int32_t height = *(int32_t*)&header[22]; // Signed for top-down
  uint16_t bitsPerPixel = *(uint16_t*)&header[28];

  // Validate BMP
  if (bitsPerPixel != 16 || (abs(height) > 320 && abs(height) > 170) || (width > 320 && width > 170)) {
    Serial.printf("Invalid BMP: %s (bpp=%d, width=%d, height=%d)\n", filename, bitsPerPixel, width, height);
    bmpFile.close();
    return;
  }

  // Adjust for top-down BMP
  bool topDown = height < 0;
  height = abs(height);

  // Allocate buffer for entire image
  size_t bufferSize = width * height * sizeof(uint16_t);
  uint16_t* buffer = (uint16_t*)ps_malloc(bufferSize);
  if (!buffer) {
    Serial.printf("Failed to allocate buffer (%d bytes) for %s\n", bufferSize, filename);
    bmpFile.close();
    return;
  }

  // Seek to pixel data
  bmpFile.seek(dataOffset);

  // Read all pixels into buffer
  for (int row = 0; row < height; row++) {
    for (int col = 0; col < width; col++) {
      uint8_t pixel[2];
      bmpFile.read(pixel, 2);
      // Big-endian BMP: read as little-endian for display
      buffer[row * width + col] = (pixel[1] << 8) | pixel[0];
    }
  }

  // Draw entire image at once
  int drawY = topDown ? y : y + (height - 1);
  int drawHeight = topDown ? height : -height;
  sprite.pushImage(x, drawY, width, abs(height), buffer);

  // Free buffer
  free(buffer);
  bmpFile.close();
  Serial.printf("Displayed %s (width=%d, height=%d)\n", filename, width, height);
}

uint16_t read16(fs::File &f) {
  uint8_t lsb = f.read();
  uint8_t msb = f.read();
  return (msb << 8) | lsb;
}

uint32_t read32(fs::File &f) {
  uint8_t b0 = f.read();
  uint8_t b1 = f.read();
  uint8_t b2 = f.read();
  uint8_t b3 = f.read();
  return (b3 << 24) | (b2 << 16) | (b1 << 8) | b0;
}