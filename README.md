# 📡 ESP32 Dual-Node Morse Communication & Ultrasonic Radar System

ระบบสื่อสารรหัสมอร์สไร้สายและตรวจจับสิ่งกีดขวางระยะใกล้ด้วย ESP32 จำนวน 2 บอร์ด โดยแบ่งการทำงานเป็นโหนดส่งข้อมูล (**Transmitter Node**) ที่มาพร้อมคีย์บอร์ดเสมือนและเรดาร์อัลตราโซนิก และโหนดรับข้อมูล (**Receiver Node**) ที่ทำหน้าที่เป็น Wi-Fi Access Point ถอดรหัสและแสดงสัญญาณเสียง/ภาพ

---

## 📌 ภาพรวมสถาปัตยกรรมระบบ (System Architecture)
+-------------------------------------------------------------+
|              Node 1: Transmitter (Client / STA)             |
|                                                             |
|  [Ultrasonic + Pot] ----> Distance Check ----> Trigger SOS  |
|  [4x Buttons]       ----> Virtual Keyboard -> Send Morse    |
|  [Dual OLED]        ----> Radar (0x3C) & UI (0x3C via 2-Bus)|
+-------------------------------------------------------------+
|
HTTP GET via Wi-Fi AP
(Endpoint: /send?code=...)
v
+-------------------------------------------------------------+
|               Node 2: Receiver (Server / AP)                |
|                                                             |
|  [SoftAP Mode]      ----> "ESP32-Morse" (192.168.4.1)       |
|  [Web Server]       ----> Instant 200 OK (Non-blocking)     |
|  [Buzzer]           ----> Dot/Dash Sound Playback           |
|  [Single OLED]      ----> Step-by-Step & Full Message       |
+-------------------------------------------------------------+


---

## ✨ ฟีเจอร์หลัก (Key Features)

### 1. โหนดส่ง (Transmitter Node)
- **Virtual Keyboard (OLED 2):** เลือกตัวอักษร A-Z, 0-9 และ Backspace ด้วยระบบ 4 ปุ่มกด
  - กดสั้นเพื่อพิมพ์ / กดค้างที่ปุ่ม Action (> 800ms) เพื่อส่งข้อความ
- **Dynamic Wi-Fi Status Icon:** แสดงสถานะการเชื่อมต่อ Wi-Fi และระดับ RSSI แบบเรียลไทม์ที่มุมขวาบนของหน้าจอ
- **Ultrasonic Radar & Auto-SOS (OLED 1):** 
  - วัดระยะด้วย HC-SR04 และปรับค่า Threshold ผ่าน Potentiometer
  - ส่งรหัสฉุกเฉิน `... --- ...` (SOS) ไปยังโหนดรับอัตโนมัติทันที 1 ครั้งเมื่อมีวัตถุกีดขวางระยะปลอดภัย
- **Dual I2C Bus Management:** ใช้งานหน้าจอ OLED Address `0x3C` พร้อมกัน 2 จอได้อย่างอิสระ ผ่านฮาร์ดแวร์ `TwoWire(0)` และ `TwoWire(1)`

### 2. โหนดรับ (Receiver Node)
- **Stand-alone Access Point:** ปล่อยสัญญาณ Wi-Fi Hotspot ในตัว ไม่ต้องพึ่งพาเราเตอร์ภายนอก
- **Non-blocking Request Handling:** ตอบรับสถานะ HTTP 200 OK ทันที เพื่อป้องกันไม่ให้โหนดส่งติดค้างในลูป
- **Synchronized Audio & Visual Output:**
  - แสดงตัวอักษรขนาดใหญ่ด้านบนและรหัสมอร์สด้านล่าง ค้างไว้ตัวละ 1 วินาที
  - ขับสัญญาณเสียงผ่าน Buzzer แยกจังหวะ จุด (Dot) และ ขีด (Dash) อัตโนมัติ
  - สลับไปแสดงข้อความที่ถอดรหัสเสร็จสมบูรณ์ค้างไว้จนกว่าจะมีชุดข้อมูลใหม่เข้ามา

---

## 🛠 ข้อมูลการต่อวงจร (Pinout Configuration)

### Node 1: Transmitter (ตัวส่ง)
| อุปกรณ์ / เซนเซอร์ | ขาอุปกรณ์ | ขา ESP32 (GPIO) | หมายเหตุ |
| :--- | :--- | :--- | :--- |
| **OLED 1 (Radar)** | SDA / SCL | GPIO 21 / GPIO 22 | I2C Bus 0 (Address: 0x3C) |
| **OLED 2 (Keyboard)** | SDA / SCL | GPIO 18 / GPIO 19 | I2C Bus 1 (Address: 0x3C) |
| **HC-SR04** | TRIG / ECHO | GPIO 13 / GPIO 12 | Ultrasonic Distance Sensor |
| **Potentiometer** | Output Pin | GPIO 34 | Analog In (ADC1_CH6) |
| **Alert LED** | Anode (+) | GPIO 5 | ผ่านตัวต้านทาน 220-330Ω |
| **Button Left** | Data Pin | GPIO 15 | Active LOW (Internal Pull-up) |
| **Button Right** | Data Pin | GPIO 4 | Active LOW (Internal Pull-up) |
| **Button Up** | Data Pin | GPIO 16 | Active LOW (Internal Pull-up) |
| **Button Action** | Data Pin | GPIO 17 | Active LOW (Internal Pull-up) |
```mermaidflowchart TB
    subgraph NODE1["Node 1: Transmitter (STA)"]
        direction TB
        ESP1["ESP32 Core (Node 1)"]
        US["Ultrasonic HC-SR04"] -->|ECHO via Divider| ESP1
        ESP1 -->|TRIG| US
        POT["Potentiometer"] -->|Threshold ADC| ESP1
        BTN["4x Push Buttons"] -->|GPIO Control| ESP1
        ESP1 -->|I2C Bus 0| OLED1["OLED 1 (Radar)"]
        ESP1 -->|I2C Bus 1| OLED2["OLED 2 (Keyboard)"]
        ESP1 -->|Alert Pin| LED["LED Indicator"]
    end

    subgraph NODE2["Node 2: Receiver (AP)"]
        direction TB
        ESP2["ESP32 Core (Node 2)"]
        ESP2 -->|I2C Bus| OLED_RX["OLED (Received Msg)"]
        ESP2 -->|GPIO 14| BUZZ["Buzzer Driver"]
    end

    ESP1 -.->|"Wi-Fi HTTP GET (/send?code=...)"| ESP2

    classDef mcu fill:#1f77b4,stroke:#fff,stroke-width:2px,color:#fff;
    classDef display fill:#ff7f0e,stroke:#fff,stroke-width:1px,color:#fff;
    classDef sensor fill:#2ca02c,stroke:#fff,stroke-width:1px,color:#fff;
    class ESP1,ESP2 mcu;
    class OLED1,OLED2,OLED_RX display;
    class US,POT,BTN,LED,BUZZ sensor;```mermaid
### Node 2: Receiver (ตัวรับ)
| อุปกรณ์ / เซนเซอร์ | ขาอุปกรณ์ | ขา ESP32 (GPIO) | หมายเหตุ |
| :--- | :--- | :--- | :--- |
| **OLED (Display)** | SDA / SCL | GPIO 21 / GPIO 22 | I2C Default Bus (Address: 0x3C) |
| **Buzzer** | Positive (+) | GPIO 14 | Active หรือ Passive Buzzer |

---

## 📦 ไลบรารีที่จำเป็น (Required Libraries)

ติดตั้งไลบรารีผ่าน **Arduino Library Manager** หรือ `platformio.ini`:
- `Adafruit SSD1306` (โดย Adafruit)
- `Adafruit GFX Library` (โดย Adafruit)
- `ezButton` (โดย ArduinoGetStarted)
- `WiFi` & `WebServer` & `HTTPClient` (มีมาพร้อมกับบอร์ด ESP32 Core)

---

## 🚀 ขั้นตอนการใช้งาน (Quick Start)

1. **อัปโหลดเฟิร์มแวร์:**
   - แฟลชโค้ดตัวรับลงใน **ESP32 Node 2**
   - แฟลชโค้ดตัวส่งลงใน **ESP32 Node 1**
2. **เปิดเครื่อง:**
   - เริ่มการทำงานของ Node 2 ก่อน เพื่อให้ระบบเริ่มกระจายสัญญาณ Hotspot `ESP32-Morse`
   - เปิด Node 1 บอร์ดจะทำการต่อ Wi-Fi เข้ากับ Node 2 อัตโนมัติ (สังเกตไอคอน Wi-Fi มุมขวาบนของ OLED 2)
3. **การส่งข้อความ:**
   - เลื่อนปุ่มเลือกตัวอักษร แล้วกดปุ่ม Action สั้นๆ เพื่อป้อนตัวอักษรเข้า Buffer
   - กดปุ่ม Action ค้างไว้ 800 ms เพื่อส่งข้อความทั้งหมด
4. **การทำงานของระบบความปลอดภัย:**
   - ปรับ Potentiometer เพื่อตั้งระยะ Threshold (5 - 50 cm)
   - หากระยะที่วัดได้ต่ำกว่าเกณฑ์ ไฟ LED บนตัวส่งจะเริ่มกะพริบรหัส SOS พร้อมยิงสัญญาณเตือนภัยไปยัง Node 2 ทันที
