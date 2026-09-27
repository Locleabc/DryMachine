#!/bin/sh
# Biên dịch + liên kết TOÀN BỘ firmware (Core + HAL + App) bằng arm-none-eabi-gcc để bắt lỗi
# khi chưa mở Keil. File .elf này chỉ để kiểm tra, firmware chính thức vẫn build bằng Keil.
# Cần: arm-none-eabi-gcc + newlib (Linux: apt install gcc-arm-none-eabi libnewlib-arm-none-eabi)
# Chạy từ thư mục gốc repo:  sh tools/gcc_check/build.sh
set -e
OUT=tools/build/gcc; mkdir -p "$OUT"
INC="-ICore/Inc -IDrivers/STM32F1xx_HAL_Driver/Inc -IDrivers/STM32F1xx_HAL_Driver/Inc/Legacy \
-IDrivers/CMSIS/Device/ST/STM32F1xx/Include -IDrivers/CMSIS/Include \
-IApp -IApp/board -IApp/control -IApp/services -IApp/ui"
for d in App/drivers/*/; do INC="$INC -I$d"; done
SRCS="$(ls Core/Src/*.c Drivers/STM32F1xx_HAL_Driver/Src/*.c) $(find App -name '*.c') tools/gcc_check/startup.c"
arm-none-eabi-gcc -mcpu=cortex-m3 -mthumb -std=gnu11 -Os -Wall -Wextra -Wno-unused-parameter \
  -DUSE_HAL_DRIVER -DSTM32F103xB -ffunction-sections -fdata-sections $INC $SRCS \
  -T tools/gcc_check/f103.ld -Wl,--gc-sections --specs=nano.specs --specs=nosys.specs -lm \
  -Wl,-Map="$OUT/fw.map" -o "$OUT/fw.elf" 2>&1 | grep -v "not implemented and will always fail\|/build/newlib" || true
arm-none-eabi-size "$OUT/fw.elf"
echo "Flash toi da 63488 B (62 KB), RAM 20480 B"
