import serial
import struct
import fcntl
import time
from enum import Enum


class BristlemouthSerial:

    class BmSerialTxMessage(Enum):
        BM_SERIAL_DEBUG = 0x00
        BM_SERIAL_ACK = 0x01
        BM_SERIAL_PUB = 0x02
        BM_SERIAL_SUB = 0x03
        BM_SERIAL_UNSUB = 0x04
        BM_SERIAL_LOG = 0x05
        BM_SERIAL_NET_MSG = 0x06
        BM_SERIAL_RTC_SET = 0x07
        BM_SERIAL_SELF_TEST = 0x08
        BM_SERIAL_NETWORK_INFO = 0x09
        BM_SERIAL_REBOOT_INFO = 0x0A
        BM_SERIAL_DFU_START = 0x30
        BM_SERIAL_DFU_CHUNK = 0x31
        BM_SERIAL_DFU_RESULT = 0x32
        BM_SERIAL_CFG_GET = 0x40
        BM_SERIAL_CFG_SET = 0x41
        BM_SERIAL_CFG_VALUE = 0x42
        BM_SERIAL_CFG_COMMIT = 0x43
        BM_SERIAL_CFG_STATUS_REQ = 0x44
        BM_SERIAL_CFG_STATUS_RESP = 0x45
        BM_SERIAL_CFG_DEL_REQ = 0x46
        BM_SERIAL_CFG_DEL_RESP = 0x47
        BM_SERIAL_CFG_CLEAR_REQ = 0x48
        BM_SERIAL_CFG_CLEAR_RESP = 0x49
        BM_SERIAL_DEVICE_INFO_REQ = 0x50
        BM_SERIAL_DEVICE_INFO_REPLY = 0x51
        BM_SERIAL_RESOURCE_REQ = 0x52
        BM_SERIAL_RESOURCE_REPLY = 0x53
        BM_SERIAL_NODE_ID_REQ = 0x60
        BM_SERIAL_NODE_ID_REPLY = 0x61
        BM_SERIAL_BAUD_RATE_REQ = 0x70
        BM_SERIAL_BAUD_RATE_REPLY = 0x71

    def __init__(self, uart=None, node_id: int = 0xC0FFEEEEF0CACC1A) -> None:
        self.node_id = node_id
        self.sub_cbs = dict()
        if uart is None:
            self.uart = serial.Serial(port="/dev/ttyAMA0", baudrate=115200, timeout=0.5)
        else:
            self.uart = uart

    def _read_until_idle(self, timeout: float = 1.0) -> bytes:
        """
        Reads from the serial port until no data is received for a specified timeout period.

        Args:
            ser (serial.Serial): Serial port object.
            timeout (float): Timeout in seconds to wait for more data.

        Returns:
            bytes: Concatenated data read from the serial port.
        """
        data = bytearray()
        last_rx_time = time.monotonic()

        while True:
            data_waiting = self.uart.in_waiting
            if data_waiting > 0:
                data.extend(self.uart.read(data_waiting))
                last_rx_time = time.monotonic()
            else:
                if time.monotonic() - last_rx_time > timeout:
                    break
                time.sleep(0.01)

        return bytes(data)

    def _process_publish_message(self, payload: str) -> None:
        """
        Processes incoming published message.

        Invokes callback associated with the topic published to the device.

        Args:
            ser (payload): Serial payload sent from companion device.
        """
        format = "<QBBH"

        try:
            pub_header = struct.unpack(format, payload[:12])
            node_id = pub_header[0]
            type = pub_header[1]
            version = pub_header[2]
            topic_len = pub_header[3]
            topic = str(payload[12 : 12 + topic_len])
            data_len = len(payload[12 + topic_len :])
            data = payload[12 + topic_len :]
            for key in self.sub_cbs:
                if topic == key:
                    self.sub_cb[topic](
                        node_id, type, version, topic_len, topic, data_len, data
                    )

        except Exception:
            print("Error unpacking publish message")

    def bristlemouth_process(self, timeout_s: float = 0.5) -> None:
        """
        Read incoming serial data until an idle line is detected.

        Handles the data based on the type of packet that has been
        received.

        Args:
            timeout_s (float): Timeout in seconds to wait for incoming serial
                               data
        """
        format = "<BBH"
        data = self._read_until_idle(timeout_s)

        if len(data) != 0:
            try:
                serial_packet = struct.unpack(format, data[:4])
                payload = data[4:]
                type = serial_packet[0]
                if type == self.BmSerialTxMessage.BM_SERIAL_PUB.value:
                    self._process_publish_message(payload)
            except Exception:
                print("Error unpacking data from read output")

    def bristlemouth_sub(self, topic: str, fn) -> Int | None:
        """
        Subscribe to a topic on the Bristlemouth network.

        Whenever data is published to this topic on the Bristlemouth network
        the callback associated through this method is invoked.
        Only one callback may be associated with a topic.

        Args:
            topic (str): Topic string to subscribe to
            fn: Function to use when topic is published to network

        Returns:
            int: The number of bytes serially written to the companion device
            None: Device could not properly send data serially
        """
        packet = (
            bytearray.fromhex("03000000")
            + len(topic).to_bytes(2, "little")
            + bytearray(topic.encode("utf-8"))
        )
        cobs = self.finalize_packet(packet)

        # Set callback for topic
        if fn is not None:
            self.sub_cbs[topic] = fn
        return self.lock_uart_and_write_bytes(cobs)

    def spotter_tx(self, data: bytes) -> int | None:
        """
        Transmit data to Sofar backend.

        This message transmits raw bytes to Sofar's backend. Viewable with the
        raw-messages API:
            https://api.sofarocean.com/api/raw-messages?spotterId={SPOT-ID}

        Args:
            data (bytes): Bytes to transmit

        Returns:
            int: The number of bytes serially written to the companion device
            None: Device could not properly send data serially
        """
        topic = b"spotter/transmit-data"
        packet = (
            self.get_pub_header()
            + len(topic).to_bytes(2, "little")
            + topic
            + b"\x01"
            + data
        )
        cobs = self.finalize_packet(packet)
        return self.lock_uart_and_write_bytes(cobs)

    def spotter_log(self, filename: str, data: str) -> int | None:
        """
        Log message to spotter SD card.

        Logs string to spotter's SD card at specified file. This file will be
        associated with the companion devices node_id and found in the
        following directory:
            /bm/{node_id}/{filename}.log

        Args:
            filename (str): File name to write data to
            data (str): String to write to file

        Returns:
            int: The number of bytes serially written to the companion device
            None: Device could not properly send data serially
        """
        topic = b"spotter/fprintf"
        packet = (
            self.get_pub_header()
            + len(topic).to_bytes(2, "little")
            + topic
            + b"\x00" * 8
            + len(filename).to_bytes(2, "little")
            + (len(data) + 1).to_bytes(2, "little")
            + filename.encode("utf-8")  # Convert filename to bytes
            + data.encode("utf-8")  # Convert data to bytes
            + b"\n"
        )
        cobs = self.finalize_packet(packet)
        return self.lock_uart_and_write_bytes(cobs)

    def spotter_print(self, data: str) -> int | None:
        """
        Print data to Spotter's console.


        Args:
            data (str): String to display on Spotter console

        Returns:
            int: The number of bytes serially written to the companion device
            None: Device could not properly send data serially
        """
        topic = b"spotter/printf"
        packet = (
            self.get_pub_header()
            + len(topic).to_bytes(2, "little")
            + topic
            + b"\x00" * 8
            + (0).to_bytes(2, "little")  # Zero filename length
            + (len(data) + 1).to_bytes(2, "little")
            + data.encode("utf-8")  # Convert data to bytes
            + b"\n"
        )
        cobs = self.finalize_packet(packet)
        return self.lock_uart_and_write_bytes(cobs)

    def lock_uart_and_write_bytes(self, bytes) -> int | None:
        """
        Lock UART interface and write raw bytes to serial interface

        Returns:
            int: The number of bytes serially written to the companion device
            None: Device could not properly send data serially
        """
        fcntl.lockf(self.uart, fcntl.LOCK_EX)
        written = self.uart.write(bytes)
        fcntl.lockf(self.uart, fcntl.LOCK_UN)
        return written

    def finalize_packet(self, packet: bytearray) -> bytes:
        """
        Used to add CRC and COBS encode packets.

        Returns:
            bytes: COBS encoded packet
        """
        checksum = self.crc(0, packet)
        packet[2] = checksum & 0xFF
        packet[3] = (checksum >> 8) & 0xFF
        cobs = self.cobs_encode(packet) + b"\x00"
        return cobs

    def get_pub_header(self) -> bytearray:
        """
        Header used for publishing data to companion device.

        Header bytes for serial publish message.

        Returns:
            bytearray: Bytes for publish header
        """
        return (
            bytearray.fromhex("02000000")
            + self.node_id.to_bytes(8, "little")
            + bytearray.fromhex("0101")
        )

    # Adapted from https://github.com/cmcqueen/cobs-python
    def cobs_encode(self, in_bytes: bytes) -> bytes:
        """
        COBS encode input data.

        Args:
            in_bytes (bytes): Bytes to COBS encode

        Returns:
            bytes: COBS encoded bytes
        """
        final_zero = True
        out_bytes = bytearray()
        idx = 0
        search_start_idx = 0
        for in_char in in_bytes:
            if in_char == 0:
                final_zero = True
                out_bytes.append(idx - search_start_idx + 1)
                out_bytes += in_bytes[search_start_idx:idx]
                search_start_idx = idx + 1
            else:
                if idx - search_start_idx == 0xFD:
                    final_zero = False
                    out_bytes.append(0xFF)
                    out_bytes += in_bytes[search_start_idx : idx + 1]
                    search_start_idx = idx + 1
            idx += 1
        if idx != search_start_idx or final_zero:
            out_bytes.append(idx - search_start_idx + 1)
            out_bytes += in_bytes[search_start_idx:idx]
        return bytes(out_bytes)

    def crc(self, seed: int, src: bytes) -> int:
        """
        CRC16 calculation based on CCIT.

        Args:
            seed (int): Initial seed for the CRC16 calculation
            src (bytes): Data to calculate the CRC of

        Returns:
            int: Calculated CRC16
        """
        e, f = 0, 0
        for i in src:
            e = (seed ^ i) & 0xFF
            f = e ^ ((e << 4) & 0xFF)
            seed = (seed >> 8) ^ (((f << 8) & 0xFFFF) ^ ((f << 3) & 0xFFFF)) ^ (f >> 4)
        return seed
