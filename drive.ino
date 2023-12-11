#include <SparkFun_TB6612.h>                      //引入TB6612FNG函式庫
#include <SoftwareSerial.h>                  //引入SoftwareSerial函式庫

const int AIN1 = 16;                                              //D0
const int AIN2 = 4;                                               //D2
const int BIN1 = 0;                                               //D3
const int BIN2 = 2;                                               //D4
const int PWMA = 14;                                              //D5
const int PWMB = 12;                                              //D6
const int STBY = 5;                                               //D1

const int offsetA = 1;                                 //定義通道A正反轉
const int offsetB = 1;                                 //定義通道B正反轉
char status;                                          //儲存接收到的字元
int speed = 120;                                        //馬達轉速0~255
int mappedSpeed = map(speed, 0, 255, 0, -255);            //Remap為負值

SoftwareSerial BT(13, 15);                         //D7, D8 設為 RX, TX

Motor motorA = Motor(AIN1, AIN2, PWMA, offsetA, STBY); //建立物件motorA
Motor motorB = Motor(BIN1, BIN2, PWMB, offsetB, STBY); //建立物件motorB

void setup(){
  os_update_cpu_frequency(80);                     //設置CPU頻率為80MHz
  pinMode(STBY, OUTPUT);
  digitalWrite(STBY, LOW);                 //將STBY設為LOW避免開機誤動作
  BT.begin(9600);                                     //Baud Rate 9600
}

void loop(){
  if (BT.available()) {
    status = BT.read();
  }

  switch (status){                          //根據接收到字元執行對應動作
    case 'F':
      forward(motorA, motorB, speed);
      status = 'N';
      break;
    case 'B':
      back(motorA, motorB, mappedSpeed);
      status = 'N';
      break;
    case 'R':
      right(motorA, motorB, speed);
      status = 'N';
      break;
    case 'L':
      left(motorA, motorB, speed);
      status = 'N';
      break;
    case 'S':
      brake(motorA, motorB);
      status = 'N';
      break;
  }
}


