#include <Arduino.h>
#include <WiFi.h>
#include <ESP32Servo.h>
#include "DHTesp.h"
#include <FirebaseESP32.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

#define WIFI_SSID "Redmi 9"
#define WIFI_PASSWORD "pppppppp"

#define FIREBASE_AUTH "YRW2Reo4174TgB49Oj4p7mBldGAbq0wr6mRmhgEW" 
#define FIREBASE_HOST "https://aplikasi-jemuran-iot-default-rtdb.asia-southeast1.firebasedatabase.app/" 

const char* timeAPI = "http://worldtimeapi.org/api/timezone/Asia/Jakarta";
const int LDR = 13;
int HUJAN = 14;
int DHT_PIN = 15;

int pos = 0;
int value = 0;
bool buka = true;
String cuaca = "";
String kontrolmode = "";

unsigned long previousMillis = 0;
unsigned long interval = 30000;

DHTesp dhtSensor;
Servo servo;

FirebaseData firebaseData;

void setup(){
  Serial.begin(9600);
  dhtSensor.setup(DHT_PIN, DHTesp::DHT22);
  pinMode(LDR, INPUT);
  pinMode(HUJAN, INPUT);
  servo.attach(32, 500, 2400);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED){
    Serial.print(".");
    delay(300);
  }
  Serial.println();
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());
  Serial.println();

  Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);
  Firebase.reconnectWiFi(true);
}

void loop(){

    unsigned long currentMillis = millis();
    // Jika WiFi mati, menyambung ulang otomatis, aktifkan mode atap otomatis
    if ((WiFi.status() != WL_CONNECTED) && (currentMillis - previousMillis >=interval)) {
      Serial.print(millis());
      Serial.println("Reconnecting to WiFi...");
      WiFi.disconnect();
      WiFi.reconnect();
      previousMillis = currentMillis;

      if (((cuaca == "Mendung") || (cuaca == "Hujan")) && buka){
          tutupAtap();
        } else if(((cuaca == "Cerah") || (cuaca == "Berawan")) && !buka){
          bukaAtap();
        }
    }
    
    TempAndHumidity dhtData = dhtSensor.getTempAndHumidity(); 
    String basah = "";
    String temp = "";
    String cahaya = "";
  
    if (digitalRead(13) == 0){
      cahaya = "cerah";
    }else{
      cahaya = "gelap";
    }
    if (dhtData.temperature >= 30){
      temp = "panas";
    }else{
      temp = "dingin";
    }
    if(digitalRead(HUJAN) == 0){
      basah = "ya";
    }else{
      basah = "tidak";
    }
    lookUpTableFuzzy(basah, cahaya, temp);
    Serial.println(cuaca);
  
    Firebase.setStringAsync(firebaseData, "/Sensor/data/suhu", dhtData.temperature);
    Firebase.setStringAsync(firebaseData, "/Sensor/data/kelembapan", dhtData.humidity);
    
    if (Firebase.setString(firebaseData, "/Sensor/data/cuaca", cuaca))
      {
        Serial.println("Berhasil Kirim Cuaca");
      }
      else
      {
        Serial.println("Gagal");
        Serial.println("Keterangan gagal : " + firebaseData.errorReason());
      }
  
  
    if (Firebase.getString(firebaseData, "/kontrol/mode")){
      Serial.println(firebaseData.stringData());
      kontrolmode = firebaseData.stringData();
      Serial.println(kontrolmode);
  
      if (kontrolmode == "otomatis") {
        
        if (((cuaca == "Mendung") || (cuaca == "Hujan")) && buka){
          tutupAtap();
          sendNotifToFirebase("Cuaca mendung/hujan, atap tertutup");
          addLogData("Cuaca mendung/hujan, atap tertutup");
        } else if(((cuaca == "Cerah") || (cuaca == "Berawan")) && !buka){
          bukaAtap();
          sendNotifToFirebase("Cuaca cerah, atap terbuka");
          addLogData("Cuaca cerah, atap terbuka");
        }
      }
      else if (kontrolmode == "buka atap" && !buka){ 
        bukaAtap();
      }
      else if (kontrolmode == "tutup atap" && buka){
        tutupAtap();
      }
    }
    Firebase.setIntAsync(firebaseData, "/status/active", rand());
    delay(3000);
  
}
  

void bukaAtap(){
    for (pos = 90; pos >= 0; pos -= 1) {
    servo.write(pos);
    delay(15);
  }
  buka = true;
    if (Firebase.setString(firebaseData, "/status/atap", "terbuka"))
    {
      Serial.println("Berhasil Kirim Data");
    }
    else
    {
      Serial.println("Gagal");
      Serial.println("Keterangan gagal : " + firebaseData.errorReason());
      Serial.println("------------------------------------");
      Serial.println();
    }
//   sendNotifToFirebase("Cuaca cerah, atap terbuka");
}

void tutupAtap(){
  for (pos = 0; pos <= 90; pos += 1) {
    servo.write(pos);
    delay(15);
  }
  buka = false;
    if (Firebase.setString(firebaseData, "/status/atap", "tertutup"))
    {
      Serial.println("Berhasil Kirim Data");
    }
    else
    {
      Serial.println("Gagal");
      Serial.println("Keterangan gagal : " + firebaseData.errorReason());
      Serial.println("------------------------------------");
      Serial.println();
    }
  
}

void sendNotifToFirebase(String message) {
  HTTPClient http;
  String response;

  String url = "https://fcm.googleapis.com/fcm/send";

  String data = "{" ;
  data = data + "\"to\": \"eLMp4LhRS3eE6cUdacfzBl:APA91bF0v1inKTXjmsryXcdc5ertHytMmvPYvsAWHCjxhtpaZwUWcRS6PKDlmLohUzSc02hJp8HTOTCqnKfVfCqtK_BaDd4BssyrXQU5iK-GP9DWofNfcjUmHyYLZe8q2F_ztq6u3Q3-\"," ;
  data = data + "\"notification\": {" ; 
  data = data + "\"body\":\"" + message +"\"," ;
  data = data + "\"title\" : \"Sistem Jemuran\", \"subtitle\" : \"Sistem Jemuran\", \"sound\" : \"default\"" ; 
  data = data + "} }" ;

  Serial.println(data);

  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", "key=AAAAJCbtQ1M:APA91bE0uYA4xteDo_YmYBcT1d1yXqjlbMrB_YyY3viYQacy6p4yGwQMH3-ORd-WvgXt20s3v4YOZj0tt6EfjlXkPJxmjPuiOXmgXBfLnG7OoS6TReXufKFE1X9gen51vAOBRT-_8xiH");
  
  http.POST(data);
  response = http.getString();
  Serial.println(response);

}

String getTime() {
  HTTPClient http;
  http.useHTTP10(true);
  http.begin(timeAPI);
  http.GET();
  String result = http.getString();

  DynamicJsonDocument doc(2048);
  DeserializationError error = deserializeJson(doc, result);

  // Test if parsing succeeds.
  if (error) {
    Serial.print("deserializeJson() failed: ");
    Serial.println(error.c_str());
  }

  String timenow = doc["datetime"].as<String>();
  http.end();
  String valid_time = timenow.substring(0, 10) + ", (" + timenow.substring(11, 16) + " WIB)"; 
  Serial.println(valid_time);
  return valid_time;
}

void addLogData(String message) {
  HTTPClient http;
  String response;

  String url = "https://aplikasi-jemuran-iot-default-rtdb.asia-southeast1.firebasedatabase.app/log_data.json";

  String data = "{\"data\": \"" + message + "\",\"time\": \"" + getTime() + "\"}";

  Serial.println(data);

  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  
  http.POST(data);
  response = http.getString();
  Serial.println(response);

}


void lookUpTableFuzzy(String basah, String cahaya, String temperature){

  if(basah == "ya" && cahaya == "gelap" && temperature == "dingin"){
    cuaca = "Hujan";
  }else if(basah == "ya" && cahaya == "gelap" && temperature == "panas"){
    cuaca = "Hujan";
  }else if(basah == "ya" && cahaya == "cerah" && temperature == "dingin"){
    cuaca = "Hujan";
  }else if(basah == "ya" && cahaya == "cerah" && temperature == "panas"){
    cuaca = "Hujan";
  }else if(basah == "tidak" && cahaya == "gelap" && temperature == "dingin"){
    cuaca = "Mendung";
  }else if(basah == "tidak" && cahaya == "gelap" && temperature == "panas"){
    cuaca = "Berawan";
  }else if(basah == "tidak" && cahaya == "cerah" && temperature == "dingin"){
    cuaca = "Cerah";
  }else if(basah == "tidak" && cahaya == "cerah" && temperature == "panas"){
    cuaca = "Cerah";
  }
}
