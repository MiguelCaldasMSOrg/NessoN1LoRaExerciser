# Nesso N1 LoRa Exerciser Feature Guide

This document explains every feature exposed by the Nesso N1 LoRa Exerciser. It is written for a reader who understands general radio and wireless communication, but has not previously worked with LoRa, Gaussian frequency-shift keying, the Semtech SX1262, or LoRa peer-to-peer protocols.

The same sketch is normally loaded onto two Arduino Nesso N1 boards. The boards identify themselves, discover one another, exchange packets, and run controlled radio experiments. A third or additional boards are useful for peer-table, contention, slotted-access, and relay experiments.

The implementation described here is [NessoN1LoRaExerciser.ino](NessoN1LoRaExerciser.ino).

## Read This First

### This Is Raw LoRa, Not LoRaWAN

LoRa is Semtech's long-range chirp modulation. LoRaWAN, meaning Long Range Wide Area Network, is a networking standard built on top of LoRa. LoRaWAN defines gateways, device joining, network addressing, security, and application delivery rules.

This exerciser does not implement LoRaWAN. It uses the SX1262 radio directly and places a small, human-readable protocol over raw LoRa or Gaussian frequency-shift keying packets. It therefore:

- does not join a LoRaWAN network;
- does not communicate with LoRaWAN gateways;
- does not use LoRaWAN device addresses or frame formats;
- does not encrypt or authenticate packets;
- does not provide network-managed channel access; and
- does not provide guaranteed delivery.

Think of it as a packet-radio laboratory with a custom protocol, not as a production network stack.

### The Radio Traffic Is Plaintext

Anyone using compatible radio settings can receive the packets, forge a node identifier, inject commands understood by the protocol, acknowledge messages, or create relay traffic. Do not transmit secrets. The node identifier is for convenient identification, not authentication.

### Radio Regulations Remain the Operator's Responsibility

The supplied profiles use 868.1 megahertz and up to 14 decibels relative to one milliwatt of transmitter power. This is a common European test configuration, but it is not automatically legal everywhere. Permitted frequency, radiated power, occupied bandwidth, duty cycle, dwell time, and channel-access rules depend on location, antenna gain, equipment classification, and application.

The `duty` and `cad auto` features are experimental controls. They do not certify compliance. In particular:

- transmitter power in decibels relative to one milliwatt is not the same as equivalent isotropically radiated power, which also includes antenna gain and feed losses;
- the duty limiter does not know regional sub-bands or regulatory observation windows;
- channel activity detection recognizes compatible LoRa activity, not every possible signal in the channel; and
- the 1.2-second transmit interval is a demonstration guard, not a regulatory rule.

Verify the applicable rules before connecting an antenna and transmitting.

## System Model

The exerciser spans several layers. Keeping them separate makes the observations easier to interpret.

| Layer | What the exerciser controls |
| --- | --- |
| Hardware | The Nesso N1's ESP32-C6 microcontroller, SX1262 transceiver, antenna switch, low-noise amplifier, display, battery monitor, buttons, and status light |
| Physical layer | LoRa or Gaussian frequency-shift keying modulation, frequency, bandwidth, spreading factor, coding rate, power, preamble, headers, packet checksum, and receive settings |
| Channel access | A fixed transmit interval, optional LoRa channel activity detection, randomized backoff, optional airtime limiting, and optional time slots |
| Link protocol | Discovery, node identifiers, broadcast and selected-peer delivery, sequence numbers, acknowledgements, retries, and duplicate suppression |
| Exercises | Text, ping, telemetry, benchmarks, profile sweeps, channel surveys, fragmented transfers, relaying, power modes, and diagnostics |

The SX1262 is half-duplex: it cannot transmit and receive simultaneously. The sketch places it in standby before transmission and returns it to receive mode afterward. While transmitting, the onboard indicator is lit and the board's receive low-noise amplifier is disabled. The amplifier is restored for reception.

The display and radio share a Serial Peripheral Interface bus. The board's control signals also use an Inter-Integrated Circuit bus through the Nesso N1 support hardware. These buses are board implementation details; the user normally interacts through the touchscreen, front button, remote command APIs, or serial monitor.

## LoRa Fundamentals

### Chirp Spread Spectrum

LoRa uses chirp spread spectrum. A chirp is a signal whose instantaneous frequency moves across the configured channel bandwidth during one symbol. Information is represented by where the cyclic chirp begins. The receiver correlates the incoming chirp with the expected chirp pattern and recovers the symbol even when the signal is below the in-channel noise power.

This explains two characteristics that may initially seem unusual:

1. A decoded LoRa packet may report a negative signal-to-noise ratio. The signal power can be below the measured noise power and still correlate successfully.
2. Increasing the spreading factor can improve sensitivity, but makes every symbol much longer. Range and robustness are purchased with airtime, latency, energy, and reduced network capacity.

LoRa is not ordinary narrowband frequency-shift keying and is not frequency hopping. Each chirp sweeps within the selected channel, while the channel's center frequency remains fixed.

### Frequency

All nodes in an exchange must use the same center frequency. The four supplied LoRa profiles all use 868.1 MHz.

Changing spreading factor does not change the center frequency. Changing mode from LoRa to Gaussian frequency-shift keying also retains the active profile's center frequency.

### Bandwidth

Bandwidth is the width of the LoRa chirp channel. It is expressed here in kHz.

For the same spreading factor:

- more bandwidth gives shorter symbols and a higher nominal data rate;
- less bandwidth integrates less thermal noise and generally improves sensitivity; and
- different bandwidths are not mutually decodable.

The supplied profiles use either 125 or 250 kHz.

### Spreading Factor

The spreading factor, abbreviated SF, selects the number of possible chirp positions in a symbol. An SF value of `N` provides `2^N` chirp positions. The sketch uses SF7, SF10, and SF12.

The approximate LoRa symbol duration is:

$$T_{symbol} = \frac{2^{SF}}{bandwidth}$$

At 125 kHz:

- SF7 has a symbol duration of approximately 1.024 milliseconds;
- SF10 has a symbol duration of approximately 8.192 milliseconds; and
- SF12 has a symbol duration of approximately 32.768 milliseconds.

A higher spreading factor usually gives better receiver sensitivity and more tolerance of weak signals, but the longer symbols increase packet airtime dramatically. Longer airtime also increases collision exposure and energy used per delivered byte. Nodes must use the same spreading factor for this exerciser.

Different LoRa spreading factors have some useful cross-correlation properties, but they are not perfectly orthogonal in real radios and this sketch does not listen on several spreading factors at once.

### Coding Rate

The coding rate, abbreviated CR, controls forward error correction. Forward error correction adds redundant bits so the receiver can recover some damaged information without requesting a retransmission.

The profile notation `4/5` means four information bits are represented using five coded bits. `4/8` adds more redundancy. A lower information fraction generally improves robustness at the cost of more airtime. Both ends must use compatible coding settings.

### Preamble

The preamble is a known symbol sequence at the beginning of a packet. It gives the receiver time to detect the transmission, establish timing, and synchronize the demodulator.

Longer preambles improve acquisition opportunities, especially for receivers that are not listening continuously, but add airtime to every packet. The supplied LoRa profiles use 8, 12, or 16 preamble symbols. Gaussian frequency-shift keying uses a separate fixed 32-bit preamble.

### Sync Word

The sync word is a physical-layer discriminator used after preamble detection. The LoRa configuration uses `0x12`. Nodes with incompatible sync words will not exchange these packets. The sketch does not expose a command to change the sync word.

In Gaussian frequency-shift keying mode, the sketch uses the two-byte sync sequence `0x2D 0xD4`.

### Transmit Power

Transmit power, abbreviated TX power, is configured in dBm, meaning decibels relative to one milliwatt. A 3 dB increase is approximately twice the conducted power; a 10 dB increase is ten times the conducted power.

More power can improve link margin, but does not solve interference, antenna, fading, desensitization, or regulatory problems. The `fast` profile intentionally uses 10 dBm while the other profiles use 14 dBm, so a profile comparison changes both bandwidth and power.

### Airtime

Airtime is the interval during which a complete packet occupies the radio channel. It includes the preamble, physical header when present, encoded payload, error-correction overhead, and packet checksum when enabled.

Airtime depends on all of the following:

- modulation mode;
- bandwidth;
- spreading factor;
- coding rate;
- preamble length;
- explicit or implicit header mode;
- packet checksum setting; and
- packet length.

The sketch asks RadioLib to calculate airtime after every successful local transmission. Cumulative airtime appears in `s` and `diag` output. It is calculated airtime for this board's transmissions, not measured spectrum occupancy and not the peer's airtime.

## Gaussian Frequency-Shift Keying Fundamentals

Gaussian frequency-shift keying, abbreviated GFSK, represents digital symbols by shifting the carrier frequency. A Gaussian filter smooths the symbol transitions before modulation, reducing abrupt spectral sidelobes compared with unfiltered frequency-shift keying.

The GFSK mode provides a useful contrast with LoRa:

| Property | LoRa | GFSK in this sketch |
| --- | --- | --- |
| Signal form | Chirps spanning the selected bandwidth | Frequency shifts shaped by a Gaussian filter |
| Primary design tradeoff | Sensitivity and range versus airtime | Bit rate and occupied spectrum versus sensitivity |
| Configured nominal rate | Depends on bandwidth, spreading factor, and coding rate | 50 kilobits per second |
| Preamble unit | LoRa symbols | Bits |
| Signal-to-noise report | Read from the LoRa packet status | Reported as `0.0` because the sketch does not obtain a GFSK value |
| Channel activity detection | Available | Not used by this sketch |
| Low-power receive duty cycling | Available | Not used by this sketch |

The fixed GFSK parameters are:

| Parameter | Value |
| --- | ---: |
| Bit rate | 50.0 kilobits per second |
| Frequency deviation | 25.0 kHz |
| Receiver bandwidth | 156.2 kHz |
| Preamble | 32 bits |
| Sync sequence | `0x2D 0xD4` |

The selected LoRa profile still supplies the center frequency and TX power in GFSK mode. Its LoRa bandwidth, spreading factor, coding rate, and LoRa preamble are ignored until LoRa mode is restored.

LoRa and GFSK are not mutually decodable. Both boards must use the same mode and compatible packet settings.

## Signal Measurements

### Received Signal Strength Indicator

Received Signal Strength Indicator, abbreviated RSSI, estimates received power in dBm. A more negative value is weaker. For example, `-110 dBm` is weaker than `-90 dBm`.

The exerciser reports:

- RSSI for the last decoded packet;
- per-peer last RSSI;
- average RSSI of benchmark replies; and
- instantaneous RSSI samples during diagnostics and surveys.

RSSI alone does not predict packet delivery. Bandwidth, noise, interference, spreading factor, coding, fading, and receiver implementation also matter.

### Signal-to-Noise Ratio

Signal-to-noise ratio, abbreviated SNR, is reported in dB, meaning decibels. Positive SNR means the estimated signal power exceeds the noise power in the measurement bandwidth. LoRa can decode at negative SNR because of its processing gain.

The exerciser records SNR for decoded LoRa packets and averages reply SNR during a benchmark. It reports `0.0 dB` for GFSK packets because that path does not collect a GFSK SNR measurement. That zero is a placeholder, not evidence of an exactly 0 dB link.

### Frequency Error

In LoRa mode, `diag` reports an estimated frequency error in hertz. It can reveal relative oscillator offset and other frequency displacement between the received signal and the local receiver. It is meaningful only after suitable packet reception and should not be treated as a calibrated frequency-counter measurement.

## Supplied Radio Profiles

The four profiles are fixed in the sketch. Profile numbers `1` through `4` and the names below are accepted by the `profile` command.

| Number | Name | Frequency | Bandwidth | Spreading factor | Coding rate | TX power | Preamble | Symbol duration | Approximate nominal physical-layer bit rate |
| ---: | --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 1 | `default` | 868.1 MHz | 125 kHz | SF7 | 4/5 | 14 dBm | 8 symbols | 1.024 ms | 5.47 kilobits per second |
| 2 | `fast` | 868.1 MHz | 250 kHz | SF7 | 4/5 | 10 dBm | 8 symbols | 0.512 ms | 10.94 kilobits per second |
| 3 | `robust` | 868.1 MHz | 125 kHz | SF10 | 4/5 | 14 dBm | 12 symbols | 8.192 ms | 0.98 kilobits per second |
| 4 | `maximum-range` | 868.1 MHz | 125 kHz | SF12 | 4/8 | 14 dBm | 16 symbols | 32.768 ms | 0.18 kilobits per second |

The nominal rates are modulation-level approximations before preamble, headers, packet checksum, this sketch's protocol header, acknowledgements, retransmissions, and mandatory spacing. Application throughput will be much lower.

The names describe intended tradeoffs, not guaranteed distance:

- `default` is the baseline for general testing.
- `fast` doubles bandwidth and lowers power by 4 dB. It reduces airtime substantially but also changes two link-budget variables.
- `robust` uses longer SF10 symbols and a longer preamble.
- `maximum-range` combines SF12, stronger 4/8 error correction, and the longest preamble. It has the longest airtime and lowest network capacity.

Actual range depends on antennas, polarization, height, terrain, Fresnel-zone clearance, obstructions, interference, receiver noise, cable loss, installation, and local rules. The exerciser deliberately measures outcomes instead of predicting a distance.

## Startup Defaults

When no valid stored configuration exists, reset, upload, or deep-sleep wake starts from these values. A valid stored configuration overrides the corresponding radio, packet, access, Wi-Fi, and Bluetooth values.

| Setting | Startup value |
| --- | --- |
| Radio mode | LoRa |
| LoRa profile | `default` |
| Frequency | 868.1 MHz |
| Packet cyclic redundancy check | On, two bytes |
| LoRa header | Explicit |
| In-phase/quadrature polarity | Normal |
| Low-data-rate optimization | Automatic |
| GFSK whitening | On |
| Receive gain | Power-saving |
| Low-power receive duty cycling | Off |
| Automatic channel activity detection | Off |
| Airtime duty limiter | Off; stored percentage is 1 percent |
| Slotted access | Off |
| Relay forwarding | Off |
| Peer selection | Automatic |
| Radio regulator selection | Low-dropout regulator mode |
| Wi-Fi station SSID | Unset |
| Wi-Fi station address | DHCP |
| Recovery access point | Fixed `Nesso-<node-id>`, password `nesso-lora`, `192.168.4.1/24` |
| Bluetooth Low Energy name | `Nesso-<node-id>`; unpaired and unencrypted |

Startup proceeds as follows:

1. The serial connection starts at 115,200 bits per second and waits for a host for up to two seconds.
2. The node identifier is derived from the ESP32-C6 electronic-fuse Media Access Control address.
3. The versioned configuration blob and separate visual inactivity timeout are read from Non-Volatile Storage; defaults remain active if either is absent or invalid.
4. Board input/output, radio power, antenna controls, and the shared bus are initialized.
5. The AW32001 charger is configured and enabled once using the vendor library defaults.
6. The display and touch controller are initialized, then the battery gauge and charger status are sampled.
7. The SX1262 starts with the restored LoRa/GFSK and packet configuration and enters receive mode.
8. The board attempts the configured Wi-Fi station for 12 seconds, then starts the fixed recovery access point if credentials are absent or association fails.
9. The HTTP server and Bluetooth Low Energy service start.
10. An immediate broadcast `HELLO` packet is sent if radio initialization succeeded.
11. The first periodic `HELLO` is scheduled 25 to almost 30 seconds later.
12. The first telemetry packet is scheduled 15 to almost 30 seconds later.

The timing offsets are deterministic values derived from the node identifier. They spread two boards' periodic traffic without requiring synchronized clocks. Later `HELLO` intervals are 25 to almost 30 seconds, and later telemetry intervals are 60 to almost 75 seconds.

## User Interfaces

### Serial Monitor

Use 115,200 bits per second and enable a newline or carriage return terminator. Commands are lowercase and case-sensitive. Enter `?` or `help` to print the compact onboard command list.

The input buffer holds approximately 1,232 characters so that a maximum-length transfer command can fit. Additional characters beyond that bound are ignored until the line ends.

### Buttons

Both user buttons are polled. The front button is edge-triggered with a 250-millisecond debounce interval. A short side-button press disables visual output, while a continuous three-second hold prevents accidental full shutdown.

| Button | Action |
| --- | --- |
| Front `KEY1` | Ping while Home is visible, apply an on-screen radio confirmation, or return to Home from another page |
| Side `KEY2` | Tap to disable visuals; hold continuously for three seconds to power off the board |

The front-button ping obeys the same transmit-access controls as a serial `p` command. The first button or touch contact after visuals are disabled restores them and is consumed without invoking its normal action. No-visuals mode only blanks the LCD/backlight and suppresses the transmit indicator; CPU, radio, Wi-Fi, Bluetooth, timers, commands, and exercises remain active. A completed side-button hold stops those subsystems and disables the radio path. The Nesso's `POWEROFF` signal goes to a programmed power controller, not a simple level-controlled latch: shutdown sends five low/high pulses with 50 ms per level, following [M5Stack's Nesso implementation](https://github.com/m5stack/M5Unified/blob/master/src/utility/Power_Class.inl). Holding that signal continuously high can reset rather than power off the board. If power remains, the CPU enters deep sleep with all wake sources disabled. The separate hardware power button switches the board on again.

### Touchscreen

The 240-by-135 landscape touchscreen is the primary disconnected interface. Raw portrait touch coordinates are swapped and inverted before landscape hit-testing. Every page header includes a tiny battery gauge and charge percentage. The cached native BQ27220 state-of-charge reading and AW32001 charger state are refreshed once per minute; each I2C operation has a 20-millisecond timeout. Cyan indicates charging, yellow or red indicates low charge, and green indicates normal charge. Gray `--%` means no valid gauge sample, and a gray percentage with `!` marks a last-good sample that is stale after a failed read or two minutes without a successful refresh. Serial status reports gauge and charger state separately; HTTP status exposes freshness, independent charge state, and `null` for stale numeric values. A persistent bottom navigation bar selects five pages:

| Page | Controls and information |
| --- | --- |
| Home | Current mode/profile and node identifier; `HELLO`, `PING`, preset `TEXT`, and `TELEM` actions; selected peer or latest received text |
| Radio | LoRa/GFSK mode plus the four profiles; every selection opens a confirmation and then uses the synchronized three-second radio-change protocol |
| Test | One-shot CAD, a 10-packet/64-byte benchmark, a five-packet/64-byte four-profile sweep, Stop, and the latest result |
| Access | Automatic CAD, 1% duty pacing, synchronized slots, LoRa low-power receive, boosted receive gain, and relay-forwarding toggles |
| Peers | Four discovered peers per page with RSSI, explicit selection, paging, and automatic-selection reset |

Touch input is accepted once per contact, so holding a control does not repeat it. Button hit boxes exactly match the drawn controls. Notices expire after three seconds, active tests refresh once per second, and idle pages refresh every 15 seconds. By default, visuals turn off after 60 seconds without physical touch or button activity.

Radio mode/profile changes require explicit Apply or Cancel confirmation. Apply sends the same synchronized command used by `mode sync` or `profile sync`; it does not silently change only the local board. The front button can also apply this confirmation. Deliberate changes are persisted on both peers; automated sweep transitions are explicitly transient.

The results view shows mode/profile, sent and received counts, delivery percentage, average round-trip time, RSSI, SNR, application goodput, and local transmit airtime. Full comma-separated reports remain on the serial output.

### Remote Command APIs

Both remote transports accept the same lowercase command strings listed in the Complete Command Map. Commands enter a shared four-entry fixed-size queue and are passed to `handleCommand()` on the main loop, exactly like a completed serial line. This preserves validation, exclusions, state machines, radio behavior, and serial logging in one command implementation.

#### Wi-Fi HTTP API

A board with configured station credentials joins that network. DHCP is the default, or `wifi address <a.a.a.a/8-30>` selects a static local address and Classless Inter-Domain Routing prefix. Static mode configures no gateway or Domain Name System server, so it is intended for clients on the same subnet.

If station credentials are absent or the initial 12-second association fails, the board starts a non-configurable recovery access point named `Nesso-<node-id>`, with password `nesso-lora` and address `http://192.168.4.1`. After a later station disconnect, the recovery access point appears after 30 seconds and is removed if the station reconnects.

| Method and path | Behavior |
| --- | --- |
| `POST /command` | Queue the raw `text/plain` request body as one command |
| `GET /command?command=<url-encoded-command>` | Convenience form for short commands |
| `GET /status` | Return current node, peer, radio, test, transport, queue, and last-command state as JSON |
| `GET /` | Return a small endpoint description |

Example from PowerShell using the board's active station or recovery address:

```powershell
Invoke-RestMethod -Method Post -Uri http://192.168.4.1/command -ContentType text/plain -Body 'profile sync robust'
```

Accepted commands receive HTTP status `202` and `{"status":"queued"}`. Empty commands receive `400`; commands longer than 1,232 bytes receive `413`; a full or unavailable queue receives `503`. HTTP acceptance means queued, not that the radio action ultimately succeeded.

#### Bluetooth Low Energy API

The Bluetooth Low Energy device name is also `Nesso-<node-id>`.

| Purpose | Generic Attribute Profile UUID | Property |
| --- | --- | --- |
| Service | `7bbf0001-6ba5-4e35-9f1f-8d36a7f34c01` | Service |
| Command | `7bbf0002-6ba5-4e35-9f1f-8d36a7f34c01` | Write and write without response |
| Ingress status | `7bbf0003-6ba5-4e35-9f1f-8d36a7f34c01` | Read |

An ordinary characteristic write is one command. The installed Bluetooth stack limits an attribute to 512 bytes. Longer commands, including a maximum-size transfer, use an application-level assembly sequence with chunks no larger than 506 bytes:

```text
@begin:<total-command-length>
@data:<first-command-chunk>
@data:<next-command-chunk>
@end
```

`@cancel` discards the partial command. The readable ingress-status characteristic reports `ready`, `chunk_ready`, `chunk:<received>/<expected>`, validation errors, `queued`, or `queue_full`.

#### Remote API Boundaries

The initial APIs intentionally duplicate command ingress, not the complete serial output stream. Detailed reports still print to serial; HTTP provides `/status`, BLE provides ingress status, and the touchscreen exposes common live state and benchmark results.

The recovery access-point password is fixed in source, and the Bluetooth Low Energy service is unpaired and unencrypted. Station link protection depends on the configured network. There is no user authorization, per-command permission model, replay protection, or application encryption. These interfaces are appropriate only for a controlled laboratory. Anyone who reaches HTTP or Bluetooth can invoke every command, including radio changes and deep sleep.

### Persistent Configuration

The sketch stores one versioned fixed-size configuration blob plus a separate visual-timeout value in the ESP32 Non-Volatile Storage partition. Keeping the timeout separate preserves compatibility with existing stored network and radio settings. It constructs a zero-initialized current blob and compares it byte-for-byte with the last loaded or written blob. `putBytes()` is called only when a successful setting change produces different bytes, limiting flash wear.

The following commands use the same serial, HTTP, and Bluetooth command path:

| Command | Behavior |
| --- | --- |
| `config` | Print storage, Wi-Fi, Bluetooth, and radio configuration with the password masked |
| `config defaults` | Restore and persist all defaults; reject while a long exercise is active |
| `config restart` | Restart and apply pending Wi-Fi or Bluetooth identity changes |
| `wifi ssid <name>` | Store a 1-to-32-byte station SSID |
| `wifi password <value>` | Store an 8-to-63-byte station password |
| `wifi password open` | Select an open station network |
| `wifi address dhcp` | Restore station DHCP |
| `wifi address <a.a.a.a/8-30>` | Store a valid unicast station address and CIDR prefix |
| `wifi clear` | Clear station credentials; the next boot uses the fixed recovery access point |
| `ble name <name>` | Store a 1-to-24-byte Bluetooth Low Energy device name |
| `visuals timeout off` | Disable automatic visual blanking |
| `visuals timeout <seconds>` | Persist an inactivity timeout from 1 through 86,400 seconds |

Wi-Fi credentials/address and the Bluetooth name take effect after restart. LoRa mode/profile, cyclic redundancy check, header, in-phase/quadrature, low-data-rate optimization, whitening, automatic channel detection, duty limiting, low-power receive, receive gain, relay state, and the visual inactivity timeout apply immediately and persist after successful application. The current visual-output state, slotted access, slot epoch, peers, counters, exercises, transfers, results, and relay history remain volatile.

Normal sketch uploads preserve Non-Volatile Storage because erase-all is disabled in the workspace board options. Enabling **Erase All Flash Before Sketch Upload**, changing to an incompatible partition layout, or issuing `config defaults` removes or replaces saved values. This build does not enable Non-Volatile Storage encryption, so the stored Wi-Fi password remains plaintext in flash.

## Complete Command Map

| Command | Purpose |
| --- | --- |
| `?` or `help` | Print the compact command list |
| `t <text>` | Send acknowledged text to the selected peer, or broadcast when no usable peer is selected |
| `p` | Send a ping request |
| `h` | Send a discovery `HELLO` immediately |
| `s` | Print local status and then transmit telemetry |
| `c` | Perform one LoRa channel activity detection scan |
| `mode lora` | Change only this board to LoRa |
| `mode gfsk` | Change only this board to GFSK |
| `mode sync lora` | Request a delayed LoRa change on the peer and this board |
| `mode sync gfsk` | Request a delayed GFSK change on the peer and this board |
| `profiles` | List the four LoRa profiles |
| `profile <name-or-number>` | Change only this board's active profile |
| `profile sync <name-or-number>` | Request a delayed profile change on the peer and this board |
| `options` | Print cyclic redundancy check, header, in-phase/quadrature, low-data-rate optimization, and whitening state |
| `crc on` or `crc off` | Enable or disable the physical packet checksum |
| `iq normal` or `iq inverted` | Select LoRa in-phase/quadrature polarity |
| `header explicit` | Use variable-length LoRa packets with an on-air header |
| `header implicit <bytes>` | Use a fixed LoRa packet length from 24 through 220 bytes |
| `ldro auto`, `ldro on`, or `ldro off` | Select automatic or forced low-data-rate optimization |
| `whitening on` or `whitening off` | Control GFSK data whitening |
| `rxgain power` or `rxgain boosted` | Select power-saving or boosted receive gain |
| `lowpower on` or `lowpower off` | Control LoRa receive duty cycling |
| `cad auto on` or `cad auto off` | Control automatic LoRa channel checks and randomized backoff before transmission |
| `duty off` | Disable airtime-based transmit spacing |
| `duty <percent>` | Enable airtime-based spacing at a value greater than 0 and no more than 100 |
| `slots on`, `slots off`, or `slots sync` | Control local or synchronized eight-slot access |
| `survey` | Survey five frequencies around the active center frequency |
| `survey <startMHz> <endMHz> <stepKHz> <samples>` | Run a custom receive survey |
| `survey stop` | Cancel the survey and restore the configured frequency and receive mode |
| `diag` | Print SX1262 and application diagnostics as comma-separated values |
| `diag clear` | Clear latched SX1262 device-error bits |
| `diag calibrate` | Run SX1262 image calibration for the active frequency |
| `visuals` | Print the visual-output state and inactivity timeout |
| `visuals on` or `visuals off` | Enable or disable visual output without suspending normal operation |
| `visuals timeout off` or `visuals timeout <seconds>` | Disable or set the persistent inactivity timeout |
| `sleep <seconds>` | Enter timer-controlled deep sleep and restart on wake |
| `peers` | Print the peer table as comma-separated values |
| `peer auto` | Clear the explicit selection and return to automatic peer selection |
| `peer <node-id>` | Lock normal unicast traffic to a discovered peer |
| `benchmark <packets> [bytes]` | Run a request/reply benchmark; payload defaults to 64 bytes |
| `benchmark report` | Print current or retained benchmark state |
| `benchmark stop` | Stop an active benchmark and print its partial result |
| `sweep <packets> [bytes]` | Benchmark all four LoRa profiles; payload defaults to 64 bytes |
| `sweep stop` | Stop the sweep and any benchmark it started |
| `transfer <text>` | Send 1 through 1,152 bytes using fragmentation and selective acknowledgement |
| `transfer status` | Print incoming and outgoing transfer state |
| `transfer resume` | Resume a paused outgoing transfer |
| `transfer cancel` | Discard the outgoing transfer |
| `relay on` or `relay off` | Enable or disable forwarding of received relay packets |
| `relay send <node-id-or-*> <1-8 hops> <text>` | Broadcast a bounded relay message toward a node or all nodes |
| `config` | Print persistent and active configuration with the password masked |
| `config defaults` | Restore and persist defaults |
| `config restart` | Restart to apply pending Wi-Fi/Bluetooth changes |
| `wifi ssid <name>` | Store station SSID |
| `wifi password <value-or-open>` | Store station password or select an open network |
| `wifi address dhcp` | Select station DHCP |
| `wifi address <a.a.a.a/8-30>` | Store a static station address and CIDR prefix |
| `wifi clear` | Clear station credentials and return to recovery AP on restart |
| `ble name <name>` | Store the Bluetooth Low Energy device name |

## Custom Packet Protocol

Every application packet is plain text with this frame:

```text
N1L|<type>|<source>|<destination>|<sequence>|<body>
```

The fields mean:

- `N1L` is the protocol marker.
- `type` is one character selecting the packet function.
- `source` is the sender's node identifier.
- `destination` is a node identifier or `*` for broadcast.
- `sequence` is a decimal 32-bit sequence number.
- `body` contains type-specific data.

Carriage returns and newlines in a body are replaced with spaces. Normal bodies are clipped to 150 bytes. The complete frame may not exceed 220 bytes. A body may contain additional `|` characters because the parser treats everything after the sequence delimiter as body data.

The packet types are:

| Type | Name | Body |
| --- | --- | --- |
| `H` | Discovery hello | `hello` |
| `T` | Text | User text |
| `A` | Text acknowledgement | `T:<acknowledged-sequence>` |
| `P` | Ping request | `ping` |
| `R` | Ping reply | `P:<ping-sequence>;pong` |
| `S` | Telemetry | `up=<seconds>;vbat=<volts>;charge=<percent>;heap=<bytes>` |
| `B` | Benchmark request | Session, sample index, local send time, and generated padding |
| `b` | Benchmark reply | Exact echo of the benchmark request body |
| `M` | Delayed radio change | Mode code, zero-based profile index, delay in milliseconds, and persistence flag |
| `F` | Transfer fragment | Transfer identifier, fragment index, count, 16-bit checksum, and data |
| `K` | Transfer acknowledgement | Transfer identifier and received-fragment bitmap |
| `L` | Relayed text | Message identifier, original source, final destination, remaining hops, and text |
| `Q` | Slot synchronization | Delay until the shared slot epoch in milliseconds |

Malformed frames, frames sent by this node, and unicast frames for another node are ignored. Unknown packet types are reported but not processed.

Sequence numbers begin at 1 after every restart. Each peer record remembers only the most recently accepted sequence number. An immediately repeated sequence is recognized as a duplicate; this is useful for text retransmission. It is not a replay-protection window, and an older out-of-order packet may not be recognized as a duplicate.

## Node Identity, Discovery, and Peer Selection

### Node Identifier

The sketch reads the ESP32-C6 electronic-fuse Media Access Control address, folds it into 32 bits, and displays the result as eight uppercase hexadecimal characters. An example is `3A91C2F0`.

The identifier is stable for a board but is not guaranteed globally unique after folding, is not secret, and is not authenticated.

### Discovery

Each board sends a broadcast `H` packet:

- once during startup when the radio is ready;
- when `h` is entered; and
- periodically every 25 to almost 30 seconds when no long-running exercise suppresses periodic traffic.

Any valid packet, not only `H`, updates the sender's peer record. The `H` type produces an explicit `discovered peer` message.

### Peer Table

The table holds eight peers. When it is full, a newly observed peer replaces the least recently seen entry. Each entry stores:

- node identifier;
- time last heard;
- last RSSI;
- last SNR;
- total valid packets associated with the peer;
- immediately repeated sequence count; and
- whether it is selected.

`peers` prints:

```text
peer,last_seen_ms,rssi_dbm,snr_db,packets,duplicates,selected
```

Despite the heading, `last_seen_ms` is the age of the last observation in milliseconds, not an absolute timestamp.

### Automatic and Locked Selection

In automatic mode, the most recently heard valid peer becomes the selected peer. Automatic selection is recency-based, not strongest-signal selection.

If the selected automatic peer has not been heard for three minutes, normal outgoing traffic falls back to broadcast. `peer auto` also clears the current selection, so traffic broadcasts until another peer is heard.

`peer <node-id>` locks selection to a peer already present in the table. A locked peer does not automatically expire from routing, even when it has not been heard recently. Use `peer auto` to release the lock.

## Text Messaging

Enter:

```text
t Hello from node A
```

The sender:

1. selects the locked or recent peer, or `*` for broadcast;
2. allocates one sequence number;
3. transmits a `T` packet;
4. waits 3.5 seconds for an `A` packet containing that sequence; and
5. retries until a maximum of three transmission attempts has been reached.

Only one acknowledged text may be pending at a time. Text is rejected while a benchmark, profile sweep, outgoing transfer, or delayed radio change is active.

The receiver displays and prints a new text packet, including RSSI and SNR, and sends an acknowledgement. If the same sequence is received again immediately, the receiver does not display the text twice but does acknowledge it again. This lets the sender recover when the text arrived but its first acknowledgement was lost.

For broadcast text, the first matching acknowledgement from any receiver completes the sender's pending operation. This is not an acknowledgement from every listener.

The initial text transmission and its retries are not placed in the deferred reply queue. If a channel-access rule blocks an attempt, that attempt can fail and the normal acknowledgement timeout drives the next retry.

## Ping and Pong

`p` sends a `P` packet to the selected peer or to broadcast. A receiver prints the source and returns an `R` packet with the original ping sequence in its body.

The originating board prints the reply body. Ping does not maintain a pending state, calculate round-trip time, retry, or match replies beyond displaying them. Use `benchmark` when quantified round-trip measurements are required.

## Status and Telemetry

### Local Status

The `s` command first prints:

- node and selected peer;
- mode, profile, frequency, modulation parameters, and TX power;
- radio-ready state;
- battery voltage and estimated charge;
- cumulative transmitted packets, received packets, and transmit failures;
- calculated cumulative TX airtime;
- automatic channel detection, duty limiting, and slot state; and
- low-power receive and receive-gain state.

It then sends telemetry. `s` is therefore not a read-only command.

### Telemetry

Telemetry is a `S` packet containing:

```text
up=<uptime-seconds>;vbat=<voltage>;charge=<percent>;heap=<free-bytes>
```

`heap` is currently free dynamic random-access memory on the ESP32-C6. The telemetry receiver prints the body with the packet's RSSI and SNR.

Telemetry is sent:

- by `s`;
- by the Home page `TELEM` control; and
- periodically, first after 15 to almost 30 seconds and then every 60 to almost 75 seconds.

Periodic discovery and telemetry pause during benchmarks, sweeps, delayed radio changes, and outgoing transfers. Manually requested traffic is still subject to the shared access controls.

## Changing Mode and Profile

### Local Changes

`mode lora`, `mode gfsk`, and `profile <value>` immediately reinitialize only the local SX1262. A peer left on the old settings will no longer decode the node.

In LoRa mode, a profile selects all fields in the profile table. In GFSK mode, a profile change selects only frequency and TX power, because the GFSK bit rate, deviation, receive bandwidth, preamble, and sync sequence are fixed.

### Synchronized Changes

`mode sync ...` and `profile sync ...` send an `M` packet using the current radio settings. The packet tells the addressed peer or broadcast listeners to apply a new mode and profile after three seconds. The sender schedules the same change locally.

This mechanism is delayed coordination, not shared-clock synchronization:

- the receiver schedules relative to when it processed the packet;
- no acknowledgement confirms that every intended peer received the request;
- a peer already using incompatible settings cannot hear the request;
- clock drift is not corrected; and
- a missed request leaves the sender and peer on different settings.

Received radio-change packets are accepted only when their requested delay is from 1 through 30 seconds. The interactive synchronized commands use three seconds.

Reinitializing the modem reapplies the packet checksum, sync word, whitening, in-phase/quadrature, header, low-data-rate optimization, and gain settings because switching modem type resets the SX1262 packet engine.

## Packet Options

Except for synchronized mode and profile changes, packet options are local commands. Apply compatible values manually on both boards before expecting communication.

### Packet Cyclic Redundancy Check

A cyclic redundancy check, abbreviated CRC, detects many accidental bit errors. `crc on` configures the SX1262 to append and verify a two-byte packet CRC. Corrupted packets are rejected by the radio and reported as `CRC mismatch`. `crc off` disables that protection.

This physical packet CRC is separate from the 16-bit whole-message checksum used by fragmented transfers. Turning the packet CRC off is useful for controlled error experiments, but is a poor normal operating choice.

The setting applies to both LoRa and GFSK.

### Explicit and Implicit LoRa Headers

`header explicit` uses an on-air LoRa header that lets the receiver learn packet parameters including payload length. This is the startup default and supports variable frame lengths.

`header implicit <bytes>` removes the explicit header and configures a fixed expected length from 24 through 220 bytes. The sender pads a shorter custom frame with zero bytes to the exact length and uses RadioLib's byte-buffer transmit API. A frame longer than the configured implicit length is rejected. The received text ends at the first zero byte, preserving legitimate trailing spaces in fragment data. Both boards must run this framing version; older space-padded implicit packets are not supported.

Both boards must configure the same fixed length. Implicit mode can reduce overhead, but it is inconvenient for variable-length text and easy to misconfigure.

This setting applies only to LoRa. It remains stored while GFSK is active and is reapplied when LoRa returns.

### In-Phase/Quadrature Polarity

In-phase and quadrature, abbreviated I/Q, are the two orthogonal components used to represent a radio signal in a complex baseband receiver. LoRa I/Q inversion reverses the chirp polarity expected by the modem.

Use `iq normal` or `iq inverted`. Both communicating boards must use a compatible polarity. Inversion can deliberately separate traffic classes in some LoRa systems, but here it is primarily an interoperability experiment.

This setting applies only to LoRa.

### Low-Data-Rate Optimization

Low-data-rate optimization, abbreviated LDRO, changes LoRa symbol processing to tolerate oscillator drift during very long symbols. It is most relevant to high spreading factors and narrow bandwidths.

- `ldro auto` lets RadioLib choose according to symbol duration and is the recommended general setting.
- `ldro on` forces the optimization.
- `ldro off` prevents it.

Forcing the wrong value can make long-symbol links unreliable or produce incompatible modulation. This setting applies only to LoRa.

### GFSK Whitening

Whitening combines payload bits with a known pseudo-random sequence before transmission and reverses the operation at the receiver. It prevents long repetitive bit patterns, improves transition density, and reduces pattern-dependent spectral behavior. It is not encryption.

Use `whitening on` or `whitening off`. Both GFSK endpoints must agree. The value is stored in LoRa mode but has an effect only when GFSK is initialized.

### Inspecting Options

`options` prints the currently stored values for CRC, header mode, I/Q polarity, LDRO, and GFSK whitening. It does not print access controls or receive gain; use `s` for those.

## Shared Transmit-Access Path

Every normal transmit path passes through the same access function. The checks occur in this order:

1. the fixed 1.2-second minimum interval;
2. optional airtime duty limiting;
3. optional slotted access; and
4. optional LoRa channel activity detection and randomized backoff.

A blocked attempt increments the `access_deferrals` diagnostic counter. This common path covers interactive traffic, acknowledgements, benchmark packets, transfer fragments, and relay forwarding.

Not every caller handles a deferral in the same way. Protocol replies use a small deferred queue; state machines try again later; one-shot commands such as a manually requested `HELLO`, ping, or telemetry may simply fail for that invocation.

### Fixed Minimum Interval

After a transmission attempt reaches the radio, another packet cannot start for 1.2 seconds. This limits demonstration traffic and gives the half-duplex peer opportunities to respond.

The benchmark and transfer state machines voluntarily wait at least 1.3 seconds between their own request or fragment attempts. The common 1.2-second check still applies.

### Manual Channel Activity Detection

`c` performs one Channel Activity Detection scan, abbreviated CAD. The radio enters standby, looks for a compatible LoRa preamble, reports `channel free` or `LoRa preamble detected`, and returns to receive mode.

CAD is not a general-purpose energy detector. It can miss:

- GFSK;
- Wi-Fi, conventional frequency modulation, or unrelated interference;
- LoRa using incompatible physical settings;
- a packet that starts after the scan; and
- another node that performs CAD at the same instant and then transmits simultaneously.

It requires a ready radio in LoRa mode.

### Automatic CAD and Randomized Backoff

`cad auto on` runs CAD before each LoRa transmission. When a preamble is detected, the sketch waits a random number of 50-millisecond slots and tries again, for at most five scans.

The maximum random windows for the five busy observations are 1, 3, 7, 15, and 31 slots respectively. A random wait may also be zero. If all five scans remain busy, the transmission is deferred and the receiver is restarted.

This resembles carrier-sense multiple access with randomized backoff, but it is not full collision avoidance. There is no reservation, acknowledgement at the access layer, or collision detection during transmission. The `cad_busy` diagnostic counts busy CAD observations.

Automatic CAD is bypassed in GFSK mode even when its stored switch is on.

### Airtime-Based Duty Limiting

`duty <percent>` enables spacing based on the calculated airtime of each successful local transmission. The next permitted time is approximately:

$$next\ interval = packet\ airtime \times \frac{100}{requested\ percent}$$

For example, a calculated 20-millisecond packet at 1 percent schedules the next transmission approximately 2,000 milliseconds after that transmission. The fixed 1.2-second interval also remains in force, so the stricter delay wins.

Accepted percentages are greater than 0 and no more than 100. `duty off` disables the limiter without changing the stored percentage.

This is a per-packet pacing mechanism, not a rolling regulatory accounting system. It does not track frequency sub-bands, other transmitters, antenna gain, dwell limits, or airtime used by another board.

### Slotted Access

Slotted access divides a repeating two-second cycle into eight 250-millisecond slots. A board's slot is:

```text
folded numeric node identifier modulo 8
```

When enabled, the board transmits only during that slot.

`slots on` starts a local epoch immediately. Independently entering it on several boards does not align their epochs.

`slots sync` temporarily disables local slots, broadcasts a `Q` packet, and schedules a common epoch three seconds later on the sender and listeners. This reduces initial offset, but it does not correct later clock drift. Nodes can still collide when:

- more than one identifier maps to the same one of eight slots;
- a synchronization packet is missed;
- clocks drift;
- packet airtime extends beyond 250 milliseconds; or
- hidden nodes cannot hear one another.

Long LoRa packets, especially at SF12, can be longer than a slot. The feature controls when transmission starts; it does not stop a packet at the slot boundary.

### Deferred Protocol Replies

Acknowledgements, pong replies, benchmark echoes, transfer bitmaps, and forwarded relay packets first try the normal transmit path. If transmission cannot proceed, a bounded four-entry queue preserves them.

Queued entries are retried no sooner than the 1.2-second interval. An entry is dropped after its retry counter exceeds ten. The bounded queue prevents an unreachable or congested peer from consuming memory indefinitely. If all four entries are occupied, another reply is dropped with a queue-full message.

## Receive and Power Features

### Power-Saving and Boosted Receive Gain

`rxgain power` selects the SX1262 power-saving receive-gain setting. `rxgain boosted` selects the boosted-gain setting, which can improve sensitivity at the cost of additional receive current.

Boosted gain cannot fix an overloaded receiver, poor antenna, or strong in-band interference, and may not improve every environment. Compare delivery, RSSI, and battery behavior rather than assuming it is always preferable.

This command changes receive gain directly without reinitializing the full modem.

### LoRa Low-Power Receive Duty Cycling

`lowpower on` asks RadioLib to configure the SX1262's automatic receive duty-cycle mode using the active LoRa preamble length. The receiver alternates sleep and listening intervals instead of remaining continuously active. This can reduce receive energy, but increases the risk of missing traffic when timing or preamble assumptions are unsuitable.

The sketch uses this feature only in LoRa mode. In GFSK mode, setting the switch to `on` is retained and shown by status, but reception remains continuous. Returning to LoRa makes the stored switch effective again.

Use a longer preamble and controlled traffic when exploring receive duty cycling. Measure missed packets as well as current consumption.

### Timed Deep Sleep

`sleep <seconds>` accepts a positive duration. It:

1. prints and flushes the serial message;
2. stops the HTTP server, Wi-Fi access point, and Bluetooth Low Energy stack;
3. puts the SX1262 to sleep;
4. clears the display;
5. configures the ESP32-C6 timer wakeup; and
6. enters deep sleep.

Wakeup restarts the sketch from startup. Volatile state is lost, including peer records, counters, profile changes, pending text, transfers, relay history, and access-control settings. The node sends a new startup `HELLO` when the radio is ready.

## Benchmarks

### Request/Reply Operation

Use:

```text
benchmark <1-1000 packets> [24-120 bytes]
```

The optional payload size defaults to 64 bytes. The initiator creates a random session identifier and sends `B` requests. Each request body contains the session, sample index, the initiator's local send time, and generated letter padding to the requested body length. A receiver returns the body unchanged in a lowercase `b` reply.

Because the initiator's timestamp is echoed, round-trip time, abbreviated RTT, is measured with one local clock. The boards do not need synchronized clocks.

If the destination begins as broadcast, the first accepted reply binds the benchmark to that responder. Later responses from other nodes are ignored. Requests sent before binding may nevertheless cause several listeners to reply, so quiet two-board tests are easiest to interpret.

Successful requests are spaced by at least 1.3 seconds and remain subject to duty, slot, and CAD controls. A failed transmit increments the attempt and failure counters, but the state machine keeps trying until the requested number of packets has actually been transmitted. The five-second reply timeout begins only after all requested transmissions have succeeded. Use `benchmark stop` if access controls or radio failure prevent forward progress.

### Benchmark Output

The final comma-separated values header is:

```text
benchmark_reason,mode,profile,session,requested,attempts,send_failures,sent,replies,delivery_percent,min_rtt_ms,avg_rtt_ms,max_rtt_ms,avg_rssi_dbm,avg_snr_db,throughput_bps,airtime_ms
```

The fields mean:

| Field | Meaning |
| --- | --- |
| `benchmark_reason` | `complete`, `timeout`, `stopped`, `running`, or `idle` |
| `mode` | LoRa or GFSK |
| `profile` | Active profile name; in GFSK this identifies the frequency and power source |
| `session` | Random benchmark identifier |
| `requested` | Requested successful request transmissions |
| `attempts` | Calls that attempted to transmit a request |
| `send_failures` | Request attempts rejected or failed locally |
| `sent` | Successfully transmitted requests |
| `replies` | Unique accepted replies |
| `delivery_percent` | Replies divided by requested packets, multiplied by 100 |
| `min_rtt_ms`, `avg_rtt_ms`, `max_rtt_ms` | Minimum, average, and maximum application RTT in milliseconds |
| `avg_rssi_dbm` | Mean RSSI of accepted replies |
| `avg_snr_db` | Mean SNR of accepted replies; GFSK contributes placeholder zeros |
| `throughput_bps` | Replied payload bits divided by total benchmark elapsed time |
| `airtime_ms` | Increase in this board's calculated TX airtime during the benchmark |

`throughput_bps` is application goodput under this test's request spacing, replies, losses, and waiting time. It is not the raw modem bit rate. RTT includes both radio directions, peer processing, access delays, and any deferred reply delay.

The airtime field uses a global local-transmission counter. Other transmissions made by this board during a benchmark can contaminate it. Avoid manual commands and unrelated traffic during controlled measurements.

`benchmark report` prints a report without stopping the test. `benchmark stop` prints a partial report and stops it.

## Automated Profile Sweep

Use:

```text
sweep <1-1000 packets> [24-120 bytes]
```

The payload defaults to 64 bytes. The initiator:

1. requests synchronized LoRa `default` mode and waits 4.5 seconds;
2. runs a benchmark;
3. requests `fast`, waits, and benchmarks;
4. repeats for `robust` and `maximum-range`; and
5. requests a return to LoRa `default`.

Only the initiating board needs the `sweep` command. The responder follows the `M` radio-change packets and automatically echoes benchmark requests.

The sweep always uses LoRa and always ends by requesting the default profile. It does not restore a previous GFSK mode or a non-default profile. A missed mode-change packet can strand a peer on different settings. Each benchmark produces one comma-separated report.

`sweep stop` stops the sweep and its benchmark. It does not guarantee restoration of the default profile; issue an appropriate synchronized mode/profile command afterward.

Avoid starting a sweep while an outgoing transfer is already active. The transfer command refuses to start during a sweep, but the inverse command order is not fully guarded and a profile change can disrupt the transfer.

## Frequency Survey

### Default Survey

`survey` scans five frequencies centered on the active profile:

```text
center - 0.2 MHz
center - 0.1 MHz
center
center + 0.1 MHz
center + 0.2 MHz
```

It takes three samples at each frequency.

### Custom Survey

Use:

```text
survey <startMHz> <endMHz> <stepKHz> <samples>
```

Validation requires:

- LoRa mode and a ready radio;
- start frequency at least 150 MHz;
- end frequency no more than 960 MHz;
- end frequency not below start frequency;
- a positive step;
- at least one sample; and
- no more than 200 calculated frequency points.

The sample field is stored in an unsigned eight-bit value, so use 1 through 255.

For every frequency and sample, the radio listens for 10 milliseconds, reads instantaneous RSSI, enters standby, and runs LoRa CAD. Output is:

```text
frequency_mhz,average_rssi_dbm,lora_detections,samples
```

`average_rssi_dbm` is a short receive-power sample, not a calibrated spectrum-analyzer trace. `lora_detections` counts compatible LoRa preamble detections, not all channel users. Step size does not change the receiver bandwidth, so adjacent samples can overlap heavily.

The survey advances incrementally through frequency tuning, RSSI sampling, and deadline-bounded CAD. Touch, buttons, serial, and remote commands remain serviced; normal radio packet delivery pauses because the transceiver is scanning. Use `survey stop` or Stop on the Test page to cancel. Completion, cancellation, and timeout restore the active profile frequency and configured receive mode. A survey range accepted by the hardware is not necessarily a legal transmit range.

## Radio and Link Diagnostics

`diag` emits:

```text
metric,value
mode,<LoRa-or-GFSK>
chip_status,<hexadecimal-status>
packet_status,<hexadecimal-status>
device_errors,<hexadecimal-error-bitmap>
irq_flags,<hexadecimal-interrupt-bitmap>
instant_rssi_dbm,<current-receive-power>
last_packet_rssi_dbm,<last-decoded-packet-power>
last_packet_snr_db,<last-decoded-LoRa-SNR-or-GFSK-placeholder>
frequency_error_hz,<LoRa-only-estimate>
tx_packets,<successful-local-transmissions>
rx_packets,<successfully-read-radio-packets>
tx_airtime_us,<calculated-local-airtime-in-microseconds>
access_deferrals,<blocked-access-attempts>
cad_busy,<busy-automatic-CAD-observations>
free_heap,<available-dynamic-memory-bytes>
```

`IRQ` means interrupt request. The raw SX1262 status, packet status, error, and interrupt values are bitmaps intended to be decoded with the SX1262 documentation. They should not be interpreted as application packet counts.

`diag clear` clears the SX1262's latched device-error bits. It does not reset peer statistics, packet counts, airtime, access deferrals, or CAD counts. Rebooting resets the application counters.

`diag calibrate` runs SX1262 image calibration at the active profile frequency. Image calibration adjusts the radio's internal receive path for its frequency band. It is not antenna tuning, peer synchronization, or a correction of the reported frequency error.

## Fragmented Transfers

Normal packet bodies are limited to 150 bytes. `transfer <text>` adds a small transfer protocol for 1 through 1,152 bytes.

### Fragmentation

The sender:

- creates a random 32-bit transfer identifier;
- calculates a 16-bit cyclic redundancy check over the complete text using polynomial `0x1021` and initial value `0xFFFF`;
- divides the data into at most 16 fragments of 72 bytes; and
- sends `F` packets containing identifier, index, total count, whole-message checksum, and fragment data.

This transfer checksum is independent of the SX1262 packet CRC. The packet CRC protects each radio frame when enabled; the transfer checksum validates the final reassembled text.

### Selective Acknowledgement

After each incomplete valid fragment, the receiver returns a `K` packet with a 16-bit bitmap. Bit `N` is set when fragment `N` has arrived. A complete bitmap is sent only after whole-message CRC validation. The sender adopts the newest cumulative snapshot, rejects older or duplicate acknowledgement sequence numbers, and sends only fragments whose bits remain clear. A zero bitmap after a CRC failure resets the sender's progress instead of being merged with older acknowledgements.

When a complete round has been sent, the sender waits 1.8 seconds for acknowledgement progress. It performs at most five rounds. If fragments are still missing, the transfer is paused rather than discarded.

`transfer status` shows:

- transfer identifier;
- fragment count;
- acknowledged bitmap in hexadecimal;
- current round; and
- active, paused, or idle state.

`transfer resume` preserves the data and acknowledged bitmap, resets the round counter, and retries missing fragments. `transfer cancel` discards only the outgoing transfer.

### Receiver Behavior

The receiver holds one incomplete transfer at a time. A conflicting transfer from another source is rejected while the current one remains incomplete and has not expired. An incomplete transfer expires 30 seconds after its last accepted fragment.

When every bit is present, fragments are concatenated in index order and the whole-message checksum is verified. On mismatch, the receive bitmap is cleared and a zero-bitmap acknowledgement requests retransmission. No complete acknowledgement is sent for a bad checksum. On success, the complete bitmap is acknowledged, the full text is printed, and a clipped form is shown on the display. Fragment boundaries preserve spaces and other text whitespace.

If the initial destination was broadcast, the first valid transfer acknowledgement binds the outgoing transfer to that responder. The operation is therefore one-to-one after binding, not reliable multicast.

The protocol has bounded retries, so it is recoverable rather than guaranteed. It supports one outgoing transfer and one incoming transfer, not concurrent streams or persistent storage.

## Bounded Broadcast Relaying

Relaying demonstrates multi-hop flooding. It is easiest to observe with at least three boards arranged so the source and destination cannot communicate directly but both can reach an intermediate board.

### Sending

Use:

```text
relay send <node-id-or-*> <1-8 hops> <text>
```

The source creates a random message identifier and broadcasts an `L` packet containing:

- the message identifier;
- original source;
- final node identifier or `*`;
- remaining hop count; and
- text clipped to 90 bytes.

Sending does not require `relay on` at the source. That switch controls forwarding of received messages.

### Receiving and Forwarding

A node records each original-source and message-identifier pair. It displays the text when the final destination is its own identifier or `*`.

The node forwards when all of these conditions are true:

- `relay on` is active;
- more than one hop remains;
- the final destination is not this node; and
- the message has not already been seen recently.

Forwarding decrements the hop count and broadcasts again. A hop count of `1` permits direct reception but no forwarding.

### Loop and Flood Bounds

Each node has a circular history of 16 relay records. A record is considered recent for two minutes. The hop count and history reduce loops, but do not create routes, reserve capacity, or guarantee delivery.

After a record expires or is overwritten, a delayed duplicate could be forwarded again. Dense topologies can still produce several copies before duplicate suppression takes effect. Relay packets are plaintext, unacknowledged, and unauthenticated.

`relay off` stops forwarding but does not stop a node from receiving and displaying a relay addressed to it or to broadcast.

## Feature Interaction and Concurrency

The cooperative main loop services radio reception, serial input, queued HTTP/Bluetooth commands, touch, both user buttons, incremental surveys, deferred replies, text retries, benchmarks, radio changes, sweeps, transfers, and UI refresh. Its idle delay is two milliseconds; touch and button sampling are capped at one poll per 25 milliseconds.

Transmit and CAD waits use RadioLib's nonblocking start APIs with explicit completion deadlines. During those waits, the sketch services inputs, display updates, HTTP ingress, and serial ingress; commands are queued until the current radio operation returns. The radio-operation guard prevents those callbacks from re-entering the radio driver. CAD backoff also services the UI instead of sleeping through the whole interval. Long surveys execute one sample at a time and support cancellation.

One shared eligibility check protects manual transmissions, benchmarks, sweeps, transfers, surveys, and radio-setting changes across touchscreen, serial, HTTP, BLE, and received synchronized control packets. These operations reject competing benchmarks, sweeps, outgoing or incomplete incoming transfers, pending text acknowledgements, pending radio changes, surveys, and in-progress radio calls. The sweep alone may initiate its own profile changes and benchmarks. Protocol replies and the active exercise's traffic still use the radio's shared access controls. Stop/cancel and status commands remain available; explicit restart, sleep, and physical shutdown remain intentional ways to interrupt the application.

Only an SX1262 `RX_DONE` interrupt permits reading a received packet. TX and CAD completion share the same physical interrupt pin but are not counted or parsed as receptions. For reproducible results:

1. finish or stop the current long-running exercise;
2. apply compatible settings to all participating boards;
3. avoid unrelated commands during measurements; and
4. use `s`, `options`, `peers`, `transfer status`, and `diag` to confirm state.

## Limits and Timing Reference

| Item | Limit or timing |
| --- | --- |
| Normal body | 150 bytes |
| Complete custom frame | 220 bytes |
| Serial command buffer | Approximately 1,232 characters |
| Fixed transmit interval | 1.2 seconds |
| Text acknowledgement timeout | 3.5 seconds |
| Text attempts | 3 total |
| Periodic hello | 25 to almost 30 seconds |
| First periodic telemetry | 15 to almost 30 seconds |
| Later periodic telemetry | 60 to almost 75 seconds |
| Peer table | 8 peers |
| Automatic peer stale interval | 3 minutes |
| Deferred reply queue | 4 packets |
| Automatic CAD scans | 5 per attempted transmission |
| CAD backoff slot | 50 milliseconds |
| Duty setting | Greater than 0 through 100 percent |
| Slots | 8 slots of 250 milliseconds, 2-second cycle |
| Synchronized mode/profile delay | 3 seconds |
| Accepted received synchronization delay | 1 through 30 seconds |
| Benchmark count | 1 through 1,000 packets |
| Benchmark body | 24 through 120 bytes, default 64 |
| Benchmark final reply wait | 5 seconds after all requests are sent |
| Survey hardware range check | 150 through 960 MHz |
| Survey frequency points | At most 200 |
| Transfer size | 1 through 1,152 bytes |
| Transfer fragments | At most 16 |
| Fragment data | 72 bytes |
| Fragment acknowledgement wait | 1.8 seconds after a round |
| Transfer rounds | 5 before pause |
| Incomplete incoming transfer timeout | 30 seconds |
| Relay text | 90 bytes before protocol framing |
| Relay hops | 1 through 8 |
| Relay history | 16 records retained for 2 minutes |
| Button debounce | 250 milliseconds |
| Implicit LoRa packet length | 24 through 220 bytes |
| Remote command length | 1 through 1,232 bytes |
| Remote command queue | 4 complete commands |
| Bluetooth direct write | At most 512 bytes |
| Bluetooth assembled data chunk | At most 506 bytes after the `@data:` prefix |

## Suggested Exercises

These exercises build from basic operation to the more specialized features. Record both boards' settings before every comparison.

### 1. Discovery and Link Observation

1. Flash the same sketch to two boards.
2. Open both serial monitors at 115,200 bits per second with newline enabled.
3. Reset both boards and note their node identifiers.
4. Enter `peers` after each has heard the other.
5. Enter `h` and observe the discovery message and peer age change.
6. Compare serial RSSI/SNR with the display.

This exercises identity, startup `HELLO`, periodic discovery, the peer table, automatic selection, and display updates.

### 2. Acknowledged Text and Ping

1. Enter `t first message` on one board.
2. Observe `text acknowledged by` on the sender and RSSI/SNR on the receiver.
3. Enter `p` and observe the request and reply.
4. Move or shield one board until packets become intermittent, then send another text.
5. Observe up to three text attempts and duplicate suppression if an acknowledgement is lost.

Ping is intentionally lightweight; use the next exercise for RTT statistics.

### 3. Baseline Benchmark

1. Confirm both boards show LoRa `default` with `s`.
2. Enter `benchmark 10 64` on one board.
3. Save the comma-separated result.
4. Repeat at different separations and orientations.
5. Compare delivery, RTT, RSSI, SNR, goodput, and calculated airtime.

Do not interpret a single RSSI value as a range guarantee. Repeat enough samples to expose fading and interference.

### 4. Profile Comparison and Sweep

1. Enter `profile sync robust` and wait for both boards to change.
2. Run `benchmark 10 64`.
3. Enter `profile sync maximum-range` and repeat.
4. Enter `sweep 10 64` to automate all four profiles.
5. Confirm both boards return to LoRa `default` afterward.

Compare the measured airtime and RTT increase against symbol duration. Remember that `fast` also reduces TX power.

### 5. LoRa Versus GFSK

1. Establish a good default LoRa link and run a benchmark.
2. Enter `mode sync gfsk`.
3. Confirm GFSK status on both boards and repeat the benchmark.
4. Note the GFSK SNR placeholder and compare RTT, goodput, and delivery.
5. Enter `mode sync lora` to return.

Do not change only one board unless intentionally demonstrating incompatibility.

### 6. Packet Compatibility Experiments

Test one setting at a time and restore it before moving on:

1. Run `options` on both boards.
2. Apply `iq inverted` manually on both and verify messaging, then create a deliberate mismatch on one board.
3. Apply the same `header implicit 120` on both, send short text, and then test a frame that exceeds the fixed length.
4. Restore `header explicit`.
5. Compare `ldro auto`, `ldro on`, and `ldro off` at high spreading factor.
6. In GFSK mode, compare matching and mismatched whitening settings.
7. Use `crc off` only in a controlled test, then restore `crc on`.

These settings are local. A mismatch may make the synchronization command itself impossible to receive, so restore both through their serial monitors.

### 7. Channel Access

1. Enter `c` while the other board is idle and while it is transmitting.
2. Enable `cad auto on` on a sending board.
3. Use three or more boards to create contention and inspect `cad_busy` and `access_deferrals` with `diag`.
4. Try `duty 10`, run a short benchmark, and compare elapsed time with `duty off`.
5. Enter `slots sync` and inspect slot behavior across several node identifiers.
6. Compare with independently entered `slots on` to demonstrate unsynchronized epochs.

Long packets can cross slot boundaries. CAD can miss hidden nodes and non-LoRa interference.

### 8. Channel Survey

1. Run `survey` and save the five-row output.
2. Run a controlled transmitter on one surveyed frequency and repeat.
3. Try a finer custom scan such as `survey 867.9 868.3 50 5`, where legal to receive.
4. Compare average RSSI with LoRa detection counts.

The result is a short receiver survey, not a spectrum analyzer or regulatory channel assessment.

### 9. Receive Gain and Low-Power Reception

1. At a marginal link, compare `rxgain power` and `rxgain boosted` using repeated benchmarks.
2. Restore a stable profile and enable `lowpower on` on the receiver.
3. Send repeated text or benchmark traffic and record missed packets.
4. Compare preamble profiles and power consumption with suitable external measurement equipment.
5. Restore `lowpower off`.

The serial and display subsystems remain active, so whole-board current is not a pure SX1262 receive-current measurement.

### 10. Fragmented Transfer

1. Enter a transfer longer than 72 bytes.
2. Use `transfer status` while it progresses.
3. Introduce controlled packet loss and observe cumulative acknowledgement bitmaps.
4. If it pauses after five rounds, remove the impairment and enter `transfer resume`.
5. Try `transfer cancel` on a separate transfer.

Observe that only missing fragments are retried and the receiver validates the complete-message checksum.

### 11. Relay Flooding

Use at least three boards.

1. Record all node identifiers with `peers`.
2. Enable `relay on` only on intended intermediate boards.
3. From the source, enter `relay send <destination-id> 2 test`.
4. Observe the original source and immediate forwarding source at the destination.
5. Repeat with `*`, different hop counts, and relay forwarding disabled.
6. Watch duplicate suppression in a topology where two intermediates hear the same packet.

Keep tests small. Relaying has no routing or congestion control.

### 12. Diagnostics, Calibration, and Sleep

1. Save `diag` at startup and after traffic.
2. Compare packet counters, calculated airtime, access deferrals, and memory.
3. Consult the SX1262 documentation to decode raw status and error bitmaps.
4. Use `diag clear`, then confirm device errors were cleared.
5. Use `diag calibrate` after selecting the intended frequency profile.
6. Enter `sleep 10` and observe a full restart, a reset peer table, and a new startup `HELLO` after wake.

### 13. Disconnected and Remote Operation

1. Disconnect the USB data connection after programming and power both boards from their batteries.
2. Use Home on one board to send `HELLO`, ping, preset text, and telemetry.
3. Use Radio to synchronize `robust`, then Test to run a benchmark and inspect its on-screen result.
4. Join the fixed `Nesso-<node-id>` recovery network with password `nesso-lora`, then set station credentials with `wifi ssid`, `wifi password`, and `config restart`.
5. Reconnect through the station address and POST `p`, `s`, and `profile sync default` to `/command`, checking `/status` between commands.
6. Connect with a Bluetooth Low Energy Generic Attribute Profile client, write `peers` to the command characteristic, and read the ingress-status characteristic.
7. Test chunk assembly with a long `transfer` command.
8. Confirm that the same validation and radio behavior occurs through serial, HTTP, and Bluetooth Low Energy.

## Glossary

| Term | Meaning in this project |
| --- | --- |
| API | Application Programming Interface; HTTP and Bluetooth command ingress in this sketch |
| ACK | Acknowledgement, a reply confirming receipt of specific data |
| Airtime | Time for which a complete packet occupies the radio channel |
| Bandwidth | Width of the configured radio channel |
| BLE | Bluetooth Low Energy, the Generic Attribute Profile command transport |
| CAD | Channel Activity Detection, the SX1262's search for a compatible LoRa preamble |
| CIDR | Classless Inter-Domain Routing; slash-prefix notation for the static station subnet |
| CR | Coding Rate, the ratio controlling forward-error-correction redundancy |
| CRC | Cyclic Redundancy Check, an error-detecting checksum |
| CSV | Comma-Separated Values, the machine-readable text format used for reports |
| dB | Decibel, a logarithmic ratio unit |
| dBm | Decibels relative to one milliwatt, an absolute power unit |
| Duty cycle | Fraction of an observation interval occupied by transmission |
| DHCP | Dynamic Host Configuration Protocol, used for automatic station addressing |
| ESP32-C6 | The microcontroller used by the Nesso N1 |
| GFSK | Gaussian Frequency-Shift Keying, filtered digital frequency modulation |
| GPIO | General-Purpose Input/Output, a controllable digital hardware signal |
| HTTP | Hypertext Transfer Protocol, the Wi-Fi command transport |
| I/Q | In-phase and Quadrature, orthogonal complex baseband signal components |
| I2C | Inter-Integrated Circuit, a two-wire peripheral bus |
| IRQ | Interrupt Request, a hardware indication that an event needs service |
| LBT | Listen Before Talk, the general practice of checking a channel before transmitting |
| LDO | Low-Dropout regulator, the SX1262 regulator mode selected by this sketch |
| LDRO | Low-Data-Rate Optimization, LoRa processing for long symbol durations |
| LNA | Low-Noise Amplifier, receive-path amplification controlled by the board |
| LoRa | Semtech's long-range chirp spread spectrum modulation name |
| LoRaWAN | Long Range Wide Area Network, a network protocol built on LoRa but not implemented here |
| MAC address | Media Access Control address; hardware-derived input to the node identifier |
| N1L | This sketch's custom packet-protocol marker; it is not an industry-standard acronym |
| NVS | Non-Volatile Storage, the ESP32 flash-backed key/value store used for configuration |
| Peer | Another exerciser node heard over the radio |
| Physical layer | The modulation, coding, framing, and radio behavior that put bits on air |
| Preamble | Known symbols or bits that let a receiver detect and synchronize to a packet |
| RF | Radio Frequency |
| RSSI | Received Signal Strength Indicator, an estimate of received power |
| RTT | Round-Trip Time from request transmission to accepted reply |
| RX | Receive or receiver |
| SF | Spreading Factor, the LoRa symbol spreading parameter |
| SNR | Signal-to-Noise Ratio |
| SPI | Serial Peripheral Interface, the bus shared by the display and radio |
| SX1262 | The Semtech sub-gigahertz LoRa/GFSK transceiver on the Nesso N1 |
| Sync word | Physical-layer pattern used to distinguish compatible packet populations |
| TCXO | Temperature-Compensated Crystal Oscillator; the board uses a 3.0-volt radio oscillator supply setting |
| TDMA | Time Division Multiple Access; the slot feature is a simplified TDMA-like experiment |
| TX | Transmit or transmitter |
| Whitening | Reversible pseudo-randomization of GFSK payload bits; not encryption |

## What the Exerciser Deliberately Does Not Provide

To keep observations visible and the sketch self-contained, it does not provide:

- LoRa application encryption, remote API authorization, or secure key management;
- LoRaWAN compatibility;
- guaranteed unique node identifiers;
- routing-table discovery or optimal path selection;
- reliable multicast;
- congestion control or quality-of-service classes;
- automatic regional frequency plans or compliance enforcement;
- synchronized wall clocks;
- persistent peers, counters, exercises, transfers, results, or relay history across reset or deep sleep;
- simultaneous reception of several frequencies, spreading factors, or modes; or
- calibrated spectrum, power, current, or bit-error-rate instrumentation.

Those omissions are useful boundaries for interpreting the experiments. The exerciser exposes the interaction among physical settings, channel access, simple link behavior, and application-level reliability without hiding those mechanisms behind a larger network stack.