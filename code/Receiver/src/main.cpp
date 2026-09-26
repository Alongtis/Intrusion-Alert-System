#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ==============================================================================
// [1] การตั้งค่าหน้าจอ OLED SSD1306
// ==============================================================================
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
#define SCREEN_ADDRESS 0x3C // I2C Address มาตรฐาน (0x3C หรือ 0x3D)

// กำหนดพินบัส I2C สำหรับหน้าจอ OLED
#define I2C_SDA 21
#define I2C_SCL 22

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
WebServer server(80);

// ==============================================================================
// [2] การกำหนดขาฮาร์ดแวร์และเสียง Buzzer
// ==============================================================================
#define BUZZER_PIN 14 // ขาควบคุม Buzzer (สามารถเปลี่ยนพินได้ตามที่ต่อใช้งานจริง)

// ความยาวคลื่นเสียงของรหัสมอร์ส (หน่วย: มิลลิวินาที)
#define DOT_DURATION  100  // ความยาวเสียงจุด (.)
#define DASH_DURATION 300  // ความยาวเสียงขีด (-)
#define ELEMENT_GAP   80   // เว้นวรรคระหว่างจุด/ขีดในตัวเดียวกัน

// ==============================================================================
// [3] การตั้งค่าโหมด Access Point (Hotspot)
// ==============================================================================
const char *ssid = "ESP32-Morse";
const char *password = "12345678";

// ==============================================================================
// [4] โครงสร้างข้อมูลและตารางถอดรหัสมอร์ส (Morse Code Dictionary)
// ==============================================================================
struct MorseDict {
  const char* code;
  char letter;
};

// ตารางจับคู่รหัสมอร์สสากล A-Z, 0-9 และเว้นวรรค
const MorseDict morseTable[] = {
  {".-", 'A'},   {"-...", 'B'}, {"-.-.", 'C'}, {"-..", 'D'},
  {".", 'E'},    {"..-.", 'F'}, {"--.", 'G'},  {"....", 'H'},
  {"..", 'I'},   {".---", 'J'}, {"-.-", 'K'},  {".-..", 'L'},
  {"--", 'M'},   {"-.", 'N'},   {"---", 'O'},  {".--.", 'P'},
  {"--.-", 'Q'}, {".-.", 'R'},  {"...", 'S'},  {"-", 'T'},
  {"..-", 'U'},  {"...-", 'V'}, {".--", 'W'},  {"-..-", 'X'},
  {"-.--", 'Y'}, {"--..", 'Z'},
  {"-----", '0'}, {".----", '1'}, {"..---", '2'}, {"...--", '3'},
  {"....-", '4'}, {".....", '5'}, {"-....", '6'}, {"--...", '7'},
  {"---..", '8'}, {"----.", '9'}, {"/", ' '}     // ใช้เครื่องหมาย / แทน Space
};
const int morseTableLen = sizeof(morseTable) / sizeof(morseTable[0]);

// ==============================================================================
// [5] ฟังก์ชันควบคุมเสียง Buzzer
// ==============================================================================
// ส่งเสียงแบบจุด (.) สั้น
void playMorseDot() {
  digitalWrite(BUZZER_PIN, HIGH);
  delay(DOT_DURATION);
  digitalWrite(BUZZER_PIN, LOW);
  delay(ELEMENT_GAP);
}

// ส่งเสียงแบบขีด (-) ยาว
void playMorseDash() {
  digitalWrite(BUZZER_PIN, HIGH);
  delay(DASH_DURATION);
  digitalWrite(BUZZER_PIN, LOW);
  delay(ELEMENT_GAP);
}

// ขับเสียง Buzzer ตามสตริงรหัสมอร์สของ 1 ตัวอักษร เช่น "...-"
void playMorseSound(String token) {
  for (unsigned int i = 0; i < token.length(); i++) {
    if (token[i] == '.') {
      playMorseDot();
    } else if (token[i] == '-') {
      playMorseDash();
    }
  }
}

// ==============================================================================
// [6] ฟังก์ชันแปลงรหัสมอร์สกลับเป็นตัวอักษร
// ==============================================================================
// ถอดรหัสมอร์ส 1 ชุดเป็นตัวอักษรเดี่ยว
char morseToChar(String token) {
  token.trim();
  for (int i = 0; i < morseTableLen; i++) {
    if (token.equals(morseTable[i].code)) {
      return morseTable[i].letter;
    }
  }
  return '?'; // หากไม่ตรงกับตารางให้แสดงเครื่องหมาย ?
}

// ถอดรหัสข้อความยาวทั้งหมด (คั่นระหว่างตัวอักษรด้วย Space)
String decodeFullMorse(String morseStr) {
  String decoded = "";
  String currentToken = "";
  
  for (int i = 0; i <= morseStr.length(); i++) {
    char c = (i < morseStr.length()) ? morseStr.charAt(i) : ' ';
    if (c == ' ') {
      if (currentToken.length() > 0) {
        decoded += morseToChar(currentToken);
        currentToken = "";
      }
    } else {
      currentToken += c;
    }
  }
  return decoded;
}

// ==============================================================================
// [7] ฟังก์ชันประมวลผล แสดงผล OLED และส่งเสียง Buzzer
// ==============================================================================
void processAndDisplay(String morseInput) {
  morseInput.trim();
  
  // ----------------------------------------------------
  // ขั้นตอนที่ 1: วนแสดงผลทีละตัวอักษร + ส่งเสียง Buzzer
  // ----------------------------------------------------
  int startIdx = 0;
  while (startIdx < morseInput.length()) {
    int nextSpace = morseInput.indexOf(' ', startIdx);
    if (nextSpace == -1) nextSpace = morseInput.length();
    
    String token = morseInput.substring(startIdx, nextSpace);
    token.trim();
    
    if (token.length() > 0) {
      char decodedChar = morseToChar(token);

      // วาดหน้าจอแสดงตัวอักษรด้านบน และรหัสมอร์สด้านล่าง
      display.clearDisplay();
      
      // ส่วนหัวสถานะ
      display.setTextSize(1);
      display.setCursor(0, 0);
      display.print("Receiving...");
      display.drawLine(0, 10, 127, 10, SSD1306_WHITE);

      // ตัวอักษรเดี่ยวขนาดใหญ่ด้านบน
      display.setTextSize(3);
      display.setCursor(56, 16);
      display.print(decodedChar == ' ' ? '_' : decodedChar);

      // ชุดรหัสมอร์สด้านล่าง
      display.setTextSize(2);
      display.setCursor(10, 46);
      display.print(token);
      
      display.display();

      // ส่งเสียง Buzzer ตามรหัสของตัวนั้น (ถ้าเป็นเว้นวรรคไม่ต้องส่งเสียง)
      unsigned long soundStart = millis();
      if (decodedChar != ' ') {
        playMorseSound(token);
      }

      // คำนวณเวลาที่เหลือ เพื่อให้หน้าจอค้างรวมครบ 1 วินาที (1000 ms) ต่อตัว
      unsigned long soundElapsed = millis() - soundStart;
      if (soundElapsed < 1000) {
        delay(1000 - soundElapsed);
      }
    }
    
    startIdx = nextSpace + 1;
  }
  
  // ----------------------------------------------------
  // ขั้นตอนที่ 2: ถอดรหัสข้อความเต็มทั้งหมด
  // ----------------------------------------------------
  String fullText = decodeFullMorse(morseInput);
  
  // ----------------------------------------------------
  // ขั้นตอนที่ 3: แสดงผลข้อความทั้งหมดค้างไว้บนจอจนกว่าจะมีค่าใหม่
  // ----------------------------------------------------
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("Received (Full Msg):");
  display.drawLine(0, 10, 127, 10, SSD1306_WHITE);
  
  display.setTextSize(2);
  display.setCursor(0, 30);
  display.println(fullText);
  display.display();
}

// ==============================================================================
// [8] ฟังก์ชันจัดการคำขอ Web Server (HTTP Route Handler)
// ==============================================================================
void handleMorseEndpoint() {
  if (server.hasArg("code")) {
    String morseData = server.arg("code");
    Serial.println("[Server] รับรหัสมอร์ส: " + morseData);

    // ตอบกลับ Status 200 OK ให้ตัวส่งทันทีเพื่อป้องกัน Timeout
    server.send(200, "text/plain; charset=utf-8", "OK: Received");
    
    // เริ่มกระบวนการส่งเสียงและแสดงผลบนจอ OLED
    processAndDisplay(morseData);
  } else {
    server.send(400, "text/plain", "Error: Missing 'code' parameter");
  }
}

// ==============================================================================
// [9] ฟังก์ชันกำหนดค่าเริ่มต้นระบบ (setup)
// ==============================================================================
void setup() {
  Serial.begin(115200);

  // กำหนดโหมดขา Buzzer
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  // เริ่มต้นบัส I2C ตามขาที่ระบุ
  Wire.begin(I2C_SDA, I2C_SCL);

  // เริ่มต้นจอแสดงผล OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("เริ่มต้นจอ OLED SSD1306 ไม่สำเร็จ"));
    for (;;);
  }
  display.setTextColor(SSD1306_WHITE);
  display.clearDisplay();

  // เริ่มการปล่อยสัญญาณ Hotspot (Wi-Fi Access Point)
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid, password);
  IPAddress IP = WiFi.softAPIP();

  // แสดงผลหน้าจอ Standby พร้อมรับข้อมูล
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("AP: " + String(ssid));
  display.println("IP: " + IP.toString());
  display.println("Status: Standby...");
  display.display();

  // กำหนดเส้นทาง URL Endpoint รับข้อมูล
  server.on("/send", HTTP_GET, handleMorseEndpoint);
  server.begin();
  Serial.println("[Server] เริ่มต้น Web Server สำเร็จ พร้อมรับข้อมูลที่พอร์ต 80");
}

// ==============================================================================
// [10] วงรอบการทำงานหลัก (loop)
// ==============================================================================
void loop() {
  // คอยตรวจจับและรับ Request ที่ส่งเข้ามาทาง Wi-Fi
  server.handleClient();
}