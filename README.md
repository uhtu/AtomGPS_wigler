# AtomGPS Wigler

## Overview
AtomGPS_wigler is a wardriving tool initially **created by @lozaning** using the M5Stack Atom GPS kit. It's designed for Wi-Fi network scanning with LED status indicators. Dynamic channel hopping and deduplication, [wigle.net](wigle.net) compatible. This is free, unmaintained and for use at your own risk. 

> [!CAUTION]
> Wardriving is not legal everywhere. Check local laws, don't crime without consent.

## Flashing
### Method One: [Esptool.py](https://docs.espressif.com/projects/esptool/en/latest/esp32/)

1. Locate the device:
- Linux/macOS:
  ```bash
  ls /dev/ttyUSB*
  # or 
  ls /dev/cu.*
  ```
- Windows, check COM port in Device Manager.

2. Flash the firmware, partition and bootloader using esptool.py:
  - Clone/download this repo:
    `git clone https://github.com/lukeswitz/AtomGPS_wigler.git`
  - Navigate to the build folder
  - Flash using the following command **inside the build folder:**
    
    `esptool.py -p [YOUR_PORT] -b 1500000 --before default_reset --after hard_reset --chip esp32 write_flash --flash_mode dio --flash_size detect --flash_freq 80m 0x1000 AtomGPS_wigler_v1_4.bootloader.bin 0x8000 AtomGPS_wigler_v1_4.partitions.bin 0x10000 AtomGPS_wigler_v1_4.bin`
---

### Method 2: [Arduino IDE](https://www.arduino.cc/en/software)

1. Open **the .ino file** in Arduino IDE

> [!IMPORTANT]
> 2. Update Arduino boards and libraries (if not a fresh install). Version numbers will change with time.
>
> Dependencies (Install using the left panel in the IDE):
>- Board Manager > Arduino ESP32 Boards
>- Library Manager > M5Atom Library
<img width="213" alt="image" src="https://github.com/lukeswitz/AtomGPS_wigler/assets/10099969/8b1c22f6-5721-4fad-b9e6-9464a8fe70e2">
<img width="198" alt="image" src="https://github.com/lukeswitz/AtomGPS_wigler/assets/10099969/949ed242-9b43-44ed-a2fe-160cadb20d3d">

4. Select Board: Tools > Board > esp32 > M5Atom

5. Ensure the default settings are as below:
<img width="425" alt="image" src="https://github.com/lukeswitz/AtomGPS_wigler/assets/10099969/c9a7ffc9-69f1-44ad-92a2-acf64e64c0bf">

6. Press Upload
   
---

## Use

After flashing, the device starts scanning for Wi-Fi networks, indicating the status through LEDs and saving found networks to a Wigle.net compatible CSV file. CSV files are written to the SD card with a UTC date stamp and a run number. If you have trouble, open an issue up above in the main repo.

**LED Indicators:**
> [!NOTE]  
>- Press and hold the button during scanning to disable the LED.
- RED blink if the SD card is missing/write error.
- PURPLE blink while waiting for a GPS fix.
- GREEN blink during scanning (Optional).
