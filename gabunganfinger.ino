// PROGRAM UTAMA SMART DOORLOCK
// Menggabungkan fitur:
// - Pendaftaran sidik jari (enroll)
// - Verifikasi akses
// - Kontrol pintu + LCD
// - Telegram bot untuk kontrol akses

#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>
#include <Adafruit_Fingerprint.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <SoftwareSerial.h>

#define buzzer D8
#define SW D4
#define selenoid D5
#define tombolEnroll D3  // Tombol untuk masuk ke mode enroll

LiquidCrystal_I2C lcd(0x27, 16, 2);
SoftwareSerial mySerial(D6, D7);
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial);

// WiFi & Telegram
#define Nama_ssid "Poco"
#define Pass_WiFi "11111111"
#define BOTtoken "7610880269:AAEkdjKmW65x7oFgWRR87wfw7m9GohJlSmk"
#define CHAT_ID "6622500636"

X509List cert(TELEGRAM_CERTIFICATE_ROOT);
WiFiClientSecure client;
UniversalTelegramBot bot(BOTtoken, client);

const char* ssid = Nama_ssid;
const char* password = Pass_WiFi;
int botRequestDelay = 1000;
unsigned long lastTimeBotRan;

bool aksesID[128];  // array untuk menyimpan ID mana saja yang aktif
int enrollID = 0; // ID baru untuk daftar sidik jari
bool isEnrollMode = false;

//=============================================================
void setup() {
  Serial.begin(115200);
  lcd.begin();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Smart Doorlock");
  lcd.setCursor(0, 1);
  lcd.print("Init system...");

  pinMode(buzzer, OUTPUT);
  pinMode(selenoid, OUTPUT);
  pinMode(SW, INPUT_PULLUP);
  pinMode(tombolEnroll, INPUT_PULLUP);
  digitalWrite(selenoid, HIGH);

  finger.begin(57600);
  delay(2000);

  if (!finger.verifyPassword()) {
    lcd.clear();
    lcd.print("Sensor ERROR");
    while (1) delay(1);
  }

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(200);
  }
  client.setInsecure();
  lcd.clear();
  lcd.print("WiFi Connected");
  delay(1500);

  bot.sendMessage(CHAT_ID, "Modul aktif. Kirim 'IDx YES' untuk izinkan akses", "");
  lcd.clear();
  lcd.print("Ready & Standby");
  lcd.setCursor(0, 1);
  lcd.print("Letakkan jari");
}

//=============================================================
void loop() {
  cekTelegram();

  if (digitalRead(tombolEnroll) == LOW) {
    isEnrollMode = true;
    lcd.clear();
    lcd.print("Mode Enroll");
    lcd.setCursor(0, 1);
    lcd.print("ID: ");
    enrollID++;
    enrollFingerprint(enrollID);
    delay(3000);
    isEnrollMode = false;
    lcd.clear();
    lcd.print("Scan Mode Aktif");
  } else {
    cekFingerprint();
  }
}

//=============================================================
void cekTelegram() {
  if (millis() > lastTimeBotRan + botRequestDelay) {
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    while (numNewMessages) {
      for (int i = 0; i < numNewMessages; i++) {
        String text = bot.messages[i].text;
        int idNum = 0;
        if (text.indexOf("ID") >= 0 && text.indexOf("YES") >= 0) {
          idNum = text.substring(2, text.indexOf(" YES")).toInt();
          if (idNum > 0 && idNum < 128) {
            aksesID[idNum] = true;
            bot.sendMessage(CHAT_ID, "Akses ID" + String(idNum) + " diaktifkan", "");
          }
        }
      }
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }
    lastTimeBotRan = millis();
  }
}

//=============================================================
void cekFingerprint() {
  int id = getFingerprintIDez();
  if (id > 0) {
    lcd.clear();
    lcd.print("ID terdaftar:");
    lcd.setCursor(0, 1);
    lcd.print("ID "); lcd.print(id);
    delay(1000);
    if (aksesID[id]) {
      bot.sendMessage(CHAT_ID, "ID" + String(id) + " Akses DIIJINKAN", "");
      bukaPintu();
    } else {
      bot.sendMessage(CHAT_ID, "ID" + String(id) + " Belum Diizinkan", "");
      lcd.clear();
      lcd.print("Akses ditolak");
      tone(buzzer, 1000, 500);
      delay(1000);
    }
  } else if (id == -2) {
    lcd.clear();
    lcd.print("ID Tidak Dikenal");
    bot.sendMessage(CHAT_ID, "Sidik jari ilegal terdeteksi!", "");
    tone(buzzer, 1000);
    delay(300);
    noTone(buzzer);
  }
}

//=============================================================
void bukaPintu() {
  lcd.clear();
  lcd.print("Pintu Terbuka");
  digitalWrite(selenoid, LOW);
  tone(buzzer, 2000, 200);
  delay(200);
  digitalWrite(selenoid, HIGH);
  lcd.setCursor(0, 1);
  lcd.print("Terimakasih");
  delay(3000);
}

//=============================================================
int getFingerprintIDez() {
  uint8_t p = finger.getImage();
  if (p != FINGERPRINT_OK) return -1;
  p = finger.image2Tz();
  if (p != FINGERPRINT_OK) return -1;
  p = finger.fingerFastSearch();
  if (p != FINGERPRINT_OK) return -2;
  return finger.fingerID;
}

//=============================================================
void enrollFingerprint(uint8_t id) {
  int p = -1;
  lcd.clear();
  lcd.print("Letakkan jari...");
  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
  }
  finger.image2Tz(1);
  lcd.clear();
  lcd.print("Angkat jari...");
  delay(2000);

  p = 0;
  lcd.clear();
  lcd.print("Letakkan lagi...");
  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
  }
  finger.image2Tz(2);
  p = finger.createModel();
  if (p == FINGERPRINT_OK) {
    finger.storeModel(id);
    lcd.clear();
    lcd.print("Enroll Sukses!");
    aksesID[id] = true;
    bot.sendMessage(CHAT_ID, "Sidik jari ID" + String(id) + " telah disimpan", "");
  } else {
    lcd.clear();
    lcd.print("Gagal Enroll!");
  }
  delay(2000);
}
