📡 ESP32 Dual-Node Morse Communication & Ultrasonic Radar Systemระบบสื่อสารรหัสมอร์สไร้สายและตรวจจับสิ่งกีดขวางระยะใกล้ด้วย ESP32 จำนวน 2 บอร์ด โดยแบ่งการทำงานเป็นโหนดส่งข้อมูล (Transmitter Node) ที่มาพร้อมคีย์บอร์ดเสมือนและเรดาร์อัลตราโซนิก และโหนดรับข้อมูล (Receiver Node) ที่ทำหน้าที่เป็น Wi-Fi Access Point ถอดรหัสและแสดงสัญญาณเสียง/ภาพ📌 ภาพรวมสถาปัตยกรรมระบบ (System Architecture)+-------------------------------------------------------------+
|                Node 1: Transmitter (Client / STA)            |
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
|                Node 2: Receiver (Server / AP)               |
|                                                             |
|  [SoftAP Mode]      ----> "ESP32-Morse" (192.168.4.1)       |
|  [Web Server]       ----> Instant 200 OK (Non-blocking)     |
|  [Buzzer]           ----> Dot/Dash Sound Playback           |
|  [Single OLED]      ----> Step-by-Step & Full Message       |
+-------------------------------------------------------------+
flowchart TB
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
    class US,POT,BTN,LED,BUZZ sensor;
✨ ฟีเจอร์หลัก (Key Features)1. โหนดส่ง (Transmitter Node)Virtual Keyboard (OLED 2): เลือกตัวอักษร A-Z, 0-9 และ Backspace ด้วยระบบ 4 ปุ่มกดกดสั้นเพื่อพิมพ์ / กดค้างที่ปุ่ม Action (> 800ms) เพื่อส่งข้อความDynamic Wi-Fi Status Icon: แสดงสถานะการเชื่อมต่อ Wi-Fi และระดับ RSSI แบบเรียลไทม์ที่มุมขวาบนของหน้าจอUltrasonic Radar & Auto-SOS (OLED 1):วัดระยะด้วย HC-SR04 และปรับค่า Threshold ผ่าน Potentiometerส่งรหัสฉุกเฉิน ... --- ... (SOS) ไปยังโหนดรับอัตโนมัติทันที 1 ครั้งเมื่อมีวัตถุกีดขวางระยะปลอดภัยDual I2C Bus Management: ใช้งานหน้าจอ OLED Address 0x3C พร้อมกัน 2 จอได้อย่างอิสระ ผ่านฮาร์ดแวร์ TwoWire(0) และ TwoWire(1)2. โหนดรับ (Receiver Node)Stand-alone Access Point: ปล่อยสัญญาณ Wi-Fi Hotspot ในตัว ไม่ต้องพึ่งพาเราเตอร์ภายนอกNon-blocking Request Handling: ตอบรับสถานะ HTTP 200 OK ทันที เพื่อป้องกันไม่ให้โหนดส่งติดค้างในลูปSynchronized Audio & Visual Output:แสดงตัวอักษรขนาดใหญ่ด้านบนและรหัสมอร์สด้านล่าง ค้างไว้ตัวละ 1 วินาทีขับสัญญาณเสียงผ่าน Buzzer แยกจังหวะ จุด (Dot) และ ขีด (Dash) อัตโนมัติสลับไปแสดงข้อความที่ถอดรหัสเสร็จสมบูรณ์ค้างไว้จนกว่าจะมีชุดข้อมูลใหม่เข้ามา🛠 ข้อมูลการต่อวงจร (Pinout Configuration)Node 1: Transmitter (ตัวส่ง)อุปกรณ์ / เซนเซอร์ขาอุปกรณ์ขา ESP32 (GPIO)หมายเหตุOLED 1 (Radar)SDA / SCLGPIO 21 / GPIO 22I2C Bus 0 (Address: 0x3C)OLED 2 (Keyboard)SDA / SCLGPIO 18 / GPIO 19I2C Bus 1 (Address: 0x3C)HC-SR04TRIG / ECHOGPIO 13 / GPIO 12Ultrasonic Sensor (ECHO ควรผ่าน Voltage Divider 5V -> 3.3V)PotentiometerOutput PinGPIO 34Analog In (ADC1_CH6)Alert LEDAnode (+)GPIO 5ต่ออนุกรมตัวต้านทาน 220–330Ω (Cathode ต่อ GND)Button LeftData PinGPIO 15Active LOW (Internal Pull-up)Button RightData PinGPIO 4Active LOW (Internal Pull-up)Button UpData PinGPIO 16Active LOW (Internal Pull-up)Button ActionData PinGPIO 17Active LOW (Internal Pull-up)Node 2: Receiver (ตัวรับ)อุปกรณ์ / เซนเซอร์ขาอุปกรณ์ขา ESP32 (GPIO)หมายเหตุOLED (Display)SDA / SCLGPIO 21 / GPIO 22I2C Default Bus (Address: 0x3C)BuzzerPositive (+)GPIO 14Active หรือ Passive Buzzer📖 ตารางเทียบรหัสมอร์สสากล (Morse Code Reference Table)ตัวอักษรรหัสมอร์สตัวอักษรรหัสมอร์สตัวเลขรหัสมอร์สA.-N-.0-----B-...O---1.----C-.-.P.--.2..---D-..Q--.-3...--E.R.-.4....-F..-.S...5.....G--.T-6-....H....U..-7--...I..V...-8---..J.---W.--9----.K-.-X-..-SOS... --- ...L.-..Y-.--M--Z--..📦 ไลบรารีที่จำเป็น (Required Libraries)ติดตั้งผ่าน Arduino Library Manager หรือระบุใน platformio.ini:Adafruit SSD1306 (โดย Adafruit)Adafruit GFX Library (โดย Adafruit)ezButton (โดย ArduinoGetStarted)WiFi, WebServer & HTTPClient (มีมาพร้อมกับ ESP32 Arduino Core)🚀 ขั้นตอนการใช้งาน (Quick Start)อัปโหลดเฟิร์มแวร์:แฟลชโค้ด Receiver ลงใน ESP32 Node 2แฟลชโค้ด Transmitter ลงใน ESP32 Node 1เปิดเครื่อง:เริ่มการทำงานของ Node 2 ก่อน เพื่อให้ระบบปล่อย Wi-Fi Hotspot ESP32-Morseเปิด Node 1 บอร์ดจะเชื่อมต่อไปยัง Node 2 โดยอัตโนมัติ (สังเกตไอคอน Wi-Fi มุมขวาบนของ OLED 2)การส่งข้อความ:กดปุ่มเลื่อนทิศทางเพื่อเลือกตัวอักษร จากนั้นกดปุ่ม Action สั้นๆ เพื่อบันทึกเข้า Bufferกดปุ่ม Action ค้างไว้ > 800 ms เพื่อส่งชุดข้อความไปยัง Node 2ระบบความปลอดภัย (Auto-SOS):หมุน Potentiometer เพื่อกำหนดระยะ Threshold (5 - 50 cm)หากเซนเซอร์ตรวจพบสิ่งกีดขวางในระยะอันตราย LED จะติดเตือนและส่งรหัส SOS ไปยัง Node 2 ทันที🔧 การแก้ไขปัญหาเบื้องต้น (Troubleshooting)จอ OLED จอมืด / ไม่แสดงผล:ตรวจสอบสายไฟเลี้ยง (3.3V หรือ 5V ตามสเปกของโมดูล) และกราวด์ (GND)ใน Node 1 ตรวจสอบว่ากำหนดขา SDA/SCL แยกกัน 2 Bus ชัดเจน (TwoWire(0) ขา 21/22 และ TwoWire(1) ขา 18/19)Node 1 เชื่อมต่อ Wi-Fi ของ Node 2 ไม่สำเร็จ:ตรวจสอบว่า Node 2 เปิดทำงานและปล่อยสัญญาณ Hotspot สำเร็จแล้วหรือไม่ตรวจสอบชื่อ SSID (ESP32-Morse) และ Password ในโค้ดของ Node 1 ให้ตรงกับ Node 2ระยะจาก Ultrasonic เพี้ยนหรือไม่ตอบสนอง:เซนเซอร์ HC-SR04 ต้องใช้ไฟเลี้ยง 5V (ไฟ 3.3V อาจทำให้ตรวจจับระยะไม่เสถียร)ขา ECHO ต้องต่อผ่านตัวแบ่งแรงดัน (Voltage Divider) ก่อนเข้า GPIO 12 ของ ESP32 เสมอ เพื่อความปลอดภัย
