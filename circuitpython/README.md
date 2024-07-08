# Bristlemouth Serial Client for CircuitPython

It's super easy to get started with embedded code using CircuitPython.

As of July 2024, the first goal of the code in this circuitpython folder is to provide minimal, basic, easy access from CircuitPython to the most useful features of a Bristlemouth network as part of a Sofar Ocean Smart Mooring:

- Sending real-time telemetry via Spotter's satellite/cellular connection
- Writing logs to the Spotter SD card

## Setup

- Update your Bridge to v0.11.1 and Spotter to v2.15.1. See below.
- Flash the `serial_bridge` app to a Bristlemouth dev kit.
- Wire TX, RX, and GND between the dev kit and your CircuitPython board.
- Copy `bm_serial.py` to the lib directory of your CircuitPython board
- Call `spotter_tx` and `spotter_log` from your CircuitPython code!

That's all! For help updating Spotter firmware, keep reading.

---

## Updating Spotter Firmware

The most difficult step as of July 2024 is updating Spotter firmware to support the new functionality.
Soon Sofar Ocean will publish a comprehensive firmware management guide.
In the meantime, here are brief instructions focused on the CircuitPython use case.

You should only need to do this once to enable CircuitPython.

### Update Bridge Firmware

Download `bridge_release-v0.11.1.zip` from https://github.com/bristlemouth/bm_protocol/releases/tag/v0.11.1 and extract it to find `bridge_release-v0.11.1.elf.dfu.bin` needed below.

1. Remove the SD card from Spotter, and put it in your computer.
2. Copy the file `bridge_release-v0.11.1.elf.dfu.bin` to the SD card.
3. Properly eject the SD card from your computer.
4. Put the SD card back into the Spotter.
5. Connect to the Spotter serial terminal.

You should see
```
 _____        __             _____             _   _
/  ___|      / _|           /  ___|           | | | |
\ `--.  ___ | |_ __ _ _ __  \ `--. _ __   ___ | |_| |_ ___ _ __
 `--. \/ _ \|  _/ _` | '__|  `--. \ '_ \ / _ \| __| __/ _ \ '__|
/\__/ / (_) | || (_| | |    /\__/ / |_) | (_) | |_| ||  __/ |
\____/ \___/|_| \__,_|_|    \____/| .__/ \___/ \__|\__\___|_|
                                  | |
                                  |_| v2.9.0
```
possibly with a different version number.

The Bridge is a circuit board inside Spotter. You need to find the Bristlemouth node ID of your Bridge.

6. Enter `bridge test` in the Spotter serial terminal. The response should include a line like this:

```
2024-07-08T19:05:03.207Z [BRIDGE] [INFO] Self test result from 2b64cf4c62a575a0 - 1
```

where 2b64cf4c62a575a0 is the node ID of the Bridge.

7. Update Bridge firmware.

Enter `bridge dfu bridge_release-v0.11.1.elf.dfu.bin 0xYOURNODEID 300000` where YOU REPLACE YOURNODEID in that command with the one you received above, KEEPING the initial `0x`, for example `bridge dfu bridge_release-v0.11.1.elf.dfu.bin 0x2b64cf4c62a575a0 300000`. It's best to carefully assemble this command in a text editor, like your code editor, then copy and paste it into the serial terminal.

8. Wait for success!

Keep the serial terminal open and your computer unlocked, watching the progress of the update. Initially, you should see lines like this:

```
Sending 0:1024
ACK 0
Sending 1024:1024
ACK 400
Sending 2048:1024
ACK 800
Sending 3072:1024
ACK C00
Sending 4096:1024
ACK 1000
```

This is the Spotter main firmware transferring the firmware binary to the Bridge in chunks. At the end of the transfer you should see this:

```
Sending 250880:1024
ACK 3D400
Sending 251904:740
ACK 3D800
2024-07-08T20:53:43.542Z [BM_DFU] [INFO] Transfer complete!
2024-07-08T20:53:43.542Z [BM_DFU] [INFO] Transitioning to state: update
2024-07-08T20:53:43.542Z [BM_DFU] [INFO] File transferred, entering update phase.
2024-07-08T20:53:43.542Z [BM_DFU] [INFO] Opening bridge_release-v0.11.1.elf.dfu.bin
```

Keep waiting!

With luck, finally, a few seconds later, you should see this:

```
2024-07-08T20:53:53.695Z [BM_DFU] [INFO] Node 2b64cf4c62a575a0 update status: 1, 0
Update finished: 2b64cf4c62a575a0 success: 1 err:0
```

After which you should see a bunch of output related to the Bridge rebooting.

You must confirm that you see `success: 1 err:0`. Otherwise, the update may have failed. You can try the `bridge dfu` command again without issue. If failure persists, search/ask for help on the forum, pasting into your post especially the final 20-ish lines of success/failure output, starting from a Sending/ACK line. You also need to share the exact command you entered in the Spotter serial terminal.

### Update Spotter Firmware

DO NOT update Spotter Firmware until first updating Bridge firmware above.

1. Download the [Bristlemouth Spotter v2.15.1 Firmware Installer zip](https://5311899.fs1.hubspotusercontent-na1.net/hubfs/5311899/Spotter%20firmware%20updates%20page/v2.15.1/Spotter_BM_Firmware_Update_v2.15.1.zip)
2. Close all serial terminals
3. Connect a USB cable between your computer and Spotter
4. Run the firmware update installer for your specific computer platform, carefully following the [firmware update instructions from Sofar's support documentation](https://sofarocean.notion.site/Spotter-Firmware-Update-Process-c3e6b090d65a4eadb8fe0af868df37c5#a78fee4106c247d2817ac5bfc9bce049).

DO NOT INTERRUPT IT! It can take several minutes to run. Keep you computer open and unlocked, and watch the output of the script.

If everything succeeds, you should see "Successfully updated Spotter firmware!" when the script finishes running.


### Flash `serial_bridge` to the Dev Kit

1. Unplug cables from Spotter and any other devices to ensure you don't accidentally program them!
2. Connect to the Dev Kit's serial terminal over USB. NOT SPOTTER!!
3. Enter the `info` command and confirm that you are connected to the Dev Kit.
4. Enter the `bootloader` command. This should cause the serial terminal to close as the Dev Kit reboots into the ROM bootloader for use with dfu-util. Ignore the errors.
5. Run this command: `dfu-util -l`. Confirm that you see output like this:

```
dfu-util 0.11

Copyright 2005-2009 Weston Schmidt, Harald Welte and OpenMoko Inc.
Copyright 2010-2021 Tormod Volden and Stefan Schmidt
This program is Free Software and has ABSOLUTELY NO WARRANTY
Please report bugs to http://sourceforge.net/p/dfu-util/tickets/

Found DFU: [0483:df11] ver=0200, devnum=5, cfg=1, intf=0, path="1-1.4", alt=2, name="@OTP Memory   /0x0BFA0000/01*512 e", serial="208338744D30"
Found DFU: [0483:df11] ver=0200, devnum=5, cfg=1, intf=0, path="1-1.4", alt=1, name="@Option Bytes   /0x40022040/01*64 e", serial="208338744D30"
Found DFU: [0483:df11] ver=0200, devnum=5, cfg=1, intf=0, path="1-1.4", alt=0, name="@Internal Flash   /0x08000000/256*08Kg", serial="208338744D30"
```

6. Run this command: `dfu-util -d 0483:df11 -a 0 -s 0x08000000:leave -D serial_bridge_debug.elf.unified.bin`

DO NOT INTERRUPT IT! It can take a few minutes to run. When it finishes you should see:

```
Download done.
File downloaded successfully
Submitting leave request...
dfu-util: Error during download get_status
```

Ignore the error.

7. Connect to the Dev Kit's serial terminal.
8. Enter the `info` command. Confirm you see `APP_NAME: serial_bridge-dbg` and `FW Version` should show v0.11.1.

*Congrats!* Continue with CircuitPython over serial!
