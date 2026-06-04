#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>

// ==========================================
// 1. PENGATURAN WIFI & IDENTITAS ALAT
// ==========================================
const char* ssid = "SilentNode"; 
const char* password = "F4thur2310!Net"; 

const char* device_id = "6";
const char* kecamatan_id = "7"; 

// ==========================================
// 2. PENGATURAN TELEGRAM
// ==========================================
String BOTtoken = "8925509982:AAFdV_dpynCI0v3H2DkiSXfRZhD3j5x0HaU";
String CHAT_ID = "-1003884844228"; 

// TARGET KAMAR (THREAD) TELEGRAM HANYA UNTUK BTP
int THREAD_ID_BTP = 3;

// ==========================================
// 3. PENGATURAN WEB FATHUR
// ==========================================
const char* serverName = "https://smart-drinase.vercel.app/api/handleEsp"; 

// ==========================================
// DEFINISI PIN SENSOR & LED
// ==========================================
const int trigPin = 5;
const int echoPin = 18;
const int ledMerah = 2;   // BAHAYA
const int ledPutih = 4;   // WASPADA
const int ledHijau = 15;  // AMAN

// Kita hanya butuh 1 jalur Secure karena sistemnya bergantian rapi
WiFiClientSecure clientSecure; 

// VARIABEL TIMER & DEBOUNCING
unsigned long lastWebTime = 0;
unsigned long timerDelayWeb = 5000; 

unsigned long dangerStartTime = 0; 
bool isDangerConfirmed = false;    
const unsigned long dangerThreshold = 10000; // Timer 10 Detik

// ==========================================
// FUNGSI KHUSUS KIRIM TELEGRAM (ANTI-GAGAL)
// ==========================================
void kirimTelegram(String pesan) {
  if(WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = "https://api.telegram.org/bot" + BOTtoken + "/sendMessage";
    http.begin(clientSecure, url);
    http.addHeader("Content-Type", "application/json");

    // Format JSON API Resmi Telegram (Support Topik/Thread)
    String payload = "{\"chat_id\":\"" + CHAT_ID + "\", \"message_thread_id\":" + String(THREAD_ID_BTP) + ", \"text\":\"" + pesan + "\", \"parse_mode\":\"Markdown\"}";
    
    int httpResponseCode = http.POST(payload);
    Serial.print(">> Status Kirim Telegram API: ");
    Serial.println(httpResponseCode);
    
    // Jika ada error, Telegram akan memberi tahu alasannya di Serial Monitor
    if (httpResponseCode > 0) {
      Serial.println(http.getString());
    } else {
      Serial.println("Koneksi ke Telegram Terputus!");
    }
    http.end();
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  pinMode(ledMerah, OUTPUT);
  pinMode(ledPutih, OUTPUT);
  pinMode(ledHijau, OUTPUT);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(1000);
  WiFi.begin(ssid, password);
  Serial.print("Menghubungkan WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Terhubung!");
  
  clientSecure.setInsecure(); // Wajib untuk SSL Telegram & Vercel
  
  // Mengirim pesan tes awal
  String pesanAktif = "✅ *Sistem EWS CAREBA Siaga BTP Aktif!*\nSensor mulai memantau parit 24/7...";
  kirimTelegram(pesanAktif);
}

void loop() {
  // --- BACA JARAK SENSOR ---
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  
  long duration = pulseIn(echoPin, HIGH);
  float distance = duration * 0.034 / 2;

  Serial.print("Jarak Air: ");
  Serial.print(distance);
  Serial.println(" cm");

  // --- LOGIKA CERDAS: LEVEL AIR (1 KALI NOTIFIKASI) ---
  if (distance < 6) { 
    if (dangerStartTime == 0) {
      dangerStartTime = millis(); 
    }

    if (millis() - dangerStartTime >= dangerThreshold) {
      digitalWrite(ledMerah, HIGH);
      digitalWrite(ledPutih, LOW);
      digitalWrite(ledHijau, LOW);

      if (!isDangerConfirmed) {
        Serial.println("MENYIAPKAN NOTIFIKASI TELEGRAM...");
        
        float tinggiAir = 15.0 - distance; 
        
        String pesanBahaya = "🚨 *PERINGATAN DARURAT (Node 01 - BTP)* 🚨\n\n";
        pesanBahaya += "Kondisi air drainase meluap dan mencapai level *BAHAYA!*\n\n";
        pesanBahaya += "📊 *Data Sensor Real-time:*\n";
        pesanBahaya += "🌊 Ketinggian Genangan: *" + String(tinggiAir, 1) + " cm*\n";
        pesanBahaya += "📏 Sisa Ruang ke Parit: *" + String(distance, 1) + " cm*\n\n";
        pesanBahaya += "Warga disekitar BTP harap segera siaga dan amankan barang!";

        // Eksekusi fungsi kirim
        kirimTelegram(pesanBahaya);
        
        isDangerConfirmed = true; 
      }
    } else {
      digitalWrite(ledMerah, LOW);
      digitalWrite(ledPutih, HIGH);
      digitalWrite(ledHijau, LOW);
    }
  } 
  else {
    dangerStartTime = 0; 
    isDangerConfirmed = false; 
    
    if (distance >= 6 && distance <= 10) { 
      digitalWrite(ledMerah, LOW);
      digitalWrite(ledPutih, HIGH);
      digitalWrite(ledHijau, LOW);
    } 
    else { 
      digitalWrite(ledMerah, LOW);
      digitalWrite(ledPutih, LOW);
      digitalWrite(ledHijau, HIGH);
    }
  }

  // --- KIRIM DATA KE WEB FATHUR ---
  if ((millis() - lastWebTime) > timerDelayWeb) {
    if(WiFi.status() == WL_CONNECTED) {
      HTTPClient http;
      http.begin(clientSecure, serverName); 
      http.addHeader("Content-Type", "application/json"); 
      
      String jsonPayload = "{\"device_id\":\"" + String(device_id) + "\", \"kecamatan_id\":\"" + String(kecamatan_id) + "\", \"waterLevel\":" + String(distance) + "}"; 
      
      int httpResponseCode = http.POST(jsonPayload);
      
      Serial.print("Kirim Web Fathur | Response code: ");
      Serial.println(httpResponseCode);
      
      http.end();
    }
    lastWebTime = millis();
  }
  
  delay(500); 
}