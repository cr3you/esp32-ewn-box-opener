/*
 * ------------------------------------------------------------------------
 * Project Name: esp32-ewn-box-opener
 * Description: Esp32 port of the Box Opener client for mining EWN tokens.
 * Author: Crey
 * Repository: https://github.com/cr3you/esp32-ewn-box-opener/
 * Date: 2024.10.04 
 * Version: 1.0.1
 * License: MIT
 * ------------------------------------------------------------------------
 */

#include "bip39/bip39.h"
#include <Arduino.h>
#include "WiFi.h"
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <SPI.h>
#include <TFT_eSPI.h>

TFT_eSPI    tft = TFT_eSPI();

//=====wifi setup
const char *ssid = ""; // <---------------------- SET THIS !!!
const char *password = ""; // <-------------- SET THIS !!!

//=====Box Opener client setup
const char *apiUrl = "https://api.erwin.lol/"; // mainnet
//const char *apiUrl = "https://devnet-api.erwin.lol/"; // devnet
const char *apiKey = ""; // <---------------------- SET THIS !!!


const int numGuesses = 50;
String mnemonics[numGuesses]; // bip39 mnemonic table
int sleepTime = 10000; // default sleep time in ms
int dataS = 0; //succesful data count
int dataF = 0; //failed data count
int dataT = 0; //Timeout count


void setup()
{
  tft.init();

  tft.setRotation(1);

  tft.fillScreen(TFT_BLACK);
  
  // Set "cursor" at top left corner of display (0,0) and select font 4
  tft.setCursor(0, 0, 2);

  Serial.begin(115200);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi ..");
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print('.');
    delay(1000);
  }
  Serial.println();
  Serial.println(WiFi.localIP()); // print local IP

  Serial.printf("===============\n");
  Serial.printf("Box-opener started\n");

}

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
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.println("Generated guesses");
  Serial.printf("➡️ Submitting to oracle\n");
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.println("Submitting to oracle");

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
    if (httpResponseCode == 202)
    {
      Serial.println("✅ Guesses accepted");
      tft.setTextColor(TFT_GREEN, TFT_BLACK);
      tft.println("Guesses accepted");
      dataS ++;
      ret = false;
    }
    else if (httpResponseCode == 404) // "Closed Box Not Found"
    {
      Serial.printf("❌ Guesses rejected (%d): %s\n", httpResponseCode, response.c_str());
      tft.setTextColor(TFT_RED, TFT_BLACK);
      tft.println("Guesses rejected");
      tft.println("Box Not Found");
      dataF ++;
      ret = false;
    }
    else // other errors
    {
      Serial.printf("❌ Guesses rejected (%d): %s\n", httpResponseCode, response.c_str());
      tft.setTextColor(TFT_RED, TFT_BLACK);
      tft.println("Guesses rejected");
      tft.println(response.c_str());
      dataF ++;
      ret = true;
    }
  }
  else // even more other errors :V maybe do a reconnect?
  {
    Serial.printf("❌ Error in HTTP request: %s\n", http.errorToString(httpResponseCode).c_str());
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.println("Read Timeout");
    dataT ++;
    ret = true;
  }

  http.end();
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.print(dataS);
  tft.print("S  ");
  tft.setTextColor(TFT_RED, TFT_BLACK);
  tft.print(dataF);
  tft.print("F  ");
  tft.print(dataT);
  tft.print("T");
  return ret;
}

//====== main loop ====
void loop()
{
  
  tft.fillScreen(TFT_BLACK);// Set "cursor" at top left corner of display (0,0) and select font 4
  tft.setCursor(0, 0, 4);
  //--- reconnect wifi if it is not connected by some reason
  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.print("WiFi disconnected, trying to reconnect..\n");
    WiFi.disconnect();
    WiFi.reconnect();
  }
    
  Serial.println("⚙️ Generating guesses...");

  generateMnemonics(mnemonics);

  bool rateLimited = submitGuesses(mnemonics, apiUrl, apiKey);

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
  delay(sleepTime);
}
