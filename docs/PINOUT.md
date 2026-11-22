# Mapping chân toàn bộ mạch: Smart Parking

Khớp với hằng số trong `arduino/smart_parking/smart_parking.ino` và `pi/pi_lcd.py`.
**Luôn rút USB trước khi cắm hoặc rút dây.**

---

## 1. Arduino Uno: hàng DIGITAL

| Chân | Nối vào | Chân của linh kiện | Ghi chú |
|---|---|---|---|
| D0 (RX) | — | — | **để trống**: USB Serial dùng chân này |
| D1 (TX) | — | — | **để trống**: USB Serial dùng chân này |
| **D2** | LED xanh | chân dài (+), qua điện trở 220Ω | sáng khi còn chỗ |
| **D3** | LED đỏ | chân dài (+), qua điện trở 220Ω | sáng khi bãi đầy |
| **D4** | HC-SR04 cổng **ra** | Echo | |
| **D5** | HC-SR04 cổng **ra** | Trig | |
| **D6** | HC-SR04 cổng **vào** | Echo | |
| **D7** | HC-SR04 cổng **vào** | Trig | |
| **D8** | Buzzer | + | |
| **D9** | Servo | dây **cam** (tín hiệu) | barrier |
| **D10** | HC-05 | **TXD** | cắm thẳng (SoftwareSerial RX) |
| **D11** | HC-05 | **RXD**, đi **qua mạch chia áp** | bắt buộc chia áp (SoftwareSerial TX) |
| **D12** | LED đèn bãi | chân dài (+), qua điện trở 220Ω | bật khi trời tối |
| D13 | — | — | trống (LED "L" có sẵn trên board) |

## 2. Arduino Uno: hàng ANALOG IN

| Chân | Nối vào | Chân của linh kiện | Ghi chú |
|---|---|---|---|
| **A0** | LDR | điểm giữa của LDR và điện trở 10kΩ | đo ánh sáng, 0–1023 |
| A1 | — | — | trống |
| A2 | — | — | trống |
| A3 | — | — | trống |
| **A4** | LCD1602 #1 | **SDA** | I2C, địa chỉ 0x27 (hoặc 0x3F) |
| **A5** | LCD1602 #1 | **SCL** | I2C |

## 3. Arduino Uno: hàng POWER

| Chân | Nối vào |
|---|---|
| **5V** | hàng **+** breadboard. Cấp cho VCC của LCD #1, 2 HC-SR04, HC-05, dây đỏ servo, một đầu LDR |
| **GND** | hàng **−** breadboard. Nối GND của tất cả linh kiện, chân ngắn LED, buzzer −, dây nâu servo, điện trở 10k của LDR, đáy mạch chia áp HC-05 |
| 3.3V · VIN · IOREF · RESET · AREF | không dùng |

Breadboard dài thường có hàng nguồn **đứt ở giữa**. Nối thêm hai dây để nửa trái và nửa phải cùng có nguồn.

## 4. Nhìn trên board

```
              ┌─ DIGITAL ─────────────────────────────────────────────────────┐
              │ D13 D12  D11   D10   D9    D8   D7   D6   D5   D4   D3   D2  D1 D0
              │  ·  đèn  HC05  HC05  servo còi  Trig Echo Trig Echo đỏ   xanh ✗  ✗
              │     bãi  RXD*  TXD              ─ vào ─   ─ ra ─
     USB ▓▓   │
              │  POWER                     ANALOG IN
              │  3V3  5V   GND GND VIN    A0   A1 A2 A3  A4   A5
              │   ·   (+)  (−) (−)  ·     LDR   ·  ·  ·  SDA  SCL
              └───────────────────────────────────────── LCD #1 ─────────────┘
   * D11 → mạch chia áp → RXD của HC-05        ✗ = để trống
```

---

## 5. Theo từng linh kiện

### LCD1602 I2C #1 (trên Arduino)
Bốn chân nằm trên module nhỏ ở mặt sau LCD. Nhìn chữ in cạnh chân, vì thứ tự có thể khác nhau tùy hãng.

| LCD | Arduino |
|---|---|
| GND | GND |
| VCC | 5V |
| SDA | **A4** |
| SCL | **A5** |

Không thấy chữ: vặn biến trở xanh phía sau LCD. Vẫn không được: đổi `0x27` thành `0x3F` trong code.

### HC-SR04 cổng vào và cổng ra

| HC-SR04 | Cổng vào | Cổng ra |
|---|---|---|
| VCC | 5V | 5V |
| Trig | **D7** | **D5** |
| Echo | **D6** | **D4** |
| GND | GND | GND |

Arduino chạy 5V nên chân Echo cắm thẳng, không cần chia áp (khác với khi nối vào Pi). Đặt hai cảm biến quay về hai hướng khác nhau.
Nếu không lắp cảm biến cổng ra: sửa thành `#define USE_EXIT_SENSOR 0`.

### LDR (cảm biến ánh sáng), mạch chia áp

```
5V ──── LDR ────┬──── A0
                │
              10kΩ  (nâu-đen-cam)
                │
GND ────────────┘
```
Sáng thì giá trị lớn, tối thì giá trị nhỏ. Ngưỡng tối là `DARK_LDR = 300`; chỉnh lại theo phòng lab.

### Ba LED (cùng một kiểu nối)

```
Dx ── 220Ω (đỏ-đỏ-nâu) ── chân DÀI LED (+)   chân NGẮN LED (−) ── GND
```

| LED | Chân |
|---|---|
| Xanh (còn chỗ) | **D2** |
| Đỏ (hết chỗ) | **D3** |
| Vàng hoặc trắng (đèn bãi) | **D12** |

### Buzzer

| Buzzer | Arduino |
|---|---|
| + (chân dài, hoặc chân có dấu +) | **D8** |
| − | GND |

### Servo (barrier)

| Dây servo | Arduino |
|---|---|
| Nâu | GND |
| Đỏ | 5V |
| Cam | **D9** |

Nếu Arduino tự reset mỗi khi servo quay: cấp 5V cho servo từ nguồn riêng và **nối chung GND** với Arduino.

### HC-05 (Bluetooth)

| HC-05 | Arduino |
|---|---|
| VCC | 5V |
| GND | GND |
| TXD | **D10** (cắm thẳng) |
| RXD | **D11**, đi qua mạch chia áp bên dưới |
| STATE, EN | không dùng |

RXD của HC-05 chỉ chịu được 3.3V. Chọn một trong hai cách chia áp:

```
Cách A: 3 điện trở 1kΩ (khoảng 3.3V)    Cách B: 220Ω + 330Ω (khoảng 3.0V)
D11 ── 1k ──┬── RXD                     D11 ── 220Ω ──┬── RXD
            1k                                       330Ω
            1k                                        │
            │                                        GND
           GND
```

---

## 6. Raspberry Pi 3B+

| Pi | Nối vào | Ghi chú |
|---|---|---|
| Cổng USB bất kỳ | cáp USB của Arduino | Serial dự phòng (`/dev/ttyACM0`) |
| Bluetooth có sẵn trên Pi | ghép với HC-05 | `/dev/rfcomm0` (xem README) |
| Chân 2 (5V), hoặc chân 1 (3.3V) | VCC của LCD #2 | 3.3V an toàn hơn nếu chữ vẫn rõ |
| Chân 3 (GPIO2, SDA) | SDA của LCD #2 | |
| Chân 5 (GPIO3, SCL) | SCL của LCD #2 | |
| Chân 6 (GND) | GND của LCD #2 | |

```
Header Pi (nhìn từ trên, cổng USB quay xuống dưới)
  chân 1  3.3V  ●  ●  5V    chân 2   ← VCC LCD #2
  chân 3  SDA   ●  ●  5V    chân 4
  chân 5  SCL   ●  ●  GND   chân 6   ← GND LCD #2
          ...
```
Bật I2C: `sudo raspi-config` → Interface Options → I2C → Enable. Kiểm tra bằng `i2cdetect -y 1` (thấy 27 hoặc 3f). Nếu thấy 3f thì chạy `python3 app.py --lcd-addr 0x3f`.

---

## 7. Checklist trước khi cắm USB

- [ ] Mọi GND đều về hàng −; hàng − đã nối GND Arduino
- [ ] Không có dây nào cắm vào D0 hoặc D1
- [ ] RXD của HC-05 đi **qua mạch chia áp**, không nối thẳng D11
- [ ] LED: chân dài cắm phía điện trở, chân ngắn cắm GND
- [ ] LDR có điện trở 10k xuống GND (thiếu điện trở này thì A0 nhảy số lung tung)
- [ ] Servo: nâu GND, đỏ 5V, cam D9
- [ ] Không có chỗ nào chập giữa 5V và GND (đo thử bằng đồng hồ nếu có)
