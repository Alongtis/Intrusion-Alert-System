#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ezButton.h>
#include <WiFi.h>
#include <HTTPClient.h>

// ==========================================
// [1] การตั้งค่าหน้าจอ OLED
// ==========================================
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

// กำหนดขา I2C แยก 2 บัสสำหรับจอคู่ (Address 0x3C ทั้งคู่)
#define I2C1_SDA 21
#define I2C1_SCL 22
#define I2C2_SDA 18
#define I2C2_SCL 19

TwoWire I2C_Bus1 = TwoWire(0);
TwoWire I2C_Bus2 = TwoWire(1);

Adafruit_SSD1306 display1(SCREEN_WIDTH, SCREEN_HEIGHT, &I2C_Bus1, -1); // จอแสดงระยะและเตือนภัย SOS
Adafruit_SSD1306 display2(SCREEN_WIDTH, SCREEN_HEIGHT, &I2C_Bus2, -1); // จอคีย์บอร์ดและพิมพ์ข้อความ

// ==========================================
// [2] การตั้งค่า Wi-Fi และ HTTP Client
// ==========================================
const char *WIFI_SSID = "ESP32-Morse";
const char *WIFI_PASS = "12345678";
const char *SERVER_HOST = "192.168.4.1"; // IP ของฝั่ง Server (ตัวรับ)
const int SERVER_PORT = 80;

// ==========================================
// [3] การกำหนดขาฮาร์ดแวร์และเซนเซอร์
// ==========================================
// Ultrasonic HC-SR04
const int TRIG_PIN = 13;
const int ECHO_PIN = 12;

// โพเทนชิโอมิเตอร์สำหรับปรับเกณฑ์ระยะ (Threshold)
const int POT_PIN = 34;

// สัญญาณไฟแสดงสถานะ 
const int LED_PIN = 5;

// ปุ่มกด 4 ปุ่ม (ใช้งาน ezButton แบบ Active LOW ต่อลง GND)
ezButton btnLeft(15);   // ปุ่มเลื่อนซ้าย
ezButton btnRight(4);   // ปุ่มเลื่อนขวา
ezButton btnUp(16);     // ปุ่มเลื่อนขึ้นแถวบน
ezButton btnAction(17); // ปุ่ม Action (กดสั้น = เลือกตัวอักษร, กดค้าง = ส่งข้อความ)

// ==========================================
// [4] โครงสร้างคีย์บอร์ดและตัวแปรข้อความ
// ==========================================
const char CHAR_LIST[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 <-";
const int TOTAL_CHARS = sizeof(CHAR_LIST) - 1;
const int COLS = 13;

int selectedIndex = 0;
String messageBuffer = "";
const int MAX_MSG_LEN = 16;

// ตัวแปรสำหรับตรวจจับการกดค้างของปุ่ม Action
unsigned long actionPressTime = 0;
bool isLongPressHandled = false;
const unsigned long LONG_PRESS_TIME = 800; // เกณฑ์หน่วงเวลากดค้าง (มิลลิวินาที)

// ==========================================
// [5] ตัวแปรจับเวลาและสถานะ SOS
// ==========================================
unsigned long now = 0;
unsigned long lastSensorRead = 0;

unsigned long sosTimer = 0;
int sosStep = 0;
bool isSOSActive = false;
bool sosSentToServer = false; // แฟล็กป้องกันการส่ง SOS ซ้ำซ้อน

// จังหวะไฟกระพริบ SOS
const int SOS_PATTERN[] = {
    // S (...)
    1, 120, 0, 100, 1, 120, 0, 100, 1, 120, 0, 250,
    // O (---)
    1, 360, 0, 100, 1, 360, 0, 100, 1, 360, 0, 250,
    // S (...)
    1, 120, 0, 100, 1, 120, 0, 100, 1, 120, 0, 600
};
const int SOS_STEPS = sizeof(SOS_PATTERN) / sizeof(SOS_PATTERN[0]);

// ตารางรหัสมอร์ส A-Z และ 0-9
const char *MORSE_TABLE[] = {
    ".-", "-...", "-.-.", "-..", ".", "..-.", "--.", "....", "..",           // A-I
    ".---", "-.-", ".-..", "--", "-.", "---", ".--.", "--.-", ".-.",         // J-R
    "...", "-", "..-", "...-", ".--", "-..-", "-.--", "--..",                 // S-Z
    "-----", ".----", "..---", "...--", "....-", ".....", "-....", "--...", "---..", "----." // 0-9
};

// ==========================================
// [6] ฟังก์ชันแปลงและจัดการรหัสมอร์ส
// ==========================================
String getMorseCode(char c)
{
  if (c >= 'A' && c <= 'Z')
    return MORSE_TABLE[c - 'A'];
  if (c >= 'a' && c <= 'z')
    return MORSE_TABLE[c - 'a'];
  if (c >= '0' && c <= '9')
    return MORSE_TABLE[c - '0' + 26];
  if (c == ' ')
    return "/";
  return "";
}

String buildFullMorsePayload(String text)
{
  String fullMorse = "";
  for (unsigned int i = 0; i < text.length(); i++)
  {
    String code = getMorseCode(text.charAt(i));
    if (code.length() > 0)
    {
      if (fullMorse.length() > 0)
        fullMorse += " ";
      fullMorse += code;
    }
  }
  return fullMorse;
}

// ==========================================
// [7] ฟังก์ชันสื่อสาร Wi-Fi / HTTP
// ==========================================
void sendMorseToServer(String morsePayload)
{
  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("[WiFi] ยังไม่ได้เชื่อมต่อ Wi-Fi ข้ามการส่งข้อมูล");
    return;
  }

  HTTPClient http;
  http.setTimeout(1500); // กำหนด Timeout ไม่ให้บล็อกการทำงานนานเกินไป

  String encodedMorse = "";
  for (unsigned int i = 0; i < morsePayload.length(); i++)
  {
    if (morsePayload[i] == ' ')
      encodedMorse += "%20";
    else
      encodedMorse += morsePayload[i];
  }

  String url = "http://" + String(SERVER_HOST) + ":" + String(SERVER_PORT) + "/send?code=" + encodedMorse;
  Serial.print("[HTTP] กำลังส่งข้อมูลไปที่: ");
  Serial.println(url);

  http.begin(url);
  int httpCode = http.GET();

  if (httpCode > 0)
  {
    Serial.printf("[HTTP] ผลตอบรับสำเร็จ Status Code: %d\n", httpCode);
    String response = http.getString();
    Serial.println("[HTTP] Server Response: " + response);
  }
  else
  {
    Serial.printf("[HTTP] ส่งข้อมูลล้มเหลว Error: %s\n", http.errorToString(httpCode).c_str());
  }
  http.end();
}

// ==========================================
// [8] ฟังก์ชันเซนเซอร์และฮาร์ดแวร์พื้นฐาน
// ==========================================
float readDistanceCM()
{
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, 25000);
  if (duration == 0)
    return 999.0;
  return (duration * 0.0343) / 2.0;
}

void morseDot()
{
  digitalWrite(LED_PIN, HIGH);
  delay(120);
  digitalWrite(LED_PIN, LOW);
  delay(100);
}

void morseDash()
{
  digitalWrite(LED_PIN, HIGH);
  delay(360);
  digitalWrite(LED_PIN, LOW);
  delay(100);
}

// ==========================================
// [9] ฟังก์ชันวาดไอคอน Wi-Fi บน OLED
// ==========================================
// วาดไอคอนสถานะการเชื่อมต่อ Wi-Fi ที่พิกัด (x, y)
void drawWiFiIcon(int x, int y, bool isConnected)
{
  if (isConnected)
  {
    // จุดสัญญาณฐานล่าง
    display2.fillCircle(x + 5, y + 8, 1, SSD1306_WHITE);
    // คลื่นสัญญาณชั้นที่ 1
    display2.drawCircleHelper(x + 5, y + 8, 4, 1 | 2, SSD1306_WHITE);
    // คลื่นสัญญาณชั้นที่ 2
    display2.drawCircleHelper(x + 5, y + 8, 7, 1 | 2, SSD1306_WHITE);
  }
  else
  {
    // สัญลักษณ์เมื่อตัดการเชื่อมต่อ (กากบาท X)
    display2.drawLine(x + 1, y + 1, x + 9, y + 9, SSD1306_WHITE);
    display2.drawLine(x + 1, y + 9, x + 9, y + 1, SSD1306_WHITE);
  }
}

// ==========================================
// [10] ฟังก์ชันวาดกราฟิกบนหน้าจอ OLED
// ==========================================
void drawDisplay1_SOS(float dist, int threshold)
{
  display1.clearDisplay();
  display1.setTextSize(3);
  display1.setTextColor(SSD1306_WHITE);
  display1.setCursor(38, 4);
  display1.print("SOS");

  int yLine = 38;
  display1.fillCircle(14, yLine, 3, SSD1306_WHITE);
  display1.fillCircle(24, yLine, 3, SSD1306_WHITE);
  display1.fillCircle(34, yLine, 3, SSD1306_WHITE);

  display1.fillRect(46, yLine - 2, 10, 5, SSD1306_WHITE);
  display1.fillRect(59, yLine - 2, 10, 5, SSD1306_WHITE);
  display1.fillRect(72, yLine - 2, 10, 5, SSD1306_WHITE);

  display1.fillCircle(90, yLine, 3, SSD1306_WHITE);
  display1.fillCircle(100, yLine, 3, SSD1306_WHITE);
  display1.fillCircle(110, yLine, 3, SSD1306_WHITE);

  display1.setTextSize(1);
  display1.setCursor(8, 52);
  display1.printf("ALERT! %0.1f < %d cm", dist, threshold);
  display1.display();
}

void drawDisplay1_Normal(float dist, int threshold)
{
  display1.clearDisplay();
  display1.setTextColor(SSD1306_WHITE);
  display1.setTextSize(1);
  display1.setCursor(0, 2);
  display1.println("--- DISTANCE RADAR ---");

  display1.setCursor(0, 18);
  display1.print("Threshold: ");
  display1.print(threshold);
  display1.println(" cm");

  display1.setCursor(0, 32);
  display1.print("Distance : ");
  display1.setTextSize(2);
  display1.setCursor(0, 44);
  display1.print(dist, 1);
  display1.setTextSize(1);
  display1.print(" cm");

  display1.drawRect(80, 44, 45, 14, SSD1306_WHITE);
  int fillW = map((int)constrain(dist, 0, 50), 0, 50, 0, 41);
  display1.fillRect(82, 46, fillW, 10, SSD1306_WHITE);

  display1.display();
}

// จอ 2: หน้าจอแป้นพิมพ์ พร้อมไอคอน Wi-Fi ที่มุมขวาบน
void drawDisplay2_Keyboard()
{
  display2.clearDisplay();
  display2.setTextColor(SSD1306_WHITE);

  // แถบแสดงข้อความ
  display2.setTextSize(1);
  display2.setCursor(0, 2);
  display2.print("MSG: ");
  display2.print(messageBuffer.length() > 0 ? messageBuffer : "_");

  // วาดไอคอนสถานะ Wi-Fi ที่มุมขวาบน (x=116, y=1)
  drawWiFiIcon(116, 1, (WiFi.status() == WL_CONNECTED));

  display2.drawLine(0, 12, 127, 12, SSD1306_WHITE);

  char curr = CHAR_LIST[selectedIndex];
  display2.setCursor(0, 16);
  display2.print("SEL:[");
  display2.print(curr);
  display2.print("] ");
  if (curr != '<' && curr != '-')
  {
    display2.print("M:");
    display2.print(getMorseCode(curr));
  }
  else
  {
    display2.print("(BACKSPACE)");
  }

  // วาดตารางคีย์บอร์ด
  int startX = 2;
  int startY = 30;
  for (int i = 0; i < TOTAL_CHARS; i++)
  {
    int row = i / COLS;
    int col = i % COLS;
    int x = startX + col * 9;
    int y = startY + row * 11;

    if (i == selectedIndex)
    {
      display2.fillRect(x - 1, y - 1, 9, 10, SSD1306_WHITE);
      display2.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
    }
    else
    {
      display2.setTextColor(SSD1306_WHITE);
    }

    display2.setCursor(x, y);
    display2.print(CHAR_LIST[i]);
  }

  display2.display();
}

void drawDisplay2_Transmitting(char currentChar, String morseCode)
{
  display2.clearDisplay();
  display2.setTextColor(SSD1306_WHITE);
  display2.setTextSize(1);
  display2.setCursor(0, 2);
  display2.print(">> SENDING MORSE <<");

  // แสดงไอคอน Wi-Fi ขณะกำลังส่งด้วย
  drawWiFiIcon(116, 1, (WiFi.status() == WL_CONNECTED));

  display2.drawLine(0, 12, 127, 12, SSD1306_WHITE);

  display2.setTextSize(3);
  display2.setCursor(10, 24);
  display2.print(currentChar);

  display2.setTextSize(2);
  display2.setCursor(50, 28);
  display2.print(morseCode);

  display2.setTextSize(1);
  display2.setCursor(0, 54);
  display2.print("Full: ");
  display2.print(messageBuffer);

  display2.display();
}

// ==========================================
// [11] การประมวลผลส่งข้อความที่พิมพ์
// ==========================================
void transmitMorseMessage()
{
  if (messageBuffer.length() == 0)
    return;

  String morseToSend = buildFullMorsePayload(messageBuffer);
  sendMorseToServer(morseToSend);

  for (unsigned int i = 0; i < messageBuffer.length(); i++)
  {
    char c = messageBuffer.charAt(i);
    String code = getMorseCode(c);

    drawDisplay2_Transmitting(c, code);

    if (c == ' ')
    {
      delay(400);
      continue;
    }

    for (unsigned int j = 0; j < code.length(); j++)
    {
      if (code[j] == '.')
      {
        morseDot();
      }
      else if (code[j] == '-')
      {
        morseDash();
      }
    }
    delay(250);
  }

  messageBuffer = "";
  drawDisplay2_Keyboard();
}

// ==========================================
// [12] การจัดการไฟกะพริบ SOS (Non-blocking)
// ==========================================
void handleNonBlockingSOS()
{
  if (!isSOSActive && sosStep == 0)
  {
    digitalWrite(LED_PIN, LOW);
    return;
  }

  unsigned long currentMillis = millis();
  if (currentMillis - sosTimer >= SOS_PATTERN[sosStep + 1])
  {
    sosTimer = currentMillis;
    sosStep += 2;
    if (sosStep >= SOS_STEPS)
    {
      sosStep = 0;
    }
    int state = SOS_PATTERN[sosStep];
    digitalWrite(LED_PIN, state ? HIGH : LOW);
  }
}

// ==========================================
// [13] ฟังก์ชันเริ่มต้นระบบ (setup)
// ==========================================
void setup()
{
  Serial.begin(115200);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT);
  pinMode(POT_PIN, INPUT);
  digitalWrite(LED_PIN, LOW);

  pinMode(15, INPUT_PULLUP);
  pinMode(4, INPUT_PULLUP);
  pinMode(16, INPUT_PULLUP);
  pinMode(17, INPUT_PULLUP);

  btnLeft.setDebounceTime(100);
  btnRight.setDebounceTime(100);
  btnUp.setDebounceTime(100);
  btnAction.setDebounceTime(100);

  I2C_Bus1.begin(I2C1_SDA, I2C1_SCL, 400000);
  I2C_Bus2.begin(I2C2_SDA, I2C2_SCL, 400000);

  if (!display1.begin(SSD1306_SWITCHCAPVCC, 0x3C))
    Serial.println(F("เริ่มต้นจอ OLED 1 ไม่สำเร็จ"));
  if (!display2.begin(SSD1306_SWITCHCAPVCC, 0x3C))
    Serial.println(F("เริ่มต้นจอ OLED 2 ไม่สำเร็จ"));

  display1.clearDisplay();
  display2.clearDisplay();

  display2.setTextSize(1);
  display2.setTextColor(SSD1306_WHITE);
  display2.setCursor(0, 10);
  display2.println("Connecting WiFi...");
  display2.println(WIFI_SSID);
  display2.display();

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 16)
  {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.println("\n[WiFi] เชื่อมต่อสำเร็จ IP: " + WiFi.localIP().toString());
  }
  else
  {
    Serial.println("\n[WiFi] เชื่อมต่อล้มเหลว กำลังทำงานในโหมด Offline...");
  }

  drawDisplay2_Keyboard();
}

// ==========================================
// [14] วงรอบการทำงานหลัก (loop)
// ==========================================
void loop()
{
  // ตรวจจับสถานะการเชื่อมต่อ Wi-Fi ทุกๆ 5 วินาที หากมีการเปลี่ยนแปลงให้อัปเดตหน้าจอทันที
  static wl_status_t lastWiFiStatus = WL_DISCONNECTED;
  static unsigned long lastWiFiCheck = 0;
  if (millis() - lastWiFiCheck > 5000)
  {
    lastWiFiCheck = millis();
    wl_status_t currentStatus = WiFi.status();
    if (currentStatus != WL_CONNECTED)
    {
      WiFi.reconnect();
    }
    if (currentStatus != lastWiFiStatus)
    {
      lastWiFiStatus = currentStatus;
      drawDisplay2_Keyboard(); // รีเฟรชหน้าจอเพื่ออัปเดตไอคอน Wi-Fi
    }
  }

  btnLeft.loop();
  btnRight.loop();
  btnUp.loop();
  btnAction.loop();

  // --- ปุ่มที่ 1: เลื่อนซ้าย ---
  if (btnLeft.isPressed())
  {
    selectedIndex = (selectedIndex - 1 + TOTAL_CHARS) % TOTAL_CHARS;
    drawDisplay2_Keyboard();
  }

  // --- ปุ่มที่ 2: เลื่อนขวา ---
  if (btnRight.isPressed())
  {
    selectedIndex = (selectedIndex + 1) % TOTAL_CHARS;
    drawDisplay2_Keyboard();
  }

  // --- ปุ่มที่ 3: เลื่อนขึ้นแถวบน ---
  if (btnUp.isPressed())
  {
    if (selectedIndex >= COLS)
    {
      selectedIndex -= COLS;
    }
    else
    {
      int targetIndex = selectedIndex + (COLS * 2);
      if (targetIndex >= TOTAL_CHARS)
        targetIndex = TOTAL_CHARS - 1;
      selectedIndex = targetIndex;
    }
    drawDisplay2_Keyboard();
  }

  // --- ปุ่มที่ 4: Action ---
  if (btnAction.isPressed())
  {
    actionPressTime = millis();
    isLongPressHandled = false;
  }

  if (btnAction.getState() == LOW)
  {
    if (!isLongPressHandled && (millis() - actionPressTime >= LONG_PRESS_TIME))
    {
      isLongPressHandled = true;
      transmitMorseMessage();
    }
  }

  if (btnAction.isReleased())
  {
    if (!isLongPressHandled)
    {
      char c = CHAR_LIST[selectedIndex];
      if (c == '<' || c == '-')
      {
        if (messageBuffer.length() > 0)
          messageBuffer.remove(messageBuffer.length() - 1);
      }
      else
      {
        if (messageBuffer.length() < MAX_MSG_LEN)
          messageBuffer += c;
      }
      drawDisplay2_Keyboard();
    }
  }

  handleNonBlockingSOS();

  // ตรวจวัดระยะทางเซนเซอร์ทุก 200 ms
  now = millis();
  if (now - lastSensorRead > 200)
  {
    lastSensorRead = now;

    int rawPot = analogRead(POT_PIN);
    int threshold = map(rawPot, 0, 4095, 5, 50);
    float distance = readDistanceCM();

    if (distance > 0 && distance < threshold)
    {
      isSOSActive = true;
      if (!sosSentToServer)
      {
        sendMorseToServer("... --- ...");
        sosSentToServer = true;
      }
    }
    else
    {
      isSOSActive = false;
      sosSentToServer = false;
    }

    if (isSOSActive || sosStep != 0)
    {
      drawDisplay1_SOS(distance, threshold);
    }
    else
    {
      drawDisplay1_Normal(distance, threshold);
    }
  }
}