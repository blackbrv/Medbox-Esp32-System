#include <Arduino.h>
#include <Wire.h>
#include <RTClib.h>
#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <FirebaseJson.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <Adafruit_Fingerprint.h>
#include <LiquidCrystal_I2C.h>
#include "Audio.h"
#include "HX711.h"
#include "addons/TokenHelper.h"

#define WIFI_SSID "Psg567uber" //Lacasa salam dari Ibrahim, Psg567uber, Deklarasi Untuk SSID WiFi
#define WIFI_PASSWORD "Akca130797" //iyatunggu, Akca130797, Deklarasi Password WiFi
#define API_KEY "AIzaSyB-o1L9crZrI-IqR8S5jmMtMopJeDb98mk" // Deklarasi API_KEY untuk Firebase
#define DATABASE_URL "https://madbox4195-default-rtdb.firebaseio.com/" // Deklarasi url untuk firebase
#define ESP32_KEY "Alat_Test" //Deklarasi untuk nama alat 
#define FIREBASE_PROJECT_ID "madbox4195"
#define USER_EMAIL "esp.admin@esp32.com"
#define USER_PASSWORD "adminesp32"

//Deklarasi Pin untuk modul MAX98357A
#define I2S_DOUT 25
#define I2S_BCLK 27
#define I2S_LRC 26

const int SDA_PIN = 21; // SDA pin pada ESP32 RTC
const int SCL_PIN = 22; // SCL pin pada ESP32 RTC
const int LOADCELL_DOUT_PIN = 14;
const int LOADCELL_SCK_PIN = 13;
const int dirPin = 18;  // DIR pin pada ESP32 untuk Motor stepper
const int stepPin = 19; // Step pin pada ESP32 untuk Motor stepper
const int stepsPerRevolution = 200; // Deklarasi berapa step stepper harus bergerak
String daysOfTheWeek[7] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

Audio audio; // Deklarasi Library Audio dengan audio
HX711 scale; // Deklarasi Library HX711 dengan scale
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;
RTC_DS3231 rtc;


unsigned long dataMillis = 0;
bool taskCompleted = false;
int data = 1;
int maxIndexData = 3;
int verifnilai = 0;

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 25200, 60000);
LiquidCrystal_I2C lcd(0x27, 16, 2); // set the LCD address to 0x27 for a 16 chars and 2 line display

#define mySerial Serial2
const int btn = 12;
const int relaySolePin = 15;
const int relaySole2Pin = 5;
const int relayMotorPin = 4;

int FFID = 0;
int idFinger = 1;

// Deklarasi Adafruit Fingerpirnt library sebagai Adafruit_Fingerprint ---------------------------------------------------------------------------

Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial);
uint8_t id;


void checkDaTime();
uint8_t getFingerprintEnroll();
int getFingerprintID();
void EnrollFinger();
void FingerFound();
void stepperRun();

String getData(String field) {
  DateTime now = rtc.now();
  int date = now.day();
  int month = now.month();
  int year = now.year();

  char buffer [25] = "";

  sprintf(buffer, "%d-%d-%04d", date, month, year);

  String nowDocumentPath = String(ESP32_KEY) + "/hour_data_" + buffer; //!!MUSH USE THIS FOR DOCUMENTPATH
  String documentPath = String(ESP32_KEY) + "/hour_data_16-1-2024";
  //Serial.println(nowDocumentPath);
  if (Firebase.Firestore.getDocument(&fbdo, FIREBASE_PROJECT_ID, "", documentPath.c_str()));
  //Serial.printf(fbdo.payload().c_str());
  else
    Serial.println(fbdo.errorReason());
  FirebaseJson jason;
  jason.setJsonData(fbdo.payload().c_str());
  FirebaseJsonData jasonData;
  jason.get(jasonData, "fields/jam" + field + "/stringValue");
  String value = jasonData.stringValue;
  //Serial.println(value);
  return value;
}

void setup()
{
  Serial.begin(115200);
  lcd.init();         // initialize the lcd
  lcd.backlight();    // Turn on the LCD screen backlight

  audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT); // Modul MAX98357A
  audio.setVolume(100);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD); //"Wokwi-GUEST", "", 6; WIFI_SSID, WIFI_PASSWORD
  Serial.print("Connecting to Wi-Fi");
  unsigned long ms = millis();
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(300);
  }
  Serial.println();
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());
  Serial.println();

  // Kode Check RTC

  if (!rtc.begin())
  {
    Serial.println("Couldn't find RTC");
    while (1)
      ;
  }

  if (rtc.lostPower()) {
    Serial.println("RTC lost power, setting the time!");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  timeClient.begin();
  while (!timeClient.update()) {
    timeClient.forceUpdate();
  }

  rtc.adjust(DateTime(timeClient.getEpochTime()));

  // Code Setup Loadcell Tare ---------------------------------------------------------------------------

  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
  scale.set_scale(-813.556);
  Serial.print("Sedang melakukan tare...");
  scale.tare();
  delay(500);

  // Code Setup Firebase ---------------------------------------------------------------------------

  Serial.printf("Firebase Client v%s\n\n", FIREBASE_CLIENT_VERSION);

  config.api_key = API_KEY;

  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;

  config.token_status_callback = tokenStatusCallback;

  Firebase.reconnectNetwork(true);

  fbdo.setResponseSize(2048);

  Firebase.begin(&config, &auth);

  Serial.print("Firebase Connected");

  // Code Setup Fingerprint ---------------------------------------------------------------------------

  Serial.println("\n\nAdafruit Fingerprint sensor");
  mySerial.begin(57600, SERIAL_8N1, 16, 17); // 17 for RX, 16 for TX

  if (finger.verifyPassword()) {
    Serial.println("Found fingerprint sensor!");
  } else {
    Serial.println("Did not find fingerprint sensor :");
    while (1) {
      delay(1);
    }
  }

  Serial.println(F("Reading sensor parameters"));
  finger.getParameters();
  Serial.print(F("Status: 0x")); Serial.println(finger.status_reg, HEX);
  Serial.print(F("Sys ID: 0x")); Serial.println(finger.system_id, HEX);
  Serial.print(F("Capacity: ")); Serial.println(finger.capacity);
  Serial.print(F("Security level: ")); Serial.println(finger.security_level);
  Serial.print(F("Device address: ")); Serial.println(finger.device_addr, HEX);
  Serial.print(F("Packet len: ")); Serial.println(finger.packet_len);
  Serial.print(F("Baud rate: ")); Serial.println(finger.baud_rate);

  finger.getTemplateCount();

  if (finger.templateCount == 0) {
    Serial.print("Sensor doesn't contain any fingerprint data. Please run the 'enroll' example.");
  }
  else {
    Serial.println("Waiting for valid finger...");
    Serial.print("Sensor contains "); Serial.print(finger.templateCount); Serial.println(" templates");
  }

  // Deklarasi Pin ---------------------------------------------------------------------------

  pinMode(btn, INPUT_PULLUP);
  pinMode(relaySolePin, OUTPUT);
  pinMode(relaySole2Pin, OUTPUT);
  pinMode(relayMotorPin, OUTPUT);
  pinMode(stepPin, OUTPUT);
  pinMode(dirPin, OUTPUT);

}

void loop()
{
  audio.loop();
  checkDaTime();
  EnrollFinger();
}

//Code Fingerprint ---------------------------------------------------------------------------

void FingerFound() {
  FFID = getFingerprintID();
  if (FFID > 0) {
    digitalWrite(relaySolePin, HIGH);
    digitalWrite(relaySole2Pin, HIGH);
    delay(5000);
    digitalWrite(relaySolePin, LOW);
    digitalWrite(relaySole2Pin, LOW);
  }
  else if (FFID == 0)
  {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("No Finger");
    lcd.setCursor(0, 2);
    lcd.print("Detected");
    Serial.println("No Finger detected");
    delay(2000);
    lcd.clear();
  }
  else if (FFID == -1)
  {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Didnt Find");
    lcd.setCursor(0, 2);
    lcd.print("a match");
    Serial.println("Didnt Find a match");
    delay(2000);
    lcd.clear();
  }
  else if (FFID == -2)
  {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Module Error");
    Serial.println("Module Error");
    delay(2000);
    lcd.clear();
  }
}

void EnrollFinger() {
  //Serial.println("System is Ready");
  //Serial.println("Please Press The Button to register a fingerprint");
  int buttonState = digitalRead(btn); //dont forget to add this
  if (buttonState == 0)
  {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Daftarkan");
    lcd.setCursor(0, 2);
    lcd.print("sidik jari anda");
    lcd.clear();
    Serial.println("Ready to enroll a fingerprint!");
    //Serial.println("Please type in the ID # (from 1 to 127) you want to save this finger as...");
    id = idFinger;
    //Serial.print("Enrolling ID #");
    Serial.println(id);
    while (!getFingerprintEnroll())break;
    idFinger++;
  }
  else if (btn == 1)
  {
    FingerFound();
  }
}

uint8_t getFingerprintEnroll() {
  int p = -1;
  Serial.print("Waiting for valid finger to enroll as #"); Serial.println(id);
  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    switch (p) {
      case FINGERPRINT_OK:
        Serial.println("Image taken");
        break;
      case FINGERPRINT_NOFINGER:
        Serial.println(".");
        break;
      case FINGERPRINT_PACKETRECIEVEERR:
        Serial.println("Communication error");
        break;
      case FINGERPRINT_IMAGEFAIL:
        Serial.println("Imaging error");
        break;
      default:
        Serial.println("Unknown error");
        break;
    }
    yield();
  }

  p = finger.image2Tz(1);
  switch (p) {
    case FINGERPRINT_OK:
      Serial.println("Image converted");
      break;
    case FINGERPRINT_IMAGEMESS:
      Serial.println("Image too messy");
      return p;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      return p;
    case FINGERPRINT_FEATUREFAIL:
      Serial.println("Could not find fingerprint features");
      return p;
    case FINGERPRINT_INVALIDIMAGE:
      Serial.println("Could not find fingerprint features");
      return p;
    default:
      Serial.println("Unknown error");
      return p;
  }

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Lepaskan jari");
  Serial.println("Remove finger");
  delay(2000);
  p = 0;
  while (p != FINGERPRINT_NOFINGER) {
    p = finger.getImage();
    yield();
  }
  Serial.print("ID "); Serial.println(id);
  p = -1;
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Tempelkan jari");
  lcd.setCursor(0, 2);
  lcd.print("yang sama");
  delay(1000);
  lcd.clear();
  Serial.println("Place same finger again");
  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    switch (p) {
      case FINGERPRINT_OK:
        Serial.println("Image taken");
        break;
      case FINGERPRINT_NOFINGER:
        Serial.print(".");
        break;
      case FINGERPRINT_PACKETRECIEVEERR:
        Serial.println("Communication error");
        break;
      case FINGERPRINT_IMAGEFAIL:
        Serial.println("Imaging error");
        break;
      default:
        Serial.println("Unknown error");
        break;
    }
  }

  p = finger.image2Tz(2);
  switch (p) {
    case FINGERPRINT_OK:
      Serial.println("Image converted");
      break;
    case FINGERPRINT_IMAGEMESS:
      Serial.println("Image too messy");
      return p;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      return p;
    case FINGERPRINT_FEATUREFAIL:
      Serial.println("Could not find fingerprint features");
      return p;
    case FINGERPRINT_INVALIDIMAGE:
      Serial.println("Could not find fingerprint features");
      return p;
    default:
      Serial.println("Unknown error");
      return p;
  }


  Serial.print("Creating model for #");  Serial.println(id);

  p = finger.createModel();
  if (p == FINGERPRINT_OK) {
    Serial.println("Prints matched!");
  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    Serial.println("Communication error");
    return p;
  } else if (p == FINGERPRINT_ENROLLMISMATCH) {
    Serial.println("Fingerprints did not match");
    return p;
  } else {
    Serial.println("Unknown error");
    return p;
  }

  Serial.print("ID "); Serial.println(id);
  p = finger.storeModel(id);
  if (p == FINGERPRINT_OK) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Sidikjari");
    lcd.setCursor(0, 2);
    lcd.print("Berhasil");
    delay(1000);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Disimpan");
    Serial.println("Stored!");
    Serial.println("");
    delay(3000);
  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    Serial.println("Communication error");
    Serial.println("");
    return p;
  } else if (p == FINGERPRINT_BADLOCATION) {
    Serial.println("Could not store in that location");
    Serial.println("");
    return p;
  } else if (p == FINGERPRINT_FLASHERR) {
    Serial.println("Error writing to flash");
    Serial.println("");
    return p;
  } else {
    Serial.println("Unknown error");
    Serial.println("");
    return p;
  }
}

int getFingerprintID() {
  uint8_t p = finger.getImage();
  switch (p) {
    case FINGERPRINT_OK:
      Serial.println("Image taken");
      break;
    case FINGERPRINT_NOFINGER:
      //Serial.println("No finger detected");
      return 0;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      return -2;
    case FINGERPRINT_IMAGEFAIL:
      Serial.println("Imaging error");
      return -2;
    default:
      Serial.println("Unknown error");
      return -2;
  }

  // OK success!

  p = finger.image2Tz();
  switch (p) {
    case FINGERPRINT_OK:
      Serial.println("Image converted");
      break;
    case FINGERPRINT_IMAGEMESS:
      Serial.println("Image too messy");
      return -1;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      return -2;
    case FINGERPRINT_FEATUREFAIL:
      Serial.println("Could not find fingerprint features");
      return -2;
    case FINGERPRINT_INVALIDIMAGE:
      Serial.println("Could not find fingerprint features");
      return -2;
    default:
      Serial.println("Unknown error");
      return -2;
  }

  // OK converted!
  p = finger.fingerFastSearch();
  if (p == FINGERPRINT_OK) {
    Serial.println("Found a print match!");
  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    Serial.println("Communication error");
    return -2;
  } else if (p == FINGERPRINT_NOTFOUND) {
    Serial.println("Did not find a match");
    return -1;
  } else {
    Serial.println("Unknown error");
    return -2;
  }
  // found a match!
  Serial.print("Found ID #"); Serial.print(finger.fingerID);
  Serial.print(" with confidence of "); Serial.println(finger.confidence);
  return finger.fingerID;
}

void checkDaTime() {
  FirebaseJson checkJson;
  DateTime tnow = rtc.now();
  int tdate = tnow.day();
  int tmonth = tnow.month();
  int tyear = tnow.year();
  int tHour = tnow.hour();
  int tMinutes = tnow.minute();
  char dataPath [25] = "";
  sprintf(dataPath, "%d-%d-%04d", tdate, tmonth, tyear);
  String doc_Path = String(ESP32_KEY) + "/" + dataPath;
  for (data; data < maxIndexData;)
  {
    DateTime rNow = rtc.now();
    int rHour = rNow.hour();
    int rMinute = rNow.minute();
    String index = String(data);
    String jam = getData(index);
    int separatorIndex = jam.indexOf(':');
    if (separatorIndex != -1)
    {
      String jamString = jam.substring(0, separatorIndex);
      String menitString = jam.substring(separatorIndex + 1);
      int fJam = jamString.toInt();
      int fMinute = menitString.toInt();
      Serial.print("Jam Sekarang : ");
      Serial.print(rHour);
      Serial.print(" : ");
      Serial.println(rMinute);
      Serial.print("Jam Obat ke -");
      Serial.print(index);
      Serial.print(" = ");
      Serial.print(fJam);
      Serial.print(" : ");
      Serial.println(fMinute);
      delay(1000);
      if (fJam == rHour && fMinute == rMinute)
      {
        Serial.print("Saatnya Minum Obat !!");
        stepperRun();
        // Kode Loadcell --------------------------------------------------------
        if (scale.is_ready()) {
          long firstReading = scale.get_units(10);
          Serial.print("Membaca data pertama: ");
          Serial.print(firstReading);
          Serial.println(" Gram");
          delay(5000);
          long secondReading = scale.get_units(10);
          Serial.print("Membaca data kedua: ");
          Serial.print(secondReading);
          Serial.println(" Gram");

          if (secondReading > verifnilai) {
            checkJson.set("fields/minum_jam" + index + "/stringValue", "1");
            if (Firebase.Firestore.createDocument(&fbdo, FIREBASE_PROJECT_ID, "", doc_Path.c_str(), checkJson.raw()))
              Serial.println("Data 1 Stored, at Jam" + index);
            else if (fbdo.errorReason().indexOf("already exists" > 0))
            {
              if (Firebase.Firestore.patchDocument(&fbdo, FIREBASE_PROJECT_ID, "", doc_Path.c_str(), checkJson.raw(), "minum_jam" + index))
                Serial.println("Data 1 Stored, at Jam" + index);
              else
                Serial.println(fbdo.errorReason());
            }
          } else if (secondReading == firstReading) {
            checkJson.set("fields/minum_jam" + index + "/stringValue", "0");
            if (Firebase.Firestore.createDocument(&fbdo, FIREBASE_PROJECT_ID, "", doc_Path.c_str(), checkJson.raw()))
              Serial.println("Data 0 Stored, at Jam" + index);
            else if (fbdo.errorReason().indexOf("already exists" > 0))
            {
              if (Firebase.Firestore.patchDocument(&fbdo, FIREBASE_PROJECT_ID, "", doc_Path.c_str(), checkJson.raw(), "minum_jam" + index))
                Serial.println("Data 0 Stored, at Jam" + index);
              else
                Serial.println(fbdo.errorReason());
            }
          }
        } else {
          Serial.println("HX711 tidak ditemukan");
        }
        // ----------------------------------------------------------------------
        data++;
      }
    }
  }
  Serial.println("Tidak ada jadwal lagi Selamat Beraktifitas !! :)");
  Serial.print("Jam Sekarang : ");
  Serial.println(String(tHour) + " : " + String(tMinutes));
  delay(1000);
  if (tHour == 13 && tMinutes == 30)
  {
    Serial.println("Semoga Cepat Sembuh !!");
    ESP.restart();
  }

}

void stepperRun() {
  digitalWrite(relayMotorPin, HIGH);
  digitalWrite(dirPin, HIGH);
  for (int x = 0; x < stepsPerRevolution; x++) {
    digitalWrite(stepPin, HIGH);
    delayMicroseconds(1000);
    digitalWrite(stepPin, LOW);
    delayMicroseconds(1000);
    digitalWrite(relayMotorPin, LOW);
  }
}