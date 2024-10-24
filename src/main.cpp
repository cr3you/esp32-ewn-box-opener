/*
 * ------------------------------------------------------------------------
 * Project Name: esp32-ewn-box-opener
 * Description: Esp32 port of the Box Opener client for mining EWN tokens.
 * Author: Crey
 * Repository: https://github.com/cr3you/esp32-ewn-box-opener/
 * Date: 2024.10.24 
 * Version: 1.1.1
 * License: MIT
 * ------------------------------------------------------------------------
 */
#define VERSION "1.1.1"
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
      //Wifi_Connect();
    }
    else if (data=="delconfig"){
      config_set = false;
      if (LittleFS.remove(config_filename)) {
        Serial.println("Config file deleted successfully");
      } else {
        Serial.println("Failed to delete config file!");
      }
    }
  #endif
    else{
      Serial.printf("Unknown command!\n");
    }
  }// serial available 
}
//-------------------------------------------------------------------



void Wifi_Connect(){
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi ..");
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

  for (int i = 0; i < numGuesses; i++)
  {
    jsonDoc.add(mnemonics[i]);
  }

  Serial.printf("🔑️ Generated %u guesses\n", numGuesses);
  Serial.printf("➡️ Submitting to oracle\n");

  String jsonString;
  serializeJson(jsonDoc, jsonString);

  HTTPClient http;
  http.begin(apiUrl + "/submit_guesses");
  http.addHeader("x-api-key", apiKey);
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(15000); // increase default 5s to 15s

  int httpResponseCode = http.POST(jsonString);


  if (httpResponseCode > 0)
  {
    String response = http.getString();
    String t = response.substring(0,30);
    switch (httpResponseCode)
    {
      case 202:
        Serial.println("✅ Guesses accepted");
        ret = false;
        break;
      case 401: //several errors "Invalid API Key", "Box Opener Not Charged!"...
        Serial.printf("❌ Guesses rejected (%d): %s\n", httpResponseCode, response.c_str());
        if (t.startsWith("Box Opener Not Charged")) {// "Box Opener Not Charged!"
            Serial.printf("\n! CHARGE ME !\n\n");
        }
        ret = false;
        break;
      case 404: // "Closed Box Not Found"
        Serial.printf("❌ Guesses rejected (%d): %s\n", httpResponseCode, response.c_str());
        ret = false;
        break;
      case 500: // "Internal Server Error"
        Serial.printf("❌ Guesses rejected (%d): Internal Server Error\n", httpResponseCode);
        ret = false;
        break;
      case 502: // "Bad Gateway"
        Serial.printf("❌ Guesses rejected (%d): Bad Gateway\n", httpResponseCode);
        ret = false;
        break;
      case 530: // "Argo Tunnel Error"
        Serial.printf("❌ Guesses rejected (%d): Argo Tunnel Error\n", httpResponseCode);
        ret = false;
        break;
      default: // other errors
        //Serial.printf("❌ Guesses rejected (%d): %s\n", httpResponseCode, t.c_str()); // show first 30 chars of response
        Serial.printf("❌ Guesses rejected (%d): Unspecified Error\n", httpResponseCode);
        ret = true;
        break;
    } // switch end
  }
  else // even more other errors :V maybe do a reconnect?
  {
    Serial.printf("❌ Error in HTTP request: %s\n", http.errorToString(httpResponseCode).c_str());
    ret = true;
  }

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

  delay(3000); // a bit of time to turn on serial terminal if you don't use the one in IDE

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
      Serial.println("restart in 20s");
      delay(20000);
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
 
  Serial.printf("Box-opener started\n");
  Serial.printf("==================\n");
  
}



//============================ main loop =======================================================
unsigned long t1=0;
unsigned long t2=0;
bool box_opener_sleeping = false;

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
      t2 = (long)(millis()+15000); // show warning every 15s
      Serial.printf("\n! WARNING box-opener not configured ! type: help\n");
    }
  }

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
    t2 = (long)(millis()+sleepTime); // set next timestamp
    box_opener_sleeping = true;
    //delay(sleepTime);
  }

  delay(10);

}

