#include <Arduino.h>
#include <SPI.h>
#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <TFT_eSPI.h>
#include <HTTPClient.h>
#include "json.hpp"
#include <ESP32Time.h>
#include "icons.h"
#include <WiFiManager.h>
#include <Arduino.h>
#include <Adafruit_SH1106.h>
#define _TASK_SCHEDULING_OPTIONS
#define _TASK_STATUS_REQUEST
#define _TASK_SLEEP_ON_IDLE_RUN
#define _TASK_PRIORITY
#define _TASK_WDT_IDS
#define _TASK_TIMECRITICAL
#define _TASK_INLINE
#define _TASK_THREAD_SAFE
#include <TaskScheduler.h>
#include <TJpg_Decoder.h>
#define FS_NO_GLOBALS
#include <FS.h>
#ifdef ESP32
  #include "SPIFFS.h" // ESP32 only
#endif
using json = nlohmann::json;

#define TFT_DC 2
#define TFT_CS 15
#define OLED_RESET -1
#define NSTARS 512
#define SD_CS   5

Adafruit_SH1106 display(OLED_RESET);
TFT_eSPI tft = TFT_eSPI(TFT_CS, TFT_DC);
WiFiManager wifiManager;
ESP32Time rtc(0);
Scheduler ts,tts,ttts;
StatusRequest download_complete, download_failed, weather_process_complete, is_on_network;

#define CALIBRATION_FILE "/TouchCalData1"
#define REPEAT_CAL false

const String weatherurl = "https://api.open-meteo.com/v1/forecast?latitude=44.4323&longitude=26.1063&hourly=temperature_2m,apparent_temperature,precipitation_probability,precipitation,weathercode,uv_index&daily=weathercode,temperature_2m_max,temperature_2m_min,apparent_temperature_max,apparent_temperature_min,uv_index_max,precipitation_sum,precipitation_probability_max&timezone=auto&forecast_days=3";
const String timeurl = "http://worldtimeapi.org/api/timezone/Europe/Bucharest";
float hourly_weather[72][6]={0}; //Temp, AppTemp, Precip%, Precip, WCode, UV
float daily_weather[3][8]={0}; //WCode, TempM, Tempm, AppTempM, AppTempm, UVMax, PrecipSum, Precip%Max
int offset = -13;
bool lr=true;
bool setupComplete=false;
String forecast;
bool is_data_updated=false;
bool wifi_display_status=false;
int update_minute=-1;
int SDFiles=-3;
File root;
int currentPhoto=0;
int currentGraph=0;
String graphName[]={"", "Temperature", "Precipitation", "WeatherCode + UV", "Today", "Tomorrow", "In 2 Days"};

uint8_t sx[NSTARS] = {};
uint8_t sy[NSTARS] = {};
uint8_t sz[NSTARS] = {};
uint8_t za, zb, zc, zx;
uint16_t x, y;


void getWeather();
void getTime();
void displayTime();
int getRandom(int lower, int upper);
void clockTaskCallback();
void networkTaskCallback();
void networkTaskCallbackEnd();
void downloadTaskCallback();
void dimDisplayCallback();
void undimDisplayCallback();
void screenSaverCallback();
void processWeather();
void downloadFailedTaskCallback();
void configModeCallback(WiFiManager *wifiManager);
bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap);
void tftCyclePicture();
void tftGraphTaskCallback();
void tftGraphStartTaskCallback();
void tftMenuTaskCallback();
void printDirectory(File dir, int numTabs);
void touch_calibrate();


//void setTime();

// Fast 0-255 random number generator from http://eternityforest.com/Projects/rng.php:
uint8_t rng()
{
  zx++;
  za = (za^zc^zx);
  zb = (zb+za);
  zc = ((zc+(zb>>1))^za);
  return zc;
}

//(60-rtc.getSecond())*1000 + 
Task clockTask(60000, TASK_FOREVER, &clockTaskCallback, &ts, false);
Task networkTask(&networkTaskCallback, &ts);
Task downloadTask(&downloadTaskCallback, &ttts);
Task undimOledDisplayTask(&undimDisplayCallback, &ts);
Task dimOledDisplayTask(&dimDisplayCallback, &ts);
Task screenSaverTask(150, TASK_FOREVER, &screenSaverCallback, &ts, false);
//Task screenSaverTask(&screenSaverCallback, &ts);
Task processWeatherTask(TASK_IMMEDIATE, 72, &processWeather, &tts, false);
Task downloadFailedTask(&downloadFailedTaskCallback, &ts);
Task tftTouchPollingTask(2000, TASK_FOREVER, &tftGraphStartTaskCallback, &tts, false);
Task tftMenuTask(250, TASK_FOREVER, &tftMenuTaskCallback, &tts, false);
Task tftGraphTask(42, TASK_FOREVER, &tftGraphTaskCallback, &ts, false);



void setup() {
  pinMode(32, OUTPUT);
  digitalWrite(32, HIGH);
  Serial.begin(115200);
  while(!Serial) {}
  za = random(256);
  zb = random(256);
  zc = random(256);
  zx = random(256);

  if (!SD.begin(SD_CS)) {
    Serial.println(F("SD.begin failed!"));
    while (1) delay(0);
  }
  Serial.println("\r\nInitialisation done.");

  root=SD.open("/");
  printDirectory(root,0);
  Serial.print("There are: ");
  Serial.print(SDFiles);
  Serial.println(" photos.");

  tft.init();
  tft.setTextColor(0xFFFF, 0x0000);
  tft.setSwapBytes(true);
  tft.setRotation(3);
  touch_calibrate();
  tft.fillScreen(TFT_BLACK);

  TJpgDec.setJpgScale(1);
  TJpgDec.setCallback(tft_output);

  ts.setHighPriorityScheduler(&tts);
  tts.setHighPriorityScheduler(&ttts);
  clockTask.setSchedulingOption(TASK_SCHEDULE);
  screenSaverTask.enable();

  rtc.setTime(1667437900);
  display.begin(SH1106_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setCursor(22, 6);
  display.setTextSize(2);
  display.println("Welcome");
  display.setTextSize(1);
  display.drawBitmap(0, 0, splash, 128, 64, 1);
  display.display();
  delay(5000);
  Serial.println("Starting Wifi");
  wifiManager.setConfigPortalTimeout(180);
  wifiManager.setTimeout(300);
  wifiManager.setClass("invert"); // dark theme
  wifiManager.setAPCallback(configModeCallback);
  wifiManager.setScanDispPerc(true); // display percentages instead of graphs for RSSI
  wifiManager.autoConnect("WeatherByFabz");
  getTime();
  getWeather();
  wifiManager.disconnect();
  Serial.println("Done with Wifi");
  processWeatherTask.restart();
  setCpuFrequencyMhz(80);
  display.dim(true);
  displayTime();

  setupComplete=true;

  Serial.println("Starting Tasks");
  clockTask.enableDelayed((59-rtc.getSecond())*1000 + 1001-rtc.getMillis());
  screenSaverTask.disable();
  tftTouchPollingTask.enable();
  //networkTask.enableDelayed(15*3600000 + (59-rtc.getSecond())*1000 + (999-rtc.getMillis()));
  //networkTask.enableDelayed((59-rtc.getMinute())*3600000 + (59-rtc.getSecond())*1000 + (999-rtc.getMillis()));
}

void loop() {
  ts.execute();
}

void configModeCallback(WiFiManager *wifiManager) {
  Serial.println("Entered Config Mode");
  Serial.println(WiFi.softAPIP());
  display.clearDisplay();
  display.setCursor(0,1);
  display.print("Can't connect to lastwifi. Connect with   your phone to:");
  display.setCursor(25,40);
  display.println(wifiManager->getConfigPortalSSID());
  display.drawBitmap(112, 48, no_wifi, 13, 13, 1);
  wifi_display_status=true;
  display.display();
}

void clockTaskCallback() {
  Serial.print("Clock Task Callback ");
  displayTime();
  //update_minute=rtc.getMinute();
}

void networkTaskCallback() {
  setCpuFrequencyMhz(240);
  Serial.println("Network Task Callback");
  is_on_network.setWaiting();
  download_complete.setWaiting();
  is_data_updated=false;
  int attempt_counter=0;
  while(WiFi.status() != WL_CONNECTED && attempt_counter<3)
  {
    Serial.println("Attempting to connect");
    wifiManager.autoConnect("WeatherByFabz");
    attempt_counter++;
  }
  if(WiFi.status() == WL_CONNECTED) {
    Serial.println("Network success.");
    downloadTask.restart();
  }
  else {
    Serial.println("Network error.");
    return;
  }
  displayTime();
  networkTask.setCallback(&networkTaskCallbackEnd);
  networkTask.waitFor(&download_complete);
  Serial.println("Network task done");
  yield(); 
}

void networkTaskCallbackEnd() {
  Serial.println("Network task end start");
  wifiManager.disconnect();
  download_complete.setWaiting();
  Serial.println("Network Task Callback Complete");
  networkTask.setCallback(&networkTaskCallback);
  is_on_network.signalComplete();
  networkTask.disable();
  Serial.println("Network task end done");
  setCpuFrequencyMhz(80);
  yield();
}

void screenSaverCallback()
{
  //if(screenSaverTask.getOverrun()<0)
    //Serial.println(screenSaverTask.getOverrun());
  //unsigned long t0 = micros();
  uint8_t spawnDepthVariation = 255;

  for(int i = 0; i < NSTARS; ++i)
  {
    if (sz[i] <= 1)
    {
      sx[i] = 160 - 120 + rng();
      sy[i] = rng();
      sz[i] = spawnDepthVariation--;
    }
    else
    {
      int old_screen_x = ((int)sx[i] - 160) * 256 / sz[i] + 160;
      int old_screen_y = ((int)sy[i] - 120) * 256 / sz[i] + 120;

      // This is a faster pixel drawing function for occassions where many single pixels must be drawn
      tft.drawPixel(old_screen_x, old_screen_y,TFT_BLACK);

      sz[i] -= 2;
      if (sz[i] > 1)
      {
        int screen_x = ((int)sx[i] - 160) * 256 / sz[i] + 160;
        int screen_y = ((int)sy[i] - 120) * 256 / sz[i] + 120;
  
        if (screen_x >= 0 && screen_y >= 0 && screen_x < 320 && screen_y < 240)
        {
          uint8_t r, g, b;
          r = g = b = 255 - sz[i];
          tft.drawPixel(screen_x, screen_y, tft.color565(r,g,b));
        }
        else
          sz[i] = 0; // Out of screen, die.
      }
    }
  }
  //unsigned long t1 = micros();
  //static char timeMicros[8] = {};

 // Calcualte frames per second
  //Serial.println(1.0/((t1 - t0)/1000000.0));
  yield();
}

void downloadTaskCallback() {
  Serial.println("Download Task Callback");
  if(rtc.getHour()==12 && rtc.getMinute()==0) //Configure how often to refresh
  //time from the web.
  //if(rtc.getMinute()%2==0)
  {
    getTime();
  }
  getWeather();
    //clockTask.waitFor(&weather_process_complete);
  Serial.println("Download task done");
  yield();
}

void downloadFailedTaskCallback() {
  
}

void processWeather() {
  Serial.println("Processing weather task");
  json obj = json::parse(forecast.c_str());
  int i = processWeatherTask.getRunCounter() -1;
  hourly_weather[i][0] = obj["hourly"]["temperature_2m"][i].get<float>();
  hourly_weather[i][1] = obj["hourly"]["apparent_temperature"][i].get<float>();
  hourly_weather[i][2] = obj["hourly"]["precipitation_probability"][i].get<float>();
  hourly_weather[i][3] = obj["hourly"]["precipitation"][i].get<float>();
  hourly_weather[i][4] = obj["hourly"]["weathercode"][i].get<float>();
  hourly_weather[i][5] = obj["hourly"]["uv_index"][i].get<float>();

  if(i<3)
  {
    daily_weather[i][0] = obj["daily"]["weathercode"][i].get<float>();
    daily_weather[i][0] = obj["daily"]["temperature_2m_max"][i].get<float>();
    daily_weather[i][0] = obj["daily"]["temperature_2m_min"][i].get<float>();
    daily_weather[i][0] = obj["daily"]["apparent_temperature_max"][i].get<float>();
    daily_weather[i][0] = obj["daily"]["apparent_temperature_min"][i].get<float>();
    daily_weather[i][0] = obj["daily"]["uv_index_max"][i].get<float>();
    daily_weather[i][0] = obj["daily"]["precipitation_sum"][i].get<float>();
    daily_weather[i][0] = obj["daily"]["precipitation_probability_max"][i].get<float>();
  }
  if(i>=71) {
    weather_process_complete.signalComplete();
    is_data_updated=true;
    displayTime();
  }
  yield();
}

void undimDisplayCallback() {
  Serial.println("Un-dimming display");
  display.dim(false);
  dimOledDisplayTask.restartDelayed(15000);
  yield();
}

void dimDisplayCallback() {
  Serial.println("Dimming display");
  display.dim(true);
  yield();
  //dimDisplayTask.restartDelayed(60000);
}

int getRandom(int lower, int upper) {
    return lower + static_cast<int>(rand() % (upper - lower + 1));
}

void getWeather() {
  Serial.println("getWeather");
  display.clearDisplay();
  display.setCursor(42, 2);
  display.println("Getting");
  display.drawRect(18, 14, 90, 18, 1);
  display.setCursor(22, 16);
  display.setTextSize(2);
  display.println("WEATHER");
  display.setTextSize(1);
  display.drawBitmap(46, 34, bit_sun, 32, 32, 1);
  if(WiFi.status() == WL_CONNECTED) {
    display.drawBitmap(112, 48, yes_wifi, 13, 13, 1);
    wifi_display_status=true;
  }
  display.display();
  HTTPClient http;
  http.useHTTP10(true);
  http.begin(weatherurl);
  http.GET();
  forecast = http.getString();
  http.end();
  download_complete.signalComplete();
  weather_process_complete.setWaiting();
  processWeatherTask.restart();
  yield();
}

void getTime() {
  Serial.println("getTime");
  HTTPClient http;
  http.useHTTP10(true);
  http.begin(timeurl);
  http.GET();
  String result = http.getString();
  json obj = json::parse(result.c_str());
  unsigned long unixtime = obj["unixtime"].get<unsigned long>();
  int offset = obj["dst_offset"].get<int>();
  rtc.setTime(unixtime+offset+7200);
  http.end();
  clockTask.restartDelayed((59-rtc.getSecond())*1000 + 1001-rtc.getMillis());
  yield();
}

void displayTime() {
  Serial.println(" Switching shown time");
  Serial.println(networkTask.isEnabled());
  wifi_display_status=false;
  int hour = rtc.getHour(true);
  float temperature = hourly_weather[hour][1];
  int weather = hourly_weather[hour][4];
  if(lr)
    if(offset++ > 14)
    lr=false;
  else if(offset-- < -14)
    lr=true;
  display.clearDisplay();
  display.setTextSize(3);
  display.setCursor(21+offset, 12);
  display.print(hour);
  display.print(":");
  if(rtc.getMinute()<10)
    display.print("0");
  display.println(rtc.getMinute());
  if(weather < 2)
    display.drawBitmap(90+offset, 39, sunny, 24, 24, 1);
  else if(weather > 1 && weather < 5)
    display.drawBitmap(90+offset, 39, cloudy, 24, 24, 1);
  else if(weather > 49 && weather < 60)
    display.drawBitmap(90+offset, 39, drizzle, 24, 24, 1);
  else if((weather > 59 && weather < 68) || (weather > 79 && weather < 83))
    display.drawBitmap(90+offset, 39, rain, 24, 24, 1);
  else if((weather > 69 && weather < 78) || weather==85 || weather == 86)
    display.drawBitmap(90+offset, 39, snow, 24, 24, 1);
  else
    display.drawBitmap(90+offset, 39, storm, 24, 24, 1);
  display.setTextSize(2);
  display.setCursor(18+offset, 40);
  display.print(temperature,1);
  display.print((char)247);
  display.print("C");
  int CircleY;
  if(hour<12)
    CircleY=130-hour*10;
  else CircleY=20+(hour-12)*10;
  display.drawCircle(64, CircleY, 62, 1);
  display.setTextSize(1);
  if(WiFi.status() == WL_CONNECTED) {
    display.drawBitmap(112, 48, yes_wifi, 13, 13, 1);
    wifi_display_status=true;
  }
  else if(!is_data_updated) {
    display.drawBitmap(112, 48, no_wifi, 13, 13, 1);
    wifi_display_status=true;
  }
  display.display();
  if(hour>7)
    undimOledDisplayTask.restart();
  if(rtc.getMinute()%20==0 && !networkTask.isEnabled() && rtc.getMinute()!=update_minute)
    networkTask.restart();
  update_minute=rtc.getMinute();
  if(!currentGraph)
    tftCyclePicture();
  yield();
}

bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap)
{
  if ( y >= tft.height() ) return 0;
  tft.pushImage(x, y, w, h, bitmap);
  return 1;
  yield();
}

void tftCyclePicture() {
  tftGraphTask.disable();
  tftMenuTask.disable();
  //if(tftTouchPollingTask.isEnabled())
  tftTouchPollingTask.disable();
  if(currentPhoto>=(SDFiles))
    currentPhoto=1;
  else currentPhoto++;
  Serial.print("Current photo: ");
  Serial.println(currentPhoto);
  String filename = "/" + String(currentPhoto) + ".jpg";
  TJpgDec.drawSdJpg(0, 0, filename);
  tftTouchPollingTask.enable();
  yield();
}

void printDirectory(File dir, int numTabs) {
  while (true) {
    File entry =  dir.openNextFile();
    if (! entry) {
      break;
    }
    SDFiles++;
    if (entry.isDirectory()) {
      printDirectory(entry, numTabs + 1);
    }
    entry.close();
  }
}

void tftGraphTaskCallback() {
  int i = tftGraphTask.getRunCounter();
  if(i == 1) {
    tft.drawRect(18, 18, 284, 144, TFT_WHITE);
    tft.drawRect(20, 20, 280, 140, TFT_BLACK);

  }
}

void tftGraphStartTaskCallback() {
  x=0;y=0;
  if (tft.getTouch(&x, &y)) {
    currentGraph=1;
    tftMenuTask.enable();
  }
}

void tftMenuTaskCallback() {
  x=0;y=0;
  if(tftMenuTask.getRunCounter()==1) {
    tftTouchPollingTask.disable();
    tft.fillScreen(TFT_BLACK);

    tft.drawRoundRect(8, 188, 44, 44, 5, TFT_MINT);
    tft.drawRoundRect(10, 190, 40, 40, 5, TFT_MUTEDPURPLE);
    tft.drawRoundRect(73, 208, 44, 24, 5, TFT_MINT);
    tft.drawRoundRect(75, 210, 40, 20, 5, TFT_MINT2);
    tft.drawRoundRect(138, 208, 44, 24, 5, TFT_MINT);
    tft.drawRoundRect(140, 210, 40, 20, 5, TFT_MINT2);
    tft.drawRoundRect(203, 208, 44, 24, 5, TFT_MINT);
    tft.drawRoundRect(205, 210, 40, 20, 5, TFT_MINT2);
    tft.drawRoundRect(268, 188, 44, 44, 5, TFT_MINT);
    tft.drawRoundRect(270, 190, 40, 40, 5, TFT_MUTEDPURPLE);

    tft.setCursor(19, 194);
    tft.setTextSize(1);
    tft.print("TEMP");
    tft.setCursor(15, 213);
    tft.setTextSize(1);
    tft.print((int)hourly_weather[rtc.getHour(true)][1]);

    //tft.pushImage(84, 208, 24, 24, left_icon);
    tft.setCursor(92, 216);
    tft.print("<");
    //tft.pushImage(148, 208, 24, 24, close_icon);
    tft.setCursor(156, 216);
    tft.print("X");
    //tft.pushImage(212, 208, 24, 24, right_icon);
    tft.setCursor(220, 216);
    tft.print(">");

    tft.setCursor(275, 194);
    tft.setTextSize(1);
    tft.print("RAIN%");
    tft.setCursor(278, 213);
    tft.setTextSize(1);
    tft.print(hourly_weather[rtc.getHour(true)][2]);
    tftGraphTask.enable();
  }
  tft.getTouch(&x, &y);
  if(y>188) {
    if(x>138 && x<182) {
      tftGraphTask.disable();
      tftTouchPollingTask.enable();
      currentGraph=0;
      tftCyclePicture();
      tftMenuTask.disable();
    }
    if(x>73 && x<117 && currentGraph>1) {
      currentGraph--;
      tftGraphTask.restart();
    }
    if(x>268 && x<312 && currentGraph<6) {
      currentGraph++;
      tftGraphTask.restart();
    }
  }
}

void touch_calibrate()
{
  uint16_t calData[5];
  uint8_t calDataOK = 0;

  // check file system exists
  if (!SPIFFS.begin()) {
    Serial.println("Formating file system");
    SPIFFS.format();
    SPIFFS.begin();
  }

  // check if calibration file exists and size is correct
  if (SPIFFS.exists(CALIBRATION_FILE)) {
    if (REPEAT_CAL)
    {
      // Delete if we want to re-calibrate
      SPIFFS.remove(CALIBRATION_FILE);
    }
    else
    {
      File f = SPIFFS.open(CALIBRATION_FILE, "r");
      if (f) {
        if (f.readBytes((char *)calData, 14) == 14)
          calDataOK = 1;
        f.close();
      }
    }
  }

  if (calDataOK && !REPEAT_CAL) {
    // calibration data valid
    tft.setTouch(calData);
  } else {
    // data not valid so recalibrate
    tft.fillScreen(TFT_BLACK);
    tft.setCursor(20, 0);
    tft.setTextFont(2);
    tft.setTextSize(1);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);

    tft.println("Touch corners as indicated");

    tft.setTextFont(1);
    tft.println();

    if (REPEAT_CAL) {
      tft.setTextColor(TFT_RED, TFT_BLACK);
      tft.println("Set REPEAT_CAL to false to stop this running again!");
    }

    tft.calibrateTouch(calData, TFT_MAGENTA, TFT_BLACK, 15);

    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.println("Calibration complete!");

    // store data
    File f = SPIFFS.open(CALIBRATION_FILE, "w");
    if (f) {
      f.write((const unsigned char *)calData, 14);
      f.close();
    }
  }
}