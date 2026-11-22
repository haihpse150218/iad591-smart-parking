/*
 * SMART PARKING - Device layer (Arduino Uno)
 * IAD591 Capstone
 *
 * Cam bien : HC-SR04 #1 (cong vao), HC-SR04 #2 (cong ra, tuy chon), LDR (A0)
 * Dau ra   : Servo barrier, LCD1602 I2C, LED xanh/do, LED den bai, buzzer
 * Giao tiep: USB Serial (D0/D1) + HC-05 qua SoftwareSerial (D10/D11), 9600 baud
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
const byte SERVO_PIN = 9;
const byte LED_GREEN = 2, LED_RED = 3, LED_LIGHT = 12;
const byte BUZZER = 8;
const byte BT_RX = 10, BT_TX = 11;       // D10 <- TXD HC-05 ; D11 -> chia ap 1k/2k -> RXD HC-05

// ------------------- CAU HINH -------------------
#define USE_EXIT_SENSOR 1     // 0 neu khong lap HC-SR04 thu 2
const int  CAPACITY = 4;      // so cho trong bai
const int  CAR_CM = 10;       // < 10 cm = co xe
const int  CLEAR_CM = 15;     // > 15 cm = xe da di (tre de chong nhieu)
const byte ON_COUNT = 3;      // 3 lan doc lien tiep < CAR_CM moi tinh la CO xe
const byte OFF_COUNT = 6;     // 6 lan doc lien tiep > CLEAR_CM (~0.5 s) moi tinh la xe DA DI
const int  DARK_LDR = 300;    // LDR < 300 = troi toi -> bat den bai
const int  GATE_OPEN_DEG = 90, GATE_CLOSED_DEG = 0;
const unsigned long GATE_HOLD_MS = 3000;
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
String bufUsb, bufBt;

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

void setGate(bool open) {
  gateOpen = open;
  gate.write(open ? GATE_OPEN_DEG : GATE_CLOSED_DEG);
  if (open) gateCloseAt = millis() + GATE_HOLD_MS;
}

void report() {
  String s = "{\"d1\":";
  s += distIn;
  s += ",\"d2\":";  s += USE_EXIT_SENSOR ? distOut : -1;
  s += ",\"ldr\":"; s += ldr;
  s += ",\"cars\":"; s += cars;
  s += ",\"cap\":"; s += CAPACITY;
  s += ",\"gate\":"; s += gateOpen ? 1 : 0;
  s += ",\"light\":"; s += lightOn ? 1 : 0;
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
  else { out("{\"ev\":\"bad_cmd\"}"); return; }
  report();   // phan hoi ngay trang thai moi
}

void pollPort(Stream &port, String &buf) {
  while (port.available()) {
    char ch = port.read();
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

  // Giu cong mo khi con xe dung truoc cong
  if (gateOpen && (carAtIn || carAtOut)) gateCloseAt = millis() + GATE_HOLD_MS;
  if (gateOpen && autoMode && (long)(millis() - gateCloseAt) >= 0) setGate(false);

  // Den bai theo LDR
  if (!lightManual) lightOn = ldr < DARK_LDR;

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
  gate.attach(SERVO_PIN);
  setGate(false);
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

  if (millis() - lastLcd >= 300) { lastLcd = millis(); drawLcd(); }
  if (millis() - lastReport >= REPORT_MS) { lastReport = millis(); report(); }
  delay(40);
}
