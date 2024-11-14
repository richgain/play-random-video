/*******************************************************************************
 * AVI Player example
 *
 * Dependent libraries:
 * Arduino_GFX: https://github.com/moononournation/Arduino_GFX.git
 * avilib: https://github.com/lanyou1900/avilib.git
 * libhelix: https://github.com/pschatzmann/arduino-libhelix.git
 * ESP32_JPEG: https://github.com/esp-arduino-libs/ESP32_JPEG.git
 *
 * Setup steps:
 * 1. Change your LCD parameters in Arduino_GFX setting
 * 2. Upload AVI file
 *   FFat/LittleFS:
 *     upload FFat (FatFS) data with ESP32 Sketch Data Upload:
 *     ESP32: https://github.com/lorol/arduino-esp32fs-plugin
 *   SD:
 *     Copy files to SD card
 *
 * Video Format:
 * code "cvid": Cinepak
 * cod "MJPG": MJPEG
 *
 * Audio Format:
 * code 1: PCM
 * code 85: MP3
 ******************************************************************************/
const char *root = "/root";
const char *avi_folder = "/avi320x480";

// #ifdef CANVAS_R1
// const char *avi_folder = "/avi480x320";
// #else
// const char *avi_folder = "/avi320x480";
// #endif


#include "JC3248W535.h"

// set up for random play

#define MAX_FILES 100
// Array to store file names
char fileNames[MAX_FILES][13];  // 8.3 filename format: 8 chars + dot + 3 chars + null terminator
int fileCount = 0;



#include <FFat.h>
#include <LittleFS.h>
#include <SPIFFS.h>
#include <SD.h>
#include <SD_MMC.h>

size_t output_buf_size;
uint16_t *output_buf;

#include "AviFunc.h"
#include "esp32_audio.h"

void setup()
{
  Serial.begin(115200);
  // Serial.setDebugOutput(true);
  // while(!Serial);
  Serial.println("AVI Player");

  randomSeed(analogRead(0));  // Initialize random number generator

  // If display and SD shared same interface, init SPI first
  #ifdef SPI_SCK
    SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI);
  #endif

  #ifdef GFX_EXTRA_PRE_INIT
    GFX_EXTRA_PRE_INIT();
  #endif

  // Init Display
  // if (!gfx->begin())
  if (!gfx->begin(GFX_SPEED))
  {
    Serial.println("gfx->begin() failed!");
  }
  gfx->fillScreen(BLACK);
  #ifdef CANVAS
    gfx->flush();
  #endif

  #ifdef GFX_BL
    #if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR < 3)
      ledcSetup(0, 1000, 8);
      ledcAttachPin(GFX_BL, 0);
      ledcWrite(0, 204);
    #else  // ESP_ARDUINO_VERSION_MAJOR >= 3
      ledcAttachChannel(GFX_BL, 1000, 8, 1);
      ledcWrite(GFX_BL, 204);
    #endif // ESP_ARDUINO_VERSION_MAJOR >= 3
  #endif // GFX_BL

  // gfx->setTextColor(WHITE, BLACK);
  // gfx->setTextBound(60, 60, 240, 240);

  #ifdef AUDIO_MUTE_PIN
    pinMode(AUDIO_MUTE_PIN, OUTPUT);
    digitalWrite(AUDIO_MUTE_PIN, HIGH);
  #endif

  i2s_init();

  #if defined(SD_D1)
    #define FILESYSTEM SD_MMC
      SD_MMC.setPins(SD_SCK, SD_MOSI /* CMD */, SD_MISO /* D0 */, SD_D1, SD_D2, SD_CS /* D3 */);
      if (!SD_MMC.begin(root, false /* mode1bit */, false /* format_if_mount_failed */, SDMMC_FREQ_HIGHSPEED))
  #elif defined(SD_SCK)
    #define FILESYSTEM SD_MMC
      pinMode(SD_CS, OUTPUT);
      digitalWrite(SD_CS, HIGH);
      SD_MMC.setPins(SD_SCK, SD_MOSI /* CMD */, SD_MISO /* D0 */);
      if (!SD_MMC.begin(root, true /* mode1bit */, false /* format_if_mount_failed */, SDMMC_FREQ_HIGHSPEED))
  #elif defined(SD_CS)
    #define FILESYSTEM SD
      if (!SD.begin(SD_CS, SPI, 80000000, "/root"))
  #else
    #define FILESYSTEM FFat
      if (!FFat.begin(false, root))
      // if (!LittleFS.begin(false, root))
      // if (!SPIFFS.begin(false, root))
      #endif
    {
      Serial.println("ERROR: File system mount failed!");
    }
  else
  {
    output_buf_size = gfx->width() * gfx->height() * 2;
    #ifdef RGB_PANEL
        output_buf = gfx->getFramebuffer();
    #else
        output_buf = (uint16_t *)aligned_alloc(16, output_buf_size);
    #endif
    if (!output_buf)
      {
        Serial.println("output_buf aligned_alloc failed!");
      }

    avi_init();
  }

  Serial.println("Setup complete. Starting randomized playback...");

  playRandomAVIFiles(avi_folder);  // Specify your directory here

}

void loop() {
}


void playRandomAVIFiles(const char* dirPath) {
  listAVIFiles(dirPath);
  shuffleFiles();
  playFiles(dirPath);
}


void listAVIFiles(const char* dirPath) {

  File dir = FILESYSTEM.open(dirPath);
  if (!dir) {
    Serial.println("Failed to open directory");
    return;
  }
  
  while (true) {
    File entry = dir.openNextFile();
    if (!entry) {
      // No more files
      break;
    }
    
    if (!entry.isDirectory()) {
      String fileName = entry.name();
      if ((fileName.endsWith(".AVI") || fileName.endsWith(".avi")) && fileCount < MAX_FILES) {
        strcpy(fileNames[fileCount], fileName.c_str());
        fileCount++;
      }
    }
    
    entry.close();
  }
  
  dir.close();
  
  Serial.print("Found ");
  Serial.print(fileCount);
  Serial.println(" AVI files.");
}


void shuffleFiles() {
  for (int i = fileCount - 1; i > 0; i--) {
    int j = random(i + 1);
    // Swap fileNames[i] and fileNames[j]
    char temp[13];
    strcpy(temp, fileNames[i]);
    strcpy(fileNames[i], fileNames[j]);
    strcpy(fileNames[j], temp);
  }
  
  Serial.println("Files shuffled.");
}


void playFiles(const char* dirPath) {
  for (int i = 0; i < fileCount; i++) {
    Serial.print("Playing (");
    Serial.print(i + 1);
    Serial.print("/");
    Serial.print(fileCount);
    Serial.print("): ");
    Serial.println(fileNames[i]);
    playAVIFile(dirPath, fileNames[i]);
  }
}


void playAVIFile(const char* dirPath, const char* fileName) {
  char fullPath[100];
  sprintf(fullPath, "%s/%s", dirPath, fileName);
  
  File aviFile = FILESYSTEM.open(fullPath);
  if (!aviFile) {
    Serial.println("Failed to open file");
    return;
  }

  if (!aviFile.isDirectory())
  {
    std::string s = aviFile.name();
    // if ((!s.starts_with(".")) && (s.ends_with(".avi")))
    if ((s.rfind(".", 0) != 0) && ((int)s.find(".avi", 0) > 0))
    {
      s = root;
      s += aviFile.path();
      if (avi_open((char *)s.c_str()))

      {
        Serial.println("AVI start");
        gfx->fillScreen(BLACK);

        if (avi_aRate > 0)
        {
          i2s_set_sample_rate(avi_aRate);
        }

        avi_feed_audio();

        if (avi_aFormat == PCM_CODEC_CODE)
        {
          Serial.println("Start play PCM audio task");
          BaseType_t ret_val = pcm_player_task_start();
          if (ret_val != pdPASS)
          {
            Serial.printf("pcm_player_task_start failed: %d\n", ret_val);
          }
        }
        else if (avi_aFormat == MP3_CODEC_CODE)
        {
          Serial.println("Start play MP3 audio task");
          BaseType_t ret_val = mp3_player_task_start();
          if (ret_val != pdPASS)
          {
            Serial.printf("mp3_player_task_start failed: %d\n", ret_val);
          }
        }
        else
        {
          Serial.println("No audio task");
        }

        avi_start_ms = millis();

        Serial.println("Start play loop");
        while (avi_curr_frame < avi_total_frames)
        {
          avi_feed_audio();
          if (avi_decode())
          {
            avi_draw(0, 0);
          }
        }

        avi_close();
        Serial.println("AVI end");

        // avi_show_stat();
        // delay(5000); // 5 seconds
      }
    }
  aviFile.close();
  }
}