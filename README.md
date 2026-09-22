# 📡 ESP32 Intrusion Alert System

ระบบสื่อสารรหัสมอร์สไร้สายและตรวจจับสิ่งกีดขวางระยะใกล้ด้วย ESP32 จำนวน 2 บอร์ด โดยแบ่งการทำงานเป็นโหนดส่งข้อมูล (**Transmitter Node**) ที่มาพร้อมคีย์บอร์ดเสมือนและเรดาร์อัลตราโซนิก และโหนดรับข้อมูล (**Receiver Node**) ที่ทำหน้าที่เป็น Wi-Fi Access Point ถอดรหัสและแสดงสัญญาณเสียง/ภาพ

---
## 📖 ภาพรวมของโปรเจกต์ (Project Overview)

โปรเจกต์นี้เป็นการพัฒนาระบบสื่อสารรหัสมอร์สไร้สายแบบสองทิศทางจำลอง และระบบเตือนภัยเรดาร์ระยะใกล้ด้วยไมโครคอนโทรลเลอร์ **ESP32 จำนวน 2 บอร์ด** ทำงานประสานกันผ่านเครือข่าย Wi-Fi ท้องถิ่น (Local Wi-Fi Network) โดยไม่จำเป็นต้องพึ่งพาเราเตอร์หรืออินเทอร์เน็ตภายนอก

* **Node 1 (Transmitter / Client):** ทำหน้าที่เป็นสถานีตรวจจับและควบคุมอินพุต ติดตั้งเซนเซอร์ Ultrasonic HC-SR04 สำหรับสแกนระยะวัตถุแบบเรียลไทม์ควบคู่กับ Potentiometer เพื่อปรับเปลี่ยนระยะความปลอดภัย (Threshold) พร้อมระบบแป้นพิมพ์จำลอง 4 ปุ่ม (Up, Left, Right, Action) ที่แสดงผลบนหน้าจอคู่ (Dual OLED SSD1306 บนบัส I2C0 และ I2C1) เมื่อตรวจพบสิ่งกีดขวางจะทำการยิงรหัส SOS อัตโนมัติ หรือผู้ใช้งานสามารถพิมพ์ข้อความเพื่อส่งรหัสมอร์สผ่าน HTTP Request ได้
* **Node 2 (Receiver / Access Point):** ทำหน้าที่เป็น Wi-Fi Hotspot ("ESP32-Morse") และ Web Server เมื่อได้รับรหัสผ่านพอร์ต HTTP GET `/send?code=...` ระบบจะตอบรับสัญญาณทันทีแบบ Non-blocking จากนั้นประมวลผลถอดรหัสมอร์ส แสดงผลทีละตัวอักษรบนหน้าจอ OLED ค้างไว้ 1 วินาที พร้อมสั่งขับเสียง Active/Passive Buzzer แยกจังหวะจุดและขีดอย่างแม่นยำ ก่อนจะสรุปข้อความทั้งหมดค้างไว้บนหน้าจอ

---

## 📌 ภาพรวมสถาปัตยกรรมระบบ (System Architecture)

```mermaid
flowchart TD
    subgraph NODE1["Node 1: Transmitter (STA)"]
        ESP1["ESP32 Core (Node 1)"]
        POT["Potentiometer"] -->|Threshold ADC| ESP1
        BTN["4x Push Buttons"] -->|GPIO Control| ESP1
        ESP1 -->|TRIG| US["Ultrasonic HC-SR04"]
        US -->|ECHO via Divider| ESP1
        ESP1 -->|I2C Bus 0| OLED1["OLED 1 (Radar)"]
        ESP1 -->|I2C Bus 1| OLED2["OLED 2 (Keyboard)"]
        ESP1 -->|Alert Pin| LED["LED Indicator"]
    end

    NODE1 ==>|"Wi-Fi HTTP GET (/send?code=...)"| NODE2

    subgraph NODE2["Node 2: Receiver (AP)"]
        ESP2["ESP32 Core (Node 2)"]
        ESP2 -->|I2C Bus| OLED_RX["OLED (Received Msg)"]
        ESP2 -->|GPIO 14| BUZZ["Buzzer Driver"]
    end

    classDef mcu fill:#1f77b4,stroke:#fff,stroke-width:2px,color:#fff;
    classDef display fill:#ff7f0e,stroke:#fff,stroke-width:1px,color:#fff;
    classDef sensor fill:#2ca02c,stroke:#fff,stroke-width:1px,color:#fff;
    class ESP1,ESP2 mcu;
    class OLED1,OLED2,OLED_RX display;
    class US,POT,BTN,LED,BUZZ sensor;
```


---

## ✨ ฟีเจอร์หลัก (Key Features)

### 1. โหนดส่ง (Transmitter Node)
- **Virtual Keyboard (OLED 2):** เลือกตัวอักษร A-Z, 0-9 และ Backspace ด้วยระบบ 4 ปุ่มกด
  - กดสั้นเพื่อพิมพ์ / กดค้างที่ปุ่ม Action (> 800ms) เพื่อส่งข้อความ
- **Dynamic Wi-Fi Status Icon:** แสดงสถานะการเชื่อมต่อ Wi-Fi และระดับ RSSI แบบเรียลไทม์ที่มุมขวาบนของหน้าจอ (ไอคอนจะหายไปทันทีเมื่อสัญญาณขาดหาย)
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
| **OLED 1 (Radar)** | SDA / SCL | GPIO 21 / GPIO 22 | I2C Bus 0 (Address: `0x3C`) |
| **OLED 2 (Keyboard)** | SDA / SCL | GPIO 18 / GPIO 19 | I2C Bus 1 (Address: `0x3C`) |
| **HC-SR04** | TRIG / ECHO | GPIO 13 / GPIO 12 | ECHO ผ่าน Voltage Divider (1kΩ / 2kΩ) |
| **Potentiometer** | Output Pin | GPIO 34 | Analog In (ADC1_CH6) ช่วงแรงดัน 0-3.3V |
| **Alert LED** | Anode (+) | GPIO 5 | ต่ออนุกรมตัวต้านทาน 220-330Ω |
| **Button Left** | Data Pin | GPIO 15 | Active LOW (Internal Pull-up) |
| **Button Right** | Data Pin | GPIO 4 | Active LOW (Internal Pull-up) |
| **Button Up** | Data Pin | GPIO 16 | Active LOW (Internal Pull-up) |
| **Button Action** | Data Pin | GPIO 17 | Active LOW (Internal Pull-up) |

### Node 2: Receiver (ตัวรับ)
| อุปกรณ์ / เซนเซอร์ | ขาอุปกรณ์ | ขา ESP32 (GPIO) | หมายเหตุ |
| :--- | :--- | :--- | :--- |
| **OLED (Display)** | SDA / SCL | GPIO 21 / GPIO 22 | I2C Default Bus (Address: `0x3C`) |
| **Buzzer** | Positive (+) | GPIO 14 | ต่อผ่านทรานซิสเตอร์ขับโหลด หรือต่อโมดูล Active Buzzer |

---

## 📦 ไลบรารีที่จำเป็น (Required Libraries)

ติดตั้งไลบรารีผ่าน **Arduino Library Manager** หรือ `platformio.ini`:
- `Adafruit SSD1306` (โดย Adafruit)
- `Adafruit GFX Library` (โดย Adafruit)
- `ezButton` (โดย ArduinoGetStarted)
- `WiFi`, `WebServer`, `HTTPClient` (มีมาพร้อมกับบอร์ด ESP32 Core)

---

## 🚀 ขั้นตอนการใช้งาน (Quick Start)

1. **อัปโหลดเฟิร์มแวร์:**
   - แฟลชโค้ดตัวรับลงใน **ESP32 Node 2**
   - แฟลชโค้ดตัวส่งลงใน **ESP32 Node 1**
2. **เปิดเครื่อง:**
   - เริ่มการทำงานของ Node 2 ก่อน เพื่อให้ระบบเริ่มกระจายสัญญาณ Hotspot `ESP32-Morse`
   - เปิด Node 1 บอร์ดจะทำการต่อ Wi-Fi เข้ากับ Node 2 อัตโนมัติ (สังเกตไอคอน Wi-Fi มุมขวาบนของ OLED 2)
3. **การส่งข้อความ:**
   - เลื่อนปุ่มซ้าย/ขวา/ขึ้น เพื่อเลือกตัวอักษร
   - กดปุ่ม Action สั้นๆ เพื่อพิมพ์ตัวอักษรลง Buffer
   - กดปุ่ม Action ค้างไว้ 800 ms เพื่อส่งข้อความทั้งหมด
4. **การทำงานของระบบความปลอดภัย:**
   - ปรับ Potentiometer เพื่อตั้งระยะ Threshold (5 - 50 cm)
   - หากระยะที่วัดได้ต่ำกว่าเกณฑ์ ไฟ LED บนตัวส่งจะเริ่มกะพริบรหัส SOS พร้อมยิงสัญญาณเตือนภัยไปยัง Node 2 ทันที 1 ครั้ง

---
## 📷 IMAGE HARDWARE

| Node 1: Transmitter (ตัวส่ง) | Node 2: Receiver (ตัวรับ) |
| :---: | :---: |
| <img src="https://github.com/user-attachments/assets/32a707c5-ddec-46e0-829f-806725102c35" alt="Node 1 Transmitter" width="380" /> | <img src="https://github.com/user-attachments/assets/2f4e68d8-11ab-4fdd-b42a-5be6008c924b" alt="Node 2 Receiver" width="380" /> |

---

## 🎥 Video Demonstration

| Video 1: Transmission | Video 2: Reception |
| :---: | :---: |
| https://github.com/user-attachments/assets/db6be04e-9f73-46ef-90a6-82b5907f0424 | https://github.com/user-attachments/assets/6cf24224-a0fb-4488-9bad-6ed21e289395 |

---

## 📑 รายการเอกสารทางเทคนิค (Component Datasheets)

รวบรวม Datasheet สำหรับอุปกรณ์อิเล็กทรอนิกส์และโมดูลทั้งหมดที่ใช้งานในระบบ:

| อุปกรณ์ / โมดูล | ฟังก์ชันในวงจร | เอกสารอ้างอิงทางเทคนิค (Datasheet) |
| :--- | :--- | :--- |
| **ESP32 (ESP-WROOM-32)** | ไมโครคอนโทรลเลอร์ Dual-Core สื่อสาร Wi-Fi/BLE | [Espressif ESP-WROOM-32 Datasheet](https://documentation.espressif.com/esp32-wroom-32_datasheet_en.pdf) |
| **SSD1306** | ตัวควบคุมจอแสดงผลกราฟิก Monochrome 128x64 OLED | [Solomon Systech SSD1306 Datasheet](https://cdn-shop.adafruit.com/datasheets/SSD1306.pdf) |
| **HC-SR04** | เซนเซอร์วัดระยะทางอัลตราโซนิกความแม่นยำสูง | [SparkFun HC-SR04 User Manual & Datasheet](https://www.alldatasheet.com/datasheet-pdf/view/1132204/ETC2/HCSR04.html) |

---


## 📑 เอกสารและคู่มือการใช้งาน (Documentation & Manual)

สามารถศึกษาคู่มือการใช้งานระบบ รายละเอียดโครงงาน และขั้นตอนการทดสอบแบบเต็มได้ที่เอกสาร Microsoft Sway:

> 🔗 **เข้าสู่คู่มือออนไลน์:** [คลิกที่นี่เพื่อเปิดคู่มือ (ESP32 Morse & Radar System Manual)](https://sway.cloud.microsoft/r6qcSzhI2a8yqh35?ref=Link)

---
## 🏗️ บล็อกไดอะแกรมระบบ (System Block Diagram)

```mermaid
flowchart TD
    subgraph NODE1["Node 1: Transmitter (STA Mode)"]
        ESP1["ESP32 Core (Node 1)"]
        POT["Potentiometer"] -->|Threshold ADC1_CH6| ESP1
        BTN["4x Push Buttons"] -->|GPIO Control| ESP1
        ESP1 -->|TRIG Pulse| US["Ultrasonic HC-SR04"]
        US -->|ECHO via 1k/2k Divider| ESP1
        ESP1 -->|I2C Bus 0 (GPIO 21/22)| OLED1["OLED 1 (Radar Display 0x3C)"]
        ESP1 -->|I2C Bus 1 (GPIO 18/19)| OLED2["OLED 2 (Keyboard Display 0x3C)"]
        ESP1 -->|GPIO 5| LED["Alert LED Indicator"]
    end

    NODE1 ==>|"Wi-Fi 802.11 b/g/n (HTTP GET /send?code=...)"| NODE2

    subgraph NODE2["Node 2: Receiver (SoftAP Mode)"]
        ESP2["ESP32 Core (Node 2 / Web Server)"]
        ESP2 -->|I2C Default (GPIO 21/22)| OLED_RX["OLED (Received Message 0x3C)"]
        ESP2 -->|GPIO 14| BUZZ["Buzzer Driver"]
    end

    classDef mcu fill:#1f77b4,stroke:#fff,stroke-width:2px,color:#fff;
    classDef display fill:#ff7f0e,stroke:#fff,stroke-width:1px,color:#fff;
    classDef sensor fill:#2ca02c,stroke:#fff,stroke-width:1px,color:#fff;
    class ESP1,ESP2 mcu;
    class OLED1,OLED2,OLED_RX display;
    class US,POT,BTN,LED,BUZZ sensor;
```

---

## 🔄 แผนผังการทำงานเชิงลึก (Detailed Flowcharts)

### 1. โหนดส่ง (Node 1: Transmitter Logic Flow)

```mermaid
flowchart TD
    Start1(["เริ่มต้นการทำงาน (Power ON)"]) --> Setup1["กำหนดค่า PinMode, เริ่มต้น I2C0, I2C1<br/>เริ่มจอ OLED ทั้งสองจอ และเชื่อมต่อ Wi-Fi (STA)"]
    Setup1 --> CheckWiFi{"Wi-Fi เชื่อมต่อสำเร็จ?"}
    CheckWiFi -- ใช่ --> IconOn["แสดงไอคอน Wi-Fi มุมขวาบนจอ OLED 2"]
    CheckWiFi -- ไม่ใช่ / หลุด --> IconOff["เคลียร์พื้นที่ไอคอน Wi-Fi บนจอ OLED 2"]
    
    IconOn --> LoopStart1["เข้าสู่วงรอบการทำงานหลัก (loop)"]
    IconOff --> LoopStart1

    %% ส่วนการจัดการปุ่มกด
    LoopStart1 --> ReadButtons["สแกนสถานะปุ่มกด 4 ปุ่ม (ezButton.loop)"]
    ReadButtons --> CheckLeft{"ปุ่ม Left กด?"}
    CheckLeft -- ใช่ --> MoveLeft["เลื่อน Cursor ไปทางซ้าย (-1)<br/>อัปเดตหน้าจอคีย์บอร์ด"] --> ReadRadar
    CheckLeft -- ไม่ใช่ --> CheckRight{"ปุ่ม Right กด?"}
    
    CheckRight -- ใช่ --> MoveRight["เลื่อน Cursor ไปทางขวา (+1)<br/>อัปเดตหน้าจอคีย์บอร์ด"] --> ReadRadar
    CheckRight -- ไม่ใช่ --> CheckUp{"ปุ่ม Up กด?"}
    
    CheckUp -- ใช่ --> MoveUp["เลื่อน Cursor ขึ้นแถวบน (-13)<br/>อัปเดตหน้าจอคีย์บอร์ด"] --> ReadRadar
    CheckUp -- ไม่ใช่ --> CheckAction{"ปุ่ม Action กด?"}

    CheckAction -- กดสั้น --> AppendChar["เพิ่มตัวอักษรลง Buffer (หรือลบถ้าเลือก Backspace)<br/>อัปเดตหน้าจอคีย์บอร์ด"] --> ReadRadar
    CheckAction -- กดค้าง > 800ms --> SendMsg["แปลงข้อความใน Buffer เป็นสายรหัสมอร์ส<br/>ยิง HTTP GET ไปยัง Node 2<br/>กะพริบไฟ LED ตามรหัสทีละตัว แล้วเคลียร์ Buffer"] --> ReadRadar
    CheckAction -- ไม่มีการกด --> ReadRadar

    %% ส่วนเรดาร์และเตือนภัย SOS
    ReadRadar["อ่านค่า Analog จาก Potentiometer (Threshold)<br/>อ่านระยะทางจาก Ultrasonic HC-SR04"]
    ReadRadar --> CheckDistance{"ระยะทาง < Threshold?"}
    
    CheckDistance -- ใช่ (อยู่ในระยะอันตราย) --> TriggerSOS["เริ่มระบบไฟกะพริบ SOS แบบ Non-blocking<br/>แสดงหน้าจอ ALERT SOS บน OLED 1"]
    TriggerSOS --> CheckSent{"เคยส่ง SOS ไป Node 2 หรือยัง?"}
    CheckSent -- ยังไม่เคยส่ง --> SendSOSHTTP["ยิง HTTP GET รหัส '... --- ...' ไปยัง Node 2 ทันที 1 ครั้ง<br/>ตั้งค่า Flag sosSentToServer = true"] --> LoopEnd1
    CheckSent -- ส่งไปแล้ว --> LoopEnd1
    
    CheckDistance -- ไม่ใช่ (ปลอดภัย) --> ResetSOS["รีเซ็ต Flag sosSentToServer = false<br/>ดับไฟเตือน และแสดงหน้าจอเรดาร์ปกติบน OLED 1"] --> LoopEnd1

    LoopEnd1["วนกลับไปทำงานต้น loop"] --> LoopStart1
```

---

### 2. โหนดรับ (Node 2: Receiver Logic Flow)

```mermaid
flowchart TD
    Start2(["เริ่มต้นการทำงาน (Power ON)"]) --> Setup2["กำหนด PinMode Buzzer, บัส I2C<br/>เริ่มต้นจอ OLED และเปิด Hotspot Wi-Fi (SoftAP)<br/>ลงทะเบียน HTTP Route: /send"]
    Setup2 --> LoopStart2["เข้าสู่วงรอบการทำงานหลัก (loop)"]
    
    LoopStart2 --> CheckClient["server.handleClient() ตรวจสอบ Request"]
    CheckClient --> HasRequest{"ได้รับ HTTP GET /send?"}
    
    HasRequest -- ใช่ --> ExtractCode["ดึงค่ารหัสมอร์สจาก Parameter 'code'<br/>เก็บลงตัวแปร pendingMorseData และตั้ง hasNewData = true<br/>ส่ง HTTP 200 OK ตอบกลับตัวส่งทันที (Non-blocking)"] --> ProcessCheck
    HasRequest -- ไม่ใช่ --> ProcessCheck{"hasNewData == true?"}

    ProcessCheck -- ใช่ --> StartDecode["เคลียร์สถานะ hasNewData = false<br/>แยกชุดรหัสมอร์สทีละตัว (คั่นด้วย Space)"]
    ProcessCheck -- ไม่ใช่ --> LoopStart2

    StartDecode --> LoopChar["เริ่มวนอ่านทีละตัวอักษร"]
    LoopChar --> DisplayChar["แสดงตัวอักษรด้านบน (ขนาดใหญ่)<br/>แสดงรหัสมอร์สด้านล่างบนจอ OLED"]
    DisplayChar --> PlaySound["ขับเสียง Buzzer ตามจังหวะ จุด (.) / ขีด (-)<br/>(หากเป็นช่องว่างให้เว้นเงียบ)"]
    PlaySound --> Hold1Sec["หน่วงเวลารวมให้ค้างครบ 1 วินาที (1,000 ms) ต่อตัว"]
    Hold1Sec --> NextChar{"ยังมีตัวอักษรเหลืออยู่ไหม?"}
    
    NextChar -- มี --> LoopChar
    NextChar -- ครบทุกตัวแล้ว --> FullMessage["ถอดรหัสข้อความเป็นประโยคเต็มสมบูรณ์<br/>แสดงผลข้อความทั้งหมดค้างไว้บนจอ OLED"] --> LoopStart2
```

---

## ⚡ แผนภาพวงจรและการเชื่อมต่อ (Circuit Diagram & Schematics)

### 1. วงจรโหนดส่ง (Node 1: Transmitter)
```
                          +-------------------------------+
                          |        ESP32 DevKit V1        |
                          |                               |
        [+3.3V] ----------| 3V3                       GND |---------- [GND Common]
        [+5V Vin] --------| VIN                       EN  |
                          |                               |
        OLED 1 (SDA) -----| GPIO 21 (I2C0 SDA)    GPIO 23 |
        OLED 1 (SCL) -----| GPIO 22 (I2C0 SCL)    GPIO 19 |---------- OLED 2 (SCL)
                          |                       GPIO 18 |---------- OLED 2 (SDA)
        HC-SR04 (TRIG) ---| GPIO 13                GPIO 5 |---------- [220Ω] -> LED (+) -> GND
        HC-SR04 (ECHO)* --| GPIO 12                GPIO 17|---------- SW_Action -> GND
                          | GPIO 14                GPIO 16|---------- SW_Up     -> GND
        Potentiometer ----| GPIO 34 (ADC1_CH6)      GPIO 4 |---------- SW_Right  -> GND
                          | GPIO 35                GPIO 15|---------- SW_Left   -> GND
                          +-------------------------------+

  *วงจรลดทอนแรงดันสัญญาณ Echo (Voltage Divider):
   HC-SR04 ECHO (5V Out) ----[ 1kΩ ]----+----> GPIO 12 (รับแรงดัน 3.3V)
                                        |
                                     [ 2kΩ ]
                                        |
                                       GND
```

### 2. วงจรโหนดรับ (Node 2: Receiver)
```
                          +-------------------------------+
                          |        ESP32 DevKit V1        |
                          |                               |
        [+3.3V] ----------| 3V3                       GND |---------- [GND Common]
        [+5V Vin] --------| VIN                       EN  |
                          |                               |
        OLED (SDA) -------| GPIO 21 (I2C SDA)     GPIO 14 |---------- วงจรขับ Buzzer
        OLED (SCL) -------| GPIO 22 (I2C SCL)             |
                          +-------------------------------+

  *วงจรขับ Buzzer ผ่านทรานซิสเตอร์ NPN (ป้องกัน GPIO จ่ายกระแสเกิน):
              +5V (VIN)
                 |
                 +----------+
                 |          |
             [ Buzzer ]   [ 1N4148 Diode ] (Flyback Protection)
                 |          |
                 +----------+
                 |
             C (Collector)
                 |
   GPIO 14 ----[ 1kΩ ]---- B (Base) [2N2222 / S8050]
                 |
             E (Emitter)
                 |
                GND

---
