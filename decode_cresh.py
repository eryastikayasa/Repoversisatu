import subprocess
import sys
import re

if len(sys.argv) < 2:
    print("Cara pakai:")
    print("python tools/decode_crash.py 0x42012345")
    sys.exit(1)

address = sys.argv[1]

print()
print("===================================")
print("        ESP32-S3 CRASH DECODE")
print("===================================")
print()

# Cari firmware ELF hasil build
result = subprocess.run(
    [
        "pio",
        "pkg",
        "list"
    ],
    capture_output=True,
    text=True
)

# Cari ELF dari folder .pio/build
import glob

elf_files = glob.glob(".pio/build/*/firmware.elf")

if not elf_files:
    print("ERROR: firmware.elf tidak ditemukan.")
    print("Pastikan build berhasil.")
    sys.exit(1)

elf = elf_files[0]

print("ELF:")
print(elf)
print()

print("Crash address:")
print(address)
print()

print("Decode:")
print("-----------------------------------")

subprocess.run([
    "xtensa-esp32s3-elf-addr2line",
    "-pfiaC",
    "-e",
    elf,
    address
])

print("-----------------------------------")
