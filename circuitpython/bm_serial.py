import board
import busio


class BristlemouthSerial:
    def __init__(self, uart=None) -> None:
        if uart is None:
            self.uart = busio.UART(board.TX, board.RX, baudrate=115200)
        else:
            self.uart = uart

    def spotter_tx(self, data) -> int | None:
        packet = self.build_spotter_tx_packet(data)
        return self.uart.write(packet)

    def build_spotter_tx_packet(self, data: bytearray) -> bytes:
        packet = (
            bytearray.fromhex("020000001acccaf0eeeeffc001011500")
            + b"spotter/transmit-data\x01"
            + data
        )
        checksum = self.crc(0, packet)
        packet[2] = checksum & 0xFF
        packet[3] = (checksum >> 8) & 0xFF
        cobs = self.cobs_encode(packet) + b"\x00"
        return cobs

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

    def crc(self, seed: int, src: bytearray) -> int:
        e, f = 0, 0
        for i in src:
            e = (seed ^ i) & 0xFF
            f = e ^ ((e << 4) & 0xFF)
            seed = (seed >> 8) ^ (((f << 8) & 0xFFFF) ^ ((f << 3) & 0xFFFF)) ^ (f >> 4)
        return seed
