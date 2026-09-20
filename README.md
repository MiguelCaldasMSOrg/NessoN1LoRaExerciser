# Nesso N1 LoRa Exerciser

An Arduino sketch for exercising the SX1262 LoRa radio on two Nesso N1 boards. Flash the same sketch to both boards and they will identify themselves from their ESP32 eFuse MAC addresses, discover each other, and exchange messages over LoRa.

## Features

- Automatic peer discovery with periodic `HELLO` packets
- Text messages with acknowledgements and retry handling
- Ping and pong exchanges
- Battery, uptime, charge, and free-heap telemetry
- RSSI and SNR reporting for received packets
- LoRa channel-activity detection
- Serial, button, and onboard display interaction

## Requirements

- Two Nesso N1 boards
- An Arduino-compatible development environment with ESP32 support
- [RadioLib](https://github.com/jgromes/RadioLib)
- [Arduino_Nesso_N1](https://github.com/Nesso-Community/Arduino_Nesso_N1)
- USB data cables for programming and serial monitoring

## Getting Started

1. Install the required board support and libraries.
2. Open `NessoN1_LoRa_Exerciser.ino`.
3. Review the LoRa profile near the top of the sketch. Both boards must use identical settings.
4. Select the Nesso N1 board and the appropriate serial port.
5. Upload the sketch to both boards.
6. Open each serial monitor at 115200 baud with newline enabled.

Each board displays its eight-character node ID and periodically broadcasts discovery packets. Once another node is heard, its ID appears as the peer.

## Controls

| Input | Action |
| --- | --- |
| `t <text>` | Send text to the discovered peer, or broadcast if no peer is known |
| `p` | Ping the peer |
| `h` | Send a discovery `HELLO` |
| `s` | Print local status and send telemetry |
| `c` | Run LoRa channel-activity detection |
| `?` or `help` | Print command help |
| `KEY1` | Send a text message |
| `KEY2` | Send telemetry |

## Default Radio Profile

| Setting | Value |
| --- | --- |
| Frequency | 868.1 MHz |
| Bandwidth | 125 kHz |
| Spreading factor | 7 |
| Coding rate | 4/5 |
| Sync word | `0x12` |
| TX power | 14 dBm |
| Preamble | 8 symbols |

The default profile is intended for EU868 testing. Before transmitting, set the frequency and power to values permitted in your region and observe all applicable duty-cycle requirements.

## Packet Format

Packets use a small text protocol:

```text
N1L|<type>|<source>|<destination>|<sequence>|<body>
```

Packet types cover discovery, text, acknowledgement, ping, reply, and telemetry messages. A destination of `*` denotes a broadcast.

## License

This project is released into the public domain under the [Unlicense](LICENSE).