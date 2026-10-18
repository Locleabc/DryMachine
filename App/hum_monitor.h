/**
 * @file    hum_monitor.h
 * @brief   Chương trình THỬ cảm biến độ ẩm SHT3x/SHT4x (nhánh git test/hum-monitor).
 *          Bật bằng APP_HUM_MONITOR = 1 trong app.h → App_Init/App_Loop chạy chương trình này
 *          thay cho bộ điều khiển máy sấy (mọi relay luôn NHẢ).
 *
 *  Theo dõi bằng 3 cách:
 *   1. Keil (Debug): Watch 1 → thêm biến  g_hum  ; bật View → Periodic Window Update
 *      → số liệu tự cập nhật khi chương trình đang chạy (không cần dừng CPU).
 *      g_hum.hist_rh[] = 32 lần đọc gần nhất (vòng tròn, hist_pos = ô sẽ ghi tiếp).
 *   2. Màn hình TFT: độ ẩm / nhiệt độ lớn, trạng thái, min/max/TB, số lần đọc + lỗi, 6 lần đọc gần nhất.
 *   3. UART1 (PA9, 115200): 1 dòng log mỗi lần đọc.
 *  Lúc khởi động và mỗi lần bấm EXIT: quét bus I2C (cả 2 chiều dây) → dòng "Quét" trên màn hình.
 *  LED PC13: sáng = T 20..35 °C và RH 40..80 %; tắt = ngoài khoảng; nháy 1 Hz = cảm biến lỗi.
 *  Nút: ENTER = xoá min/max/thống kê · EXIT = khởi tạo lại cảm biến · UP/DOWN = chu kỳ đọc 0.5..10 s.
 */
#ifndef HUM_MONITOR_H
#define HUM_MONITOR_H

#include <stdbool.h>
#include <stdint.h>

#define HUM_HIST_LEN   32

typedef struct {
    /* lần đọc gần nhất */
    float    rh;               /* %RH */
    float    temp;             /* °C tại cảm biến */
    bool     ok;               /* lần đọc gần nhất thành công */
    uint8_t  status;           /* sht4x_status_t: 0 OK, 1 BUS, 2 CRC, 3 NACK, 4 BUSY */
    const char *status_text;   /* "OK", "NACK – không trả lời"… */
    /* cảm biến */
    uint8_t  addr;             /* 0x44 / 0x45 */
    uint8_t  family;           /* 0 = SHT4x, 1 = SHT3x */
    uint32_t serial;
    /* thống kê từ lần xoá gần nhất */
    float    rh_min, rh_max, rh_avg;   /* rh_avg: trung bình trượt (EMA) */
    uint32_t n_read;           /* số lần đọc */
    uint32_t n_ok;
    uint32_t n_err[5];         /* theo mã lỗi (chỉ số = status) */
    uint32_t n_recover;        /* số lần khôi phục bus I2C + init lại */
    uint8_t  consec_err;       /* lỗi liên tiếp */
    /* quét bus I2C bằng GPIO (lúc init và mỗi lần bấm EXIT) */
    uint8_t  scl_high, sda_high;       /* mức chân lúc rảnh: 1 = cao (pull-up OK) */
    uint8_t  scan_n;                   /* số địa chỉ trả lời, đấu đúng chiều */
    uint8_t  scan_addr[4];
    uint8_t  scan_swap_n;              /* >0 = cảm biến trả lời khi ĐẢO SCL/SDA → đấu ngược dây */
    uint8_t  scan_swap_addr[4];
    /* lịch sử */
    float    hist_rh[HUM_HIST_LEN];
    float    hist_temp[HUM_HIST_LEN];
    uint8_t  hist_pos;         /* ô sẽ ghi tiếp */
    uint8_t  hist_count;
    /* cấu hình */
    uint32_t period_ms;        /* chu kỳ đọc */
    uint32_t last_ms;          /* thời điểm đọc gần nhất (HAL_GetTick) */
} hum_monitor_t;

extern volatile hum_monitor_t g_hum;    /* xem trong Keil Watch */

void HumMon_Init(void);
void HumMon_Loop(void);

#endif
