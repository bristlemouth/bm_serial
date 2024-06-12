from bm_serial import BristlemouthSerial
import board
import digitalio
import time

led = digitalio.DigitalInOut(board.LED)
led.direction = digitalio.Direction.OUTPUT
bm = BristlemouthSerial()
last_send = time.time()

while True:
    now = time.time()
    if now - last_send > 60:
        led.value = True
        last_send = now
        print("publishing", now)
        bm.spotter_tx(b"foo bar baz quux")
        led.value = False
