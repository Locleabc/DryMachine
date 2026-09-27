#!/bin/sh
# Kiểm tra cú pháp App/ bằng gcc trên PC với HAL giả lập.
# Chạy: sh tools/host_check/check.sh   (từ thư mục gốc repo)
set -e
for f in App/Src/*.c; do
  gcc -std=c11 -Wall -Wextra -Wno-unused-parameter -fsyntax-only \
      -Itools/host_check -IApp/Inc "$f"
done
echo "App/: syntax OK"
