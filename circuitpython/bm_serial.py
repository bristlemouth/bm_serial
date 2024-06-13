import board
import busio


class BristlemouthSerial:

    def __init__(self, uart=None, node_id: int = 0xC0FFEEEEF0CACC1A) -> None:
        self.node_id = node_id
        if uart is None:
            self.uart = busio.UART(board.TX, board.RX, baudrate=115200)
        else:
            self.uart = uart

    def spotter_tx(self, data: bytes) -> int | None:
        topic = b"spotter/transmit-data"
        packet = (
            self.get_pub_header()
            + len(topic).to_bytes(2, "little")
            + topic
            + b"\x01"
            + data
        )
        cobs = self.finalize_packet(packet)
        return self.uart.write(cobs)

    def spotter_log(self, filename: str, data: str) -> int | None:
        topic = b"spotter/fprintf"
        packet = (
            self.get_pub_header()
            + len(topic).to_bytes(2, "little")
            + topic
            + ("\x00" * 8)
            + len(filename).to_bytes(2, "little")
            + (len(data) + 1).to_bytes(2, "little")
            + filename
            + data
            + "\n"
        )
        cobs = self.finalize_packet(packet)
        return self.uart.write(cobs)

    def finalize_packet(self, packet: bytearray) -> bytes:
        checksum = self.crc(0, packet)
        packet[2] = checksum & 0xFF
        packet[3] = (checksum >> 8) & 0xFF
        cobs = self.cobs_encode(packet) + b"\x00"
        return cobs

    def get_pub_header(self) -> bytearray:
        return (
            bytearray.fromhex("02000000")
            + self.node_id.to_bytes(8, "little")
            + bytearray.fromhex("0101")
        )

    # Adapted from https://github.com/cmcqueen/cobs-python
    def cobs_encode(self, in_bytes: bytes) -> bytes:
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
        e, f = 0, 0
        for i in src:
            e = (seed ^ i) & 0xFF
            f = e ^ ((e << 4) & 0xFF)
            seed = (seed >> 8) ^ (((f << 8) & 0xFFFF) ^ ((f << 3) & 0xFFFF)) ^ (f >> 4)
        return seed
