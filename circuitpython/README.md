# Bristlemouth Serial Client for CircuitPython

It's super easy to get started with embedded code using CircuitPython.

As of June 2024, the first goal of this repo is provide minimal, basic, easy access from CircuitPython to the most useful features of a Bristlemouth network as part of a Sofar Ocean Smart Mooring:

- Sending real-time telemetry via Spotter's satellite/cellular connection
- Writing logs to the Spotter SD card

## Setup

- Flash the `serial_bridge` app to a Bristlemouth dev kit.
- Wire TX, RX, and GND between the dev kit and your CircuitPython board.
- Copy `bm_serial.py` to the lib directory of your CircuitPython board
- Call `spotter_tx` and `spotter_log` from your CircuitPython code!
