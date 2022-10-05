#include <Arduino.h>

#include "esp_log.h"

#include <Ps3Controller.h>
#include <ESP32Servo.h>

#include "soc/soc.h"             // disable brownout problems
#include "soc/rtc_cntl_reg.h"    // disable brownout problems

#include "DFMiniMp3.h"

#define MAIN_TAG "Main"

// These are all GPIO pins on the ESP32
// Recommended pins include 2,4,12-19,21-23,25-27,32-33
// for the ESP32-S2 the GPIO pins are 1-21,26,33-42

#define PIN_RX 16 // FIXED
#define PIN_TX 17 // FIXED

#ifdef BUILD_ENV_V2
  #define PIN_LEFT_ARM 22
  #define PIN_RIGHT_ARM 25
  #define PIN_BODY 27

  #define PIN_LEFT_GUN 32
  #define PIN_RIGHT_GUN 21

  #define PIN_TRACK_A1 33
  #define PIN_TRACK_A2 19
  #define PIN_TRACK_B1 26
  #define PIN_TRACK_B2 18

  #define PIN_POWER 2

  #define PIN_CANNON 4

  // FREE 23
#else
  #ifdef BUILD_ENV_V1
    #define PIN_LEFT_ARM 18
    #define PIN_RIGHT_ARM 19
    #define PIN_BODY 14

    #define PIN_LEFT_GUN 12
    #define PIN_RIGHT_GUN 13

    #define PIN_TRACK_A1 32
    #define PIN_TRACK_A2 33
    #define PIN_TRACK_B1 25
    #define PIN_TRACK_B2 26

    #define PIN_POWER 2

    #define PIN_CANNON 4
  #else
    #error "Unsuppored Env."
  #endif

#endif

#define CHANNEL_A1 12
#define CHANNEL_A2 13
#define CHANNEL_B1 14
#define CHANNEL_B2 15

#define MAX_VOLUME 18

#define STICK_THRESHOLD 20

Servo servoBody;
Servo servoLeftArm, servoRightArm;

class Mp3Notify;
typedef DFMiniMp3<HardwareSerial, Mp3Notify> DfMp3;
HardwareSerial mySerial(2); // 16, 17
DfMp3 dfmp3(mySerial);
int volume = MAX_VOLUME; // 0~30

int center = 90;

int bodyAngle = center;
int leftArmAngle = center;
int rightArmAngle = center;

int angleStep = 5;
int servoDelay = 15;

bool circlePress = false;
bool triaglePress = false;
bool squarePress = false;
bool crossPress = false;

void init() {
  ESP_LOGI(MAIN_TAG, "Init.(Internal)");

  bodyAngle = center;
  leftArmAngle = center;
  rightArmAngle = center;

  servoBody.attach(PIN_BODY, 500, 2400);
  servoLeftArm.attach(PIN_LEFT_ARM, 500, 2400);
#ifndef BUILD_ENV_V2  
  // https://ko.aliexpress.com/item/1005002460000775.html
#else
  // https://ko.aliexpress.com/item/1005001284384678.html
  // Max travel : 145 degree(800->2200ms)
  servoRightArm.attach(PIN_RIGHT_ARM, 800, 2200);
#endif

  servoBody.write(center);
  servoLeftArm.write(center);
  servoRightArm.write(center);

  pinMode(PIN_POWER, OUTPUT);
  digitalWrite(PIN_POWER, HIGH);

  pinMode(PIN_CANNON, OUTPUT);

  dfmp3.playMp3FolderTrack(3);
}

void reset() {
  ESP_LOGI(MAIN_TAG, "Reset");
  bodyAngle = center;
  leftArmAngle = center;
  rightArmAngle = center;

  servoBody.write(center);
  servoLeftArm.write(center);
  servoRightArm.write(center);

  dfmp3.playMp3FolderTrack(3);
}

void fire(int pin) {
  for (int i = 0; i < 4; i ++) {
    digitalWrite(pin, HIGH);
    delay(80);
    digitalWrite(pin, LOW);
    delay(80);
  }
}

int battery = 0;
void notify()
{
  // RESET
  if (Ps3.event.button_down.start) {
    ESP_LOGI(MAIN_TAG, "Start(Reset)");
    reset();
  }

  // Gatling
  if ((Ps3.event.button_down.cross) || (Ps3.event.button_down.circle)) {
    dfmp3.playMp3FolderTrack(1);
  }
  if (Ps3.event.button_down.cross) {
    ESP_LOGI(MAIN_TAG, "Cross(Fire L)");
    fire(PIN_LEFT_GUN);
  }
  if (Ps3.event.button_down.circle) {
    ESP_LOGI(MAIN_TAG, "Circle(Fire R)");
    fire(PIN_RIGHT_GUN);
  }

  // Cannon
  if ((Ps3.event.button_down.square) || (Ps3.event.button_down.triangle)) {
    ESP_LOGI(MAIN_TAG, "Square or Triangle(Cannon)");
    dfmp3.playMp3FolderTrack(2);

#ifndef BUILD_ENV_V1
    digitalWrite(PIN_CANNON, HIGH);
#endif

    // Back
    ledcWrite(CHANNEL_B1, 0);
    ledcWrite(CHANNEL_B2, 255);

    ledcWrite(CHANNEL_A1, 0);
    ledcWrite(CHANNEL_A2, 255);

    delay(30); // N30

    ledcWrite(CHANNEL_B1, 0);
    ledcWrite(CHANNEL_B2, 0);

    ledcWrite(CHANNEL_A1, 0);
    ledcWrite(CHANNEL_A2, 0);

    delay(300);
#ifndef BUILD_ENV_V1
    digitalWrite(PIN_CANNON, LOW);
#endif
  }


  if (Ps3.event.button_down.l1) {
    ESP_LOGD(MAIN_TAG, "L1(Left Arm Up)");
    leftArmAngle = min(leftArmAngle + angleStep, center + 45);
    servoLeftArm.write(leftArmAngle);
  }
  if (Ps3.event.button_down.l2) {
    ESP_LOGD(MAIN_TAG, "L2(Left Arm Down)");
    leftArmAngle = max(leftArmAngle - angleStep, center - 45);
    servoLeftArm.write(leftArmAngle);
  }
  if (Ps3.event.button_down.r1) {
    ESP_LOGD(MAIN_TAG, "R1(Right Arm Up)");
#ifdef BUILD_ENV_V1
    rightArmAngle = min(rightArmAngle + angleStep, center + 45);
#endif
#ifdef BUILD_ENV_V2
    rightArmAngle = max(rightArmAngle - angleStep, center - 45);
#endif
    servoRightArm.write(rightArmAngle);
  }
  if (Ps3.event.button_down.r2) {
    ESP_LOGD(MAIN_TAG, "R2(Right Arm Down)");
#ifdef BUILD_ENV_V1
    rightArmAngle = max(rightArmAngle - angleStep, center - 45);
#endif
#ifdef BUILD_ENV_V2
    rightArmAngle = min(rightArmAngle + angleStep, center + 45);
#endif
    servoRightArm.write(rightArmAngle);
  }

  // Body
  if (Ps3.event.button_down.left) {
    ESP_LOGD(MAIN_TAG, "Left(Body)");
    bodyAngle = min(bodyAngle + 5, 180);
    servoBody.write(bodyAngle);
  }
  if (Ps3.event.button_down.right) {
    ESP_LOGD(MAIN_TAG, "Right(Body)");
    bodyAngle = max(bodyAngle - 5, 0);
    servoBody.write(bodyAngle);
  }

  // Volume
  if (Ps3.event.analog_changed.button.up) {
    ESP_LOGD(MAIN_TAG, "Up(Volume)");
    volume = min(volume + 2, MAX_VOLUME);
    dfmp3.setVolume(volume);
  }
  if (Ps3.event.analog_changed.button.down) {
    ESP_LOGD(MAIN_TAG, "Down(Volume)");
    volume = max(volume - 2, 0);
    dfmp3.setVolume(volume);
  }

  // Track
  int absLy = abs(Ps3.event.analog_changed.stick.ly);
  if (absLy < STICK_THRESHOLD) {
    ledcWrite(CHANNEL_B1, 0);
    ledcWrite(CHANNEL_B2, 0);
  } else {
    if (Ps3.event.analog_changed.stick.ly < -STICK_THRESHOLD) {
      ESP_LOGD(MAIN_TAG, "Stick(L) Forward %d", absLy);
      ledcWrite(CHANNEL_B1, absLy * 2);
      ledcWrite(CHANNEL_B2, 0);
    }
    else if (Ps3.event.analog_changed.stick.ly > STICK_THRESHOLD) {
      ESP_LOGD(MAIN_TAG, "Stick(L) Backward %d", absLy);
      ledcWrite(CHANNEL_B1, 0);
      ledcWrite(CHANNEL_B2, absLy * 2);
    }
  }

  int absRy = abs(Ps3.event.analog_changed.stick.ry);
  if (absRy < STICK_THRESHOLD) {
    ledcWrite(CHANNEL_A1, 0);
    ledcWrite(CHANNEL_A2, 0);    
  } else {
    if (Ps3.event.analog_changed.stick.ry < -STICK_THRESHOLD) {
      ESP_LOGD(MAIN_TAG, "Stick(R) Forward %d", absRy);
      ledcWrite(CHANNEL_A1, absRy * 2);
      ledcWrite(CHANNEL_A2, 0);
    }
    else if (Ps3.event.analog_changed.stick.ry > STICK_THRESHOLD) {
      ESP_LOGD(MAIN_TAG, "Stick(R) Backward %d", absRy);
      ledcWrite(CHANNEL_A1, 0);
      ledcWrite(CHANNEL_A2, absRy * 2);
    }
  }
}

void onConnect();
void onDisconnect();

void initPs3() {
  ESP_LOGI(MAIN_TAG, "Init. PS3 Wireless Controller");
  Ps3.attach(notify);
  Ps3.attachOnConnect(onConnect);
  Ps3.attachOnDisconnect(onDisconnect);
  Ps3.begin("44:44:44:44:44:44");
}

void onConnect() {
  ESP_LOGI(MAIN_TAG, "Connected");

  String address = Ps3.getAddress();

  ESP_LOGI(MAIN_TAG, "The ESP32's Bluetooth MAC address is: %s", address.c_str());

  reset();

  servoBody.attach(PIN_BODY, 500, 2400);
  servoLeftArm.attach(PIN_LEFT_ARM, 500, 2400);
  servoRightArm.attach(PIN_RIGHT_ARM, 500, 2400);

  pinMode(PIN_LEFT_GUN, OUTPUT);
  pinMode(PIN_RIGHT_GUN, OUTPUT);


  ESP_LOGD(MAIN_TAG, "Setup CHANNEL_A1 %d",  CHANNEL_A1);
  ledcSetup(CHANNEL_A1, 1000, 7); // 0~127
  ESP_LOGD(MAIN_TAG, "Setup CHANNEL_A2 %d", CHANNEL_A2);
  ledcSetup(CHANNEL_A2, 1000, 7); // 0~127
  ESP_LOGD(MAIN_TAG, "Setup CHANNEL_B1 %d", CHANNEL_B1);
  ledcSetup(CHANNEL_B1, 1000, 7); // 0~127
  ESP_LOGD(MAIN_TAG, "Setup CHANNEL_B2 %d", CHANNEL_B2);
  ledcSetup(CHANNEL_B2, 1000, 7); // 0~127

  ESP_LOGD(MAIN_TAG, "Attach PIN_TRACK_A1 %d", PIN_TRACK_A1);
  ledcAttachPin(PIN_TRACK_A1, CHANNEL_A1);
  ESP_LOGD(MAIN_TAG, "Attach PIN_TRACK_A2 %d", PIN_TRACK_A2);
  ledcAttachPin(PIN_TRACK_A2, CHANNEL_A2);
  ESP_LOGD(MAIN_TAG, "Attach PIN_TRACK_B1 %d", PIN_TRACK_B1);
  ledcAttachPin(PIN_TRACK_B1, CHANNEL_B1);
  ESP_LOGD(MAIN_TAG, "Attach PIN_TRACK_B2 %d", PIN_TRACK_B2);
  ledcAttachPin(PIN_TRACK_B2, CHANNEL_B2);

  dfmp3.playMp3FolderTrack(3);
}

void onDisconnect() {
  ESP_LOGI(MAIN_TAG, "Disconnected");
  Ps3.end();
  initPs3();
}

void setup() {
  // esp_log_level_set("*", ESP_LOG_DEBUG);
  
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); //disable brownout detector
  Serial.begin(115200);

  init();
  initPs3();

  dfmp3.begin(9600, 1000);
  dfmp3.reset();

  while(!dfmp3.isOnline()) {
    delay(10);
  }

  dfmp3.setVolume(volume);
}

void loop() {
  if(!Ps3.isConnected())
    return;

  dfmp3.loop();
  delay(1);
}

//----------------------------------------------------------------------------------
class Mp3Notify
{
public:
  static void PrintlnSourceAction(DfMp3_PlaySources source, const char *action)
  {
    if (source & DfMp3_PlaySources_Sd) {
      ESP_LOGD(MAIN_TAG, "SD Card, %s", action);
    }
    if (source & DfMp3_PlaySources_Usb) {
      ESP_LOGD(MAIN_TAG, "USB Disk, %s", action);
    }
    if (source & DfMp3_PlaySources_Flash) {
      ESP_LOGD(MAIN_TAG, "Flash, %s", action);
    }
  }
  static void OnError(DfMp3 &mp3, uint16_t errorCode)
  {
    // see DfMp3_Error for code meaning
    Serial.println();
    Serial.print("Com Error ");
    switch (errorCode)
    {
    case DfMp3_Error_Busy:
      ESP_LOGE(MAIN_TAG, "Com Error - Busy");
      break;
    case DfMp3_Error_Sleeping:
      ESP_LOGE(MAIN_TAG, "Com Error - Sleeping");
      break;
    case DfMp3_Error_SerialWrongStack:
      ESP_LOGE(MAIN_TAG, "Com Error - Serial Wrong Stack");
      break;

    case DfMp3_Error_RxTimeout:
      ESP_LOGE(MAIN_TAG, "Com Error - Rx Timeout!!!");
      break;
    case DfMp3_Error_PacketSize:
      ESP_LOGE(MAIN_TAG, "Com Error - Wrong Packet Size!!!");
      break;
    case DfMp3_Error_PacketHeader:
      ESP_LOGE(MAIN_TAG, "Com Error - Wrong Packet Header!!!");
      break;
    case DfMp3_Error_PacketChecksum:
      ESP_LOGE(MAIN_TAG, "Com Error - Wrong Packet Checksum!!!");
      break;

    default:
      ESP_LOGE(MAIN_TAG, "Com Error - %d", errorCode);
      break;
    }
  }
  static void OnPlayFinished(DfMp3 &mp3, DfMp3_PlaySources source, uint16_t track)
  {
    ESP_LOGD(MAIN_TAG, "Play finished for #%d", track);
  }
  static void OnPlaySourceOnline(DfMp3 &mp3, DfMp3_PlaySources source)
  {
    PrintlnSourceAction(source, "online");
  }
  static void OnPlaySourceInserted(DfMp3 &mp3, DfMp3_PlaySources source)
  {
    PrintlnSourceAction(source, "inserted");
  }
  static void OnPlaySourceRemoved(DfMp3 &mp3, DfMp3_PlaySources source)
  {
    PrintlnSourceAction(source, "removed");
  }
};
