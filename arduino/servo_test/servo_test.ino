/*
 * SERVO TEST - chi co servo, khong SoftwareSerial / cam bien / LCD.
 * Serial Monitor 9600 baud, line ending "Newline" (hoac "Both NL & CR").
 *
 * Noi day:  servo NAU -> GND   servo DO -> 5V   servo CAM -> D9
 *
 * Lenh:
 *   0..180   quay toi goc do (vd: 10, 90, 100)
 *   o        mo  (100 do)
 *   c        dong (10 do)
 *   s        quet 10 -> 170 -> 10, buoc 10 do
 *   l        lap mo/dong 5 lan (moi 1.5 s)
 *   a        attach (bat xung D9)
 *   d        detach (tat xung D9 -> servo phai dung yen)
 *   p        do xung thuc te tren D9
 *   v        do dien ap nguon 5V cua Arduino
 *   pin13    doi chan servo sang D13 (vd pin13, pin12, pin15 = A1) - doi day CAM theo
 *   h        in lai huong dan
 *
 * Moi lenh in log: goc, xung lenh (us), xung do duoc (pw), Vcc (mV).
 */
#include <Servo.h>

byte SERVO_PIN = 9;   // doi bang lenh pinN
const int OPEN_DEG = 100, CLOSED_DEG = 10;

Servo sv;
String buf;

long readVcc() {                       // mV, dung dien ap chuan noi 1.1V
  ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
  delay(2);
  ADCSRA |= _BV(ADSC);
  while (bit_is_set(ADCSRA, ADSC));
  long r = ADCL;
  r |= ADCH << 8;
  return r ? 1125300L / r : 0;
}

unsigned long measurePulse() {         // 0 = D9 khong phat xung
  unsigned long pw = 0;
  for (byte i = 0; i < 3 && pw == 0; i++) pw = pulseIn(SERVO_PIN, HIGH, 25000UL);
  return pw;
}

void status(const char *what) {
  long vmin = readVcc();
  unsigned long pw = sv.attached() ? measurePulse() : 0;
  // theo doi Vcc thap nhat trong 600 ms servo dang quay
  unsigned long t = millis();
  while (millis() - t < 600) { long v = readVcc(); if (v && v < vmin) vmin = v; }
  Serial.print(F("[")); Serial.print(what); Serial.print(F("] pin=")); Serial.print(SERVO_PIN); Serial.print(F(" "));
  Serial.print(F("attached=")); Serial.print(sv.attached() ? 1 : 0);
  Serial.print(F(" ang=")); Serial.print(sv.attached() ? sv.read() : -1);
  Serial.print(F(" us=")); Serial.print(sv.attached() ? sv.readMicroseconds() : 0);
  Serial.print(F(" pw=")); Serial.print(pw);
  Serial.print(F(" vmin=")); Serial.print(vmin);
  Serial.print(F("mV"));
  if (sv.attached() && pw == 0) Serial.print(F("  !! D9 KHONG PHAT XUNG"));
  if (vmin && vmin < 4500) Serial.print(F("  !! NGUON YEU (<4.5V)"));
  Serial.println();
}

void goTo(int deg, const char *what) {
  if (!sv.attached()) sv.attach(SERVO_PIN);
  sv.write(constrain(deg, 0, 180));
  status(what);
}

void help() {
  Serial.println(F("=== SERVO TEST (D9) ==="));
  Serial.println(F("0..180 = goc | o = mo 100 | c = dong 10 | s = quet | l = lap 5 lan"));
  Serial.println(F("a = attach | d = detach | p = do xung | v = do Vcc | h = huong dan"));
  Serial.println(F("pin13 = doi chan servo sang D13 (A1=15, A2=16, A3=17)"));
  Serial.print(F("Chan servo hien tai: D")); Serial.println(SERVO_PIN);
}

void handle(String c) {
  c.trim();
  c.toLowerCase();
  if (c.length() == 0) return;
  if (c.startsWith("pin")) {
    int np = c.substring(3).toInt();
    if (np < 2 || np > 17) { Serial.println(F("Chan hop le: 2..13 hoac 14..17 (A0..A3)")); return; }
    if (sv.attached()) sv.detach();
    digitalWrite(SERVO_PIN, LOW);
    SERVO_PIN = np;
    pinMode(SERVO_PIN, OUTPUT);
    digitalWrite(SERVO_PIN, LOW);
    Serial.print(F("Da doi sang chan ")); Serial.print(SERVO_PIN);
    Serial.println(F(" -> rut day CAM cam sang chan nay roi go c / o"));
    return;
  }
  if (isDigit(c[0]))      goTo(c.toInt(), "goc");
  else if (c == "o")      goTo(OPEN_DEG, "mo");
  else if (c == "c")      goTo(CLOSED_DEG, "dong");
  else if (c == "s") {
    for (int d = 10; d <= 170; d += 10) { goTo(d, "quet"); }
    for (int d = 170; d >= 10; d -= 10) { goTo(d, "quet"); }
  }
  else if (c == "l") {
    for (byte i = 0; i < 5; i++) { goTo(OPEN_DEG, "lap-mo"); delay(900); goTo(CLOSED_DEG, "lap-dong"); delay(900); }
  }
  else if (c == "a")      { sv.attach(SERVO_PIN); status("attach"); }
  else if (c == "d")      { sv.detach(); status("detach (servo phai dung yen)"); }
  else if (c == "p")      status("do xung");
  else if (c == "v")      status("do Vcc");
  else if (c == "h")      help();
  else { Serial.print(F("Lenh la: ")); Serial.println(c); help(); }
}

void setup() {
  Serial.begin(9600);
  pinMode(SERVO_PIN, OUTPUT);
  digitalWrite(SERVO_PIN, LOW);
  help();
  status("khoi dong, chua attach");
}

void loop() {
  while (Serial.available()) {
    char ch = Serial.read();
    if (ch == '\n' || ch == '\r') { if (buf.length()) handle(buf); buf = ""; }
    else if (buf.length() < 16) buf += ch;
  }
}
