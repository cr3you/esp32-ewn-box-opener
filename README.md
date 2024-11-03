# esp32-ewn-box-opener
Esp32 port of the Box Opener client for mining EWN tokens.

Original Box Opener client written in python: https://github.com/Erwin-Schrodinger-Token/ewn-box-opener

For easy import I suggest using VSCode with PlatformIO plugin.

This projest uses ciband/bip39 library and esp32 hardware random number generator to create mnemonics.

I also used this code to read and write config file: https://github.com/mo-thunderz/Esp32ConfigFile/

This is a work in progress. There are more features to be added (like GUI to set wifi credencials or the API key).
But for now it should just work.

# tl;dr if you have T-Display-S3 board
https://github.com/user-attachments/assets/d2d30b63-174e-40fe-8696-ee304904ff61

You can just flash compiled bin file from here: [creys_box_opener_t_disp_s3_v1_2_0.bin](https://github.com/cr3you/esp32-ewn-box-opener/releases/download/1.2.0/creys_box_opener_t_disp_s3_v1_2_0.bin) (right click and Save as..)

And use any esp32 flashing tool to upload it to your board at address 0x0

Example online flashing tool (open in chromium based browser like Chrome, Opera..) https://espressif.github.io/esptool-js/

After flashing set up your board using this guide: [Setting up the wifi and APIkey](README.md#setting-up-the-wifi-and-apikey)

## Importing project.
**The easiest way to import is to have VSCode (Visual Studio Code editor) installed with PlatformIO extension on your computer.**

Download whole repository to your disk (the green "Code" button somewhere in upper right-> Download ZIP).

Extract the **esp32-ewn-box-opener-main** direcotry to your disk.

Open VSCode, open PlatformIO extension (usually the ant head icon on the left strip).

Choose "Pick a folder" and open your extracted folder.

Let PlatformIO do its thing, it can take a while if you import the project for the first time.

Alternatively you could just right click on your folder and choose "open with Code"...

## Setting up to yur board.

If you have different ESP32 board than T-Display-S3 you should change it in platformio.ini file
```
[env:esp32-ewn-box-opener]
platform = espressif32
board = lilygo-t-display-s3    <--- change this
```
For the old T-Display boards:
```
board = lilygo-t-display
```
For the ESP32-S3-DevKitC-1-N8R2 and -N16R8 boards:
```
board = esp32-s3-devkitc-1
```
If you have errors for N8R2 boards, try adding this line above the build_flags section:
```
build_unflags = -DBOARD_HAS_PSRAM
```

If you have other board, search it under this link and find the board type:

https://docs.platformio.org/en/latest/boards/index.html#espressif-32

## Biuld and Upload to your board
First build the code to check if everything is set up OK

![obraz](https://github.com/user-attachments/assets/a6d4fdbc-679f-4a85-9565-03a5c5afb5b4)

If you see SUCCESS on your output window then you are ready to upload the firmware.

![obraz](https://github.com/user-attachments/assets/2886cdfe-3fb5-4129-8aeb-1d500722a7e9)

Now upload the code to your board

![obraz](https://github.com/user-attachments/assets/d2f83300-1e47-4519-a5bf-0bffa26170a2)

And now you are ready to set up your box-opener :)
## Setting up the wifi and APIkey
Get your API key from:

Mainnet - https://erwin.lol/box-opener

Devnet - https://devnet.erwin.lol/box-opener

If you flashed the firmware from VSCode then serial terminal should open and you should be able to config your board right away.

If you flashed the bin file then you need some serial terminal software (there are many out there).
Or you can use online version (in Chrome, Opera or other chromium based browser):

https://www.serialterminal.com

Open serial terminal with 115200 baudrate.

![initial_setup](https://github.com/user-attachments/assets/64751288-44b2-45cb-8561-29d5ac0a9b16)

You can read config by typing ```readconfig``` and pressing ENTER

Set up your Wifi SSID and password and apikey by using commands (do not use space after the : character):

```ssid:YOUR_WIFI_SSID```

```pass:YOUR_WIFI_PASSWORD```

```apikey:YOUR_APIKEY```

Then you write your config by typing: ```writeconfig```

You can delete the config file: ```delconfig```

If you want to use devnet, type ```devnet``` if you want to go back to mainnet, type ```mainnet```

Remember to write the config after change!

## Running.
Open serial terminal with 115200 bps baudrate and connect to the esp32 board.

If everything is OK you should see something like this:
```
Connecting to WiFi ...
192.168.145.183
===============
Box-opener started
⚙️ Generating guesses...
🔑️ Generated 50 guesses
➡️ Submitting to oracle
✅ Guesses accepted
waiting 10s for next batch...
⚙️ Generating guesses...
🔑️ Generated 50 guesses
➡️ Submitting to oracle
✅ Guesses accepted
waiting 10s for next batch...
⚙️ Generating guesses...
🔑️ Generated 50 guesses
➡️ Submitting to oracle
✅ Guesses accepted
waiting 10s for next batch...
```
## Problems.

### 1. If you have "Unknown board ID" error after changing board type in platformio.ini
Reset your boards by deleting the ".platformio/platforms" folder (on Windows it is located in user directory)

More info there: https://www.luisllamas.es/en/plaftormio-unknown-board-id/
