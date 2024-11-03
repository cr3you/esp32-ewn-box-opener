/*
 * ------------------------------------------------------------------------
 * Project Name: esp32-ewn-box-opener
 * Description: Esp32 port of the Box Opener client for mining EWN tokens.
 * Author: Crey
 * Repository: https://github.com/cr3you/esp32-ewn-box-opener/
 * Date: 2024.11.03 
 * Version: 1.2.0
 * License: MIT
 * ------------------------------------------------------------------------
 */
#define VERSION "1.2.0"
//#define USE_HARDCODED_CREDENTIALS // if for some reson LittleFS/SPIFFS does not work

#include "bip39/bip39.h"
#include <Arduino.h>
#include "WiFi.h"
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#ifndef USE_HARDCODED_CREDENTIALS
  #include <FS.h>
  #include <LittleFS.h>  // for LittleFS
  //#include <SPIFFS.h>  // for SPIFFS
#endif

#ifdef T_DISPLAY_S3
  #include <SPI.h>
  #include <TFT_eSPI.h>
  #include "cats.h"
  #include "Dongle-Regular24.h"
  #include "Dongle-Regular44.h"
  #include "Dongle-Regular72.h"
#endif

/********************************************************************
 * 
 *  YOU DON'T NEED TO SET WIFI PASSWORD AND APIKEY IN CODE ANYMORE
 * 
 *  USE SERIAL PORT TO CONFIG (type: help)
 * 
 *  IF YOU REALLY WANT TO SET CREDENTIALS IN CODE, UNCOMMENT 
 *  "USE_HARDCODED_CREDENTIALS" LINE IN PLATFORMIO.INI FILE
 *  AND SET THE LINES BELOW
 * 
 ********************************************************************/

//=====wifi setup
String ssid = "YOUR_WIFI_SSID"; // <---------------------- SET THIS !!!
String password = "YOUR_WIFI_PASSWORD"; // <-------------- SET THIS !!!

//=====Box Opener client setup
String apiKey = "YOUR_API_KEY"; // <---------------------- SET THIS !!!
bool devnet = false; // if 'true' use devnet, if 'false' use mainnet


const char *apiUrlMainnet = "https://api.erwin.lol/"; // mainnet
const char *apiUrlDevnet = "https://devnet-api.erwin.lol/"; // devnet

#define FORMAT_LITTLEFS_IF_FAILED true
const String config_filename = "/ewnconfig.json";
bool config_set = false;

const int numGuesses = 50;
String mnemonics[numGuesses]; // bip39 mnemonic table
int sleepTime = 10000; // default sleep time in ms

unsigned long t1=0;
unsigned long t2=0;
bool box_opener_sleeping = false;

#ifdef T_DISPLAY_S3
TFT_eSPI tft;
TFT_eSprite sprite_kitty = TFT_eSprite(&tft); // kitty picture
TFT_eSprite sprite_status = TFT_eSprite(&tft); // status text
TFT_eSprite sprite_bar = TFT_eSprite(&tft); // info bar at the botom
TFT_eSprite sprite_to = TFT_eSprite(&tft); // timeout field
TFT_eSprite sprite_rr = TFT_eSprite(&tft); // rejection ratio % display

// statuses to display
#define ST_ACCEPTED 1
#define ST_REJECTED 2
#define ST_ERROR    3
#define ST_CHARGE   4
#define ST_SUBMITTING 5
#define ST_NO_WIFI 6
#define ST_NO_CONFIG 7

uint16_t ratio = 10; // rejection ratio

void setStatus(uint8_t status);
void showOnBar(const char *txt);
void showCat(uint8_t n);

unsigned long t3=0; // 1s timer
unsigned long display_timer_val=0; // timer to show value

// https://rgbcolorpicker.com/565
// #846c3d -> 0x8367 //
// #db6c6a -> 0xdb6d // red
// #89523d -> 0x8a87 // 
// #ceb56c -> 0xcdad // 
// #3d8247 -> 0x3c09 // green

#define C_NORMAL_BACKGROUND 0xcdad//0x93E9 //(148,127,75) standard background
#define C_RED               0xdb6d // 0xCACA //(202,89,82) error color
#define C_BLACK             0x20A0 //(36,20,0) //"black" color
#define C_TIMER             0xED6E //(232,175,115) kitty background and timer letters
#define C_BAR_OUTLINE       0xC551 //(0xc1,0xaa,0x8b) ratio bar outline
#define C_BAR_OK            0xCDAD //(0xce,0xb5,0x6c) ratio bar inside OK color
#define C_BAR_ERROR         C_RED // ratio bar inside ERROR color
#define C_CONFIG_TABLE      0xcdad //(64,55,51) IP/config frame
#define C_KITTY_FRAME       0x4123 //(70,36,26) kitty picture inside frame 
#define C_KITTY_BACKGROUND  0xED6E //(0xe8,0xaf,0x73) // kitty background
#define C_TEXT              C_KITTY_BACKGROUND //0x5247 //(83,74,57) text color for logs and such
#define C_LOG_BACKGROUND    TFT_BLACK //0x8a87 //-TFT_BLACK
#define C_BACKGROUND        TFT_BLACK // base background

#define C_INIT_FRAME          TFT_BLACK
#define C_INIT_BACKGROUND     0x8367
#define C_INIT_FONT           TFT_BLACK
#define C_ACCEPTED_FRAME      TFT_BLACK
#define C_ACCEPTED_BACKGROUND 0x3c09
#define C_ACCEPTED_FONT       TFT_BLACK
#define C_REJECTED_FRAME      C_RED
#define C_REJECTED_BACKGROUND TFT_BLACK
#define C_REJECTED_FONT       C_RED
#define C_ERROR_FRAME         C_RED
#define C_ERROR_BACKGROUND    TFT_BLACK
#define C_ERROR_FONT          C_RED
#define C_CHARGE_FRAME        TFT_BLACK
#define C_CHARGE_BACKGROUND   0x8367
#define C_CHARGE_FONT         TFT_BLACK
#define C_SUBMIT_FRAME        TFT_BLACK
#define C_SUBMIT_BACKGROUND   0x8367
#define C_SUBMIT_FONT         TFT_BLACK
#define C_NOWIFI_FRAME        C_RED
#define C_NOWIFI_BACKGROUND   TFT_BLACK
#define C_NOWIFI_FONT         C_RED
#define C_NOCONFIG_FRAME      C_RED
#define C_NOCONFIG_BACKGROUND TFT_BLACK
#define C_NOCONFIG_FONT       C_RED

#endif //T_DISPLAY_S3

//-----------------------------------------------------------------------
//--------- c o n f i g  f i l e  h a n d l i n g -----------------------
//credit goes to: https://github.com/mo-thunderz/Esp32ConfigFile/
#ifndef USE_HARDCODED_CREDENTIALS
void writeFile(String filename, String message){
    fs::File file = LittleFS.open(filename, "w");
    if(!file){
        Serial.println("writeFile -> failed to open file for writing");
        return;
    }
    if(file.print(message)){
        Serial.println("File written");
    } else {
        Serial.println("Write failed");
    }
    file.close();
}

String readFile(String filename){
    fs::File file = LittleFS.open(filename);
    if(!file){
        Serial.println("Failed to open file for reading");
        return "";
    }
    
    String fileText = "";
    while(file.available()){
        fileText = file.readString();
    }
    file.close();
    return fileText;
}

bool readConfig() {
  String file_content = readFile(config_filename);

  int config_file_size = file_content.length();
  Serial.println("Config file size: " + String(config_file_size));

  if(config_file_size > 1024) {
    Serial.println("Config file too large");
    return false;
  }
  StaticJsonDocument<1024> doc;
  auto error = deserializeJson(doc, file_content);
  if ( error ) { 
    Serial.println("Error interpreting config file");
    return false;
  }

  const String _ssid = doc["wifi_ssid"];
  const String _password = doc["wifi_password"];
  const String _apiKey = doc["api_key"];
  const bool _config_set = doc["configured"];
  const bool _devnet = doc["devnet"];

  ssid = _ssid;
  password = _password;
  apiKey = _apiKey;
  config_set = _config_set;
  devnet = _devnet;
  return true;
}

bool saveConfig() {
  StaticJsonDocument<1024> doc;

  // write variables to JSON file
  doc["wifi_ssid"] = ssid;
  doc["wifi_password"] = password;
  doc["api_key"] = apiKey;
  doc["devnet"] = devnet;
  doc["configured"] = config_set;
  
  // write config file
  String tmp = "";
  serializeJson(doc, tmp);
  writeFile(config_filename, tmp);
  
  return true;
}
#endif
//-----------------------------------------------------------------------




//-------------------------------------------------------------------
//---------------- s e r i a l   s e t u p --------------------------
void printConfig(bool print_whole_apikey){
  Serial.print("\n-------- c o n f i g --------\n");
  Serial.printf("version: %s\n",VERSION);
  Serial.printf("box-opener configured: %s\n",config_set?"true":"false");
  Serial.printf("oracle server: %s\n",devnet?"devnet":"mainnet");
  Serial.printf("ssid: %s\npass: %s\n", ssid.c_str(), password.c_str());
  if (print_whole_apikey) Serial.printf("apikey: %s\n",apiKey.c_str());
  else Serial.printf("apikey: %s...%s\n", apiKey.substring(0,4),apiKey.substring(apiKey.length()-4));
  Serial.print("-----------------------------\n\n");
}

bool connect_to_wifi=false; // trigger to connect
void handleSerialComm(){
  String data = "";
  if (Serial.available()){
    data = Serial.readStringUntil('\n');
    data.trim();

    if (data == "help"){
      Serial.print( "\n------- h e l p -------\n");
      Serial.printf("Commands:\n");
      Serial.printf(" readconfig\n");
      Serial.printf(" ssid:YOUR_WIFI_SSID\n");
      Serial.printf(" pass:YOUR_WIFI_PASSWORD\n");
      Serial.printf(" apikey:YOUR_API_KEY\n");
      Serial.printf(" devnet\n");
      Serial.printf(" mainnet\n");
      Serial.printf(" writeconfig\n");
      Serial.printf(" delconfig\n");
      Serial.printf(" restart\n");
      Serial.printf(" ip\n");
      Serial.print( "-----------------------\n\n");
    }
    else if (data == "readconfig"){
      printConfig(false);
    }
    else if (data == "readconfigfull"){
      printConfig(true);
    }
    else if (data == "restart"){
      Serial.print("restarting in 3 seconds...\n");
      delay(3000);
      ESP.restart();
    }
    else if (data == "ip"){
      Serial.print("\nIP: ");
      Serial.println(WiFi.localIP()); 
    }
  #ifndef USE_HARDCODED_CREDENTIALS
    else if (data.startsWith("ssid:")){
      ssid = data.substring(5, data.length());
      Serial.printf("ssid set\n");
    }
    else if (data.startsWith("pass:")){
      password = data.substring(5, data.length());
      Serial.printf("pass set\n");
    }
    else if (data.startsWith("apikey:")){
      apiKey = data.substring(7, data.length());
      Serial.printf("apikey set\n");
    }
    else if (data == "mainnet"){
      devnet = false;
      Serial.printf("mainnet set\n");
    } 
    else if (data == "devnet"){
      devnet = true;
      Serial.printf("devnet set\n");
    }
    else if (data=="writeconfig"){
      config_set = true;
      saveConfig();
      connect_to_wifi = true;
      #ifdef T_DISPLAY_S3
        display_timer_val=0;
      #endif
      //Wifi_Connect();
    }
    else if (data=="delconfig"){
      config_set = false;
      if (LittleFS.remove(config_filename)) {
        Serial.println("Config file deleted successfully");
        #ifdef T_DISPLAY_S3
          showOnBar("Config file deleted successfully");
          setStatus(ST_NO_CONFIG);
          display_timer_val=0; // clear timer
        #endif
      } else {
        Serial.println("Failed to delete config file!");
        #ifdef T_DISPLAY_S3
          showOnBar("Failed to delete config file!");
        #endif
      }
    }
  #endif
    else{
      Serial.printf("Unknown command!\n");
    }
  }// serial available 
}
//-------------------------------------------------------------------



//--------------- TFT display functions ---------------------------------
#ifdef T_DISPLAY_S3
uint8_t act_page = 0;
void showPage(uint8_t page){ //prepares the screen for several display pages
  switch(page){
    case 0:
      break;
    case 1:
      break;
  }
}

#define RATIO_MAX 10
#define RATIO_OK 5

void calcRatio(uint8_t success){
  if (success) ratio++;
  else if (ratio) ratio--;

  if (ratio>RATIO_MAX) ratio = RATIO_MAX;
  uint16_t maxH = 146;
  uint16_t pr = ratio;
  uint16_t h = pr*maxH/RATIO_MAX;

  if (h<4) h=4;
  sprite_rr.fillRoundRect(2,2,12,maxH,2,TFT_BLACK); // clear inside
  if (ratio>RATIO_OK){ // "OK"
    sprite_rr.fillRoundRect(2,2+(maxH-h),12,h,2,C_BAR_OK); // OK bar color
  }
  else sprite_rr.fillRoundRect(2,2+(maxH-h),12,h,2,C_BAR_ERROR); // ERROR bar color

  sprite_rr.pushSprite(152,0);
}

// cat picture changes for given state
uint8_t accepted_table[8]={0,3,5,9,10,12,19,20}; // cat picture number
uint8_t accepred_ptr = 0;
#define ACCEPTED_MAX  7
uint8_t error_table[2]={8,24};
uint8_t error_ptr = 0;
#define ERROR_MAX  1

void setStatus(uint8_t status){
// 0 - INIT
// 1 - accepted
// 2 - rejected
// 3 - error
// 4 - charge me
// 5 - submitting
// 6 - no wifi
// 7 - no config
// sprite dimensions 150 x 30
  sprite_status.setTextDatum(MC_DATUM);
  sprite_status.loadFont(Dongle_Regular44);
  sprite_status.drawRoundRect(1,1,148,28,2,TFT_BLACK);
  switch (status){
    case 0:
      sprite_status.fillRoundRect(2,2,146,26,2,C_INIT_FRAME);  // frame
      sprite_status.fillRoundRect(4,4,142,22,2,C_INIT_BACKGROUND); // inside
      sprite_status.setTextColor(C_INIT_FONT);
      sprite_status.drawString("INIT",75,17,4);   
      showCat(1); 
      break;
    case 1: // "ACCEPTED"
      sprite_status.fillRoundRect(2,2,146,26,2,C_ACCEPTED_FRAME); // frame
      sprite_status.fillRoundRect(4,4,142,22,2,C_ACCEPTED_BACKGROUND); // inside
      sprite_status.setTextColor(C_ACCEPTED_FONT);
      sprite_status.drawString("ACCEPTED",75,17,4);
      showCat(accepted_table[accepred_ptr]);
      accepred_ptr++;
      if (accepred_ptr>ACCEPTED_MAX) accepred_ptr = 0;
      break;
    case 2: // "REJECTED"
      sprite_status.fillRoundRect(2,2,146,26,2,C_REJECTED_FRAME); // RED frame
      sprite_status.fillRoundRect(4,4,142,22,2,C_REJECTED_BACKGROUND); // inside
      sprite_status.setTextColor(C_REJECTED_FONT); // RED
      sprite_status.drawString("REJECTED",75,17,4);
      showCat(13);
      break;
    case 3: // "ERROR"
      sprite_status.fillRoundRect(2,2,146,26,2,C_ERROR_FRAME); // RED frame
      sprite_status.fillRoundRect(4,4,142,22,2,C_ERROR_BACKGROUND); // inside
      sprite_status.setTextColor(C_ERROR_FONT); // RED
      sprite_status.drawString("ERROR",75,17,4);
      showCat(error_table[error_ptr]);
      error_ptr++; if (error_ptr>ERROR_MAX) error_ptr = 0;
      break;
    case 4: // "!CHARGE!"
      sprite_status.fillRoundRect(2,2,146,26,2,C_CHARGE_FRAME); // frame
      sprite_status.fillRoundRect(4,4,142,22,2,C_CHARGE_BACKGROUND); // inside
      sprite_status.setTextColor(C_CHARGE_FONT);
      sprite_status.drawString("!CHARGE!",75,17,4);
      showCat(17);
      break;
    case 5: // "SUBMIT"
      sprite_status.fillRoundRect(2,2,146,26,2,C_SUBMIT_FRAME); // frame
      sprite_status.fillRoundRect(4,4,142,22,2,C_SUBMIT_BACKGROUND); // inside
      sprite_status.setTextColor(C_SUBMIT_FONT);
      sprite_status.drawString("SUBMIT",75,17,4);
      showCat(14);
      break;
    case 6: // "NO WIFI" connected
      sprite_status.fillRoundRect(2,2,146,26,2,C_NOWIFI_FRAME); // RED frame
      sprite_status.fillRoundRect(4,4,142,22,2,C_NOWIFI_BACKGROUND);// inside
      sprite_status.setTextColor(C_NOWIFI_FONT); // RED
      sprite_status.drawString("NO WIFI",75,17,4);
      showCat(16);
      break;      
    case 7: // "NO CONFIG"
      sprite_status.fillRoundRect(2,2,146,26,2,C_NOCONFIG_FRAME); // RED frame
      sprite_status.fillRoundRect(4,4,142,22,2,C_NOCONFIG_BACKGROUND);// inside
      sprite_status.setTextColor(C_NOCONFIG_FONT); // RED
      sprite_status.drawString("NOCONFIG",75,17,4);
      showCat(18);
    break;      
  }
  sprite_status.pushSprite(0,48);
};


void showTimeout(unsigned long useconds){
  char timer[7] = "--";
  if (useconds == -1 ){
    snprintf(timer,6,"...");
  }
  else if (useconds<=9999000){//9999s
    snprintf(timer,6,"%u",useconds/1000);
  }
  sprite_to.fillSprite(C_BACKGROUND); 
  sprite_to.drawString(timer,73,28,1); //middle
  sprite_to.pushSprite(2,98);
}

// bottom bar, 1-line console
void showOnBar(const char *txt){
  //sprite dimensions 320 x 19
  sprite_bar.fillSprite(C_LOG_BACKGROUND); // clear background
  sprite_bar.drawRect(0,0,320,19,C_NORMAL_BACKGROUND); // wywalić!!
  sprite_bar.setTextColor(C_TEXT,C_LOG_BACKGROUND);
  sprite_bar.drawString(txt,4,1,2); 
  sprite_bar.pushSprite(0,151);
}

// show cat picture
void showCat(uint8_t n){
  switch (n){
    case 0:
      sprite_kitty.pushImage(2,2,146,146,(uint16_t *)cat02);
      sprite_kitty.createPalette(cat02_palette, 16);
      break;
    case 1:
      sprite_kitty.pushImage(2,2,146,146,(uint16_t *)cat03);
      sprite_kitty.createPalette(cat03_palette, 16);
      break;
    case 2:
      sprite_kitty.pushImage(2,2,146,146,(uint16_t *)cat04);
      sprite_kitty.createPalette(cat04_palette, 16);
      break;
    case 3:
      sprite_kitty.pushImage(2,2,146,146,(uint16_t *)cat05);
      sprite_kitty.createPalette(cat05_palette, 16);
      break;
    case 4:
      sprite_kitty.pushImage(2,2,146,146,(uint16_t *)cat06);
      sprite_kitty.createPalette(cat06_palette, 16);
      break;
    case 5:
      sprite_kitty.pushImage(2,2,146,146,(uint16_t *)cat07);
      sprite_kitty.createPalette(cat07_palette, 16);
      break;
    case 6:
      sprite_kitty.pushImage(2,2,146,146,(uint16_t *)cat08);
      sprite_kitty.createPalette(cat08_palette, 16);
      break;
    case 7:
      sprite_kitty.pushImage(2,2,146,146,(uint16_t *)cat09);
      sprite_kitty.createPalette(cat09_palette, 16);
      break;
    case 8:
      sprite_kitty.pushImage(2,2,146,146,(uint16_t *)cat10);
      sprite_kitty.createPalette(cat10_palette, 16);
      break;
    case 9:
      sprite_kitty.pushImage(2,2,146,146,(uint16_t *)cat11);
      sprite_kitty.createPalette(cat11_palette, 16);
      break;
    case 10:
      sprite_kitty.pushImage(2,2,146,146,(uint16_t *)cat12);
      sprite_kitty.createPalette(cat12_palette, 16);
      break;
    case 11:
      sprite_kitty.pushImage(2,2,146,146,(uint16_t *)cat13);
      sprite_kitty.createPalette(cat13_palette, 16);
      break;
    case 12:
      sprite_kitty.pushImage(2,2,146,146,(uint16_t *)cat14);
      sprite_kitty.createPalette(cat14_palette, 16);
      break;
    case 13:
      sprite_kitty.pushImage(2,2,146,146,(uint16_t *)cat15);
      sprite_kitty.createPalette(cat15_palette, 16);
      break;
    case 14:
      sprite_kitty.pushImage(2,2,146,146,(uint16_t *)cat16);
      sprite_kitty.createPalette(cat16_palette, 16);
      break;
    case 15:
      sprite_kitty.pushImage(2,2,146,146,(uint16_t *)cat17);
      sprite_kitty.createPalette(cat17_palette, 16);
      break;
    case 16:
      sprite_kitty.pushImage(2,2,146,146,(uint16_t *)cat18);
      sprite_kitty.createPalette(cat18_palette, 16);
      break;
    case 17:
      sprite_kitty.pushImage(2,2,146,146,(uint16_t *)cat19);
      sprite_kitty.createPalette(cat19_palette, 16);
      break;
    case 18:
      sprite_kitty.pushImage(2,2,146,146,(uint16_t *)cat21);
      sprite_kitty.createPalette(cat21_palette, 16);
      break;
    case 19:
      sprite_kitty.pushImage(2,2,146,146,(uint16_t *)cat23);
      sprite_kitty.createPalette(cat23_palette, 16);
      break;
    case 20:
      sprite_kitty.pushImage(2,2,146,146,(uint16_t *)cat24);
      sprite_kitty.createPalette(cat24_palette, 16);
      break;
    case 21:
      sprite_kitty.pushImage(2,2,146,146,(uint16_t *)cat25);
      sprite_kitty.createPalette(cat25_palette, 16);
      break;
    case 22:
      sprite_kitty.pushImage(2,2,146,146,(uint16_t *)cat26);
      sprite_kitty.createPalette(cat26_palette, 16);
      break;
    case 23:
      sprite_kitty.pushImage(2,2,146,146,(uint16_t *)cat27);
      sprite_kitty.createPalette(cat27_palette, 16);
      break;
    case 24:
      sprite_kitty.pushImage(2,2,146,146,(uint16_t *)cat28);
      sprite_kitty.createPalette(cat28_palette, 16);
      break;
    default:
      sprite_kitty.pushImage(2,2,146,146,(uint16_t *)cat28);
      sprite_kitty.createPalette(cat28_palette, 16);
      break;
  }

    /* // show cat picture number
    char txt[5];
    snprintf(txt,5,"%02u",n);
    sprite_kitty.setTextColor(TFT_BLACK,C_NORMAL_BACKGROUND);
    sprite_kitty.drawString(txt,130,1,2);
    */
    sprite_kitty.drawRoundRect(1,1,148,148,2,C_KITTY_FRAME);
    sprite_kitty.pushSprite(170, 0);
}
#endif
//--------------- TFT display END ---------------------------------




void Wifi_Connect(){
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi ..");
  
  #ifdef T_DISPLAY_S3
    setStatus(ST_NO_WIFI);
    showOnBar("Connecting to WiFi ..");
  #endif
  
  while (WiFi.status() != WL_CONNECTED)
  {
    handleSerialComm(); // if wifi is wrongly configured, allow user to change config
    if (connect_to_wifi) return; // if user changed config during connecting -> try again in main loop
    Serial.print('.');
    delay(1000);
  }
  // print local IP
  Serial.print("\nIP: ");
  Serial.println(WiFi.localIP()); 
  String t = String (WiFi.localIP());

  #ifdef T_DISPLAY_S3
    // print IP on TFT
    tft.fillRect(25,1,120,18,C_LOG_BACKGROUND); // clear the IP field
    tft.setTextColor(C_TEXT,C_LOG_BACKGROUND);
    tft.drawString(WiFi.localIP().toString(), 25, 2, 2);
  #endif
}



//-------------------------------------------------------------------
//---------------- b o x  o p e n e r  c o r e :P -------------------

//===== generate table of 50 bip39 12-word mnemonics
void generateMnemonics(String *mnemonics)
{
  for (int i = 0; i < numGuesses; i++)
  {
    std::vector<uint8_t> entropy(16); // 128-bit entropy -> 12 words
    for (size_t j = 0; j < entropy.size(); j++)
    {
      entropy[j] = esp_random() & 0xFF; // extract 1 random byte
    }
    BIP39::word_list mnemonic = BIP39::create_mnemonic(entropy);
    std::string mnemonicStr = mnemonic.to_string();
    mnemonics[i] = String(mnemonicStr.c_str());
    // Serial.printf("Generated mnemonic: %s\n", mnemonics[i].c_str());
  }
}

//===== submit mnemonics to Oracle
bool submitGuesses(String *mnemonics, const String &apiUrl, const String &apiKey)
{
  bool ret = false;
  DynamicJsonDocument jsonDoc(8192); // max size of the json output, to verify! (4kB was not enough)

  char log_ch[50]="";
  String log = "";

  for (int i = 0; i < numGuesses; i++)
  {
    jsonDoc.add(mnemonics[i]);
  }

  Serial.printf("🔑️ Generated %u guesses\n", numGuesses);
  Serial.printf("➡️ Submitting to oracle\n");
  #ifdef T_DISPLAY_S3
    setStatus(ST_SUBMITTING);
    snprintf(log_ch,sizeof(log_ch),"[>] Submitting to oracle...");
    showOnBar(log_ch);
    showTimeout(-1);
  #endif

  String jsonString;
  serializeJson(jsonDoc, jsonString);

  HTTPClient http;
  http.begin(apiUrl + "/submit_guesses");
  http.addHeader("x-api-key", apiKey);
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(60000); // increase default 5s to 60s

  int httpResponseCode = http.POST(jsonString);
  
  if (httpResponseCode > 0)
  {
    String response = http.getString();
    //String t = response.substring(0,30);
    String t = response.substring(0,60);

    switch (httpResponseCode)
    {
      case 202:
        Serial.println("✅ Guesses accepted");
        snprintf(log_ch,sizeof(log_ch),"[V] Guesses accepted");
        #ifdef T_DISPLAY_S3
          setStatus(ST_ACCEPTED);
          calcRatio(1);
        #endif
        ret = false;
        break;
      case 401: //several errors "Invalid API Key", "Box Opener Not Charged!"...
        Serial.printf("❌ Guesses rejected (%d): %s\n", httpResponseCode, response.c_str());
        snprintf(log_ch,sizeof(log_ch),"[X] Rejected (%d): %s", httpResponseCode, response.c_str());
        if (t.startsWith("Box Opener Not Charged")) {// "Box Opener Not Charged!"
            Serial.printf("\n! CHARGE ME !\n\n");
            #ifdef T_DISPLAY_S3
              setStatus(ST_CHARGE);
            #endif
        }
        #ifdef T_DISPLAY_S3
          else {
              setStatus(ST_REJECTED);
          }
        calcRatio(0);
        #endif
        ret = false;
        break;
      case 404: // "Closed Box Not Found"
        Serial.printf("❌ Guesses rejected (%d): %s\n", httpResponseCode, response.c_str());
        snprintf(log_ch,sizeof(log_ch),"[X] Rejected (%d): %s", httpResponseCode, response.c_str());
        #ifdef T_DISPLAY_S3
          setStatus(ST_REJECTED);
          calcRatio(0);
        #endif
        ret = false;
        break;
      case 429: // "Guess Rate Limit Hit"
        Serial.printf("❌ Guesses rejected (%d): %s\n", httpResponseCode, response.c_str());
        snprintf(log_ch,sizeof(log_ch),"[X] Rejected (%d): %s", httpResponseCode, response.c_str());
        #ifdef T_DISPLAY_S3
          setStatus(ST_REJECTED);
          calcRatio(0);
        #endif
        ret = true;
        break;
      case 500: // "Internal Server Error"
        Serial.printf("❌ Guesses rejected (%d): Internal Server Error\n", httpResponseCode);
        snprintf(log_ch,sizeof(log_ch),"[X] Rejected (%d): Internal Server Error", httpResponseCode);
        #ifdef T_DISPLAY_S3
          setStatus(ST_ERROR);
          calcRatio(0);
        #endif
        ret = false;
        break;
      case 502: // "Bad Gateway"
        Serial.printf("❌ Guesses rejected (%d): Bad Gateway\n", httpResponseCode);
        snprintf(log_ch,sizeof(log_ch),"[X] Rejected (%d): Bad Gateway", httpResponseCode);
        #ifdef T_DISPLAY_S3
          setStatus(ST_REJECTED);
          calcRatio(0);
        #endif
        ret = false;
        break;
      case 530: // "Argo Tunnel Error"
        Serial.printf("❌ Guesses rejected (%d): Argo Tunnel Error\n", httpResponseCode);
        snprintf(log_ch,sizeof(log_ch),"[X] Rejected (%d): Argo Tunnel Error", httpResponseCode);
        #ifdef T_DISPLAY_S3
          setStatus(ST_ERROR);
          calcRatio(0);
        #endif
        ret = false;
        break;
      default: // other errors
        //Serial.printf("❌ Guesses rejected (%d): %s\n", httpResponseCode, t.c_str()); // show first 30 chars of response
        Serial.printf("❌ Guesses rejected (%d): Unspecified Error\n", httpResponseCode);
        snprintf(log_ch,sizeof(log_ch),"[X] Rejected (%d): Unspecified Error", httpResponseCode);
        #ifdef T_DISPLAY_S3
          setStatus(ST_ERROR);
          calcRatio(0);
        #endif
        ret = true;
        break;
    } // switch end
  }
  else // even more other errors :V maybe do a reconnect?
  {
    Serial.printf("❌ Error in HTTP request: %s\n", http.errorToString(httpResponseCode).c_str());
    snprintf(log_ch,sizeof(log_ch),"[X] Error in HTTP request");
    #ifdef T_DISPLAY_S3
      setStatus(ST_ERROR);
      calcRatio(0);
    #endif
    ret = true;
  }

  #ifdef T_DISPLAY_S3
    showOnBar(log_ch);
  #endif

  http.end();
  return ret;
}
//-------------------------------------------------------------------











//=============== i n i t i a l  S E T U P ====================================
void setup()
{
  Serial.setTimeout(10000); //10s for manual input
  Serial.setRxBufferSize(512); 
  Serial.setTxBufferSize(512);
  Serial.begin(115200);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);

  //delay(3000); // a bit of time to turn on serial terminal if you don't use the one in IDE



  #ifdef T_DISPLAY_S3
    tft.begin();
    tft.setRotation(3);
    // tft.setSwapBytes(true); // Make the picture color by RGB->BGRs
    tft.fillScreen(C_BACKGROUND);

    ledcAttachPin(TFT_BL, 1); // assign TFT_BL pin to channel 1
    //ledcSetup(1, 2000, 8);    // 2 kHz PWM, 8-bit resolution
    ledcSetup(1, 5000, 8);    // 5 kHz PWM, 8-bit resolution, less flicker on camera
    ledcWrite(1, 120);        // brightness 0 - 255

    //-- the cat
    sprite_kitty.setColorDepth(4);
    sprite_kitty.createSprite(150, 150);
    sprite_kitty.setSwapBytes(true);
    sprite_kitty.fillSprite(C_TIMER); 
    sprite_kitty.drawRoundRect(1,1,148,148,2,C_KITTY_FRAME);
    sprite_kitty.pushSprite(170, 0);
    showCat(1); 

    //-- bottom status bar, 1-line console
    sprite_bar.createSprite(320, 19);
    sprite_bar.fillSprite(C_LOG_BACKGROUND);
    sprite_bar.drawRect(0,0,320,19,C_NORMAL_BACKGROUND);
    sprite_bar.pushSprite(0,151);

    //-- IP section
    tft.drawRoundRect(0,0,150,20,2,C_CONFIG_TABLE);
    tft.fillRect(1,1,148,18,C_LOG_BACKGROUND);
    tft.drawFastVLine(20,0,20,C_CONFIG_TABLE); // separates "IP:" from its value
    tft.setTextColor(C_TEXT);
    tft.drawString("IP: ", 3, 2, 2);
    tft.drawString("xxx.xxx.xxx.xxx", 25, 2, 2);

    //-- main status
    tft.fillRoundRect(0,23,150,55,1,C_NORMAL_BACKGROUND); // h=25+30
    tft.setTextColor(C_BLACK,C_NORMAL_BACKGROUND,true);// 36,20,0
    tft.loadFont(Dongle_Regular44);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("STATUS", 75, 38, 1);
    tft.setTextDatum(TL_DATUM);
    tft.unloadFont();
    sprite_status.createSprite(150, 30); // status
    sprite_status.fillSprite(C_NORMAL_BACKGROUND); 
    sprite_status.drawPixel(0,30,C_BACKGROUND); // cut corners
    sprite_status.drawPixel(150,30,C_BACKGROUND); // cut corners

    //-- timer
    tft.fillRoundRect(0,80,150,70,1,C_NORMAL_BACKGROUND);
    tft.loadFont(Dongle_Regular24);
    tft.setTextColor(C_BLACK,C_NORMAL_BACKGROUND,true);
    tft.drawString("next guess in...", 2, 83, 2);
    tft.unloadFont();
    sprite_to.createSprite(146, 50); // timer
    sprite_to.fillSprite(TFT_BLACK); 
    sprite_to.setTextDatum(MC_DATUM);
    sprite_to.loadFont(Dongle_Regular72);
    sprite_to.setTextColor(C_TIMER);
    sprite_to.drawString("--",75,28,1); //'middle'
    sprite_to.pushSprite(2,98);

    //-- rejection ratio vertical progress bar
    sprite_rr.createSprite(18, 150);
    sprite_rr.fillRect(0,0,18,150,C_BACKGROUND);        //clear sprite
    sprite_rr.drawRoundRect(0,0,16,150,3,C_BAR_OUTLINE);//outline
    sprite_rr.drawRoundRect(1,1,14,148,3,C_BACKGROUND); //inside black frame

    setStatus(0); //"INIT"
    showOnBar("== Box-opener started ==");
    calcRatio(1); // show ratio bar on 100%

    /* // testmode, show all cats, statuses and change rejectin ratio bar
    unsigned char i=0;
    unsigned char j=0;
    while(1){
      setStatus(i);
      i++;
      if (i>7) i=0;

      calcRatio(0);
      if (ratio==0) ratio = 10;

      showCat(j);
      j++;
      if(j>24) j=0;

      showTimeout(j*1000);
      delay(1000);
    }
    //*/

#endif

  Serial.printf("===================\n");
  Serial.printf("Box-opener starting\n");

/*
   _________
  /        /|
 /________/ |
|  /\_/\  | |
| ( o.o ) | |
|  > ^ <  | /
|_________|/
*/
  Serial.printf("   _________\n");
  Serial.printf("  /        /|\n");
  Serial.printf(" /________/ |\n");
  Serial.printf("|  /\\_/\\  | |\n");
  Serial.printf("| ( o.o ) | |\n");
  Serial.printf("|  > ^ <  | /\n");
  Serial.printf("|_________|/\n");
  Serial.printf("\n");


#ifdef USE_HARDCODED_CREDENTIALS
  Serial.println("REMEMBER TO SET YOUR CREDENTIALS IN CODE");
  config_set = true;
#else
  Serial.println("\nReading config...\n\n");

  // Mount LITTLEFS and read config file
  if (!LittleFS.begin(false)) {
    Serial.println("LITTLEFS Mount failed");
    Serial.println("Did not find filesystem; starting format");
    // format if begin fails
    if (!LittleFS.begin(true)) {
      Serial.println("LITTLEFS mount failed");
      Serial.println("Formatting not possible");
      return;
    } else {
      Serial.println("Formatting..");
      Serial.println("restart in 5s");
      #ifdef T_DISPLAY_S3
      showOnBar("Formatting FS. Reboot in 5s...");
      #endif
      delay(5000);
      ESP.restart();
    }
  }
  else{
    Serial.println("LITTLEFS mounted successfully");
    if(readConfig() == false) {
      Serial.println("Could not read config file -> initializing new file");
      if (saveConfig()) {
        Serial.println("Config file saved");
      }   
    }
    else{
      Serial.println("Config file read succesfully.");
    }
  }
#endif

  printConfig(false);

  if (config_set) Wifi_Connect();
  #ifdef T_DISPLAY_S3
  else {
    setStatus(ST_NO_CONFIG);
    showOnBar("Configure me! type 'help' in terminal");
  }
  #endif
  Serial.printf("Box-opener started\n");
  Serial.printf("==================\n");
  
}



//============================ main loop =======================================================
uint8_t aa=0;

void loop()
{
  
  handleSerialComm();// serial setup
  if (connect_to_wifi){
    connect_to_wifi = false;
    Wifi_Connect();
  }



  t1 = millis();
  if (t1 >= t2){ // time is up
    if (config_set) box_opener_sleeping = false;
    else{
      //t2 = (long)(millis()+15000); // show warning every 15s
      t2 = (long)(millis()+2000); // show warning every 15s
      Serial.printf("\n! WARNING box-opener not configured ! type: help\n");

      #ifdef T_DISPLAY_S3
      // -- blink the status bar at the bottom
      aa++;
      if (aa & 1){
            //negative
            sprite_bar.fillSprite(TFT_BLACK); 
            sprite_bar.drawRoundRect(1,1,318,18,2,C_RED);
            sprite_bar.setTextColor(C_RED);
            sprite_bar.setTextDatum(MC_DATUM);
            sprite_bar.drawString("! BOX-OPENER NOT CONFIGURED !",159,10,2); // pozycja w ramach spritea
            sprite_bar.setTextDatum(TL_DATUM);
            sprite_bar.pushSprite(0,151); // a to gdzie ten sprite jest
      }
      else{ // positive
            sprite_bar.fillSprite(C_RED); // tło komunikatu ważnego, 148, 127,75 
            sprite_bar.drawRoundRect(1,1,318,18,2,TFT_BLACK);
            sprite_bar.setTextColor(TFT_BLACK);// 36,20,0
            sprite_bar.setTextDatum(MC_DATUM);
            sprite_bar.drawString("! BOX-OPENER NOT CONFIGURED !",159,10,2); // pozycja w ramach spritea
            sprite_bar.setTextDatum(TL_DATUM);
            sprite_bar.pushSprite(0,151); // a to gdzie ten sprite jest
      }
      //---
      #endif
    }
  }

  #ifdef T_DISPLAY_S3
  if (t1 >= t3){
    t3 = (long)(millis()+1000);
    if (display_timer_val>1000) display_timer_val-=1000;
    else display_timer_val=0;
    showTimeout(display_timer_val);
  }
  #endif

  if ((box_opener_sleeping == false) && (config_set)){
    //--- reconnect wifi if it is not connected for some reason
    if (WiFi.status() != WL_CONNECTED)
    {
      Serial.print("WiFi disconnected, trying to reconnect..\n");
      WiFi.disconnect();
      WiFi.reconnect();
    }
      
    Serial.println("⚙️ Generating guesses...");

    generateMnemonics(mnemonics);

    bool rateLimited = false;
    if (devnet) rateLimited = submitGuesses(mnemonics, apiUrlDevnet, apiKey);
    else rateLimited = submitGuesses(mnemonics, apiUrlMainnet, apiKey);

    if (rateLimited)
    {
      sleepTime += 10000;
    }
    else
    {
      sleepTime -= 1000;
    }

    if (sleepTime < 10000)
    {
      sleepTime = 10000;
    }
    if (sleepTime > 60000) // if sleep for more than a minute limit it to one minute
    {
      sleepTime = 60000;
    }

    Serial.printf("waiting %is for next batch...\n", sleepTime/1000);
    t2 = millis(); // get current timestamp
    t2 +=sleepTime; // set next timestamp for sending guesses
    #ifdef T_DISPLAY_S3
      t3 = t1+1000; // set 1s timer for counting down
      display_timer_val = sleepTime; // show time to sleep
      showTimeout(display_timer_val);
    #endif
    box_opener_sleeping = true;
  }

  delay(10);
}

