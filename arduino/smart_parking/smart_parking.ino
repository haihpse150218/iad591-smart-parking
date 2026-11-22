/*
 * SMART PARKING - Device layer (Arduino Uno)
 * IAD591 Capstone
 *
 * Cam bien : HC-SR04 #1 (cong vao), HC-SR04 #2 (cong ra, tuy chon), LDR (A0)
 * Dau ra   : Servo barrier, LCD1602 I2C, LED xanh/do, LED den bai, buzzer
 * Giao tiep: USB Serial (D0/D1) + HC-05 qua SoftwareSerial (RX D10 / TX A1), 9600 baud
 *            Gui cung 1 dong JSON ra CA HAI cong; nhan lenh tu CA HAI cong.
 *
 * Thu vien can cai: LiquidCrystal_I2C (Frank de Brabander). Servo, Wire, SoftwareSerial co san.
 */
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Servo.h>
#include <SoftwareSerial.h>

// ------------------- CHAN -------------------
const byte TRIG_IN = 7,  ECHO_IN = 6;    // HC-SR04 cong vao
const byte TRIG_OUT = 5, ECHO_OUT = 4;   // HC-SR04 cong ra (USE_EXIT_SENSOR)
const byte LDR_PIN = A0;                 // LDR len 5V, 10k xuong GND -> sang = so lon
const byte SERVO_PIN = 11;               // servo o D11 (lab: D9/D13 cam day khong an, D11 chay)
const byte LED_GREEN = 2, LED_RED = 3, LED_LIGHT = 12;
const byte BUZZER = 8;
const byte BT_RX = 10, BT_TX = A1;       // D10 <- TXD HC-05 ; A1 -> chia ap 1k/2k -> RXD HC-05

// ------------------- CAU HINH -------------------
#define USE_EXIT_SENSOR 1     // 0 neu khong lap HC-SR04 thu 2
const int  CAPACITY = 4;      // so cho trong bai
const int  CAR_CM = 10;       // < 10 cm = co xe
const int  CLEAR_CM = 15;     // > 15 cm = xe da di (tre de chong nhieu)
const byte ON_COUNT = 3;      // 3 lan doc lien tiep < CAR_CM moi tinh la CO xe
const byte OFF_COUNT = 6;     // 6 lan doc lien tiep > CLEAR_CM (~0.5 s) moi tinh la xe DA DI
const int  DARK_LDR = 600;    // LDR < 600 = troi toi -> bat den bai (lab: sang ~850, che tay ~410)
const int  LIGHT_HYST = 50;   // > 650 moi tat lai, tranh nhap nhay quanh nguong
// Xe vao/ra: quay 90 do, giu 0.5 s, quay ve 0 do. Servo chi duoc cap xung 0.7 s moi lan quay
// (detach) nen o 0 do neu cham chot co cung chi rè rè thoang qua.
const int  GATE_OPEN_DEG = 90, GATE_CLOSED_DEG = 0;
const unsigned long GATE_HOLD_MS = 500;       // giu barrier mo 0.5 s roi dong, khong cho xe di qua
const unsigned long GATE_MAX_OPEN_MS = 10000;  // cam bien ket van dong sau toi da 10 s
const unsigned long REPORT_MS = 1000;

LiquidCrystal_I2C lcd(0x27, 16, 2);      // doi 0x3F neu i2c_scanner bao khac
Servo gate;
SoftwareSerial bt(BT_RX, BT_TX);

// ------------------- TRANG THAI -------------------
int cars = 0;
bool autoMode = true;
bool gateOpen = false;
bool lightOn = false;
bool lightManual = false;     // MANUAL: den theo lenh
int distIn = -1, distOut = -1, ldr = 0;
bool carAtIn = false, carAtOut = false;
byte cntIn = 0, cntOut = 0;   // dem so lan doc lien tiep muon doi trang thai
unsigned long gateCloseAt = 0, lastReport = 0, lastLcd = 0;
unsigned long gateOpenedAt = 0;
unsigned long servoOffAt = 0;   // thoi diem ngat xung servo (0 = dang ngat)
int servoAngle = -1;            // goc dang dat cho servo (log)
long vminMove = 0;              // Vcc thap nhat (mV) trong luc servo quay
unsigned long pulseUs = 0;      // do rong xung do duoc tren chan servo (us)
const unsigned long SERVO_MOVE_MS = 700;
String bufUsb, bufBt;
unsigned long btRxBytes = 0;     // dem byte nhan qua HC-05 (chan doan)

// ------------------- HAM TIEN ICH -------------------
int readCm(byte trig, byte echo) {
  digitalWrite(trig, LOW);  delayMicroseconds(2);
  digitalWrite(trig, HIGH); delayMicroseconds(10);
  digitalWrite(trig, LOW);
  unsigned long us = pulseIn(echo, HIGH, 25000UL);   // ~4 m
  if (us == 0) return 400;                           // khong co vat -> xa
  return (int)(us / 58UL);
}

// In ra ca 2 cong
void out(const String &s) {
  Serial.println(s);
  bt.println(s);
}

void sendEvent(const char *ev) {
  out(String("{\"ev\":\"") + ev + "\",\"cars\":" + cars + "}");
}

void beep(int ms) { tone(BUZZER, 2000, ms); }

// Do dien ap nguon 5V cua Arduino (mV) bang dien ap chuan noi 1.1V - khong can day them.
// Neu servo lay nguon tu hang + cua Arduino, Vcc tut khi servo quay = nguon yeu.
long readVcc() {
  ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
  delay(2);
  ADCSRA |= _BV(ADSC);
  while (bit_is_set(ADCSRA, ADSC));
  long r = ADCL;
  r |= ADCH << 8;
  return r ? 1125300L / r : 0;   // 1.1V * 1023 * 1000
}

// Servo chi duoc cap xung khi dang quay (SERVO_MOVE_MS), sau do detach.
// Ly do: SoftwareSerial (HC-05) tat ngat moi lan gui -> xung servo bi meo -> servo giat moi giay.
void setGate(bool open) {
  bool changed = (open != gateOpen) || !gate.attached();
  gateOpen = open;
  if (changed) {
    gate.attach(SERVO_PIN);
    servoAngle = open ? GATE_OPEN_DEG : GATE_CLOSED_DEG;
    gate.write(servoAngle);
    servoOffAt = millis() + SERVO_MOVE_MS;
    if (servoOffAt == 0) servoOffAt = 1;
    // Do xung THUC TE tren chan servo (doc lai chinh chan output). 0 = khong phat xung.
    // Xung dau tien sau attach co the cham -> thu toi da 3 chu ky (20 ms/chu ky)
    pulseUs = 0;
    for (byte i = 0; i < 3 && pulseUs == 0; i++) pulseUs = pulseIn(SERVO_PIN, HIGH, 25000UL);
    vminMove = readVcc();
    // LOG servo: goc, xung lenh (us), xung do duoc tren chan servo (pw), Vcc luc bat dau (mV)
    out(String("{\"ev\":\"servo\",\"ang\":") + servoAngle +
        ",\"us\":" + gate.readMicroseconds() + ",\"pw\":" + pulseUs +
        ",\"vcc\":" + vminMove + ",\"pin\":" + SERVO_PIN + "}");
  }
  if (open) {
    if (changed) gateOpenedAt = millis();
    gateCloseAt = millis() + GATE_HOLD_MS;
  }
}

void servoIdle() {
  if (!servoOffAt) return;
  long v = readVcc();                       // theo doi Vcc trong luc servo dang quay
  if (v && v < vminMove) vminMove = v;
  if ((long)(millis() - servoOffAt) >= 0) {
    gate.detach();
    servoOffAt = 0;
    // vmin < 4500 mV: nguon yeu khi servo quay; ok = 1 neu du dien
    out(String("{\"ev\":\"servo_off\",\"ang\":") + servoAngle +
        ",\"pw\":" + pulseUs + ",\"vmin\":" + vminMove +
        ",\"ok\":" + ((pulseUs > 400 && vminMove >= 4500) ? 1 : 0) + "}");
  }
}

void report() {
  String s = "{\"d1\":";
  s += distIn;
  s += ",\"d2\":";  s += USE_EXIT_SENSOR ? distOut : -1;
  s += ",\"ldr\":"; s += ldr;
  s += ",\"cars\":"; s += cars;
  s += ",\"cap\":"; s += CAPACITY;
  s += ",\"gate\":"; s += gateOpen ? 1 : 0;
  s += ",\"ang\":"; s += servoAngle;
  s += ",\"sp\":"; s += gate.attached() ? 1 : 0;
  s += ",\"vcc\":"; s += readVcc();
  s += ",\"light\":"; s += lightOn ? 1 : 0;
  s += ",\"brx\":"; s += btRxBytes;     // so byte nhan tren D10 (HC-05 TXD) - chan doan
  s += ",\"mode\":\""; s += autoMode ? "AUTO" : "MANUAL";
  s += "\"}";
  out(s);
}

void drawLcd() {
  char l1[17], l2[17];
  snprintf(l1, sizeof l1, "Free:%d/%d %-6s", CAPACITY - cars, CAPACITY,
           gateOpen ? "OPEN" : (cars >= CAPACITY ? "FULL" : "CLOSE"));
  snprintf(l2, sizeof l2, "L:%-4d %-5s %c", ldr, lightOn ? "Light" : "Day", autoMode ? 'A' : 'M');
  lcd.setCursor(0, 0); lcd.print(l1);
  lcd.setCursor(0, 1); lcd.print(l2);
}

// ------------------- LENH TU PI -------------------
void handleCmd(String c) {
  c.trim();
  c.toUpperCase();
  if (c.length() == 0) return;
  if (c == "GATE:OPEN") {
    setGate(true); sendEvent("gate_manual_open");
    // MANUAL: xe dang cho o cong vao -> mo tay thi tinh la xe vao
    if (!autoMode && carAtIn && cars < CAPACITY) { cars++; sendEvent("car_in"); }
  }
  else if (c == "GATE:CLOSE")  { setGate(false); sendEvent("gate_manual_close"); }
  else if (c == "LIGHT:ON")    { lightManual = true; lightOn = true; }
  else if (c == "LIGHT:OFF")   { lightManual = true; lightOn = false; }
  else if (c == "LIGHT:AUTO")  { lightManual = false; }
  else if (c == "MODE:AUTO")   { autoMode = true; lightManual = false; }
  else if (c == "MODE:MANUAL") { autoMode = false; }
  else if (c.startsWith("CARS:")) { cars = constrain(c.substring(5).toInt(), 0, CAPACITY); sendEvent("cars_set"); }
  else if (c == "BUZZ")        { beep(300); }
  else if (c == "PING")        { out("{\"ev\":\"pong\"}"); return; }
  else {                                   // in kem lenh tho (hex) de chan doan loi truyen
    String s = "{\"ev\":\"bad_cmd\",\"raw\":\"";
    for (unsigned int i = 0; i < c.length(); i++) {
      byte b = (byte)c[i];
      if (b < 16) s += '0';
      s += String(b, HEX);
    }
    s += "\"}";
    out(s);
    return;
  }
  report();   // phan hoi ngay trang thai moi
}

void pollPort(Stream &port, String &buf) {
  while (port.available()) {
    char ch = port.read();
    if (&port == &bt) btRxBytes++;
    if (ch == '\n' || ch == '\r') { if (buf.length()) handleCmd(buf); buf = ""; }
    else if (buf.length() < 32) buf += ch;
  }
}

// ------------------- LOGIC CUC BO -------------------
// Chong nhieu HC-SR04: chi doi trang thai khi nhieu lan doc lien tiep cung y.
// Mot lan doc loi (vd 6 cm -> 400 -> 6 cm) khong lam dem trung xe.
bool updatePresence(int d, bool present, byte &cnt) {
  bool wantFlip = present ? (d > CLEAR_CM) : (d < CAR_CM);
  if (!wantFlip) { cnt = 0; return present; }
  if (++cnt >= (present ? OFF_COUNT : ON_COUNT)) { cnt = 0; return !present; }
  return present;
}

void parkingLogic() {
  // Xe vao: phat hien canh len (khong co -> co)
  bool nowIn = updatePresence(distIn, carAtIn, cntIn);
  if (nowIn && !carAtIn) {
    if (cars < CAPACITY) {
      if (autoMode) { setGate(true); cars++; sendEvent("car_in"); beep(80); }
      else sendEvent("car_wait");          // MANUAL: cho nguoi mo tu dashboard
    } else {
      sendEvent("full"); beep(600);
    }
  }
  carAtIn = nowIn;

#if USE_EXIT_SENSOR
  bool nowOut = updatePresence(distOut, carAtOut, cntOut);
  if (nowOut && !carAtOut && cars > 0) {
    cars--; sendEvent("car_out");
    if (autoMode) setGate(true);
  }
  carAtOut = nowOut;
#endif

  if (gateOpen && autoMode) {
    if ((long)(millis() - gateCloseAt) >= 0) {
      setGate(false); sendEvent("gate_auto_close");
    } else if (millis() - gateOpenedAt >= GATE_MAX_OPEN_MS) {
      setGate(false); sendEvent("gate_timeout_close");   // cam bien bao co xe qua lau -> van dong
    }
  }

  // Den bai theo LDR
  if (!lightManual) {
    if (ldr < DARK_LDR) lightOn = true;
    else if (ldr > DARK_LDR + LIGHT_HYST) lightOn = false;
  }

  digitalWrite(LED_LIGHT, lightOn);
  digitalWrite(LED_GREEN, cars < CAPACITY);
  digitalWrite(LED_RED, cars >= CAPACITY);
}

void setup() {
  Serial.begin(9600);
  bt.begin(9600);
  pinMode(TRIG_IN, OUTPUT);  pinMode(ECHO_IN, INPUT);
  pinMode(TRIG_OUT, OUTPUT); pinMode(ECHO_OUT, INPUT);
  pinMode(LED_GREEN, OUTPUT); pinMode(LED_RED, OUTPUT); pinMode(LED_LIGHT, OUTPUT);
  pinMode(BUZZER, OUTPUT);
  setGate(false);              // attach + ve 0 do, 0.7 s sau tu detach
  lcd.init(); lcd.backlight();
  lcd.print("Smart Parking");
  lcd.setCursor(0, 1); lcd.print("IAD591 starting");
  delay(1000);
  lcd.clear();
  out("{\"ev\":\"boot\"}");
}

void loop() {
  pollPort(Serial, bufUsb);
  pollPort(bt, bufBt);

  distIn = readCm(TRIG_IN, ECHO_IN);
#if USE_EXIT_SENSOR
  delay(10);
  distOut = readCm(TRIG_OUT, ECHO_OUT);
#endif
  ldr = analogRead(LDR_PIN);
  parkingLogic();
  servoIdle();

  if (millis() - lastLcd >= 300) { lastLcd = millis(); drawLcd(); }
  if (millis() - lastReport >= REPORT_MS) { lastReport = millis(); report(); }
  delay(40);
}
