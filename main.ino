#include <ESP8266WiFi.h>                                        //引入Wi-Fi函式庫
#include <WiFiClientSecure.h>                                   //為了建立安全連線
#include <PubSubClient.h>                                   //為了建立MQTT client
#include <ArduinoJson.h>                                       //為了轉換JSON格式
#include <time.h>                                          //時間相關的標準c函式庫
#include <Wire.h>                                               //為了建立I2C通訊
#include <Adafruit_INA219.h>                                       //INA219函式庫
#include <DHT.h>                                                    //DHT11函式庫
#include "Secrets.h"                            //包含SSID、密碼、Thingname、Topic
//                                                   、endpoint、憑證與私鑰的標頭檔

const int leftEncoderPin = 14;                                               //D5
const int rightEncoderPin = 12;                                              //D6
volatile int leftPulseCount = 0;                                    //光遮斷器計數
volatile int rightPulseCount = 0;
const int pulsesPerRevolution = 20;                               //每轉有20次遮斷

float humidity;
float temperature;
float shuntVoltage;
float busVoltage;
float current_mA;
float loadVoltage;
float power_mW;
float leftRpm;
float rightRpm;

DHT dht(0, DHT11);                                                  //建立物件dht

Adafruit_INA219 ina219;                                          //建立物件ina219

unsigned long lastUpdateTime = 0;
unsigned long lastPublishTime = 0;
const long updateInterval = 2000;                               //數據更新間隔(ms)
const long publishInterval = 5000;                              //發佈數據間隔(ms)

void ICACHE_RAM_ATTR countLeftPulses(){                       //光遮斷次數計數函式
  leftPulseCount++;
}
void ICACHE_RAM_ATTR countRightPulses(){
  rightPulseCount++;
}


WiFiClientSecure net;                                         //建立安全連線物件net
BearSSL::X509List cert(cacert);                                       //伺服器憑證
BearSSL::X509List client_crt(client_cert);                            //客戶端憑證
BearSSL::PrivateKey key(privkey);                                          //私鑰
PubSubClient client(net);

                                                      //顯示接收到的Topic與payload
void messageReceived(char *topic, byte *payload, unsigned int length) {
  Serial.print("Received [");
  Serial.print(topic);
  Serial.print("]: ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
}

void connectAWS() {                                      //連線至Wi-Fi網路與AWS IoT
  delay(3000);
  WiFi.mode(WIFI_STA);                                   //將ESP8266設為station模式
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);                           //連線至Wi-Fi網路

  Serial.println(("Connecting to SSID: ") + String(WIFI_SSID));

  while (WiFi.status() != WL_CONNECTED) {                                //等待連線
    delay(1000);
  }

  configTime(TIME_ZONE * 3600, 0, "pool.ntp.org", "time.nist.gov");      //同步時間

  net.setTrustAnchors(&cert);                                       //設定憑證與私鑰
  net.setClientRSACert(&client_crt, &key);

  client.setServer(MQTT_HOST, 8883);                           //設定endpoint與port
  client.setCallback(messageReceived);           //接收到訊息時 呼叫messageReceived()

  Serial.println("Connecting to AWS IoT");

  while (!client.connect(THINGNAME)) {                                //等待MQTT連線
    delay(1000);
  }

  if (!client.connected()) {                                          //檢查連線狀態
    Serial.println("AWS IoT Timeout!");
    return;
  }

  
  client.subscribe(AWS_IOT_SUBSCRIBE_TOPIC);                              //訂閱主題

  Serial.println("AWS IoT Connected!\n\n");
  delay(2000);
}

void publishMessage() {                                         //發佈訊息至 AWS IoT
  time_t currentTime = time(nullptr);                                     //取得時間

  StaticJsonDocument<200> doc;                                     //建立JSON物件doc
  doc["time"] = currentTime;                                         //將數據存入doc
  doc["humidity"] = humidity;                                    //doc["Key"]=value
  doc["temperature"] = temperature;
  doc["leftRpm"] = leftRpm;
  doc["righRpm"] = rightRpm;
  doc["busVoltage"] = round(busVoltage * 100.0) / 100.0;
  doc["shuntVoltage"] = round(shuntVoltage * 100.0) / 100.0;
  doc["loadVoltage"] = round(loadVoltage * 100.0) / 100.0;
  doc["current"] = round(current_mA * 100.0) / 100.0;
  doc["power"] = round(power_mW * 100.0) / 100.0;

  char jsonBuffer[512];              
  serializeJson(doc, jsonBuffer);               //轉為字串{"Key":value;}供AWS IoT處理
  client.publish(AWS_IOT_PUBLISH_TOPIC, jsonBuffer);                   //發佈至Topic
}

void setup() {
  os_update_cpu_frequency(160);                                 //設置CPU頻率為160Mhz
  delay(2000);
  Serial.begin(115200);
  Serial.println();
  connectAWS();                                             //連線至Wi-Fi網路與AWS IoT
                                                           //使用外部中斷檢測光遮斷次數
  attachInterrupt(digitalPinToInterrupt(leftEncoderPin), countLeftPulses, FALLING);
  attachInterrupt(digitalPinToInterrupt(rightEncoderPin), countRightPulses, FALLING);

  ina219.begin();                                                       //初始化ina219
  ina219.setCalibration_32V_1A();                                           //設定量程
  dht.begin();                                                             //初始化dht
}


void loop() {

  if (millis() - lastUpdateTime >= updateInterval) {          //每2000ms更新感測器數據
    lastUpdateTime = millis();
    
    humidity = dht.readHumidity();                                             //濕度
    temperature = dht.readTemperature();                                       //溫度

                                                                            //計算RPM
    leftRpm = (leftPulseCount * 60.0) / pulsesPerRevolution / updateInterval * 1000;
    rightRpm = (rightPulseCount * 60.0) / pulsesPerRevolution / updateInterval * 1000;
    leftPulseCount = 0;
    rightPulseCount = 0;

    shuntVoltage = ina219.getShuntVoltage_mV();                           //ina219數據
    busVoltage = ina219.getBusVoltage_V();
    current_mA = ina219.getCurrent_mA();
    power_mW = ina219.getPower_mW();
    loadVoltage = busVoltage + (shuntVoltage / 1000);

                                                       //顯示感測器數據於Serial Monitor
    String output = "Humidity:      " + String(humidity) +      " %\n"
                    "Temperature:   " + String(temperature) +   " °C\n\n"
                    "Left RPM:      " + String(leftRpm) +       "\n"
                    "Right RPM:     " + String(rightRpm) +      "\n\n"
                    "Bus Voltage:   " + String(busVoltage) +    " V\n"
                    "Shunt Voltage: " + String(shuntVoltage) +  " mV\n"
                    "Load Voltage:  " + String(loadVoltage) +   " V\n"
                    "Current:       " + String(current_mA) +    " mA\n"
                    "Power:         " + String(power_mW) +      " mW\n"
                    " \n--------------------\n";

    Serial.println(output);

  }


  if (millis() - lastPublishTime >= publishInterval) {        //每5000ms發佈至AWS IoT
    lastPublishTime = millis();
    if (!client.connected()) {
      connectAWS();
    } else {
      client.loop();
      publishMessage();
    }
  }
}
