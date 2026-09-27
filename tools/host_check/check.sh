#!/bin/sh
# Kiểm tra App/ trên PC (không cần board, không cần Keil):
#   1. Biên dịch cú pháp toàn bộ App/ với HAL giả lập (-Wall -Wextra)
#   2. Kiểm tra quy tắc phụ thuộc giữa các tầng (docs/ARCHITECTURE.md)
#   3. Chạy test logic control/dryer_ctrl (không cần HAL)
#   4. Mô phỏng giao diện: chạy nguyên App/ui với màn hình giả lập, bấm phím theo kịch bản,
#      lưu ảnh từng màn hình vào tools/build/ui/*.ppm
# Chạy từ thư mục gốc repo:  sh tools/host_check/check.sh
set -e
OUT=tools/build; mkdir -p "$OUT"
INC="-Itools/host_check -IApp -IApp/board -IApp/control -IApp/services -IApp/ui"
for d in App/drivers/*/; do INC="$INC -I$d"; done

echo "== 1. Syntax =="
for f in $(find App -name '*.c'); do
  gcc -std=c11 -Wall -Wextra -Wno-unused-parameter -fsyntax-only $INC "$f"
done
echo "   OK"

echo "== 2. Quy tac phu thuoc =="
fail=0
bad() { echo "   VI PHAM: $1"; fail=1; }
grep -rn '#include "' App/drivers  | grep -v 'stm32f1xx_hal.h\|font5x7.h' \
  | awk -F: '{split($1,a,"/"); f=a[length(a)]; sub(/\.[ch]$/,"",f); if ($0 !~ "\""f".h\"") print}' \
  | while read l; do echo "   VI PHAM drivers: $l"; done | tee "$OUT/dep.txt"
[ -s "$OUT/dep.txt" ] && fail=1
grep -rln '#include "\(stm32\|main\|board\|sensors\|settings\|ui\|relay\|button\)' App/control && bad "control phai thuan C"
grep -rln '#include "\(main\|board\|ui\)\.h"' App/services && bad "services khong duoc include main/board/ui"
grep -rln '#include "\(main\|board\|dryer_ctrl\|sensors\|settings\|button\|relay\)\.h"' App/ui && bad "ui chi duoc dung ili9341 + util_fmt"
grep -rln '#include "main.h"' App --include=*.c --include=*.h | grep -v 'App/board/board.c' && bad "chi board.c duoc include main.h"
[ $fail = 0 ] && echo "   OK"

echo "== 3. Test logic dieu khien =="
gcc -std=c11 -Wall -IApp/control tools/host_check/test_ctrl.c App/control/dryer_ctrl.c -o "$OUT/test_ctrl"
"$OUT/test_ctrl" | tail -1

echo "== 4. Mo phong giao dien =="
mkdir -p "$OUT/ui"
gcc -std=c11 -Wall -Wno-unused-parameter -Itools/host_check -IApp/ui -IApp/drivers/ili9341 -IApp/services \
    App/ui/*.c App/drivers/ili9341/font5x7.c App/services/util_fmt.c tools/host_check/ui_sim/*.c -o "$OUT/ui_sim"
"$OUT/ui_sim" "$OUT/ui" | tail -1
exit $fail
