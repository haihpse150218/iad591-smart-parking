# Kế hoạch thi — Smart Parking (IAD591)

Đề: 6 giờ tại lab = tối đa 1 giờ chuẩn bị + 5 giờ làm. Code đã viết sẵn trong repo này và **đã test bằng giả lập** (`python app.py --sim`, `python -m unittest test_link`). **Chưa chạy trên mạch thật**, nên ngày thi chủ yếu là lắp mạch, nạp code và chỉnh ngưỡng.

Nguyên tắc: **chạy được trước, đẹp sau**. Mỗi mốc đều phải có một bản chạy được để còn quay video.

## Phân công gợi ý (3–4 người)
| Người | Việc |
|---|---|
| A – phần cứng | lắp mạch theo bảng chân trong README, kiểm từng linh kiện |
| B – Arduino | nạp sketch, Serial Monitor, chỉnh `CAR_CM`, `DARK_LDR`, góc servo |
| C – Pi | clone repo, cài, ghép HC-05, chạy `app.py`, kiểm DB |
| D – tài liệu | slide 8–10 trang, screenshot, kịch bản + quay video |

## Timeline
| Giờ | Việc | Xong khi |
|---|---|---|
| 0:00–0:20 | Clone repo lên Pi, `pip3 install -r pi/requirements.txt`, chạy `python3 app.py --sim` | mở được dashboard từ laptop qua `http://<IP-Pi>:5000` |
| 0:00–1:00 | Lắp LCD + servo + HC-SR04 vào + LDR + LED + buzzer; nạp sketch | Serial Monitor ra JSON, LCD hiện `Free:4/4` |
| 1:00–1:45 | Cắm USB Arduino ↔ Pi, chạy `python3 app.py` | badge **USB Serial**, đưa tay trước cảm biến thì số xe tăng → **mốc 7–8 điểm** |
| 1:45–2:45 | Lắp HC-05 (chia áp RXD), `bluetoothctl` pair, `rfcomm bind` | badge **Bluetooth**; rút HC-05 → sau ~5 s tự về USB Serial → **mốc 9–10** |
| 2:45–3:15 | LCD1602 thứ hai lên Pi (bật I2C, `i2cdetect -y 1`), HC-SR04 cổng ra, chỉnh ngưỡng | LCD Pi hiện `Trong x/4`; đủ kịch bản xe vào / ra / đầy |
| 3:15–3:45 | Screenshot dashboard, chụp ảnh mạch, xuất CSV | đủ hình cho slide |
| 3:45–4:30 | Quay video 3–7 phút (kịch bản dưới) | 1 bản video hoàn chỉnh |
| 4:30–5:00 | Slide, push code + `pi/data/schema` lên GitHub, nộp Classroom | nộp xong |

## Phương án dự phòng
| Sự cố | Cách xử lý |
|---|---|
| HC-05 không ghép được | demo bằng USB Serial (vẫn đạt 7–8), nói rõ trong slide |
| LCD không hiện | chạy `i2c_scanner`, đổi địa chỉ `0x27` ↔ `0x3F`, vặn biến trở tương phản |
| Arduino reset khi servo quay | nguồn 5V riêng cho servo, chung GND |
| Servo rè rè / lắc, không quay | servo đang lấy nguồn từ Pi mà không chung GND → cấp servo từ 5V/GND của Arduino |
| HC-SR04 đọc lung tung | tăng `CAR_CM`/`CLEAR_CM`, đặt cảm biến thẳng, dùng bìa cứng làm "xe" |
| LCD trên Pi không hiện | `i2cdetect -y 1`, chạy `--lcd-addr 0x3f`; vặn biến trở; không được thì `--no-lcd` |
| Thiếu HC-SR04 thứ 2 | `#define USE_EXIT_SENSOR 0`, xe ra bằng nút −1 |
| Pi không có Internet | Chart.js đã đóng gói trong `pi/static/`, không cần CDN |
| Mở cổng USB làm Arduino reset | Pi tự gửi lại `CARS:n` và `MODE` khi nhận sự kiện `boot` |

## Kịch bản video (khoảng 5 phút)
1. **(0:00)** Giới thiệu nhóm, đề tài, sơ đồ 3 tầng (1 slide).
2. **(0:30)** Quay cận mạch: chỉ từng linh kiện (2 cảm biến siêu âm, LDR, servo, LCD, LED, buzzer, HC-05).
3. **(1:15)** Cảm biến: đưa "xe" vào cổng → barrier mở, LCD `Free:3/4`, LED xanh. Che LDR → đèn bãi bật.
4. **(2:00)** Truyền dữ liệu: Serial Monitor / terminal Pi in JSON; dashboard badge **Bluetooth**; LCD trên Pi đổi số chỗ trống cùng lúc.
5. **(2:30)** Dashboard: số chỗ trống, trạng thái, biểu đồ số xe + ánh sáng, bảng sự kiện.
6. **(3:15)** Điều khiển: Mở/Đóng cổng, Bật đèn, chuyển MANUAL (xe tới chỉ "chờ", bấm mở mới vào), Còi.
7. **(4:00)** Làm đầy bãi → còi + LED đỏ + "Bãi đầy". Rút HC-05 → tự chuyển USB Serial.
8. **(4:30)** Dữ liệu lưu trữ: `sqlite3 ... select` trên Pi + tải CSV.

## Dàn ý slide (9 trang)
1. Tên đề tài + danh sách thành viên
2. Giới thiệu hệ thống: bài toán bãi xe, mục tiêu
3. Kiến trúc 3 tầng (Device / Gateway / Application)
4. Sơ đồ kết nối phần cứng (bảng chân trong README)
5. Luồng dữ liệu: cảm biến → JSON → BT/Serial → Pi → SQLite → Flask → trình duyệt; lệnh đi ngược lại
6. Database schema + API
7. Kết quả: ảnh mạch + screenshot dashboard
8. Đối chiếu rubric + tính năng mở rộng (tự chuyển kênh, khôi phục số xe, MANUAL, CSV)
9. Kết luận, hạn chế, hướng phát triển
