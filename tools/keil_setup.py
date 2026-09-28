#!/usr/bin/env python3
"""
keil_setup.py – gắn thư mục App/ vào project Keil do CubeMX sinh ra (chạy lại bao nhiêu lần cũng được).

Chạy từ thư mục gốc repo, SAU KHI CubeMX đã GENERATE CODE:
    python tools/keil_setup.py

Việc script làm:
  1. MDK-ARM/DryMachine.uvprojx
     - thêm các nhóm App/... với toàn bộ file .c trong App/ (quét lại mỗi lần chạy → file mới tự vào)
     - thêm Include Paths của App/
     - IROM1 Size = 0xF800 (chừa 2 page Flash cuối cho lịch sử lỗi + thông số)
     - bật MicroLIB, ngôn ngữ C = gnu11 (nếu dùng Arm Compiler 6)
     - ARMCC 5: thêm --no_multibyte_chars (chuỗi tiếng Việt UTF-8)
  2. Core/Src/main.c: chèn #include "app.h", App_Init(), App_Loop() vào vùng USER CODE
  3. Core/Src/rtc.c: CubeMX luôn sinh HAL_RTC_SetTime(00:00) trong MX_RTC_Init → mỗi lần cấp điện
     sẽ xoá giờ. Chèn vào vùng USER CODE Check_RTC_BKUP: nếu đã chỉnh giờ (BKP_DR1 = 0xA5A5,
     khớp RTC_MAGIC trong board.c) thì return, giữ nguyên bộ đếm RTC.
Trước khi sửa, bản gốc được lưu thành *.bak.
"""
import os
import re
import shutil
import sys
import xml.etree.ElementTree as ET

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PROJECT = os.path.join(ROOT, "MDK-ARM", "DryMachine.uvprojx")
MAIN_C = os.path.join(ROOT, "Core", "Src", "main.c")
RTC_C = os.path.join(ROOT, "Core", "Src", "rtc.c")
APP = os.path.join(ROOT, "App")
IROM_START = "0x8000000"   # Flash STM32 bắt đầu ở 0x08000000 (CubeMX để trống = 0x0 → sai)
IROM_SIZE = "0xF800"
GROUP_PREFIX = "App/"

# nhóm Keil → thư mục con trong App (đệ quy)
GROUPS = [
    ("App/app",      ["."],        False),   # chỉ file ở App/ (không đệ quy)
    ("App/board",    ["board"],    True),
    ("App/drivers",  ["drivers"],  True),
    ("App/services", ["services"], True),
    ("App/control",  ["control"],  True),
    ("App/ui",       ["ui"],       True),
]


def rel_from_mdk(path):
    return os.path.relpath(path, os.path.dirname(PROJECT)).replace("\\", "/")


def c_files(sub, recursive):
    base = os.path.join(APP, sub)
    out = []
    if recursive:
        for d, _, files in os.walk(base):
            out += [os.path.join(d, f) for f in files if f.endswith(".c")]
    else:
        out = [os.path.join(base, f) for f in os.listdir(base) if f.endswith(".c")]
    return sorted(out)


def include_dirs():
    dirs = set()
    for d, _, files in os.walk(APP):
        if any(f.endswith(".h") for f in files):
            dirs.add(d)
    return sorted(rel_from_mdk(d) for d in dirs)


def set_text(parent, tag, value):
    el = parent.find(tag)
    if el is None:
        el = ET.SubElement(parent, tag)
    el.text = value


def patch_project():
    if not os.path.exists(PROJECT):
        sys.exit(f"Khong thay {PROJECT}\n-> Mo DryMachine.ioc bang STM32CubeMX va bam GENERATE CODE truoc.")
    if not os.path.exists(PROJECT + ".bak"):
        shutil.copyfile(PROJECT, PROJECT + ".bak")

    ET.register_namespace("xsi", "http://www.w3.org/2001/XMLSchema-instance")
    tree = ET.parse(PROJECT)
    root = tree.getroot()
    incs = include_dirs()

    for target in root.iter("Target"):
        name = target.findtext("TargetName")
        opt = target.find("TargetOption")

        # 1. IROM1 = 0x08000000, 62 KB  (phải đặt CẢ địa chỉ bắt đầu)
        for ocr in opt.iter("OCR_RVCT4"):
            set_text(ocr, "StartAddress", IROM_START)
            set_text(ocr, "Size", IROM_SIZE)

        # 2. MicroLIB
        for misc in opt.iter("ArmAdsMisc"):
            set_text(misc, "useUlib", "1")

        # 3. C include path + ngôn ngữ
        cads = opt.find("TargetArmAds/Cads")
        if cads is not None:
            inc_el = cads.find("VariousControls/IncludePath")
            cur = [p for p in (inc_el.text or "").split(";") if p]
            for p in incs:
                if p not in cur:
                    cur.append(p)
            inc_el.text = ";".join(cur)
            common = target.find("uAC6")
            if common is None:
                common = opt.find("TargetCommonOption/uAC6")
            if common is not None and common.text == "1":
                set_text(cads, "v6Lang", "6")          # gnu11 (Arm Compiler 6)
            # ARMCC 5: chuỗi UTF-8 tiếng Việt → tắt xử lý ký tự đa byte (tránh cảnh báo #870-D)
            misc = cads.find("VariousControls/MiscControls")
            if misc is not None and "--no_multibyte_chars" not in (misc.text or ""):
                misc.text = ((misc.text or "") + " --no_multibyte_chars").strip()

        # 4. Groups
        groups = target.find("Groups")
        for g in list(groups):
            if (g.findtext("GroupName") or "").startswith(GROUP_PREFIX):
                groups.remove(g)
        n_files = 0
        for gname, subs, recursive in GROUPS:
            files = [f for s in subs for f in c_files(s, recursive)]
            if not files:
                continue
            g = ET.SubElement(groups, "Group")
            ET.SubElement(g, "GroupName").text = gname
            fl = ET.SubElement(g, "Files")
            for f in files:
                fe = ET.SubElement(fl, "File")
                ET.SubElement(fe, "FileName").text = os.path.basename(f)
                ET.SubElement(fe, "FileType").text = "1"
                ET.SubElement(fe, "FilePath").text = rel_from_mdk(f)
                n_files += 1
        print(f"[{name}] IROM1={IROM_START}+{IROM_SIZE}, MicroLIB, {len(incs)} include path, {n_files} file .c trong App/")

    ET.indent(tree, space="  ")
    tree.write(PROJECT, encoding="UTF-8", xml_declaration=True)


MAIN_EDITS = [
    ("/* USER CODE BEGIN Includes */", '#include "app.h"'),
    ("/* USER CODE BEGIN 2 */",        "  App_Init();"),
    ("/* USER CODE BEGIN 3 */",        "    App_Loop();"),
]


def patch_main():
    if not os.path.exists(MAIN_C):
        sys.exit(f"Khong thay {MAIN_C}")
    src = open(MAIN_C, encoding="utf-8", errors="surrogateescape").read()
    orig = src
    nl = "\r\n" if "\r\n" in src else "\n"
    for marker, line in MAIN_EDITS:
        if marker not in src:
            sys.exit(f"main.c thieu '{marker}' – kiem tra lai file CubeMX sinh ra")
        # chỉ chèn nếu vùng USER CODE đó chưa có dòng này
        end = marker.replace("BEGIN", "END")
        block = src[src.index(marker):src.index(end)]
        if line.strip() not in block:
            src = src.replace(marker, marker + nl + line, 1)
    if src != orig:
        if not os.path.exists(MAIN_C + ".bak"):
            shutil.copyfile(MAIN_C, MAIN_C + ".bak")
        open(MAIN_C, "w", encoding="utf-8", errors="surrogateescape", newline="").write(src)
        print("main.c: da chen App_Init() / App_Loop()")
    else:
        print("main.c: da co san, khong doi")


RTC_GUARD = "  if (HAL_RTCEx_BKUPRead(&hrtc, RTC_BKP_DR1) == 0xA5A5U) return;   /* da chinh gio -> giu nguyen RTC */"


def patch_rtc():
    if not os.path.exists(RTC_C):
        print("rtc.c: khong co (RTC chua bat trong CubeMX?)")
        return
    src = open(RTC_C, encoding="utf-8", errors="surrogateescape").read()
    marker = "/* USER CODE BEGIN Check_RTC_BKUP */"
    if marker not in src:
        print("rtc.c: khong thay vung Check_RTC_BKUP, bo qua")
        return
    block = src[src.index(marker):src.index("/* USER CODE END Check_RTC_BKUP */")]
    if "RTC_BKP_DR1" in block:
        print("rtc.c: da co san, khong doi")
        return
    nl = "\r\n" if "\r\n" in src else "\n"
    if not os.path.exists(RTC_C + ".bak"):
        shutil.copyfile(RTC_C, RTC_C + ".bak")
    src = src.replace(marker, marker + nl + RTC_GUARD, 1)
    open(RTC_C, "w", encoding="utf-8", errors="surrogateescape", newline="").write(src)
    print("rtc.c: da chan HAL_RTC_SetTime khi da chinh gio")


if __name__ == "__main__":
    patch_project()
    patch_main()
    patch_rtc()
    print("Xong. Mo MDK-ARM/DryMachine.uvprojx bang Keil va bam Build (F7).")
