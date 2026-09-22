# Nesso N1 LoRa Exerciser

An Arduino sketch for exercising the SX1262 LoRa radio on two Nesso N1 boards. Flash the same sketch to both boards and they will identify themselves from their ESP32 eFuse MAC addresses, discover each other, and exchange messages over LoRa.

All sketch behavior and radio settings are contained in `NessoN1LoRaExerciser.ino`. The repository's VS Code and GitHub configuration is optional; the sketch can be opened and uploaded directly with the Arduino IDE once the board support and required libraries are installed.

See [FEATURES.md](FEATURES.md) for a complete introduction to LoRa and GFSK concepts, every exerciser feature and command, protocol behavior, limits, diagnostics, and guided experiments.

## Features

- Automatic peer discovery with periodic `HELLO` packets
- Text messages with acknowledgements and retry handling
- Ping and pong exchanges
- Battery, uptime, charge, and free-heap telemetry
- RSSI and SNR reporting for received packets
- LoRa channel-activity detection
- Touchscreen operation with five action-oriented pages, live results, and battery status
- Front-button shortcuts without requiring a serial connection
- Tap-to-disable visuals with a configurable inactivity timeout
- Three-second side-button hold for complete hardware shutdown
- Wi-Fi HTTP and Bluetooth Low Energy command APIs
- Persistent Wi-Fi, Bluetooth, LoRa, packet, and access configuration in NVS
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
- USB data cables for programming; serial monitoring is optional after upload

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
2. Open `NessoN1LoRaExerciser.ino`.
3. Review the LoRa profile near the top of the sketch. Both boards must use identical settings.
4. Select the Nesso N1 board and the appropriate serial port.
5. Upload the sketch to both boards.
6. Use the touchscreen, front button, Wi-Fi API, Bluetooth Low Energy API, or an optional serial monitor at 115200 baud with newline enabled.

Each board displays its eight-character node ID and periodically broadcasts discovery packets. Once another node is heard, its ID appears as the peer.

## Build Options

### Arduino IDE

The workspace files are not required when using the Arduino IDE. Open `NessoN1LoRaExerciser.ino`, select **Arduino Nesso N1**, select the board's serial port, and upload the sketch.

### Arduino CLI

The board's fully qualified board name is `esp32:esp32:arduino_nesso_n1`. A fresh Arduino CLI installation can be prepared and used as follows:

```powershell
$esp32Index = "https://espressif.github.io/arduino-esp32/package_esp32_index.json"
arduino-cli core update-index --additional-urls $esp32Index
arduino-cli core install esp32:esp32@3.3.11 --additional-urls $esp32Index
arduino-cli lib install "Arduino_Nesso_N1@1.0.0" "RadioLib@7.7.1"
arduino-cli compile --fqbn esp32:esp32:arduino_nesso_n1 --build-path ../NessoN1LoRaExerciser-build .
```

### VS Code

Open `NessoN1LoRaExerciser.code-workspace` to use the repository configuration. It enables the installed Arduino CLI, selects the sketch and Nesso N1 board, associates `.ino` files with C++, and supplies explicit ESP32, Nesso, RadioLib, display, and sensor include paths.

The Arduino extension writes reusable build output to the sibling directory `../NessoN1LoRaExerciser-build`. Arduino requires this directory to be outside the workspace. Its `compile_commands.json` is also used by C/C++ IntelliSense. Automatic IntelliSense generation is disabled so the checked-in configuration is not overwritten.

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
- Wi-Fi, Bluetooth, LoRa mode/profile, packet options, CAD, duty limiting, low-power receive, receive gain, and relay settings are restored from nonvolatile storage when available.
- Battery charging is configured and enabled once during startup using the Nesso library defaults. Gauge and charger status are then sampled once per minute using timeout-bounded I2C transactions.
- The display backlight and indicator LED turn off after 60 seconds without a physical button or touch action by default. Radio, Wi-Fi, Bluetooth, application processing, and active exercises continue normally.
- A board with station credentials joins that Wi-Fi network. If association fails, it starts the fixed recovery access point `Nesso-<node-id>` at `192.168.4.1/24` with password `nesso-lora`.

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
| `visuals on\|off` | Enable or disable the display backlight and indicator output |
| `visuals timeout off\|<seconds>` | Disable or set the persistent 1-to-86,400-second inactivity timeout |
| Front `KEY1` | Ping on Home, confirm a pending radio change, or return Home |
| Side `KEY2` | Tap to disable visuals; hold for three seconds to power off the board |

### Touchscreen Controls

The bottom navigation bar provides five pages:

| Page | Actions |
| --- | --- |
| Home | Send `HELLO`, ping, preset acknowledged text, or telemetry |
| Radio | Synchronize LoRa/GFSK mode and any of the four profiles after confirmation |
| Test | Run CAD, a 10-packet benchmark, a five-packet profile sweep, stop a test, or inspect results |
| Access | Toggle automatic CAD, 1% duty pacing, synchronized slots, low-power receive, boosted gain, and relay forwarding |
| Peers | Browse discovered nodes, lock selection to a peer, or return to automatic selection |

Radio changes require an on-screen confirmation. The front button confirms that dialog, pings from Home, and otherwise returns to Home. Every page header shows charge percentage and a tiny battery gauge, refreshed once per minute. Cyan indicates charging, yellow or red indicates a low battery, and green indicates normal charge. Gray `--%` means no valid reading; a gray percentage followed by `!` is stale after a failed read or two minutes without a successful refresh. Touch coordinates are rotated to match the landscape display.

A short side-button press disables visual output. The first subsequent touch or button press restores it without triggering the hidden control. The same state can be selected with `visuals on` or `visuals off`; `visuals timeout <seconds>` changes and persists the inactivity period, while `visuals timeout off` disables automatic blanking. No-visuals mode is not a system low-power state: reception, transmissions, Wi-Fi, Bluetooth, timers, commands, and exercises continue normally. Holding the side button continuously for three seconds still performs complete hardware shutdown.

Shutdown sends five low/high pulses, with each level held for 50 ms, to the Nesso power controller. This replaces holding `POWEROFF` continuously high, which can reset the board instead. Wi-Fi, BLE, the display backlight, and the radio are stopped first. If the controller leaves the CPU powered, the fallback is deep sleep with all wake sources disabled, not an automatic restart. Use the separate physical power button to switch the board on again.

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

### Visuals, Channel Access And Power

| Input | Action |
| --- | --- |
| `visuals` | Show visual-output state and inactivity timeout |
| `visuals on\|off` | Enable or disable visual output without changing runtime operation |
| `visuals timeout off\|<seconds>` | Disable or set the persistent inactivity timeout |
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
| `survey stop` | Cancel the survey and restore the active frequency and receive mode |
| `diag` | Print SX1262 and link diagnostics as CSV |
| `diag clear` | Clear SX1262 device errors |
| `diag calibrate` | Run SX1262 image calibration at the active frequency |

Benchmark output includes send attempts, send failures, delivered replies, delivery percentage, minimum/average/maximum RTT, average RSSI and SNR, throughput, and transmit airtime. A broadcast benchmark binds to its first responder so results never combine multiple links.

Surveys advance one sample at a time; the Test page's Stop control also cancels them. Transmit, CAD, and backoff waits have deadlines and service screen/input work and command ingress. Serial/HTTP/BLE commands received during those waits are queued until the radio operation finishes. Conflicting exercises, profile changes, and packet/access settings use shared exclusion checks regardless of input transport; status and explicit stop/cancel commands remain available.

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

A complete transfer is acknowledged only after CRC validation. A failed CRC sends a zero bitmap so the sender retries every fragment; acknowledgement sequence numbers reject stale snapshots. Fragment whitespace is preserved. Implicit packets are padded with zero bytes instead of spaces, so both peers must run this version when using implicit mode.

### Persistent Configuration

These commands work identically over serial, HTTP, and Bluetooth Low Energy:

| Command | Action |
| --- | --- |
| `config` | Print persisted and active configuration without revealing the Wi-Fi password |
| `config defaults` | Restore Wi-Fi, Bluetooth, LoRa, packet, and access defaults |
| `config restart` | Restart and apply pending Wi-Fi/Bluetooth identity changes |
| `wifi ssid <name>` | Store a station network name of 1 to 32 bytes |
| `wifi password <value>` | Store a station password of 8 to 63 bytes |
| `wifi password open` | Select an open station network |
| `wifi address dhcp` | Obtain the station address through DHCP |
| `wifi address <a.a.a.a/8-30>` | Store a static station address and CIDR prefix |
| `wifi clear` | Clear station credentials and use the fixed recovery AP after restart |
| `ble name <name>` | Store a Bluetooth Low Energy name of 1 to 24 bytes |

Wi-Fi and Bluetooth identity changes take effect after restart. Static station mode configures the local address and subnet only, with no default gateway or DNS server; it is intended for control from the same subnet. LoRa, access, and visual-timeout changes take effect immediately and are saved after successful application. Automated sweep profile changes and the current visual-output state remain transient.

The versioned configuration blob and separate visual-timeout value are stored in the ESP32 Non-Volatile Storage partition. A byte-for-byte comparison prevents blob writes when nothing changed. Normal sketch uploads preserve these values; enabling **Erase All Flash Before Sketch Upload** clears them. The selected build does not encrypt Non-Volatile Storage, so Wi-Fi credentials are stored in plaintext flash.

## Remote Command API

The HTTP and Bluetooth Low Energy transports accept the same command strings as the serial monitor. Commands enter a shared four-entry queue and execute through the existing command parser on the main loop. Serial output remains available for detailed command results; `GET /status` and the touchscreen expose current state.

### Wi-Fi HTTP

At startup a board with configured station credentials attempts to join that network for 12 seconds. DHCP is the default; a configured CIDR address is used instead when selected. If credentials are absent or association fails, the board creates this non-configurable recovery network:

| Setting | Value |
| --- | --- |
| Network name | `Nesso-<node-id>` |
| Password | `nesso-lora` |
| Address | `http://192.168.4.1` |

After a later station disconnect, the recovery AP starts after 30 seconds. It is removed if the station reconnects. The HTTP service is available on the station address or recovery address, whichever is active.

Send a command as the raw body of an HTTP POST:

```powershell
Invoke-RestMethod -Method Post -Uri http://192.168.4.1/command -ContentType text/plain -Body 'benchmark 10 64'
```

For short commands, `GET /command?command=p` is also accepted. URL-encode spaces when using GET. `GET /status` returns JSON containing node, radio, peer, exercise, transport, queue, and last-command state.

The command endpoint returns HTTP `202` with `{"status":"queued"}` when accepted. Empty, oversized, and queue-full commands return `400`, `413`, and `503` respectively.

### Bluetooth Low Energy

Connect to the `Nesso-<node-id>` device and use these custom Generic Attribute Profile UUIDs:

| Purpose | UUID | Property |
| --- | --- | --- |
| Service | `7bbf0001-6ba5-4e35-9f1f-8d36a7f34c01` | Service |
| Command | `7bbf0002-6ba5-4e35-9f1f-8d36a7f34c01` | Write or write without response |
| Ingress status | `7bbf0003-6ba5-4e35-9f1f-8d36a7f34c01` | Read |

Write an ordinary command directly to the command characteristic. Bluetooth Low Energy attributes are limited to 512 bytes in the installed stack. To send a longer command, use this sequence with data chunks no larger than 506 bytes:

```text
@begin:<total-command-length>
@data:<first-command-chunk>
@data:<next-command-chunk>
@end
```

Use `@cancel` to discard an incomplete command. Read the ingress-status characteristic for `ready`, chunk progress, validation errors, or the final queue result.

### Security

This initial laboratory API uses a shared fixed password for the recovery AP and an unpaired, unencrypted Bluetooth Low Energy characteristic. Station security depends on the configured network. The API has no user authorization, application encryption, replay defense, or command allowlist. Operate it only in a controlled environment and change or disable these interfaces before using the sketch around untrusted devices.

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

`LORA_FREQUENCY_MHZ` sets every profile's frequency. `LORA_TX_POWER_DBM` is the profile power ceiling (`fast` is additionally capped at 10 dBm, `maximum-range` at 14 dBm). The other named LoRa defaults configure the `default` profile; the alternative profiles retain their documented modulation settings.

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

The GitHub Actions workflow runs firmware regressions, Arduino Lint in strict sketch mode, and a compile for `esp32:esp32:arduino_nesso_n1` on pushes to `master`, pull requests, and manual dispatches. The workflow installs the pinned board core and library versions listed above.

Run the host regressions with `python tests/regressions.py` (Python 3 and a C++17 `g++` or `clang++` compiler are required; `CXX` can select the compiler). They compile actual sketch functions against simulated touch, button, display, radio, serial, and I2C hardware to check coordinate rotation, no-visuals timing and wake behavior, IRQ classification, transfer integrity, shared guards, timeout responsiveness, survey cancellation, and battery freshness. They complement, but do not replace, tests on physical boards.

## License

This project is released into the public domain under the [Unlicense](LICENSE).