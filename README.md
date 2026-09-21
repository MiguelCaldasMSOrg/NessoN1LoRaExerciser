# Nesso N1 LoRa Exerciser

An Arduino sketch for exercising the SX1262 LoRa radio on two Nesso N1 boards. Flash the same sketch to both boards and they will identify themselves from their ESP32 eFuse MAC addresses, discover each other, and exchange messages over LoRa.

All sketch behavior and radio settings are contained in `NessoN1_LoRa_Exerciser.ino`. The repository's VS Code and GitHub configuration is optional; the sketch can be opened and uploaded directly with the Arduino IDE once the board support and required libraries are installed.

## Features

- Automatic peer discovery with periodic `HELLO` packets
- Text messages with acknowledgements and retry handling
- Ping and pong exchanges
- Battery, uptime, charge, and free-heap telemetry
- RSSI and SNR reporting for received packets
- LoRa channel-activity detection
- Serial, button, and onboard display interaction
- Runtime LoRa profiles and synchronized peer switching
- GFSK modulation with synchronized LoRa/GFSK changes
- Link benchmarks and automated profile sweeps with CSV results
- Automatic CAD/backoff, airtime throttling, and synchronized transmit slots
- Multi-frequency RSSI and LoRa-activity surveys
- SX1262 receive-duty-cycle mode, boosted RX gain, and timed deep sleep
- LoRa CRC, header, IQ, and LDRO controls plus GFSK whitening
- Eight-peer discovery table and explicit peer selection
- CRC-checked fragmented transfers with selective acknowledgements and resume
- Bounded broadcast relaying with duplicate and hop-limit protection
- Raw SX1262 status, packet, IRQ, error, calibration, and link diagnostics

## Requirements

- Two Nesso N1 boards
- An Arduino-compatible development environment with the Espressif ESP32 board platform
- [RadioLib](https://github.com/jgromes/RadioLib)
- [Arduino_Nesso_N1](https://github.com/arduino-libraries/Arduino_Nesso_N1)
- USB data cables for programming and serial monitoring

The repository automation currently verifies the following versions:

| Component | Version |
| --- | --- |
| Arduino CLI | 1.5.1 |
| Espressif ESP32 core | 3.3.11 |
| Arduino_Nesso_N1 | 1.0.0 |
| RadioLib | 7.7.1 |

Compatible later versions may also work.

## Getting Started

1. Install the required board support and libraries.
2. Open `NessoN1_LoRa_Exerciser.ino`.
3. Review the LoRa profile near the top of the sketch. Both boards must use identical settings.
4. Select the Nesso N1 board and the appropriate serial port.
5. Upload the sketch to both boards.
6. Open each serial monitor at 115200 baud with newline enabled.

Each board displays its eight-character node ID and periodically broadcasts discovery packets. Once another node is heard, its ID appears as the peer.

## Build Options

### Arduino IDE

The workspace files are not required when using the Arduino IDE. Open `NessoN1_LoRa_Exerciser.ino`, select **Arduino Nesso N1**, select the board's serial port, and upload the sketch.

### Arduino CLI

The board's fully qualified board name is `esp32:esp32:arduino_nesso_n1`. A fresh Arduino CLI installation can be prepared and used as follows:

```powershell
$esp32Index = "https://espressif.github.io/arduino-esp32/package_esp32_index.json"
arduino-cli core update-index --additional-urls $esp32Index
arduino-cli core install esp32:esp32@3.3.11 --additional-urls $esp32Index
arduino-cli lib install "Arduino_Nesso_N1@1.0.0" "RadioLib@7.7.1"
arduino-cli compile --fqbn esp32:esp32:arduino_nesso_n1 --build-path ../NessoN1_LoRa_Exerciser-build .
```

### VS Code

Open `NessoN1_LoRa_Exerciser.code-workspace` to use the repository configuration. It enables the installed Arduino CLI, selects the sketch and Nesso N1 board, associates `.ino` files with C++, and supplies explicit ESP32, Nesso, RadioLib, display, and sensor include paths.

The Arduino extension writes reusable build output to the sibling directory `../NessoN1_LoRa_Exerciser-build`. Arduino requires this directory to be outside the workspace. Its `compile_commands.json` is also used by C/C++ IntelliSense. Automatic IntelliSense generation is disabled so the checked-in configuration is not overwritten.

## Runtime Behavior

- Each board derives an eight-character node ID from its ESP32 eFuse MAC address.
- A `HELLO` is sent at startup and then approximately every 25 to 30 seconds.
- Telemetry is first sent after approximately 15 to 30 seconds and then every 60 to 75 seconds.
- Text messages are acknowledged. An unacknowledged message is attempted up to three times, with a 3.5-second acknowledgement timeout.
- Duplicate text packets are not displayed twice, but they are acknowledged again so the sender can recover from a lost acknowledgement.
- A 1.2-second minimum transmit interval provides a simple demonstration duty-cycle guard.
- Until a peer is known, text, ping, and telemetry packets use the broadcast destination `*`.
- Automatic peer selection returns to broadcast after three minutes without hearing the selected peer. An explicitly selected peer remains locked.
- Protocol replies blocked by access controls enter a four-packet deferred queue and are retried up to ten times.
- Benchmarks, sweeps, transfers, and pending radio changes are kept mutually exclusive to avoid contaminating measurements or state.
- Automatic CAD, duty-percentage throttling, slotted access, low-power receive, boosted gain, and relay forwarding are disabled at startup.

## Controls

### Basic Controls

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

### Radio And Packet Controls

| Input | Action |
| --- | --- |
| `mode lora\|gfsk` | Change only this node's modulation |
| `mode sync lora\|gfsk` | Schedule the same modulation on listening peers after three seconds |
| `profiles` | List the four LoRa profiles |
| `profile <name\|number>` | Change only this node's LoRa profile |
| `profile sync <name\|number>` | Schedule the same profile on listening peers after three seconds |
| `options` | Print packet-option state |
| `crc on\|off` | Enable or disable packet CRC |
| `iq normal\|inverted` | Select normal or inverted LoRa IQ |
| `header explicit` | Use variable-length LoRa packets |
| `header implicit <bytes>` | Use fixed-length LoRa packets from 24 to 220 bytes |
| `ldro auto\|on\|off` | Select automatic or forced low-data-rate optimization |
| `whitening on\|off` | Control GFSK whitening |
| `rxgain power\|boosted` | Select power-saving or boosted SX1262 receive gain |

Packet settings must match at both ends. Use synchronized commands for modulation and profile changes; apply the other packet options manually on both boards.

### Channel Access And Power

| Input | Action |
| --- | --- |
| `cad auto on\|off` | Run LoRa CAD with randomized backoff before each transmission |
| `duty off` | Disable calculated airtime throttling |
| `duty <percent>` | Limit transmissions using calculated time on air |
| `slots on\|off` | Control local eight-slot access using 250 ms slots |
| `slots sync` | Broadcast a shared slot epoch and enable slots after three seconds |
| `lowpower on\|off` | Control SX1262 receive-duty-cycle mode |
| `sleep <seconds>` | Enter ESP32 timed deep sleep; the sketch restarts on wake |

The fixed 1.2-second transmit interval always remains active. The percentage supplied to `duty` is an experimental airtime limiter, not a declaration that a given transmission is legal.

### Measurements And Diagnostics

| Input | Action |
| --- | --- |
| `benchmark <packets> [bytes]` | Run 1 to 1000 request/reply samples with 24 to 120 byte bodies |
| `benchmark report` | Print the current or most recent benchmark result |
| `benchmark stop` | Stop the current benchmark and print its partial result |
| `sweep <packets> [bytes]` | Benchmark every LoRa profile and return both boards to `default` |
| `sweep stop` | Stop the current sweep |
| `survey` | Survey five channels centered on the active frequency |
| `survey <startMHz> <endMHz> <stepKHz> <samples>` | Survey up to 200 channels and print CSV |
| `diag` | Print SX1262 and link diagnostics as CSV |
| `diag clear` | Clear SX1262 device errors |
| `diag calibrate` | Run SX1262 image calibration at the active frequency |

Benchmark output includes send attempts, send failures, delivered replies, delivery percentage, minimum/average/maximum RTT, average RSSI and SNR, throughput, and transmit airtime. A broadcast benchmark binds to its first responder so results never combine multiple links.

### Peers, Transfers, And Relaying

| Input | Action |
| --- | --- |
| `peers` | Print the eight-entry peer table and link statistics |
| `peer auto` | Return to automatic peer selection |
| `peer <node-id>` | Lock unicast traffic to a discovered peer |
| `transfer <text>` | Send up to 1,152 bytes as a fragmented transfer |
| `transfer status` | Print incoming and outgoing transfer state |
| `transfer resume` | Resume a transfer paused after its retry limit |
| `transfer cancel` | Discard the outgoing transfer |
| `relay on\|off` | Control forwarding of relay packets |
| `relay send <node-id\|*> <1-8 hops> <text>` | Send a bounded broadcast relay message |

Transfers use up to 16 fragments of 72 bytes, a CRC-16 over the complete text, selective acknowledgement bitmaps, five automatic transmission rounds, and a 30-second receiver timeout. Relay nodes remember 16 message IDs for two minutes to suppress duplicates and loops.

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

## LoRa Profiles

| Name | Frequency | Bandwidth | Spreading factor | Coding rate | TX power | Preamble |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| `default` | 868.1 MHz | 125 kHz | 7 | 4/5 | 14 dBm | 8 symbols |
| `fast` | 868.1 MHz | 250 kHz | 7 | 4/5 | 10 dBm | 8 symbols |
| `robust` | 868.1 MHz | 125 kHz | 10 | 4/5 | 14 dBm | 12 symbols |
| `maximum-range` | 868.1 MHz | 125 kHz | 12 | 4/8 | 14 dBm | 16 symbols |

The `sweep` command coordinates and benchmarks these profiles in order. It prints one CSV result per profile and schedules both boards back to `default` when complete.

## GFSK Profile

GFSK uses the selected profile's frequency and TX power with fixed packet parameters:

| Setting | Value |
| --- | ---: |
| Bit rate | 50.0 kbps |
| Frequency deviation | 25.0 kHz |
| Receiver bandwidth | 156.2 kHz |
| Preamble | 32 bits |

SNR is a LoRa-specific measurement; received GFSK packets report an SNR value of zero.

## Packet Format

Packets use a small text protocol:

```text
N1L|<type>|<source>|<destination>|<sequence>|<body>
```

A destination of `*` denotes a broadcast.

| Type | Meaning | Body |
| --- | --- | --- |
| `H` | Discovery hello | `hello` |
| `T` | Text message | User-provided text |
| `A` | Text acknowledgement | `T:<acknowledged-sequence>` |
| `P` | Ping | `ping` |
| `R` | Ping reply | `P:<ping-sequence>;pong` |
| `S` | Telemetry | Uptime, battery voltage, charge level, and free heap |
| `B` | Benchmark request | Session, sample index, send time, and padding |
| `b` | Benchmark reply | Echo of the benchmark request body |
| `M` | Synchronized radio change | Mode, profile index, and apply delay |
| `F` | Transfer fragment | Transfer ID, fragment index/count, CRC-16, and data |
| `K` | Transfer acknowledgement | Transfer ID and received-fragment bitmap |
| `L` | Relay message | Message ID, origin, final destination, hops, and text |
| `Q` | Slotted-access synchronization | Delay until the shared slot epoch |

Protocol acknowledgements and replies that encounter the transmit guard are queued and retried. The queue is intentionally bounded so a disconnected or misconfigured peer cannot consume memory indefinitely.

## Automation

The GitHub Actions workflow runs Arduino Lint in strict sketch mode and compiles the sketch for `esp32:esp32:arduino_nesso_n1` on pushes to `master`, pull requests, and manual dispatches. The workflow installs the pinned board core and library versions listed above.

## License

This project is released into the public domain under the [Unlicense](LICENSE).