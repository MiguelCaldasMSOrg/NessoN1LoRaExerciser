/*
  Arduino Nesso N1 - two-node SX1262 LoRa exerciser

  Flash this same sketch to two Nesso N1 boards. Each board derives a short
  node ID from its ESP32 eFuse MAC address, discovers the other node with
  HELLO packets, and can exchange text, pings, acknowledgements, and
  telemetry.

  All sketch behavior and radio settings are contained in this .ino file.
  The repository's VS Code and GitHub configuration is optional; Arduino IDE
  users only need the board support and required libraries listed below.

  Features:
    - Automatic peer discovery with periodic HELLO packets
    - Text messages with acknowledgements and retry handling
    - Ping and pong exchanges
    - Battery, uptime, charge, and free-heap telemetry
    - RSSI and SNR reporting for received packets
    - LoRa channel-activity detection
    - Serial, button, and onboard display interaction
    - Touchscreen controls for messaging, radio settings, tests, access, and peers
    - Wi-Fi HTTP and Bluetooth Low Energy APIs using the serial command parser
    - Runtime LoRa profile selection and synchronized peer switching
    - GFSK modulation with synchronized LoRa/GFSK changes
    - Packet delivery, RTT, throughput, RSSI, SNR, and airtime benchmarks
    - Automated benchmark sweeps across all LoRa profiles
    - Automatic CAD with randomized backoff and optional slotted access
    - Airtime-based duty-cycle throttling
    - Multi-frequency channel surveys with CSV output
    - SX1262 receive-duty-cycle mode, deep sleep, and boosted RX gain
    - CRC, explicit/implicit header, IQ inversion, LDRO, and whitening controls
    - An eight-entry peer table with explicit peer selection
    - CRC-checked fragmented text transfers with selective acknowledgements
    - Bounded broadcast relaying with duplicate and hop-limit protection
    - Raw SX1262 status, packet, IRQ, error, calibration, and link diagnostics

  Required libraries:
    - RadioLib
    - Arduino_Nesso_N1

  Library sources:
    RadioLib: https://github.com/jgromes/RadioLib
    Arduino_Nesso_N1: https://github.com/arduino-libraries/Arduino_Nesso_N1

  Versions verified by the repository automation:
    Arduino CLI 1.5.1
    Espressif ESP32 core 3.3.11
    Arduino_Nesso_N1 1.0.0
    RadioLib 7.7.1

  Arduino IDE:
    Open this file, select "Arduino Nesso N1", select the board's serial
    port, and upload. The workspace files are not required.

  Arduino CLI:
    Board manager URL:
      https://espressif.github.io/arduino-esp32/package_esp32_index.json
    Fully qualified board name:
      esp32:esp32:arduino_nesso_n1
    Install the ESP32 core and required libraries, then compile with:
      arduino-cli compile --clean --fqbn esp32:esp32:arduino_nesso_n1 .

  VS Code:
    Open NessoN1LoRaExerciser.code-workspace. The workspace enables the
    Arduino CLI and selects this sketch and the Nesso N1 board. The supplied
    Arduino extension configuration writes reusable output to the sibling
    directory ../NessoN1LoRaExerciser-build so builds are cached outside the
    sketch directory. IntelliSense uses that directory's compile_commands.json
    plus explicit Nesso, ESP32, RadioLib, display, and sensor include paths.

  Serial monitor: 115200 baud, newline enabled.

  Commands:
    t <text>  Send text to the discovered peer, or broadcast if no peer is known
    p         Ping the peer
    h         Send a discovery HELLO
    s         Print local status and send telemetry
    c         Run a LoRa channel-activity detection scan
    ?         Print this help
    help      Print this help
    mode lora|gfsk
          Change this node's modulation locally
    mode sync lora|gfsk
          Schedule the same modulation change on listening peers
    profiles  List the four LoRa profiles
    profile <name|number>
          Change this node's LoRa profile locally
    profile sync <name|number>
          Schedule the same LoRa profile on listening peers
    options   Print packet option state
    crc on|off
          Enable or disable the radio packet CRC
    iq normal|inverted
          Set LoRa IQ polarity
    header explicit|implicit <bytes>
          Select LoRa header mode; implicit length is 24 to 220 bytes
    ldro auto|on|off
          Select automatic or forced LoRa low-data-rate optimization
    whitening on|off
          Control GFSK data whitening
    rxgain power|boosted
          Select SX1262 power-saving or boosted receive gain
    lowpower on|off
          Control SX1262 receive-duty-cycle mode
    cad auto on|off
          Run LoRa CAD with randomized backoff before transmissions
    duty off|<percent>
          Disable or set airtime-based transmit throttling
    slots on|off|sync
          Control or synchronize eight 250 ms transmit slots
    survey [startMHz endMHz stepKHz samples]
          Measure RSSI and LoRa detections on up to 200 channels as CSV
    diag | diag clear | diag calibrate
          Print, clear, or calibrate SX1262 diagnostics
    sleep <seconds>
          Enter timed ESP32 deep sleep; the sketch restarts on wake
    peers | peer auto|<node-id>
          List peers or control destination selection
    benchmark <packets> [bytes]
          Run 1 to 1000 request/reply samples with 24 to 120 byte bodies
    benchmark stop|report
          Stop or inspect the current benchmark
    sweep <packets> [bytes] | sweep stop
          Benchmark all four LoRa profiles and return to the default
    transfer <text>
          Send 1 to 1152 bytes with fragmentation and selective ACKs
    transfer status|resume|cancel
          Inspect or control the outgoing transfer
    relay on|off
          Control forwarding of bounded broadcast relay packets
    relay send <node-id|*> <1-8 hops> <text>
          Send a relayed text message
    config | config defaults | config restart
          Show configuration, restore defaults, or restart
    wifi ssid <name> | wifi password <value|open> | wifi clear
          Configure or clear persistent Wi-Fi station credentials
    wifi address dhcp|<a.a.a.a/8-30>
          Select persistent DHCP or static local-subnet addressing
    ble name <name>
          Configure the persistent Bluetooth Low Energy device name

  Buttons:
    KEY1      Ping from Home, confirm a radio change, or return Home
    KEY2      Unused because the side button is difficult to operate

  Touchscreen:
    Home      Send HELLO, ping, preset text, or telemetry
    Radio     Synchronize modulation and LoRa profile changes
    Test      Run CAD, a benchmark, a profile sweep, or inspect results
    Access    Toggle CAD, duty pacing, slots, low-power RX, gain, and relay
    Peers     Select a discovered peer or return to automatic selection

  Remote command API:
    Wi-Fi     Joins the configured station; falls back to a fixed recovery AP
    Fallback  Nesso-<node-id>, password nesso-lora, http://192.168.4.1
    HTTP      POST text/plain commands to /command; GET state from /status
    BLE       Write commands to 7bbf0002-6ba5-4e35-9f1f-8d36a7f34c01
              under service 7bbf0001-6ba5-4e35-9f1f-8d36a7f34c01
    Both transports queue the same strings accepted by the serial monitor.

  Runtime behavior:
    - The node ID is eight hexadecimal characters derived from the ESP32
      eFuse MAC address.
    - A HELLO is sent at startup and then approximately every 25 to 30 seconds.
    - Telemetry is first sent after approximately 15 to 30 seconds and then
      every 60 to 75 seconds.
    - Unacknowledged text is attempted up to three times with a 3.5-second
      acknowledgement timeout.
    - Duplicate text is not displayed twice, but it is acknowledged again so
      the sender can recover from a lost acknowledgement.
    - A 1.2-second minimum transmit interval provides a simple demonstration
      duty-cycle guard.
    - Text, ping, and telemetry use broadcast destination * until a peer is
      known.
    - Automatic CAD, duty-percentage throttling, slotted access, low-power RX,
      boosted gain, and relay forwarding use their last persisted values.
    - Wi-Fi, BLE, LoRa, packet, access, gain, and relay settings are restored
      from NVS. Writes occur only when a stored value actually changes.
    - If configured Wi-Fi station association fails, the fixed recovery access
      point starts. Its SSID, password, address, and subnet are not configurable.
    - Protocol replies blocked by access controls enter a four-packet deferred
      queue and are retried up to ten times.
    - Automatic peer selection returns to broadcast after three minutes without
      hearing the selected peer; explicit peer selection remains locked.
    - Long-running benchmarks, sweeps, transfers, and radio changes are kept
      mutually exclusive so their traffic and measurements do not overlap.
    - Synchronized mode, profile, and slot commands use a three-second delay so
      all listening nodes can apply the change together. Local variants affect
      only the board where the command is entered.

  Default radio profile:
    Frequency:        868.1 MHz
    Bandwidth:        125 kHz
    Spreading factor: 7
    Coding rate:      4/5
    Sync word:        0x12
    TX power:         14 dBm
    Preamble:         8 symbols

  Additional LoRa profiles:
    fast:             868.1 MHz, 250 kHz, SF7, CR 4/5, 10 dBm, 8 symbols
    robust:           868.1 MHz, 125 kHz, SF10, CR 4/5, 14 dBm, 12 symbols
    maximum-range:    868.1 MHz, 125 kHz, SF12, CR 4/8, 14 dBm, 16 symbols

  GFSK profile:
    Frequency and TX power come from the selected profile.
    Bit rate:         50.0 kbps
    Deviation:        25.0 kHz
    Receiver BW:      156.2 kHz
    Preamble:         32 bits

  Packet format:
    N1L|<type>|<source>|<destination>|<sequence>|<body>

  Packet types:
    H  Discovery hello; body: hello
    T  Text message; body: user-provided text
    A  Text acknowledgement; body: T:<acknowledged-sequence>
    P  Ping; body: ping
    R  Ping reply; body: P:<ping-sequence>;pong
    S  Telemetry; body: uptime, battery voltage, charge level, and free heap
    B  Benchmark request
    b  Benchmark reply
    M  Synchronized modulation/profile change
    F  Transfer fragment with transfer ID, index, count, and CRC-16
    K  Transfer selective acknowledgement bitmap
    L  Bounded relay message with origin, destination, and remaining hops
    Q  Synchronized slotted-access epoch

  Exercise limits:
    - Benchmarks allow 1 to 1000 packets and 24 to 120 byte bodies. CSV results
      include send attempts, send failures, delivery percentage, RTT, RSSI,
      SNR, throughput, transmit airtime, and profile.
    - Transfers allow 1152 bytes split into at most 16 fragments of 72 bytes.
      Missing fragments are retried for five rounds; paused transfers can resume.
    - A receiver holds one incomplete transfer for 30 seconds.
    - Relay messages allow one to eight hops. Each node remembers 16 message IDs
      for two minutes to suppress loops and duplicates.
    - The peer table holds eight nodes with last-seen time and link statistics.

  Automation:
    GitHub Actions runs Arduino Lint in strict sketch mode and compiles for
    esp32:esp32:arduino_nesso_n1 on master pushes, pull requests, and manual
    dispatches using the versions listed above.

  License:
    Released into the public domain under the Unlicense.

  The default profile is a common EU868 test frequency. Change the radio
  settings below on both boards, and use only frequencies and power levels
  permitted in your country.
*/

// This diagnostic exerciser intentionally exposes RadioLib's low-level SX1262
// status and error commands in addition to the normal public radio API.
#define RADIOLIB_GODMODE 1
#include <RadioLib.h>
#include <Arduino_Nesso_N1.h>
#include <esp_sleep.h>
#include <WiFi.h>
#include <WebServer.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <Preferences.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

// ---------------------------------------------------------------------------
// LoRa profile - both boards must use exactly the same values.
// ---------------------------------------------------------------------------

constexpr float LORA_FREQUENCY_MHZ = 868.1f;
constexpr float LORA_BANDWIDTH_KHZ = 125.0f;
constexpr uint8_t LORA_SPREADING_FACTOR = 7;
constexpr uint8_t LORA_CODING_RATE = 5;  // 4/5
constexpr uint8_t LORA_SYNC_WORD = 0x12;
constexpr int8_t LORA_TX_POWER_DBM = 14;
constexpr uint16_t LORA_PREAMBLE_SYMBOLS = 8;
constexpr float LORA_TCXO_VOLTAGE = 3.0f;

// Keep packets short and leave room for the protocol header.
constexpr size_t MAX_BODY_LENGTH = 150;
constexpr size_t MAX_PACKET_LENGTH = 220;

// This is a simple demonstration duty-cycle guard, not a substitute for
// checking the radio rules that apply in your region.
constexpr uint32_t TX_MIN_INTERVAL_MS = 1200;
constexpr uint32_t HELLO_INTERVAL_MS = 25000;
constexpr uint32_t TELEMETRY_INTERVAL_MS = 60000;
constexpr uint32_t ACK_TIMEOUT_MS = 3500;
constexpr uint8_t MAX_TEXT_ATTEMPTS = 3;

// Nesso N1 has an SX1262 with no exposed reset pin. The board's radio power,
// LNA, and antenna switch are controlled through the I/O expander.
SX1262 radio = new Module(LORA_CS, LORA_IRQ, RADIOLIB_NC, LORA_BUSY, SPI);
NessoDisplay nessoDisplay;
NessoTouch nessoTouch;
NessoBattery nessoBattery;

constexpr uint16_t COLOR_BLACK = 0x0000;
constexpr uint16_t COLOR_WHITE = 0xFFFF;
constexpr uint16_t COLOR_CYAN = 0x07FF;
constexpr uint16_t COLOR_GREEN = 0x07E0;
constexpr uint16_t COLOR_YELLOW = 0xFFE0;
constexpr uint16_t COLOR_RED = 0xF800;
constexpr uint16_t COLOR_BLUE = 0x02DF;
constexpr uint16_t COLOR_DARK = 0x18E3;
constexpr uint16_t COLOR_GRAY = 0x7BEF;

// Runtime profiles and fixed-size state keep every experiment available in a
// single Arduino IDE sketch without requiring generated source files.
enum class RadioMode : uint8_t {
  LORA,
  FSK
};

struct LoRaProfile {
  const char* name;
  float frequencyMHz;
  float bandwidthKHz;
  uint8_t spreadingFactor;
  uint8_t codingRate;
  int8_t txPowerDbm;
  uint16_t preambleSymbols;
};

constexpr LoRaProfile LORA_PROFILES[] = {
  { "default", 868.1f, 125.0f, 7, 5, 14, 8 },
  { "fast", 868.1f, 250.0f, 7, 5, 10, 8 },
  { "robust", 868.1f, 125.0f, 10, 5, 14, 12 },
  { "maximum-range", 868.1f, 125.0f, 12, 8, 14, 16 }
};
constexpr size_t LORA_PROFILE_COUNT = sizeof(LORA_PROFILES) / sizeof(LORA_PROFILES[0]);

constexpr float FSK_BIT_RATE_KBPS = 50.0f;
constexpr float FSK_FREQUENCY_DEVIATION_KHZ = 25.0f;
constexpr float FSK_RECEIVER_BANDWIDTH_KHZ = 156.2f;
constexpr uint16_t FSK_PREAMBLE_BITS = 32;

constexpr uint8_t MAX_PEERS = 8;
constexpr uint8_t MAX_FRAGMENTS = 16;
constexpr size_t FRAGMENT_DATA_LENGTH = 72;
constexpr size_t MAX_TRANSFER_LENGTH = MAX_FRAGMENTS * FRAGMENT_DATA_LENGTH;
constexpr uint8_t MAX_RELAY_HISTORY = 16;
constexpr uint32_t RELAY_RECORD_TIMEOUT_MS = 120000;
constexpr uint32_t PEER_STALE_MS = 180000;
constexpr uint32_t FRAGMENT_RETRY_MS = 1800;
constexpr uint8_t MAX_FRAGMENT_ATTEMPTS = 5;
constexpr uint32_t INCOMING_TRANSFER_TIMEOUT_MS = 30000;
constexpr uint32_t BENCHMARK_REPLY_TIMEOUT_MS = 5000;
constexpr uint16_t BENCHMARK_MAX_PACKETS = 1000;
constexpr size_t BENCHMARK_MAX_PAYLOAD = 120;
constexpr uint32_t CSMA_SLOT_MS = 50;
constexpr uint8_t CSMA_MAX_ATTEMPTS = 5;
constexpr uint8_t TDMA_SLOT_COUNT = 8;
constexpr uint32_t TDMA_SLOT_MS = 250;
constexpr float DEFAULT_DUTY_CYCLE_PERCENT = 1.0f;
constexpr uint8_t DEFERRED_PACKET_COUNT = 4;
constexpr size_t REMOTE_COMMAND_MAX_LENGTH = MAX_TRANSFER_LENGTH + 80;
constexpr uint8_t REMOTE_COMMAND_QUEUE_DEPTH = 4;
constexpr char FALLBACK_AP_PASSWORD[] = "nesso-lora";
constexpr uint32_t WIFI_STATION_CONNECT_TIMEOUT_MS = 12000;
constexpr uint32_t WIFI_FALLBACK_DELAY_MS = 30000;
constexpr char REMOTE_BLE_SERVICE_UUID[] = "7bbf0001-6ba5-4e35-9f1f-8d36a7f34c01";
constexpr char REMOTE_BLE_COMMAND_UUID[] = "7bbf0002-6ba5-4e35-9f1f-8d36a7f34c01";
constexpr char REMOTE_BLE_STATUS_UUID[] = "7bbf0003-6ba5-4e35-9f1f-8d36a7f34c01";
constexpr char CONFIG_NAMESPACE[] = "nesso-config";
constexpr char CONFIG_KEY[] = "settings";
constexpr uint32_t PERSISTENT_CONFIG_MAGIC = 0x4E314346UL;
constexpr uint16_t PERSISTENT_CONFIG_VERSION = 1;

constexpr uint8_t CONFIG_FLAG_CAD = 1U << 0;
constexpr uint8_t CONFIG_FLAG_DUTY = 1U << 1;
constexpr uint8_t CONFIG_FLAG_LOW_POWER = 1U << 2;
constexpr uint8_t CONFIG_FLAG_RELAY = 1U << 3;
constexpr uint8_t CONFIG_FLAG_RX_BOOST = 1U << 4;
constexpr uint8_t CONFIG_FLAG_CRC = 1U << 5;
constexpr uint8_t CONFIG_FLAG_WHITENING = 1U << 6;
constexpr uint8_t CONFIG_FLAG_INVERTED_IQ = 1U << 7;

enum class RemoteEnqueueResult : uint8_t {
  QUEUED,
  EMPTY,
  TOO_LONG,
  FULL
};

struct RemoteCommand {
  char value[REMOTE_COMMAND_MAX_LENGTH + 1];
  char source[8];
};

struct PersistentConfiguration {
  uint32_t magic;
  uint16_t version;
  uint16_t size;
  char wifiSsid[33];
  char wifiPassword[64];
  char bleName[25];
  uint8_t wifiAddress[4];
  uint8_t wifiPrefixLength;
  uint8_t wifiStaticAddress;
  uint8_t radioMode;
  uint8_t profileIndex;
  uint8_t flags;
  int8_t forcedLdro;
  uint16_t implicitLength;
  float dutyPercent;
};

struct PeerInfo {
  String id;
  uint32_t lastSeenAt = 0;
  float lastRssi = 0.0f;
  float lastSnr = 0.0f;
  uint32_t packetsReceived = 0;
  uint32_t duplicatePackets = 0;
  uint32_t lastSequence = 0;
  bool haveLastSequence = false;
};

struct BenchmarkState {
  bool active = false;
  uint32_t session = 0;
  String destination;
  uint16_t requestedPackets = 0;
  uint16_t sentPackets = 0;
  uint16_t receivedReplies = 0;
  uint16_t transmitAttempts = 0;
  uint16_t transmitFailures = 0;
  size_t payloadLength = 0;
  uint32_t startedAt = 0;
  uint32_t lastSendAt = 0;
  uint32_t allSentAt = 0;
  uint32_t minimumRttMs = UINT32_MAX;
  uint32_t maximumRttMs = 0;
  uint64_t totalRttMs = 0;
  uint64_t startingAirtimeUs = 0;
  float totalRssi = 0.0f;
  float totalSnr = 0.0f;
};

struct OutgoingTransfer {
  bool active = false;
  uint32_t id = 0;
  String destination;
  String data;
  uint16_t checksum = 0;
  uint16_t acknowledgedMask = 0;
  uint8_t fragmentCount = 0;
  uint8_t attempts = 0;
  uint8_t nextFragmentIndex = 0;
  bool waitingForAcknowledgement = false;
  uint32_t lastSendAt = 0;
};

struct IncomingTransfer {
  bool active = false;
  uint32_t id = 0;
  String source;
  String fragments[MAX_FRAGMENTS];
  uint16_t receivedMask = 0;
  uint16_t checksum = 0;
  uint8_t fragmentCount = 0;
  uint32_t lastUpdateAt = 0;
};

struct RelayRecord {
  uint32_t messageId = 0;
  String origin;
  uint32_t seenAt = 0;
};

struct DeferredPacket {
  bool active = false;
  char type = 0;
  String destination;
  String body;
  uint32_t sequence = 0;
  uint32_t nextAttemptAt = 0;
  uint8_t attempts = 0;
};

struct PendingRadioConfiguration {
  bool active = false;
  RadioMode mode = RadioMode::LORA;
  uint8_t profileIndex = 0;
  uint32_t applyAt = 0;
  bool persist = true;
};

enum class SweepPhase : uint8_t {
  IDLE,
  WAITING_TO_REQUEST_PROFILE,
  WAITING_FOR_PROFILE,
  RUNNING_BENCHMARK
};

struct ProfileSweepState {
  bool active = false;
  SweepPhase phase = SweepPhase::IDLE;
  uint8_t profileIndex = 0;
  uint16_t packetCount = 0;
  size_t payloadLength = 0;
  uint32_t nextActionAt = 0;
  bool returningToDefault = false;
};

enum class UiPage : uint8_t {
  HOME,
  RADIO,
  TEST,
  ACCESS,
  PEERS,
  RESULTS
};

volatile bool packetReceivedFlag = false;
bool radioReady = false;
bool displayReady = false;
bool touchReady = false;
RadioMode radioMode = RadioMode::LORA;
uint8_t activeProfileIndex = 0;
bool listenBeforeTalkEnabled = false;
bool dutyCycleLimitEnabled = false;
bool lowPowerReceiveEnabled = false;
bool slottedAccessEnabled = false;
bool relayEnabled = false;
bool peerSelectionLocked = false;
bool receiveBoostedGainEnabled = false;
bool packetCrcEnabled = true;
bool fskWhiteningEnabled = true;
bool invertedIqEnabled = false;
int8_t forcedLdroState = -1;
size_t implicitPacketLength = 0;
uint8_t activeLoRaSyncWord = LORA_SYNC_WORD;
float dutyCyclePercent = DEFAULT_DUTY_CYCLE_PERCENT;
uint32_t nextDutyCycleTransmitAt = 0;
uint32_t slotEpochAt = 0;
uint64_t totalPacketsSent = 0;
uint64_t totalPacketsReceived = 0;
uint64_t totalTransmitAirtimeUs = 0;
uint32_t transmitFailures = 0;
uint32_t accessDeferrals = 0;
uint32_t cadBusyDetections = 0;

UiPage uiPage = UiPage::HOME;
bool uiConfirmationActive = false;
RadioMode uiPendingRadioMode = RadioMode::LORA;
uint8_t uiPendingProfileIndex = 0;
String uiNotice;
uint32_t uiNoticeUntil = 0;
uint32_t lastUiRefreshAt = 0;
uint32_t lastTouchPollAt = 0;
bool previousTouchPressed = false;
uint8_t uiPeerPage = 0;
bool benchmarkResultAvailable = false;
String benchmarkResultReason;
RadioMode benchmarkResultMode = RadioMode::LORA;
uint8_t benchmarkResultProfileIndex = 0;
uint32_t benchmarkResultElapsedMs = 0;
uint64_t benchmarkResultAirtimeUs = 0;
QueueHandle_t remoteCommandQueue = nullptr;
WebServer remoteApiServer(80);
bool remoteHttpReady = false;
bool remoteBleReady = false;
String remoteWifiSsid;
String remoteWifiPassword;
String remoteBleDeviceName;
IPAddress remoteWifiAddress;
uint8_t remoteWifiPrefixLength = 24;
bool remoteWifiStaticAddress = false;
bool remoteWifiFallbackApActive = false;
bool remoteWifiEverConnected = false;
uint32_t remoteWifiDisconnectedAt = 0;
bool transientRadioConfigurationActive = false;
String lastRemoteCommandSource;
uint32_t lastRemoteCommandAt = 0;
BLECharacteristic* remoteBleStatusCharacteristic = nullptr;
String remoteBleCommandAssembly;
size_t remoteBleExpectedLength = 0;
bool remoteBleAssemblyActive = false;
PersistentConfiguration persistedConfiguration = {};
bool persistedConfigurationAvailable = false;
bool persistedConfigurationStored = false;

PeerInfo peers[MAX_PEERS];
BenchmarkState benchmark;
OutgoingTransfer outgoingTransfer;
IncomingTransfer incomingTransfer;
RelayRecord relayHistory[MAX_RELAY_HISTORY];
uint8_t relayHistoryNext = 0;
PendingRadioConfiguration pendingRadioConfiguration;
ProfileSweepState profileSweep;
DeferredPacket deferredPackets[DEFERRED_PACKET_COUNT];

char nodeId[9] = {};
uint32_t nodeIdValue = 0;
String peerId;
String lastReceivedText;
String lastReceivedSource;
String lastPacketSource;
String serialLine;

uint32_t txSequence = 1;
uint32_t lastTxAt = 0;
uint32_t nextHelloAt = 0;
uint32_t nextTelemetryAt = 0;
uint32_t lastPacketSequence = 0;
bool haveLastPacket = false;

bool pendingTextActive = false;
uint32_t pendingTextSequence = 0;
String pendingTextDestination;
String pendingTextBody;
uint8_t pendingTextAttempts = 0;
uint32_t pendingTextLastAttempt = 0;

float lastReceivedRssi = 0.0f;
float lastReceivedSnr = 0.0f;
uint32_t lastButtonEvent = 0;
bool previousKey1Pressed = false;

struct ReceivedPacket {
  char type = 0;
  String source;
  String destination;
  uint32_t sequence = 0;
  String body;
};

void onRadioPacketReceived() {
  packetReceivedFlag = true;
}

bool timeReached(uint32_t now, uint32_t deadline) {
  return static_cast<int32_t>(now - deadline) >= 0;
}

const LoRaProfile& activeLoRaProfile() {
  return LORA_PROFILES[activeProfileIndex];
}

const char* radioModeName() {
  return radioMode == RadioMode::LORA ? "LoRa" : "GFSK";
}

PersistentConfiguration defaultPersistentConfiguration() {
  PersistentConfiguration configuration = {};
  configuration.magic = PERSISTENT_CONFIG_MAGIC;
  configuration.version = PERSISTENT_CONFIG_VERSION;
  configuration.size = sizeof(PersistentConfiguration);
  configuration.wifiSsid[0] = '\0';
  configuration.wifiPassword[0] = '\0';
  snprintf(configuration.bleName, sizeof(configuration.bleName), "Nesso-%s", nodeId);
  const uint8_t defaultAddress[] = { 192, 168, 1, 100 };
  memcpy(configuration.wifiAddress, defaultAddress, sizeof(configuration.wifiAddress));
  configuration.wifiPrefixLength = 24;
  configuration.wifiStaticAddress = 0;
  configuration.radioMode = static_cast<uint8_t>(RadioMode::LORA);
  configuration.profileIndex = 0;
  configuration.flags = CONFIG_FLAG_CRC | CONFIG_FLAG_WHITENING;
  configuration.forcedLdro = -1;
  configuration.implicitLength = 0;
  configuration.dutyPercent = DEFAULT_DUTY_CYCLE_PERCENT;
  return configuration;
}

PersistentConfiguration currentPersistentConfiguration() {
  PersistentConfiguration configuration = {};
  configuration.magic = PERSISTENT_CONFIG_MAGIC;
  configuration.version = PERSISTENT_CONFIG_VERSION;
  configuration.size = sizeof(PersistentConfiguration);
  snprintf(configuration.wifiSsid, sizeof(configuration.wifiSsid), "%s", remoteWifiSsid.c_str());
  snprintf(configuration.wifiPassword, sizeof(configuration.wifiPassword), "%s", remoteWifiPassword.c_str());
  snprintf(configuration.bleName, sizeof(configuration.bleName), "%s", remoteBleDeviceName.c_str());
  for (uint8_t index = 0; index < 4; ++index) {
    configuration.wifiAddress[index] = remoteWifiAddress[index];
  }
  configuration.wifiPrefixLength = remoteWifiPrefixLength;
  configuration.wifiStaticAddress = remoteWifiStaticAddress ? 1 : 0;
  configuration.radioMode = transientRadioConfigurationActive && persistedConfigurationAvailable ? persistedConfiguration.radioMode : static_cast<uint8_t>(radioMode);
  configuration.profileIndex = transientRadioConfigurationActive && persistedConfigurationAvailable ? persistedConfiguration.profileIndex : activeProfileIndex;
  configuration.flags = 0;
  configuration.flags |= listenBeforeTalkEnabled ? CONFIG_FLAG_CAD : 0;
  configuration.flags |= dutyCycleLimitEnabled ? CONFIG_FLAG_DUTY : 0;
  configuration.flags |= lowPowerReceiveEnabled ? CONFIG_FLAG_LOW_POWER : 0;
  configuration.flags |= relayEnabled ? CONFIG_FLAG_RELAY : 0;
  configuration.flags |= receiveBoostedGainEnabled ? CONFIG_FLAG_RX_BOOST : 0;
  configuration.flags |= packetCrcEnabled ? CONFIG_FLAG_CRC : 0;
  configuration.flags |= fskWhiteningEnabled ? CONFIG_FLAG_WHITENING : 0;
  configuration.flags |= invertedIqEnabled ? CONFIG_FLAG_INVERTED_IQ : 0;
  configuration.forcedLdro = forcedLdroState;
  configuration.implicitLength = static_cast<uint16_t>(implicitPacketLength);
  configuration.dutyPercent = dutyCyclePercent;
  return configuration;
}

size_t boundedStringLength(const char* value, size_t maximumLength) {
  size_t length = 0;
  while (length < maximumLength && value[length] != '\0') {
    ++length;
  }
  return length;
}

bool persistentConfigurationIsValid(const PersistentConfiguration& configuration) {
  if (configuration.magic != PERSISTENT_CONFIG_MAGIC || configuration.version != PERSISTENT_CONFIG_VERSION || configuration.size != sizeof(PersistentConfiguration)) {
    return false;
  }
  if (memchr(configuration.wifiSsid, '\0', sizeof(configuration.wifiSsid)) == nullptr || memchr(configuration.wifiPassword, '\0', sizeof(configuration.wifiPassword)) == nullptr || memchr(configuration.bleName, '\0', sizeof(configuration.bleName)) == nullptr) {
    return false;
  }

  const size_t ssidLength = boundedStringLength(configuration.wifiSsid, sizeof(configuration.wifiSsid));
  const size_t passwordLength = boundedStringLength(configuration.wifiPassword, sizeof(configuration.wifiPassword));
  const size_t bleNameLength = boundedStringLength(configuration.bleName, sizeof(configuration.bleName));
  const bool implicitLengthValid = configuration.implicitLength == 0 || (configuration.implicitLength >= 24 && configuration.implicitLength <= MAX_PACKET_LENGTH);
  const IPAddress address(configuration.wifiAddress[0], configuration.wifiAddress[1], configuration.wifiAddress[2], configuration.wifiAddress[3]);
  const bool credentialsValid = passwordLength == 0 || (passwordLength >= 8 && passwordLength <= 63);
  const bool addressValid = configuration.wifiStaticAddress == 0 || (configuration.wifiPrefixLength >= 1 && configuration.wifiPrefixLength <= 30 && configuration.wifiAddress[0] > 0 && configuration.wifiAddress[0] < 224 && static_cast<uint32_t>(address) != 0);
  return ssidLength <= 32 && credentialsValid && bleNameLength >= 1 && bleNameLength <= 24 && configuration.wifiStaticAddress <= 1 && addressValid && configuration.radioMode <= static_cast<uint8_t>(RadioMode::FSK) && configuration.profileIndex < LORA_PROFILE_COUNT && configuration.forcedLdro >= -1 && configuration.forcedLdro <= 1 && implicitLengthValid && isfinite(configuration.dutyPercent) && configuration.dutyPercent > 0.0f && configuration.dutyPercent <= 100.0f;
}

void applyPersistentConfiguration(const PersistentConfiguration& configuration) {
  remoteWifiSsid = configuration.wifiSsid;
  remoteWifiPassword = configuration.wifiPassword;
  remoteBleDeviceName = configuration.bleName;
  remoteWifiAddress = IPAddress(configuration.wifiAddress[0], configuration.wifiAddress[1], configuration.wifiAddress[2], configuration.wifiAddress[3]);
  remoteWifiPrefixLength = configuration.wifiPrefixLength;
  remoteWifiStaticAddress = configuration.wifiStaticAddress != 0;
  transientRadioConfigurationActive = false;
  radioMode = static_cast<RadioMode>(configuration.radioMode);
  activeProfileIndex = configuration.profileIndex;
  listenBeforeTalkEnabled = configuration.flags & CONFIG_FLAG_CAD;
  dutyCycleLimitEnabled = configuration.flags & CONFIG_FLAG_DUTY;
  lowPowerReceiveEnabled = configuration.flags & CONFIG_FLAG_LOW_POWER;
  relayEnabled = configuration.flags & CONFIG_FLAG_RELAY;
  receiveBoostedGainEnabled = configuration.flags & CONFIG_FLAG_RX_BOOST;
  packetCrcEnabled = configuration.flags & CONFIG_FLAG_CRC;
  fskWhiteningEnabled = configuration.flags & CONFIG_FLAG_WHITENING;
  invertedIqEnabled = configuration.flags & CONFIG_FLAG_INVERTED_IQ;
  forcedLdroState = configuration.forcedLdro;
  implicitPacketLength = configuration.implicitLength;
  dutyCyclePercent = configuration.dutyPercent;
}

bool loadPersistentConfiguration() {
  const PersistentConfiguration defaults = defaultPersistentConfiguration();
  applyPersistentConfiguration(defaults);
  persistedConfiguration = defaults;
  persistedConfigurationAvailable = true;
  persistedConfigurationStored = false;

  Preferences preferences;
  if (!preferences.begin(CONFIG_NAMESPACE, true)) {
    Serial.println(F("[Config] NVS unavailable; using defaults"));
    return false;
  }

  PersistentConfiguration stored = {};
  const size_t storedLength = preferences.getBytesLength(CONFIG_KEY);
  const size_t readLength = storedLength == sizeof(stored) ? preferences.getBytes(CONFIG_KEY, &stored, sizeof(stored)) : 0;
  preferences.end();
  if (readLength != sizeof(stored) || !persistentConfigurationIsValid(stored)) {
    Serial.println(F("[Config] no valid stored configuration; using defaults"));
    return false;
  }

  applyPersistentConfiguration(stored);
  persistedConfiguration = stored;
  persistedConfigurationAvailable = true;
  persistedConfigurationStored = true;
  Serial.println(F("[Config] restored stored configuration"));
  return true;
}

bool savePersistentConfigurationIfChanged() {
  const PersistentConfiguration current = currentPersistentConfiguration();
  if (persistedConfigurationAvailable && memcmp(&current, &persistedConfiguration, sizeof(current)) == 0) {
    return false;
  }

  Preferences preferences;
  if (!preferences.begin(CONFIG_NAMESPACE, false)) {
    Serial.println(F("[Config] could not open NVS for writing"));
    return false;
  }
  const size_t written = preferences.putBytes(CONFIG_KEY, &current, sizeof(current));
  preferences.end();
  if (written != sizeof(current)) {
    Serial.println(F("[Config] failed to store configuration"));
    return false;
  }

  persistedConfiguration = current;
  persistedConfigurationAvailable = true;
  persistedConfigurationStored = true;
  Serial.println(F("[Config] stored changed configuration"));
  return true;
}

bool parseWifiCidr(const String& value, IPAddress& address, uint8_t& prefixLength) {
  const int separator = value.lastIndexOf('/');
  if (separator <= 0 || separator >= static_cast<int>(value.length()) - 1) {
    return false;
  }

  const String addressText = value.substring(0, separator);
  const String prefixText = value.substring(separator + 1);
  const char* prefixStart = prefixText.c_str();
  char* prefixEnd = nullptr;
  const long parsedPrefix = strtol(prefixStart, &prefixEnd, 10);
  IPAddress parsedAddress;
  if (!parsedAddress.fromString(addressText) || prefixEnd == prefixStart || *prefixEnd != '\0' || parsedPrefix < 8 || parsedPrefix > 30 || parsedAddress[0] == 0 || parsedAddress[0] >= 224) {
    return false;
  }

  const IPAddress subnet = subnetMaskFromPrefix(static_cast<uint8_t>(parsedPrefix));
  bool networkAddress = true;
  bool broadcastAddress = true;
  for (uint8_t index = 0; index < 4; ++index) {
    networkAddress &= (parsedAddress[index] & static_cast<uint8_t>(~subnet[index])) == 0;
    broadcastAddress &= (parsedAddress[index] & static_cast<uint8_t>(~subnet[index])) == static_cast<uint8_t>(~subnet[index]);
  }
  if (networkAddress || broadcastAddress) {
    return false;
  }

  address = parsedAddress;
  prefixLength = static_cast<uint8_t>(parsedPrefix);
  return true;
}

void printPersistentConfiguration() {
  Serial.println(F("--- Persistent configuration ---"));
  Serial.print(F("Storage:       "));
  Serial.println(persistedConfigurationStored ? "NVS" : "defaults (not written)" );
  Serial.print(F("Wi-Fi SSID:    "));
  Serial.println(remoteWifiSsid.length() ? remoteWifiSsid : "(unset; recovery AP on boot)");
  Serial.print(F("Wi-Fi secret:  "));
  Serial.println(remoteWifiPassword.length() ? "(set)" : (remoteWifiSsid.length() ? "(open network)" : "(unset)"));
  Serial.print(F("Wi-Fi address: "));
  if (remoteWifiStaticAddress) {
    Serial.print(remoteWifiAddress);
    Serial.print('/');
    Serial.println(remoteWifiPrefixLength);
  } else {
    Serial.println(F("DHCP"));
  }
  Serial.print(F("Wi-Fi active:  "));
  Serial.print(remoteWifiFallbackApActive ? "recovery AP, " : "station, ");
  Serial.println(remoteWifiFallbackApActive ? WiFi.softAPIP() : WiFi.localIP());
  Serial.print(F("BLE name:      "));
  Serial.println(remoteBleDeviceName);
  Serial.print(F("Radio:         "));
  Serial.print(radioModeName());
  Serial.print(F(" / "));
  Serial.println(activeLoRaProfile().name);
  Serial.println(F("Wi-Fi and BLE identity changes take effect after restart."));
  Serial.println(F("--------------------------------"));
}

RemoteEnqueueResult enqueueRemoteCommand(const String& rawCommand, const char* source) {
  String command = rawCommand;
  command.trim();
  if (!command.length()) {
    return RemoteEnqueueResult::EMPTY;
  }
  if (command.length() > REMOTE_COMMAND_MAX_LENGTH) {
    return RemoteEnqueueResult::TOO_LONG;
  }
  if (remoteCommandQueue == nullptr) {
    return RemoteEnqueueResult::FULL;
  }

  RemoteCommand queued = {};
  memcpy(queued.value, command.c_str(), command.length());
  queued.value[command.length()] = '\0';
  snprintf(queued.source, sizeof(queued.source), "%s", source);
  return xQueueSend(remoteCommandQueue, &queued, 0) == pdPASS ? RemoteEnqueueResult::QUEUED : RemoteEnqueueResult::FULL;
}

const char* remoteEnqueueResultName(RemoteEnqueueResult result) {
  switch (result) {
    case RemoteEnqueueResult::QUEUED:
      return "queued";
    case RemoteEnqueueResult::EMPTY:
      return "empty";
    case RemoteEnqueueResult::TOO_LONG:
      return "too_long";
    case RemoteEnqueueResult::FULL:
      return "queue_full";
  }
  return "unknown";
}

void appendJsonEscaped(String& output, const String& value) {
  for (size_t index = 0; index < value.length(); ++index) {
    const char character = value.charAt(index);
    if (character == '"' || character == '\\') {
      output += '\\';
      output += character;
    } else if (character == '\b') {
      output += F("\\b");
    } else if (character == '\f') {
      output += F("\\f");
    } else if (character == '\n') {
      output += F("\\n");
    } else if (character == '\r') {
      output += F("\\r");
    } else if (character == '\t') {
      output += F("\\t");
    } else if (static_cast<uint8_t>(character) < 0x20) {
      char escape[7];
      snprintf(escape, sizeof(escape), "\\u%04X", static_cast<uint8_t>(character));
      output += escape;
    } else {
      output += character;
    }
  }
}

String remoteStatusJson() {
  String json;
  json.reserve(420);
  json += F("{\"node\":\"");
  json += nodeId;
  json += F("\",\"mode\":\"");
  json += radioModeName();
  json += F("\",\"profile\":\"");
  json += activeLoRaProfile().name;
  json += F("\",\"peer\":\"");
  appendJsonEscaped(json, peerId);
  json += F("\",\"radio_ready\":");
  json += radioReady ? F("true") : F("false");
  json += F(",\"http_ready\":");
  json += remoteHttpReady ? F("true") : F("false");
  json += F(",\"ble_ready\":");
  json += remoteBleReady ? F("true") : F("false");
  json += F(",\"wifi_mode\":\"");
  json += remoteWifiFallbackApActive ? F("fallback_ap") : F("station");
  json += F("\",\"wifi_address\":\"");
  json += remoteWifiFallbackApActive ? WiFi.softAPIP().toString() : WiFi.localIP().toString();
  json += '"';
  json += F(",\"benchmark_active\":");
  json += benchmark.active ? F("true") : F("false");
  json += F(",\"sweep_active\":");
  json += profileSweep.active ? F("true") : F("false");
  json += F(",\"queued_commands\":");
  json += remoteCommandQueue == nullptr ? 0 : uxQueueMessagesWaiting(remoteCommandQueue);
  json += F(",\"last_command_source\":\"");
  appendJsonEscaped(json, lastRemoteCommandSource);
  json += F("\",\"last_command_ms\":");
  json += lastRemoteCommandAt;
  json += '}';
  return json;
}

void sendRemoteHttpQueueResult(RemoteEnqueueResult result) {
  int statusCode = 202;
  if (result == RemoteEnqueueResult::EMPTY) {
    statusCode = 400;
  } else if (result == RemoteEnqueueResult::TOO_LONG) {
    statusCode = 413;
  } else if (result == RemoteEnqueueResult::FULL) {
    statusCode = 503;
  }

  String response = String("{\"status\":\"") + remoteEnqueueResultName(result) + "\"}";
  remoteApiServer.send(statusCode, "application/json", response);
}

IPAddress subnetMaskFromPrefix(uint8_t prefixLength) {
  const uint32_t mask = prefixLength == 0 ? 0 : 0xFFFFFFFFUL << (32 - prefixLength);
  return IPAddress(static_cast<uint8_t>(mask >> 24), static_cast<uint8_t>(mask >> 16), static_cast<uint8_t>(mask >> 8), static_cast<uint8_t>(mask));
}

bool startFallbackAccessPoint() {
  const String fallbackSsid = String("Nesso-") + nodeId;
  const IPAddress address(192, 168, 4, 1);
  const IPAddress subnet(255, 255, 255, 0);
  WiFi.mode(remoteWifiSsid.length() ? WIFI_AP_STA : WIFI_AP);
  if (!WiFi.softAPConfig(address, address, subnet) || !WiFi.softAP(fallbackSsid.c_str(), FALLBACK_AP_PASSWORD, 1, 0, 2)) {
    Serial.println(F("[Remote] recovery Wi-Fi access point failed"));
    return false;
  }

  remoteWifiFallbackApActive = true;
  Serial.print(F("[Remote] recovery AP "));
  Serial.print(fallbackSsid);
  Serial.print(F(" at http://"));
  Serial.println(WiFi.softAPIP());
  return true;
}

bool beginRemoteWifiStation() {
  if (!remoteWifiSsid.length()) {
    return false;
  }

  WiFi.mode(WIFI_STA);
  if (remoteWifiStaticAddress) {
    const IPAddress empty(0, 0, 0, 0);
    if (!WiFi.config(remoteWifiAddress, empty, subnetMaskFromPrefix(remoteWifiPrefixLength), empty, empty)) {
      Serial.println(F("[Remote] static station address configuration failed"));
      return false;
    }
  } else if (!WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE)) {
    Serial.println(F("[Remote] DHCP station configuration failed"));
    return false;
  }

  WiFi.setAutoReconnect(true);
  WiFi.begin(remoteWifiSsid.c_str(), remoteWifiPassword.length() ? remoteWifiPassword.c_str() : nullptr);
  Serial.print(F("[Remote] connecting Wi-Fi station to "));
  Serial.println(remoteWifiSsid);
  remoteWifiDisconnectedAt = millis();
  return true;
}

void maintainRemoteWifi() {
  if (!remoteWifiSsid.length()) {
    return;
  }
  if (WiFi.status() == WL_CONNECTED) {
    remoteWifiDisconnectedAt = 0;
    if (!remoteWifiEverConnected) {
      Serial.print(F("[Remote] Wi-Fi station at http://"));
      Serial.println(WiFi.localIP());
    }
    remoteWifiEverConnected = true;
    if (remoteWifiFallbackApActive) {
      WiFi.softAPdisconnect(false);
      WiFi.mode(WIFI_STA);
      remoteWifiFallbackApActive = false;
      Serial.print(F("[Remote] station recovered at http://"));
      Serial.println(WiFi.localIP());
    }
    return;
  }

  if (remoteWifiDisconnectedAt == 0) {
    remoteWifiDisconnectedAt = millis();
  }
  const uint32_t fallbackDelayMs = remoteWifiEverConnected ? WIFI_FALLBACK_DELAY_MS : WIFI_STATION_CONNECT_TIMEOUT_MS;
  if (!remoteWifiFallbackApActive && static_cast<uint32_t>(millis() - remoteWifiDisconnectedAt) >= fallbackDelayMs) {
    if (!remoteWifiEverConnected) {
      Serial.println(F("[Remote] initial station connection timed out"));
    }
    startFallbackAccessPoint();
  }
}

void setupRemoteHttpApi() {
  if (remoteCommandQueue == nullptr) {
    remoteCommandQueue = xQueueCreate(REMOTE_COMMAND_QUEUE_DEPTH, sizeof(RemoteCommand));
  }
  if (remoteCommandQueue == nullptr) {
    Serial.println(F("[Remote] command queue allocation failed"));
    return;
  }

  WiFi.persistent(false);
  if (!remoteWifiSsid.length()) {
    if (!startFallbackAccessPoint()) {
      return;
    }
  } else if (!beginRemoteWifiStation() && !startFallbackAccessPoint()) {
    return;
  }

  remoteApiServer.enableCORS(true);
  remoteApiServer.on("/", HTTP_GET, []() {
    remoteApiServer.send(200, "application/json", "{\"service\":\"Nesso N1 LoRa Exerciser\",\"command\":\"POST /command as text/plain or GET /command?command=...\",\"status\":\"GET /status\"}");
  });
  remoteApiServer.on("/status", HTTP_GET, []() {
    remoteApiServer.send(200, "application/json", remoteStatusJson());
  });
  remoteApiServer.on("/command", HTTP_POST, []() {
    sendRemoteHttpQueueResult(enqueueRemoteCommand(remoteApiServer.arg("plain"), "HTTP"));
  });
  remoteApiServer.on("/command", HTTP_GET, []() {
    String command = remoteApiServer.arg("command");
    if (!command.length()) {
      command = remoteApiServer.arg("cmd");
    }
    sendRemoteHttpQueueResult(enqueueRemoteCommand(command, "HTTP"));
  });
  remoteApiServer.onNotFound([]() {
    remoteApiServer.send(404, "application/json", "{\"status\":\"not_found\"}");
  });
  remoteApiServer.begin();
  remoteHttpReady = true;
}

void setRemoteBleIngressStatus(const String& status) {
  if (remoteBleStatusCharacteristic != nullptr) {
    remoteBleStatusCharacteristic->setValue(status);
  }
}

void resetRemoteBleAssembly() {
  remoteBleCommandAssembly = "";
  remoteBleExpectedLength = 0;
  remoteBleAssemblyActive = false;
}

class RemoteBleCommandCallbacks : public BLECharacteristicCallbacks {
public:
  void onWrite(BLECharacteristic* characteristic) override {
    const String value = characteristic->getValue();
    if (value.startsWith("@begin:")) {
      const long requestedLength = value.substring(7).toInt();
      resetRemoteBleAssembly();
      if (requestedLength <= 0 || requestedLength > static_cast<long>(REMOTE_COMMAND_MAX_LENGTH)) {
        setRemoteBleIngressStatus("invalid_length");
        return;
      }
      remoteBleExpectedLength = static_cast<size_t>(requestedLength);
      remoteBleCommandAssembly.reserve(remoteBleExpectedLength + 1);
      remoteBleAssemblyActive = true;
      setRemoteBleIngressStatus("chunk_ready");
      return;
    }

    if (value.startsWith("@data:")) {
      if (!remoteBleAssemblyActive) {
        setRemoteBleIngressStatus("no_chunk_session");
        return;
      }
      const String chunk = value.substring(6);
      if (remoteBleCommandAssembly.length() + chunk.length() > remoteBleExpectedLength) {
        resetRemoteBleAssembly();
        setRemoteBleIngressStatus("chunk_too_long");
        return;
      }
      remoteBleCommandAssembly += chunk;
      setRemoteBleIngressStatus(String("chunk:") + remoteBleCommandAssembly.length() + '/' + remoteBleExpectedLength);
      return;
    }

    if (value == "@end") {
      if (!remoteBleAssemblyActive || remoteBleCommandAssembly.length() != remoteBleExpectedLength) {
        resetRemoteBleAssembly();
        setRemoteBleIngressStatus("length_mismatch");
        return;
      }
      const RemoteEnqueueResult result = enqueueRemoteCommand(remoteBleCommandAssembly, "BLE");
      resetRemoteBleAssembly();
      setRemoteBleIngressStatus(remoteEnqueueResultName(result));
      return;
    }

    if (value == "@cancel") {
      resetRemoteBleAssembly();
      setRemoteBleIngressStatus("cancelled");
      return;
    }

    setRemoteBleIngressStatus(remoteEnqueueResultName(enqueueRemoteCommand(value, "BLE")));
  }
};

void setupRemoteBleApi() {
  if (remoteCommandQueue == nullptr) {
    remoteCommandQueue = xQueueCreate(REMOTE_COMMAND_QUEUE_DEPTH, sizeof(RemoteCommand));
  }
  if (remoteCommandQueue == nullptr) {
    Serial.println(F("[Remote] BLE command queue allocation failed"));
    return;
  }

  const String deviceName = remoteBleDeviceName;
  if (!BLEDevice::init(deviceName)) {
    Serial.println(F("[Remote] BLE initialization failed"));
    return;
  }
  BLEDevice::setMTU(517);

  BLEServer* server = BLEDevice::createServer();
  server->advertiseOnDisconnect(true);
  BLEService* service = server->createService(REMOTE_BLE_SERVICE_UUID);
  BLECharacteristic* commandCharacteristic = service->createCharacteristic(REMOTE_BLE_COMMAND_UUID, BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR);
  remoteBleStatusCharacteristic = service->createCharacteristic(REMOTE_BLE_STATUS_UUID, BLECharacteristic::PROPERTY_READ);
  commandCharacteristic->setCallbacks(new RemoteBleCommandCallbacks());
  remoteBleStatusCharacteristic->setValue("ready");
  service->start();

  BLEAdvertising* advertising = BLEDevice::getAdvertising();
  advertising->addServiceUUID(REMOTE_BLE_SERVICE_UUID);
  advertising->setScanResponse(true);
  BLEDevice::startAdvertising();
  remoteBleReady = true;

  Serial.print(F("[Remote] BLE device "));
  Serial.println(deviceName);
}

uint16_t activePreambleLength() {
  return radioMode == RadioMode::LORA ? activeLoRaProfile().preambleSymbols : FSK_PREAMBLE_BITS;
}

String clipped(const String& value, size_t maxLength) {
  if (value.length() <= maxLength) {
    return value;
  }
  if (maxLength < 4) {
    return value.substring(0, maxLength);
  }
  return value.substring(0, maxLength - 3) + "...";
}

constexpr int16_t UI_WIDTH = 240;
constexpr int16_t UI_HEADER_HEIGHT = 20;
constexpr int16_t UI_NAV_Y = 113;
constexpr int16_t UI_NAV_HEIGHT = 22;
constexpr int16_t UI_NOTICE_Y = 102;

void redrawDisplay();

void drawUiText(const String& text, int16_t x, int16_t y, uint16_t color = COLOR_WHITE, textdatum_t datum = TL_DATUM, uint8_t size = 1) {
  if (!displayReady) {
    return;
  }

  nessoDisplay.setTextDatum(datum);
  nessoDisplay.setTextSize(size);
  nessoDisplay.setTextColor(color);
  nessoDisplay.drawString(text.c_str(), x, y);
}

void drawUiButton(int16_t x, int16_t y, int16_t width, int16_t height, const String& label, bool active = false, uint8_t textSize = 1) {
  const uint16_t fillColor = active ? 0x0320 : COLOR_DARK;
  const uint16_t borderColor = active ? COLOR_GREEN : COLOR_GRAY;
  nessoDisplay.fillRoundRect(x, y, width, height, 3, fillColor);
  nessoDisplay.drawRoundRect(x, y, width, height, 3, borderColor);
  drawUiText(label, x + width / 2, y + height / 2, active ? COLOR_GREEN : COLOR_WHITE, MC_DATUM, textSize);
}

void drawUiHeader(const String& title) {
  nessoDisplay.fillRect(0, 0, UI_WIDTH, UI_HEADER_HEIGHT, COLOR_DARK);
  drawUiText(title, 4, UI_HEADER_HEIGHT / 2, COLOR_CYAN, ML_DATUM);
  drawUiText(nodeId, UI_WIDTH - 4, UI_HEADER_HEIGHT / 2, COLOR_WHITE, MR_DATUM);
}

void drawUiNavigation() {
  const char* labels[] = { "Home", "Radio", "Test", "Access", "Peers" };
  UiPage activePage = uiPage == UiPage::RESULTS ? UiPage::TEST : uiPage;
  for (uint8_t index = 0; index < 5; ++index) {
    const int16_t x = index * 48;
    const bool active = static_cast<uint8_t>(activePage) == index;
    nessoDisplay.fillRect(x, UI_NAV_Y, 48, UI_NAV_HEIGHT, active ? COLOR_BLUE : COLOR_DARK);
    nessoDisplay.drawRect(x, UI_NAV_Y, 48, UI_NAV_HEIGHT, COLOR_BLACK);
    drawUiText(labels[index], x + 24, UI_NAV_Y + UI_NAV_HEIGHT / 2, COLOR_WHITE, MC_DATUM);
  }
}

void drawUiNotice(const String& fallback = "") {
  String text = fallback;
  if (uiNotice.length() && !timeReached(millis(), uiNoticeUntil)) {
    text = uiNotice;
  }
  nessoDisplay.fillRect(0, UI_NOTICE_Y, UI_WIDTH, UI_NAV_Y - UI_NOTICE_Y, COLOR_BLACK);
  drawUiText(clipped(text, 38), UI_WIDTH / 2, UI_NOTICE_Y + 5, COLOR_YELLOW, MC_DATUM);
}

void setUiNotice(const String& text, uint32_t durationMs = 3000) {
  uiNotice = text;
  uiNoticeUntil = millis() + durationMs;
  redrawDisplay();
}

void drawHomePage() {
  String title = String(radioModeName()) + " / " + activeLoRaProfile().name;
  drawUiHeader(title);
  drawUiButton(3, 23, 115, 35, "HELLO", false, 2);
  drawUiButton(122, 23, 115, 35, "PING", false, 2);
  drawUiButton(3, 62, 115, 35, "TEXT", false, 2);
  drawUiButton(122, 62, 115, 35, "TELEM", false, 2);

  String status = peerId.length() ? String("Peer ") + peerId : "Searching for peer";
  if (lastReceivedText.length()) {
    status = String("RX: ") + lastReceivedText;
  }
  drawUiNotice(status);
}

void drawRadioPage() {
  drawUiHeader("Synchronize radio");
  drawUiButton(3, 23, 115, 28, "LoRa", radioMode == RadioMode::LORA, 2);
  drawUiButton(122, 23, 115, 28, "GFSK", radioMode == RadioMode::FSK, 2);
  drawUiButton(3, 55, 115, 21, "DEFAULT", activeProfileIndex == 0);
  drawUiButton(122, 55, 115, 21, "FAST", activeProfileIndex == 1);
  drawUiButton(3, 79, 115, 21, "ROBUST", activeProfileIndex == 2);
  drawUiButton(122, 79, 115, 21, "MAX RANGE", activeProfileIndex == 3);
  drawUiNotice("Tap, then confirm");
}

void drawTestPage() {
  drawUiHeader("Link tests");
  drawUiButton(3, 23, 115, 35, "CAD", false, 2);
  drawUiButton(122, 23, 115, 35, "BENCH 10", benchmark.active, 2);
  drawUiButton(3, 62, 115, 35, "SWEEP 5", profileSweep.active, 2);
  drawUiButton(122, 62, 115, 35, benchmark.active || profileSweep.active ? "STOP" : "RESULTS", false, 2);

  String status = "Ready";
  if (profileSweep.active) {
    status = String("Sweep ") + String(profileSweep.profileIndex + 1) + '/' + String(LORA_PROFILE_COUNT);
  } else if (benchmark.active) {
    status = String("Benchmark ") + String(benchmark.sentPackets) + '/' + String(benchmark.requestedPackets) + " RX " + String(benchmark.receivedReplies);
  }
  drawUiNotice(status);
}

void drawAccessPage() {
  drawUiHeader("Channel and receive");
  drawUiButton(3, 23, 76, 35, "CAD", listenBeforeTalkEnabled);
  drawUiButton(82, 23, 76, 35, "DUTY 1%", dutyCycleLimitEnabled);
  drawUiButton(161, 23, 76, 35, "SLOTS", slottedAccessEnabled);
  drawUiButton(3, 62, 76, 35, "LOW RX", lowPowerReceiveEnabled);
  drawUiButton(82, 62, 76, 35, "BOOST", receiveBoostedGainEnabled);
  drawUiButton(161, 62, 76, 35, "RELAY", relayEnabled);
  drawUiNotice("Green = enabled");
}

uint8_t peerCount() {
  uint8_t count = 0;
  for (const PeerInfo& peer : peers) {
    if (peer.id.length()) {
      ++count;
    }
  }
  return count;
}

PeerInfo* peerAtUiIndex(uint8_t index) {
  uint8_t current = 0;
  for (PeerInfo& peer : peers) {
    if (!peer.id.length()) {
      continue;
    }
    if (current == index) {
      return &peer;
    }
    ++current;
  }
  return nullptr;
}

void drawPeerCard(int16_t x, int16_t y, PeerInfo* peer) {
  if (peer == nullptr) {
    nessoDisplay.drawRoundRect(x, y, 87, 35, 3, COLOR_DARK);
    return;
  }

  const bool selected = peer->id == peerId;
  nessoDisplay.fillRoundRect(x, y, 87, 35, 3, selected ? 0x0320 : COLOR_DARK);
  nessoDisplay.drawRoundRect(x, y, 87, 35, 3, selected ? COLOR_GREEN : COLOR_GRAY);
  drawUiText(peer->id, x + 43, y + 10, selected ? COLOR_GREEN : COLOR_WHITE, MC_DATUM);
  drawUiText(String(peer->lastRssi, 1) + " dBm", x + 43, y + 25, COLOR_WHITE, MC_DATUM);
}

void drawPeersPage() {
  const uint8_t count = peerCount();
  const uint8_t pageCount = max<uint8_t>(1, static_cast<uint8_t>((count + 3) / 4));
  if (uiPeerPage >= pageCount) {
    uiPeerPage = 0;
  }

  drawUiHeader(String("Peers ") + String(uiPeerPage + 1) + '/' + String(pageCount));
  drawUiButton(3, 23, 53, 35, "AUTO", !peerSelectionLocked);
  drawUiButton(3, 62, 53, 35, "NEXT", false);
  const uint8_t firstIndex = uiPeerPage * 4;
  drawPeerCard(60, 23, peerAtUiIndex(firstIndex));
  drawPeerCard(150, 23, peerAtUiIndex(firstIndex + 1));
  drawPeerCard(60, 62, peerAtUiIndex(firstIndex + 2));
  drawPeerCard(150, 62, peerAtUiIndex(firstIndex + 3));
  drawUiNotice(count ? "Tap peer to lock" : "Waiting for peers");
}

void drawResultsPage() {
  drawUiHeader("Benchmark results");
  if (!benchmark.active && !benchmarkResultAvailable) {
    drawUiText("No benchmark result", UI_WIDTH / 2, 60, COLOR_WHITE, MC_DATUM, 2);
    drawUiNotice("Run BENCH or SWEEP");
    return;
  }

  const RadioMode resultMode = benchmark.active ? radioMode : benchmarkResultMode;
  const uint8_t resultProfile = benchmark.active ? activeProfileIndex : benchmarkResultProfileIndex;
  const uint32_t elapsedMs = benchmark.active ? max<uint32_t>(millis() - benchmark.startedAt, 1UL) : max<uint32_t>(benchmarkResultElapsedMs, 1UL);
  const float delivery = benchmark.requestedPackets ? 100.0f * benchmark.receivedReplies / benchmark.requestedPackets : 0.0f;
  const float averageRtt = benchmark.receivedReplies ? static_cast<float>(benchmark.totalRttMs) / benchmark.receivedReplies : 0.0f;
  const float averageRssi = benchmark.receivedReplies ? benchmark.totalRssi / benchmark.receivedReplies : 0.0f;
  const float averageSnr = benchmark.receivedReplies ? benchmark.totalSnr / benchmark.receivedReplies : 0.0f;
  const float throughput = static_cast<float>(benchmark.receivedReplies * benchmark.payloadLength * 8ULL * 1000ULL) / elapsedMs;
  const uint64_t airtimeUs = benchmark.active ? totalTransmitAirtimeUs - benchmark.startingAirtimeUs : benchmarkResultAirtimeUs;

  drawUiText(String(resultMode == RadioMode::LORA ? "LoRa " : "GFSK ") + LORA_PROFILES[resultProfile].name, 4, 25, COLOR_CYAN);
  drawUiText(String("Sent ") + benchmark.sentPackets + '/' + benchmark.requestedPackets + "  RX " + benchmark.receivedReplies + "  " + String(delivery, 1) + '%', 4, 38);
  drawUiText(String("RTT ") + String(averageRtt, 1) + " ms", 4, 51);
  drawUiText(String("RSSI ") + String(averageRssi, 1) + "  SNR " + String(averageSnr, 1), 4, 64);
  drawUiText(String("Rate ") + String(throughput, 1) + " bps", 4, 77);
  drawUiText(String("Airtime ") + String(static_cast<double>(airtimeUs) / 1000.0, 1) + " ms", 4, 90);
  drawUiNotice(benchmark.active ? "Running" : benchmarkResultReason);
}

void drawRadioConfirmation() {
  nessoDisplay.fillRoundRect(8, 23, 224, 82, 5, COLOR_BLACK);
  nessoDisplay.drawRoundRect(8, 23, 224, 82, 5, COLOR_YELLOW);
  drawUiText("SYNC RADIO?", UI_WIDTH / 2, 38, COLOR_YELLOW, MC_DATUM, 2);
  drawUiText(String(uiPendingRadioMode == RadioMode::LORA ? "LoRa / " : "GFSK / ") + LORA_PROFILES[uiPendingProfileIndex].name, UI_WIDTH / 2, 58, COLOR_WHITE, MC_DATUM);
  drawUiButton(18, 72, 96, 26, "APPLY", true, 2);
  drawUiButton(126, 72, 96, 26, "CANCEL", false, 2);
}

void redrawDisplay() {
  if (!displayReady) {
    return;
  }

  nessoDisplay.fillScreen(COLOR_BLACK);
  switch (uiPage) {
    case UiPage::HOME:
      drawHomePage();
      break;
    case UiPage::RADIO:
      drawRadioPage();
      break;
    case UiPage::TEST:
      drawTestPage();
      break;
    case UiPage::ACCESS:
      drawAccessPage();
      break;
    case UiPage::PEERS:
      drawPeersPage();
      break;
    case UiPage::RESULTS:
      drawResultsPage();
      break;
  }
  drawUiNavigation();
  if (uiConfirmationActive) {
    drawRadioConfirmation();
  }
  lastUiRefreshAt = millis();
}

void printRadioError(const char* operation, int state) {
  Serial.print(F("[LoRa] "));
  Serial.print(operation);
  Serial.print(F(" failed, code "));
  Serial.println(state);
}

int startConfiguredReceive() {
  if (lowPowerReceiveEnabled && radioMode == RadioMode::LORA) {
    return radio.startReceiveDutyCycleAuto(activePreambleLength());
  }
  return radio.startReceive();
}

// Apply packet settings after every begin() because switching modems resets
// the SX1262 packet engine.
int applyPacketOptions() {
  int state = radio.setCRC(packetCrcEnabled ? 2 : 0);
  if (state != RADIOLIB_ERR_NONE) {
    return state;
  }

  if (radioMode == RadioMode::FSK) {
    const uint8_t syncWord[] = { 0x2D, 0xD4 };
    state = radio.setSyncWord(const_cast<uint8_t*>(syncWord), sizeof(syncWord));
    if (state == RADIOLIB_ERR_NONE) {
      state = radio.setWhitening(fskWhiteningEnabled);
    }
    return state;
  }

  state = radio.setSyncWord(activeLoRaSyncWord);
  if (state == RADIOLIB_ERR_NONE) {
    state = radio.invertIQ(invertedIqEnabled);
  }
  if (state == RADIOLIB_ERR_NONE) {
    state = implicitPacketLength == 0 ? radio.explicitHeader() : radio.implicitHeader(implicitPacketLength);
  }
  if (state == RADIOLIB_ERR_NONE) {
    state = forcedLdroState < 0 ? radio.autoLDRO() : radio.forceLDRO(forcedLdroState != 0);
  }
  return state;
}

int initializeActiveRadio() {
  radioReady = false;
  packetReceivedFlag = false;
  radio.tcxoVoltage = LORA_TCXO_VOLTAGE;
  radio.useRegulatorLDO = true;

  const LoRaProfile& profile = activeLoRaProfile();
  int state;
  if (radioMode == RadioMode::LORA) {
    state = radio.begin(profile.frequencyMHz, profile.bandwidthKHz, profile.spreadingFactor, profile.codingRate, activeLoRaSyncWord, profile.txPowerDbm, profile.preambleSymbols, LORA_TCXO_VOLTAGE, true);
  } else {
    state = radio.beginFSK(profile.frequencyMHz, FSK_BIT_RATE_KBPS, FSK_FREQUENCY_DEVIATION_KHZ, FSK_RECEIVER_BANDWIDTH_KHZ, profile.txPowerDbm, FSK_PREAMBLE_BITS, LORA_TCXO_VOLTAGE, true);
  }
  if (state != RADIOLIB_ERR_NONE) {
    return state;
  }

  state = applyPacketOptions();
  if (state != RADIOLIB_ERR_NONE) {
    return state;
  }

  state = radio.setRxBoostedGainMode(receiveBoostedGainEnabled);
  if (state != RADIOLIB_ERR_NONE) {
    return state;
  }

  radio.setPacketReceivedAction(onRadioPacketReceived);
  state = startConfiguredReceive();
  if (state == RADIOLIB_ERR_NONE) {
    radioReady = true;
  }
  return state;
}

bool waitForTransmitAccess() {
  // Every transmit path shares these guards so benchmark, transfer, relay,
  // acknowledgement, and interactive traffic obey the same constraints.
  const uint32_t now = millis();
  if (lastTxAt != 0 && static_cast<uint32_t>(now - lastTxAt) < TX_MIN_INTERVAL_MS) {
    ++accessDeferrals;
    return false;
  }
  if (dutyCycleLimitEnabled && !timeReached(now, nextDutyCycleTransmitAt)) {
    ++accessDeferrals;
    return false;
  }

  if (slottedAccessEnabled) {
    if (!timeReached(now, slotEpochAt)) {
      ++accessDeferrals;
      return false;
    }
    const uint32_t cycleLength = TDMA_SLOT_COUNT * TDMA_SLOT_MS;
    const uint8_t currentSlot = static_cast<uint8_t>(((now - slotEpochAt) % cycleLength) / TDMA_SLOT_MS);
    const uint8_t nodeSlot = static_cast<uint8_t>(nodeIdValue % TDMA_SLOT_COUNT);
    if (currentSlot != nodeSlot) {
      ++accessDeferrals;
      return false;
    }
  }

  if (!listenBeforeTalkEnabled || radioMode != RadioMode::LORA) {
    return true;
  }

  for (uint8_t attempt = 0; attempt < CSMA_MAX_ATTEMPTS; ++attempt) {
    const int standbyState = radio.standby();
    if (standbyState != RADIOLIB_ERR_NONE) {
      printRadioError("standby before CAD", standbyState);
      return false;
    }

    const int scanState = radio.scanChannel();
    if (scanState == RADIOLIB_CHANNEL_FREE) {
      return true;
    }
    if (scanState != RADIOLIB_PREAMBLE_DETECTED) {
      printRadioError("automatic CAD", scanState);
      return false;
    }

    ++cadBusyDetections;
    const uint32_t maximumSlots = (1UL << min<uint8_t>(attempt + 1, 5)) - 1UL;
    delay(random(maximumSlots + 1UL) * CSMA_SLOT_MS);
  }

  ++accessDeferrals;
  startConfiguredReceive();
  return false;
}

void recordTransmitAirtime(size_t payloadLength) {
  const uint32_t airtimeUs = radio.getTimeOnAir(payloadLength);
  totalTransmitAirtimeUs += airtimeUs;
  if (!dutyCycleLimitEnabled || dutyCyclePercent <= 0.0f) {
    return;
  }

  const double intervalUs = static_cast<double>(airtimeUs) * (100.0 / dutyCyclePercent);
  const uint32_t intervalMs = static_cast<uint32_t>(min<double>(intervalUs / 1000.0, static_cast<double>(UINT32_MAX)));
  nextDutyCycleTransmitAt = millis() + intervalMs;
}

void setTransmitting(bool transmitting) {
  digitalWrite(LED_BUILTIN, transmitting ? HIGH : LOW);
  // The Nesso N1 reference implementation disables the receive LNA while TX
  // is active, then restores it for receive mode.
  digitalWrite(LORA_LNA_ENABLE, transmitting ? LOW : HIGH);
}

void makeNodeId() {
  const uint64_t mac = ESP.getEfuseMac();
  nodeIdValue = static_cast<uint32_t>(mac ^ (mac >> 32));
  snprintf(nodeId, sizeof(nodeId), "%08lX", static_cast<unsigned long>(nodeIdValue));
}

void setupNessoIo() {
  Wire.begin(SDA, SCL);
  Wire.setClock(400000);

  pinMode(KEY1, INPUT_PULLUP);
  pinMode(KEY2, INPUT_PULLUP);

  digitalWrite(LED_BUILTIN, LOW);
  pinMode(LED_BUILTIN, OUTPUT);

  // Power up the SX1262 through the Nesso N1 I/O expander.
  digitalWrite(LORA_ENABLE, LOW);
  pinMode(LORA_ENABLE, OUTPUT);
  delay(50);
  digitalWrite(LORA_ENABLE, HIGH);
  delay(50);

  digitalWrite(LORA_LNA_ENABLE, HIGH);
  pinMode(LORA_LNA_ENABLE, OUTPUT);
  digitalWrite(LORA_ANTENNA_SWITCH, HIGH);
  pinMode(LORA_ANTENNA_SWITCH, OUTPUT);
}

void setupDisplay() {
  // The LCD and radio share SPI. Keep both chip-select lines inactive before
  // either peripheral starts using the bus.
  pinMode(LCD_CS, OUTPUT);
  digitalWrite(LCD_CS, HIGH);
  pinMode(LORA_CS, OUTPUT);
  digitalWrite(LORA_CS, HIGH);
  SPI.begin(SCK, MISO, MOSI, LORA_CS);

  displayReady = nessoDisplay.begin();
  if (!displayReady) {
    Serial.println(F("[Display] initialization failed"));
    return;
  }

  touchReady = nessoTouch.begin();
  if (!touchReady) {
    Serial.println(F("[Touch] initialization failed"));
  }

  nessoDisplay.setRotation(1);
  nessoDisplay.setTextWrap(false);
  redrawDisplay();
}

void setupRadio() {
  // Nesso N1 uses a 3.0 V TCXO and the board support uses the SX1262 LDO
  // regulator configuration.
  Serial.print(F("[Radio] Initializing SX1262 in "));
  Serial.print(radioModeName());
  Serial.print(F(" mode at "));
  Serial.print(activeLoRaProfile().frequencyMHz, 3);
  Serial.println(F(" MHz"));

  const int state = initializeActiveRadio();
  if (state != RADIOLIB_ERR_NONE) {
    printRadioError("begin", state);
    return;
  }
  Serial.println(F("[Radio] ready and listening"));
}

bool parsePacket(const String& raw, ReceivedPacket& packet) {
  // Packet format:
  // N1L|<type>|<source>|<destination>|<sequence>|<body>
  if (!raw.startsWith("N1L|") || raw.length() < 12 || raw.charAt(5) != '|') {
    return false;
  }

  const int sourceEnd = raw.indexOf('|', 6);
  if (sourceEnd < 0) {
    return false;
  }
  const int destinationEnd = raw.indexOf('|', sourceEnd + 1);
  if (destinationEnd < 0) {
    return false;
  }
  const int sequenceEnd = raw.indexOf('|', destinationEnd + 1);
  if (sequenceEnd < 0) {
    return false;
  }

  packet.type = raw.charAt(4);
  packet.source = raw.substring(6, sourceEnd);
  packet.destination = raw.substring(sourceEnd + 1, destinationEnd);

  String sequenceText = raw.substring(destinationEnd + 1, sequenceEnd);
  const char* sequenceStart = sequenceText.c_str();
  char* sequenceEndPtr = nullptr;
  const unsigned long parsedSequence = strtoul(sequenceStart, &sequenceEndPtr, 10);
  if (sequenceEndPtr == sequenceStart || *sequenceEndPtr != '\0') {
    return false;
  }
  packet.sequence = static_cast<uint32_t>(parsedSequence);
  packet.body = raw.substring(sequenceEnd + 1);

  return packet.source.length() > 0 && packet.destination.length() > 0;
}

bool isForThisNode(const ReceivedPacket& packet) {
  return packet.destination == "*" || packet.destination == nodeId;
}

bool sendPacket(char type, const String& destination, const String& body, uint32_t sequence) {
  if (!radioReady) {
    Serial.println(F("[LoRa] transmit skipped: radio is not ready"));
    return false;
  }

  if (!waitForTransmitAccess()) {
    Serial.println(F("[Radio] transmit deferred by access controls"));
    return false;
  }

  String safeBody = body;
  safeBody.replace('\r', ' ');
  safeBody.replace('\n', ' ');
  safeBody = clipped(safeBody, MAX_BODY_LENGTH);

  String payload;
  payload.reserve(MAX_PACKET_LENGTH);
  payload += "N1L|";
  payload += type;
  payload += '|';
  payload += nodeId;
  payload += '|';
  payload += (destination.length() ? destination : "*");
  payload += '|';
  payload += String(sequence);
  payload += '|';
  payload += safeBody;

  if (payload.length() > MAX_PACKET_LENGTH) {
    Serial.println(F("[LoRa] transmit rejected: packet too long"));
    return false;
  }

  if (implicitPacketLength > 0) {
    if (payload.length() > implicitPacketLength) {
      Serial.println(F("[LoRa] transmit rejected: packet exceeds implicit length"));
      return false;
    }
    while (payload.length() < implicitPacketLength) {
      payload += ' ';
    }
  }

  int state = radio.standby();
  if (state != RADIOLIB_ERR_NONE) {
    printRadioError("standby before transmit", state);
    return false;
  }

  setTransmitting(true);
  state = radio.transmit(payload);
  setTransmitting(false);
  lastTxAt = millis();
  if (state == RADIOLIB_ERR_NONE) {
    ++totalPacketsSent;
    recordTransmitAirtime(payload.length());
  } else {
    ++transmitFailures;
  }

  const int receiveState = startConfiguredReceive();
  if (state != RADIOLIB_ERR_NONE) {
    printRadioError("transmit", state);
  }
  if (receiveState != RADIOLIB_ERR_NONE) {
    printRadioError("restart receive", receiveState);
  }

  return state == RADIOLIB_ERR_NONE && receiveState == RADIOLIB_ERR_NONE;
}

bool sendPacketOrDefer(char type, const String& destination, const String& body, uint32_t sequence) {
  // Protocol replies cannot always transmit immediately after reception. A
  // bounded queue preserves them without allowing unbounded memory growth.
  if (sendPacket(type, destination, body, sequence)) {
    return true;
  }
  if (!radioReady || body.length() > MAX_BODY_LENGTH) {
    return false;
  }

  for (DeferredPacket& packet : deferredPackets) {
    if (!packet.active) {
      packet.active = true;
      packet.type = type;
      packet.destination = destination;
      packet.body = body;
      packet.sequence = sequence;
      packet.nextAttemptAt = millis() + TX_MIN_INTERVAL_MS;
      packet.attempts = 1;
      Serial.print(F("[Radio] queued deferred packet type "));
      Serial.println(type);
      return true;
    }
  }

  Serial.println(F("[Radio] deferred packet queue is full"));
  return false;
}

void runDeferredPacketTask() {
  for (DeferredPacket& packet : deferredPackets) {
    if (!packet.active || !timeReached(millis(), packet.nextAttemptAt)) {
      continue;
    }
    if (sendPacket(packet.type, packet.destination, packet.body, packet.sequence)) {
      packet = DeferredPacket();
    } else {
      ++packet.attempts;
      if (packet.attempts > 10) {
        Serial.print(F("[Radio] dropped deferred packet type "));
        Serial.println(packet.type);
        packet = DeferredPacket();
        return;
      }
      packet.nextAttemptAt = millis() + TX_MIN_INTERVAL_MS;
    }
    return;
  }
}

String targetPeerOrBroadcast() {
  if (peerId.length() == 0) {
    return "*";
  }
  if (peerSelectionLocked) {
    return peerId;
  }
  for (const PeerInfo& peer : peers) {
    if (peer.id == peerId && !timeReached(millis(), peer.lastSeenAt + PEER_STALE_MS)) {
      return peerId;
    }
  }
  return "*";
}

bool sendText(const String& text) {
  if (text.length() == 0) {
    Serial.println(F("[LoRa] text message is empty"));
    return false;
  }
  if (pendingTextActive) {
    Serial.println(F("[LoRa] wait for the previous text acknowledgement"));
    return false;
  }
  if (benchmark.active || profileSweep.active || outgoingTransfer.active || pendingRadioConfiguration.active) {
    Serial.println(F("[LoRa] text is unavailable while another exercise is active"));
    return false;
  }

  pendingTextDestination = targetPeerOrBroadcast();
  pendingTextBody = clipped(text, MAX_BODY_LENGTH);
  pendingTextSequence = txSequence++;
  pendingTextAttempts = 1;

  if (!sendPacket('T', pendingTextDestination, pendingTextBody, pendingTextSequence)) {
    pendingTextDestination = "";
    pendingTextBody = "";
    pendingTextAttempts = 0;
    return false;
  }

  pendingTextActive = true;
  pendingTextLastAttempt = millis();
  Serial.print(F("[LoRa] text sent, sequence "));
  Serial.println(pendingTextSequence);
  return true;
}

bool sendHello() {
  return sendPacket('H', "*", "hello", txSequence++);
}

bool sendPing() {
  const uint32_t sequence = txSequence++;
  return sendPacket('P', targetPeerOrBroadcast(), "ping", sequence);
}

String localTelemetry() {
  const float voltage = nessoBattery.getVoltage();
  const uint16_t charge = nessoBattery.getChargeLevel();

  String body;
  body.reserve(80);
  body += "up=";
  body += String(millis() / 1000UL);
  body += ";vbat=";
  body += String(voltage, 2);
  body += ";charge=";
  body += String(charge);
  body += ";heap=";
  body += String(ESP.getFreeHeap());
  return body;
}

bool sendTelemetry() {
  const uint32_t sequence = txSequence++;
  const bool sent = sendPacket('S', targetPeerOrBroadcast(), localTelemetry(), sequence);
  if (sent) {
    Serial.println(F("[LoRa] telemetry sent"));
  }
  return sent;
}

void sendAcknowledgement(const ReceivedPacket& packet) {
  String body = "T:";
  body += String(packet.sequence);
  sendPacketOrDefer('A', packet.source, body, txSequence++);
}

void sendPong(const ReceivedPacket& packet) {
  String body = "P:";
  body += String(packet.sequence);
  body += ";pong";
  sendPacketOrDefer('R', packet.source, body, txSequence++);
}

void printStatus() {
  Serial.println();
  Serial.println(F("--- Nesso N1 LoRa status ---"));
  Serial.print(F("Node:        "));
  Serial.println(nodeId);
  Serial.print(F("Peer:        "));
  Serial.println(peerId.length() ? peerId : "(not discovered)");
  Serial.print(F("Mode:        "));
  Serial.println(radioModeName());
  Serial.print(F("Profile:     "));
  Serial.println(radioMode == RadioMode::LORA ? activeLoRaProfile().name : "GFSK fixed parameters");
  Serial.print(F("Frequency:   "));
  Serial.print(activeLoRaProfile().frequencyMHz, 3);
  Serial.println(F(" MHz"));
  if (radioMode == RadioMode::LORA) {
    Serial.print(F("Modulation:  BW "));
    Serial.print(activeLoRaProfile().bandwidthKHz, 1);
    Serial.print(F(" kHz, SF"));
    Serial.print(activeLoRaProfile().spreadingFactor);
    Serial.print(F(", CR 4/"));
    Serial.println(activeLoRaProfile().codingRate);
  } else {
    Serial.print(F("Modulation:  "));
    Serial.print(FSK_BIT_RATE_KBPS, 1);
    Serial.print(F(" kbps, deviation "));
    Serial.print(FSK_FREQUENCY_DEVIATION_KHZ, 1);
    Serial.print(F(" kHz, RX BW "));
    Serial.print(FSK_RECEIVER_BANDWIDTH_KHZ, 1);
    Serial.println(F(" kHz"));
  }
  Serial.print(F("TX power:    "));
  Serial.print(activeLoRaProfile().txPowerDbm);
  Serial.println(F(" dBm"));
  Serial.print(F("Radio:       "));
  Serial.println(radioReady ? "ready" : "not ready");
  Serial.print(F("Battery:     "));
  Serial.print(nessoBattery.getVoltage(), 2);
  Serial.print(F(" V, "));
  Serial.print(nessoBattery.getChargeLevel());
  Serial.println(F("%"));
  Serial.print(F("Packets:     TX "));
  Serial.print(static_cast<unsigned long>(totalPacketsSent));
  Serial.print(F(", RX "));
  Serial.print(static_cast<unsigned long>(totalPacketsReceived));
  Serial.print(F(", failures "));
  Serial.println(transmitFailures);
  Serial.print(F("TX airtime:  "));
  Serial.print(static_cast<double>(totalTransmitAirtimeUs) / 1000000.0, 3);
  Serial.println(F(" s"));
  Serial.print(F("Access:      CAD "));
  Serial.print(listenBeforeTalkEnabled ? "on" : "off");
  Serial.print(F(", duty "));
  if (dutyCycleLimitEnabled) {
    Serial.print(dutyCyclePercent, 3);
    Serial.print('%');
  } else {
    Serial.print(F("off"));
  }
  Serial.print(F(", slots "));
  Serial.println(slottedAccessEnabled ? "on" : "off");
  Serial.print(F("Receive:     low-power "));
  Serial.print(lowPowerReceiveEnabled ? "on" : "off");
  Serial.print(F(", gain "));
  Serial.println(receiveBoostedGainEnabled ? "boosted" : "power-save");
  Serial.println(F("----------------------------"));
}

void runChannelActivityDetection() {
  if (!radioReady || radioMode != RadioMode::LORA) {
    Serial.println(F("[LoRa] CAD requires a ready radio in LoRa mode"));
    setUiNotice("CAD requires LoRa");
    return;
  }

  int state = radio.standby();
  if (state != RADIOLIB_ERR_NONE) {
    printRadioError("standby before CAD", state);
    setUiNotice("CAD standby failed");
    return;
  }

  const int scanState = radio.scanChannel();
  if (scanState == RADIOLIB_CHANNEL_FREE) {
    Serial.println(F("[LoRa] CAD: channel free"));
    setUiNotice("CAD: channel free");
  } else if (scanState == RADIOLIB_PREAMBLE_DETECTED) {
    Serial.println(F("[LoRa] CAD: LoRa preamble detected"));
    setUiNotice("CAD: preamble found");
  } else {
    printRadioError("CAD", scanState);
    setUiNotice("CAD failed");
  }

  state = startConfiguredReceive();
  if (state != RADIOLIB_ERR_NONE) {
    printRadioError("restart receive after CAD", state);
  }
}

String takeToken(String& input) {
  input.trim();
  const int separator = input.indexOf(' ');
  if (separator < 0) {
    const String token = input;
    input = "";
    return token;
  }

  const String token = input.substring(0, separator);
  input = input.substring(separator + 1);
  input.trim();
  return token;
}

String takeDelimitedToken(String& input, char delimiter) {
  const int separator = input.indexOf(delimiter);
  if (separator < 0) {
    const String token = input;
    input = "";
    return token;
  }

  const String token = input.substring(0, separator);
  input = input.substring(separator + 1);
  return token;
}

bool parseToggle(const String& value, bool& enabled) {
  if (value == "on" || value == "1" || value == "true") {
    enabled = true;
    return true;
  }
  if (value == "off" || value == "0" || value == "false") {
    enabled = false;
    return true;
  }
  return false;
}

int findLoRaProfile(const String& value) {
  for (size_t index = 0; index < LORA_PROFILE_COUNT; ++index) {
    if (value == LORA_PROFILES[index].name || value.toInt() == static_cast<int>(index + 1)) {
      return static_cast<int>(index);
    }
  }
  return -1;
}

bool reinitializeRadio(bool persistConfiguration = true) {
  const int state = initializeActiveRadio();
  if (state != RADIOLIB_ERR_NONE) {
    printRadioError("reconfigure", state);
    return false;
  }

  Serial.print(F("[Radio] using "));
  Serial.print(radioModeName());
  Serial.print(F(" mode and profile "));
  Serial.println(activeLoRaProfile().name);
  transientRadioConfigurationActive = !persistConfiguration;
  if (persistConfiguration) {
    savePersistentConfigurationIfChanged();
  }
  redrawDisplay();
  return true;
}

void printProfiles() {
  Serial.println(F("LoRa profiles:"));
  for (size_t index = 0; index < LORA_PROFILE_COUNT; ++index) {
    const LoRaProfile& profile = LORA_PROFILES[index];
    Serial.print(index + 1);
    Serial.print(F(": "));
    Serial.print(profile.name);
    Serial.print(F(" - "));
    Serial.print(profile.frequencyMHz, 3);
    Serial.print(F(" MHz, BW "));
    Serial.print(profile.bandwidthKHz, 1);
    Serial.print(F(" kHz, SF"));
    Serial.print(profile.spreadingFactor);
    Serial.print(F(", CR 4/"));
    Serial.print(profile.codingRate);
    Serial.print(F(", "));
    Serial.print(profile.txPowerDbm);
    Serial.println(F(" dBm"));
  }
}

void printPacketOptions() {
  Serial.println(F("Packet options:"));
  Serial.print(F("  CRC:        "));
  Serial.println(packetCrcEnabled ? "on" : "off");
  Serial.print(F("  Header:     "));
  if (implicitPacketLength == 0) {
    Serial.println(F("explicit"));
  } else {
    Serial.print(F("implicit, "));
    Serial.print(implicitPacketLength);
    Serial.println(F(" bytes"));
  }
  Serial.print(F("  IQ:         "));
  Serial.println(invertedIqEnabled ? "inverted" : "normal");
  Serial.print(F("  LDRO:       "));
  Serial.println(forcedLdroState < 0 ? "automatic" : (forcedLdroState ? "forced on" : "forced off"));
  Serial.print(F("  FSK white:  "));
  Serial.println(fskWhiteningEnabled ? "on" : "off");
}

void printRadioDiagnostics() {
  if (!radioReady) {
    Serial.println(F("[Diagnostics] radio is not ready"));
    return;
  }

  Serial.println(F("metric,value"));
  Serial.print(F("mode,"));
  Serial.println(radioModeName());
  Serial.print(F("chip_status,0x"));
  Serial.println(radio.getStatus(), HEX);
  Serial.print(F("packet_status,0x"));
  Serial.println(radio.getPacketStatus(), HEX);
  Serial.print(F("device_errors,0x"));
  Serial.println(radio.getDeviceErrors(), HEX);
  Serial.print(F("irq_flags,0x"));
  Serial.println(radio.getIrqFlags(), HEX);
  Serial.print(F("instant_rssi_dbm,"));
  Serial.println(radio.getRSSI(false), 1);
  Serial.print(F("last_packet_rssi_dbm,"));
  Serial.println(lastReceivedRssi, 1);
  Serial.print(F("last_packet_snr_db,"));
  Serial.println(lastReceivedSnr, 1);
  if (radioMode == RadioMode::LORA) {
    Serial.print(F("frequency_error_hz,"));
    Serial.println(radio.getFrequencyError(), 1);
  }
  Serial.print(F("tx_packets,"));
  Serial.println(static_cast<unsigned long>(totalPacketsSent));
  Serial.print(F("rx_packets,"));
  Serial.println(static_cast<unsigned long>(totalPacketsReceived));
  Serial.print(F("tx_airtime_us,"));
  Serial.println(static_cast<unsigned long>(totalTransmitAirtimeUs));
  Serial.print(F("access_deferrals,"));
  Serial.println(accessDeferrals);
  Serial.print(F("cad_busy,"));
  Serial.println(cadBusyDetections);
  Serial.print(F("free_heap,"));
  Serial.println(ESP.getFreeHeap());
}

void runChannelSurvey(float startMHz, float endMHz, float stepKHz, uint8_t samples) {
  if (!radioReady || radioMode != RadioMode::LORA || startMHz < 150.0f || endMHz > 960.0f || endMHz < startMHz || stepKHz <= 0.0f || samples == 0) {
    Serial.println(F("[Survey] invalid range, samples, or radio mode"));
    return;
  }

  const uint32_t channelCount = static_cast<uint32_t>(((endMHz - startMHz) * 1000.0f) / stepKHz) + 1UL;
  if (channelCount > 200) {
    Serial.println(F("[Survey] limit the scan to 200 channels"));
    return;
  }

  Serial.println(F("frequency_mhz,average_rssi_dbm,lora_detections,samples"));
  for (uint32_t channel = 0; channel < channelCount; ++channel) {
    const float frequencyMHz = startMHz + channel * stepKHz / 1000.0f;
    int state = radio.standby();
    if (state == RADIOLIB_ERR_NONE) {
      state = radio.setFrequency(frequencyMHz);
    }
    if (state != RADIOLIB_ERR_NONE) {
      printRadioError("survey setFrequency", state);
      break;
    }

    float totalRssi = 0.0f;
    uint8_t detections = 0;
    for (uint8_t sample = 0; sample < samples; ++sample) {
      state = radio.startReceive();
      if (state != RADIOLIB_ERR_NONE) {
        printRadioError("survey receive", state);
        break;
      }
      delay(10);
      totalRssi += radio.getRSSI(false);
      radio.standby();
      const int scanState = radio.scanChannel();
      if (scanState == RADIOLIB_PREAMBLE_DETECTED) {
        ++detections;
      } else if (scanState != RADIOLIB_CHANNEL_FREE) {
        printRadioError("survey CAD", scanState);
      }
    }

    Serial.print(frequencyMHz, 3);
    Serial.print(',');
    Serial.print(totalRssi / samples, 1);
    Serial.print(',');
    Serial.print(detections);
    Serial.print(',');
    Serial.println(samples);
  }

  radio.setFrequency(activeLoRaProfile().frequencyMHz);
  const int receiveState = startConfiguredReceive();
  if (receiveState != RADIOLIB_ERR_NONE) {
    printRadioError("restart receive after survey", receiveState);
  }
}

void enterTimedDeepSleep(uint32_t seconds) {
  if (seconds == 0) {
    Serial.println(F("[Sleep] duration must be greater than zero"));
    return;
  }

  Serial.print(F("[Sleep] deep sleeping for "));
  Serial.print(seconds);
  Serial.println(F(" seconds; the sketch restarts on wake"));
  Serial.flush();
  if (remoteHttpReady) {
    remoteApiServer.stop();
    remoteHttpReady = false;
  }
  WiFi.softAPdisconnect(true);
  if (remoteBleReady) {
    BLEDevice::deinit(false);
    remoteBleReady = false;
  }
  radio.sleep(false);
  if (displayReady) {
    nessoDisplay.fillScreen(COLOR_BLACK);
  }
  esp_sleep_enable_timer_wakeup(static_cast<uint64_t>(seconds) * 1000000ULL);
  esp_deep_sleep_start();
}

PeerInfo* findPeer(const String& id) {
  for (PeerInfo& peer : peers) {
    if (peer.id == id) {
      return &peer;
    }
  }
  return nullptr;
}

// Peer records provide per-node duplicate detection and link statistics while
// preserving the original single selected-peer behavior.
PeerInfo* rememberPeer(const String& id, float rssi, float snr) {
  PeerInfo* peer = findPeer(id);
  if (peer == nullptr) {
    PeerInfo* oldest = &peers[0];
    for (PeerInfo& candidate : peers) {
      if (candidate.id.length() == 0) {
        oldest = &candidate;
        break;
      }
      if (candidate.lastSeenAt < oldest->lastSeenAt) {
        oldest = &candidate;
      }
    }
    *oldest = PeerInfo();
    oldest->id = id;
    peer = oldest;
  }

  peer->lastSeenAt = millis();
  peer->lastRssi = rssi;
  peer->lastSnr = snr;
  ++peer->packetsReceived;
  if (!peerSelectionLocked) {
    peerId = id;
  }
  return peer;
}

void printPeers() {
  Serial.println(F("peer,last_seen_ms,rssi_dbm,snr_db,packets,duplicates,selected"));
  const uint32_t now = millis();
  for (const PeerInfo& peer : peers) {
    if (peer.id.length() == 0) {
      continue;
    }
    Serial.print(peer.id);
    Serial.print(',');
    Serial.print(now - peer.lastSeenAt);
    Serial.print(',');
    Serial.print(peer.lastRssi, 1);
    Serial.print(',');
    Serial.print(peer.lastSnr, 1);
    Serial.print(',');
    Serial.print(peer.packetsReceived);
    Serial.print(',');
    Serial.print(peer.duplicatePackets);
    Serial.print(',');
    Serial.println(peer.id == peerId ? "yes" : "no");
  }
}

void printBenchmarkReport(const char* reason) {
  const uint32_t elapsedMs = max<uint32_t>(millis() - benchmark.startedAt, 1UL);
  const float deliveryPercent = benchmark.requestedPackets == 0 ? 0.0f : 100.0f * benchmark.receivedReplies / benchmark.requestedPackets;
  const float averageRttMs = benchmark.receivedReplies == 0 ? 0.0f : static_cast<float>(benchmark.totalRttMs) / benchmark.receivedReplies;
  const float averageRssi = benchmark.receivedReplies == 0 ? 0.0f : benchmark.totalRssi / benchmark.receivedReplies;
  const float averageSnr = benchmark.receivedReplies == 0 ? 0.0f : benchmark.totalSnr / benchmark.receivedReplies;
  const float throughputBps = static_cast<float>(benchmark.receivedReplies * benchmark.payloadLength * 8ULL * 1000ULL) / elapsedMs;
  const uint64_t benchmarkAirtimeUs = totalTransmitAirtimeUs - benchmark.startingAirtimeUs;

  if (strcmp(reason, "running") != 0 && strcmp(reason, "idle") != 0) {
    benchmarkResultAvailable = true;
    benchmarkResultReason = reason;
    benchmarkResultMode = radioMode;
    benchmarkResultProfileIndex = activeProfileIndex;
    benchmarkResultElapsedMs = elapsedMs;
    benchmarkResultAirtimeUs = benchmarkAirtimeUs;
  }

  Serial.println(F("benchmark_reason,mode,profile,session,requested,attempts,send_failures,sent,replies,delivery_percent,min_rtt_ms,avg_rtt_ms,max_rtt_ms,avg_rssi_dbm,avg_snr_db,throughput_bps,airtime_ms"));
  Serial.print(reason);
  Serial.print(',');
  Serial.print(radioModeName());
  Serial.print(',');
  Serial.print(activeLoRaProfile().name);
  Serial.print(',');
  Serial.print(benchmark.session);
  Serial.print(',');
  Serial.print(benchmark.requestedPackets);
  Serial.print(',');
  Serial.print(benchmark.transmitAttempts);
  Serial.print(',');
  Serial.print(benchmark.transmitFailures);
  Serial.print(',');
  Serial.print(benchmark.sentPackets);
  Serial.print(',');
  Serial.print(benchmark.receivedReplies);
  Serial.print(',');
  Serial.print(deliveryPercent, 1);
  Serial.print(',');
  Serial.print(benchmark.receivedReplies == 0 ? 0 : benchmark.minimumRttMs);
  Serial.print(',');
  Serial.print(averageRttMs, 1);
  Serial.print(',');
  Serial.print(benchmark.maximumRttMs);
  Serial.print(',');
  Serial.print(averageRssi, 1);
  Serial.print(',');
  Serial.print(averageSnr, 1);
  Serial.print(',');
  Serial.print(throughputBps, 1);
  Serial.print(',');
  Serial.println(static_cast<double>(benchmarkAirtimeUs) / 1000.0, 3);
}

// Benchmarks use echoed timestamps rather than synchronized clocks, allowing
// RTT and delivery measurements between any two independently booted boards.
void startBenchmark(uint16_t packetCount, size_t payloadLength, bool startedBySweep) {
  if (packetCount == 0 || packetCount > BENCHMARK_MAX_PACKETS || payloadLength < 24 || payloadLength > BENCHMARK_MAX_PAYLOAD) {
    Serial.println(F("Usage: benchmark <1-1000 packets> [24-120 bytes]"));
    return;
  }
  if ((!startedBySweep && profileSweep.active) || outgoingTransfer.active || pendingRadioConfiguration.active) {
    Serial.println(F("[Benchmark] another long-running exercise is active"));
    return;
  }

  benchmark = BenchmarkState();
  benchmarkResultAvailable = false;
  benchmark.active = true;
  benchmark.session = esp_random();
  benchmark.destination = targetPeerOrBroadcast();
  benchmark.requestedPackets = packetCount;
  benchmark.payloadLength = payloadLength;
  benchmark.startedAt = millis();
  benchmark.startingAirtimeUs = totalTransmitAirtimeUs;
  Serial.print(F("[Benchmark] session "));
  Serial.print(benchmark.session);
  Serial.print(F(", target "));
  Serial.print(benchmark.destination);
  Serial.print(F(", packets "));
  Serial.print(packetCount);
  Serial.print(F(", payload "));
  Serial.println(payloadLength);
  redrawDisplay();
}

void runBenchmarkTask() {
  if (!benchmark.active) {
    return;
  }

  const uint32_t now = millis();
  if (benchmark.sentPackets < benchmark.requestedPackets && (benchmark.lastSendAt == 0 || static_cast<uint32_t>(now - benchmark.lastSendAt) >= TX_MIN_INTERVAL_MS + 100UL)) {
    String body = String(benchmark.session) + ',' + String(benchmark.sentPackets) + ',' + String(now) + ',';
    while (body.length() < benchmark.payloadLength) {
      body += static_cast<char>('A' + benchmark.sentPackets % 26);
    }
    benchmark.lastSendAt = now;
    ++benchmark.transmitAttempts;
    if (sendPacket('B', benchmark.destination, body, txSequence++)) {
      ++benchmark.sentPackets;
      if (benchmark.sentPackets == benchmark.requestedPackets) {
        benchmark.allSentAt = millis();
      }
    } else {
      ++benchmark.transmitFailures;
    }
  }

  if (benchmark.receivedReplies >= benchmark.requestedPackets) {
    printBenchmarkReport("complete");
    benchmark.active = false;
    if (!profileSweep.active && uiPage == UiPage::TEST) {
      uiPage = UiPage::RESULTS;
    }
    redrawDisplay();
  } else if (benchmark.allSentAt != 0 && timeReached(now, benchmark.allSentAt + BENCHMARK_REPLY_TIMEOUT_MS)) {
    printBenchmarkReport("timeout");
    benchmark.active = false;
    if (!profileSweep.active && uiPage == UiPage::TEST) {
      uiPage = UiPage::RESULTS;
    }
    redrawDisplay();
  }
}

void scheduleRadioConfiguration(RadioMode mode, uint8_t profileIndex, uint32_t delayMs, bool persist = true) {
  if (profileIndex >= LORA_PROFILE_COUNT) {
    return;
  }
  pendingRadioConfiguration.active = true;
  pendingRadioConfiguration.mode = mode;
  pendingRadioConfiguration.profileIndex = profileIndex;
  pendingRadioConfiguration.applyAt = millis() + delayMs;
  pendingRadioConfiguration.persist = persist;
  Serial.print(F("[Radio] scheduled "));
  Serial.print(mode == RadioMode::LORA ? "LoRa" : "GFSK");
  Serial.print(F(" / "));
  Serial.print(LORA_PROFILES[profileIndex].name);
  Serial.print(F(" in "));
  Serial.print(delayMs);
  Serial.println(F(" ms"));
  redrawDisplay();
}

bool requestRadioConfiguration(RadioMode mode, uint8_t profileIndex, uint32_t delayMs = 3000, bool persist = true) {
  if (profileIndex >= LORA_PROFILE_COUNT) {
    return false;
  }

  String body = mode == RadioMode::LORA ? "L," : "F,";
  body += String(profileIndex);
  body += ',';
  body += String(delayMs);
  body += ',';
  body += persist ? '1' : '0';
  if (!sendPacket('M', targetPeerOrBroadcast(), body, txSequence++)) {
    return false;
  }
  scheduleRadioConfiguration(mode, profileIndex, delayMs, persist);
  return true;
}

void runPendingRadioConfigurationTask() {
  if (!pendingRadioConfiguration.active || !timeReached(millis(), pendingRadioConfiguration.applyAt)) {
    return;
  }

  radioMode = pendingRadioConfiguration.mode;
  activeProfileIndex = pendingRadioConfiguration.profileIndex;
  const bool persist = pendingRadioConfiguration.persist;
  pendingRadioConfiguration.active = false;
  reinitializeRadio(persist);
}

void scheduleSlottedAccess(uint32_t delayMs) {
  slotEpochAt = millis() + delayMs;
  slottedAccessEnabled = true;
  Serial.print(F("[Slots] synchronized epoch in "));
  Serial.print(delayMs);
  Serial.println(F(" ms"));
}

void synchronizeSlottedAccess() {
  slottedAccessEnabled = false;
  constexpr uint32_t delayMs = 3000;
  if (sendPacket('Q', "*", String(delayMs), txSequence++)) {
    scheduleSlottedAccess(delayMs);
  }
}

void startProfileSweep(uint16_t packetCount, size_t payloadLength) {
  if (packetCount == 0 || packetCount > BENCHMARK_MAX_PACKETS || payloadLength < 24 || payloadLength > BENCHMARK_MAX_PAYLOAD) {
    Serial.println(F("Usage: sweep <1-1000 packets> [24-120 bytes]"));
    return;
  }
  if (benchmark.active || pendingRadioConfiguration.active) {
    Serial.println(F("[Sweep] wait for the active benchmark or radio change"));
    return;
  }

  profileSweep.active = true;
  profileSweep.phase = SweepPhase::WAITING_FOR_PROFILE;
  profileSweep.profileIndex = 0;
  profileSweep.packetCount = packetCount;
  profileSweep.payloadLength = payloadLength;
  if (!requestRadioConfiguration(RadioMode::LORA, profileSweep.profileIndex, 3000, false)) {
    profileSweep = ProfileSweepState();
    return;
  }
  profileSweep.nextActionAt = millis() + 4500UL;
  Serial.println(F("[Sweep] started"));
  redrawDisplay();
}

void stopProfileSweep(const char* reason) {
  benchmark.active = false;
  profileSweep = ProfileSweepState();
  Serial.print(F("[Sweep] "));
  Serial.println(reason);
  redrawDisplay();
}

void runProfileSweepTask() {
  if (!profileSweep.active) {
    return;
  }

  if (profileSweep.phase == SweepPhase::WAITING_TO_REQUEST_PROFILE) {
    if (!timeReached(millis(), profileSweep.nextActionAt)) {
      return;
    }
    if (!requestRadioConfiguration(RadioMode::LORA, profileSweep.profileIndex, 3000, false)) {
      profileSweep.nextActionAt = millis() + TX_MIN_INTERVAL_MS;
      return;
    }
    if (profileSweep.returningToDefault) {
      profileSweep = ProfileSweepState();
      Serial.println(F("[Sweep] complete; returning to default profile"));
      if (uiPage == UiPage::TEST) {
        uiPage = UiPage::RESULTS;
      }
      redrawDisplay();
      return;
    }
    profileSweep.phase = SweepPhase::WAITING_FOR_PROFILE;
    profileSweep.nextActionAt = millis() + 4500UL;
    return;
  }

  if (profileSweep.phase == SweepPhase::WAITING_FOR_PROFILE) {
    if (!pendingRadioConfiguration.active && timeReached(millis(), profileSweep.nextActionAt)) {
      Serial.print(F("[Sweep] benchmarking profile "));
      Serial.println(activeLoRaProfile().name);
      startBenchmark(profileSweep.packetCount, profileSweep.payloadLength, true);
      if (benchmark.active) {
        profileSweep.phase = SweepPhase::RUNNING_BENCHMARK;
      } else {
        stopProfileSweep("stopped because benchmark could not start");
      }
    }
    return;
  }

  if (profileSweep.phase == SweepPhase::RUNNING_BENCHMARK && !benchmark.active) {
    ++profileSweep.profileIndex;
    if (profileSweep.profileIndex >= LORA_PROFILE_COUNT) {
      profileSweep.profileIndex = 0;
      profileSweep.returningToDefault = true;
      profileSweep.phase = SweepPhase::WAITING_TO_REQUEST_PROFILE;
      profileSweep.nextActionAt = millis() + TX_MIN_INTERVAL_MS;
      return;
    }

    profileSweep.phase = SweepPhase::WAITING_TO_REQUEST_PROFILE;
    profileSweep.nextActionAt = millis() + TX_MIN_INTERVAL_MS;
  }
}

uint16_t crc16(const String& data) {
  uint16_t crc = 0xFFFF;
  for (size_t index = 0; index < data.length(); ++index) {
    crc ^= static_cast<uint16_t>(static_cast<uint8_t>(data.charAt(index))) << 8;
    for (uint8_t bit = 0; bit < 8; ++bit) {
      crc = (crc & 0x8000) ? static_cast<uint16_t>((crc << 1) ^ 0x1021) : static_cast<uint16_t>(crc << 1);
    }
  }
  return crc;
}

// Transfers send only missing fragments after each cumulative bitmap ACK. The
// complete-message CRC catches reassembly errors not represented by the mask.
uint16_t completeFragmentMask(uint8_t fragmentCount) {
  return fragmentCount >= MAX_FRAGMENTS ? 0xFFFFU : static_cast<uint16_t>((1UL << fragmentCount) - 1UL);
}

void printTransferStatus() {
  Serial.print(F("Outgoing transfer: "));
  if (!outgoingTransfer.active && outgoingTransfer.data.length() == 0) {
    Serial.println(F("idle"));
  } else {
    Serial.print(outgoingTransfer.id);
    Serial.print(F(", fragments "));
    Serial.print(outgoingTransfer.fragmentCount);
    Serial.print(F(", acknowledged 0x"));
    Serial.print(outgoingTransfer.acknowledgedMask, HEX);
    Serial.print(F(", round "));
    Serial.print(outgoingTransfer.attempts);
    Serial.print(F(", state "));
    Serial.println(outgoingTransfer.active ? "active" : "paused");
  }

  Serial.print(F("Incoming transfer: "));
  if (!incomingTransfer.active) {
    Serial.println(F("idle"));
  } else {
    Serial.print(incomingTransfer.id);
    Serial.print(F(" from "));
    Serial.print(incomingTransfer.source);
    Serial.print(F(", received 0x"));
    Serial.println(incomingTransfer.receivedMask, HEX);
  }
}

void startTransfer(const String& data) {
  if (data.length() == 0 || data.length() > MAX_TRANSFER_LENGTH) {
    Serial.print(F("[Transfer] length must be 1 to "));
    Serial.print(MAX_TRANSFER_LENGTH);
    Serial.println(F(" bytes"));
    return;
  }
  if (outgoingTransfer.active) {
    Serial.println(F("[Transfer] another outgoing transfer is active"));
    return;
  }
  if (benchmark.active || profileSweep.active || pendingRadioConfiguration.active) {
    Serial.println(F("[Transfer] another long-running exercise is active"));
    return;
  }

  outgoingTransfer = OutgoingTransfer();
  outgoingTransfer.active = true;
  outgoingTransfer.id = esp_random();
  outgoingTransfer.destination = targetPeerOrBroadcast();
  outgoingTransfer.data = data;
  outgoingTransfer.checksum = crc16(data);
  outgoingTransfer.fragmentCount = static_cast<uint8_t>((data.length() + FRAGMENT_DATA_LENGTH - 1) / FRAGMENT_DATA_LENGTH);
  outgoingTransfer.attempts = 1;
  Serial.print(F("[Transfer] queued id "));
  Serial.print(outgoingTransfer.id);
  Serial.print(F(", bytes "));
  Serial.print(data.length());
  Serial.print(F(", fragments "));
  Serial.println(outgoingTransfer.fragmentCount);
}

void sendTransferAcknowledgement(const IncomingTransfer& transfer) {
  String body = String(transfer.id) + ',' + String(transfer.receivedMask);
  sendPacketOrDefer('K', transfer.source, body, txSequence++);
}

void runOutgoingTransferTask() {
  if (!outgoingTransfer.active) {
    return;
  }

  const uint16_t fullMask = completeFragmentMask(outgoingTransfer.fragmentCount);
  if (outgoingTransfer.acknowledgedMask == fullMask) {
    Serial.print(F("[Transfer] complete id "));
    Serial.println(outgoingTransfer.id);
    outgoingTransfer = OutgoingTransfer();
    return;
  }

  const uint32_t now = millis();
  if (outgoingTransfer.waitingForAcknowledgement) {
    if (!timeReached(now, outgoingTransfer.lastSendAt + FRAGMENT_RETRY_MS)) {
      return;
    }
    if (outgoingTransfer.attempts >= MAX_FRAGMENT_ATTEMPTS) {
      Serial.println(F("[Transfer] paused after retry limit; use transfer resume or cancel"));
      outgoingTransfer.active = false;
      return;
    }
    ++outgoingTransfer.attempts;
    outgoingTransfer.nextFragmentIndex = 0;
    outgoingTransfer.waitingForAcknowledgement = false;
  }

  if (outgoingTransfer.lastSendAt != 0 && static_cast<uint32_t>(now - outgoingTransfer.lastSendAt) < TX_MIN_INTERVAL_MS + 100UL) {
    return;
  }

  while (outgoingTransfer.nextFragmentIndex < outgoingTransfer.fragmentCount && (outgoingTransfer.acknowledgedMask & (1U << outgoingTransfer.nextFragmentIndex))) {
    ++outgoingTransfer.nextFragmentIndex;
  }
  if (outgoingTransfer.nextFragmentIndex >= outgoingTransfer.fragmentCount) {
    outgoingTransfer.waitingForAcknowledgement = true;
    outgoingTransfer.lastSendAt = now;
    return;
  }

  const uint8_t fragmentIndex = outgoingTransfer.nextFragmentIndex;
  const size_t offset = fragmentIndex * FRAGMENT_DATA_LENGTH;
  String body = String(outgoingTransfer.id) + ',' + String(fragmentIndex) + ',' + String(outgoingTransfer.fragmentCount) + ',' + String(outgoingTransfer.checksum) + ',' + outgoingTransfer.data.substring(offset, offset + FRAGMENT_DATA_LENGTH);
  if (sendPacket('F', outgoingTransfer.destination, body, txSequence++)) {
    ++outgoingTransfer.nextFragmentIndex;
    outgoingTransfer.lastSendAt = millis();
  }
}

void handleIncomingFragment(const ReceivedPacket& packet) {
  String fields = packet.body;
  const uint32_t transferId = static_cast<uint32_t>(strtoul(takeDelimitedToken(fields, ',').c_str(), nullptr, 10));
  const uint8_t fragmentIndex = static_cast<uint8_t>(takeDelimitedToken(fields, ',').toInt());
  const uint8_t fragmentCount = static_cast<uint8_t>(takeDelimitedToken(fields, ',').toInt());
  const uint16_t checksum = static_cast<uint16_t>(takeDelimitedToken(fields, ',').toInt());
  if (transferId == 0 || fragmentCount == 0 || fragmentCount > MAX_FRAGMENTS || fragmentIndex >= fragmentCount || fields.length() > FRAGMENT_DATA_LENGTH) {
    Serial.println(F("[Transfer] ignored invalid fragment"));
    return;
  }

  const bool currentTransferComplete = incomingTransfer.active && incomingTransfer.receivedMask == completeFragmentMask(incomingTransfer.fragmentCount);
  const bool currentTransferExpired = incomingTransfer.active && timeReached(millis(), incomingTransfer.lastUpdateAt + INCOMING_TRANSFER_TIMEOUT_MS);
  if (currentTransferComplete && incomingTransfer.id == transferId && incomingTransfer.source == packet.source) {
    sendTransferAcknowledgement(incomingTransfer);
    return;
  }
  if (incomingTransfer.active && !currentTransferComplete && !currentTransferExpired && (incomingTransfer.id != transferId || incomingTransfer.source != packet.source)) {
    Serial.println(F("[Transfer] busy receiving another transfer"));
    return;
  }
  if (!incomingTransfer.active || currentTransferComplete || currentTransferExpired || incomingTransfer.id != transferId || incomingTransfer.source != packet.source) {
    incomingTransfer = IncomingTransfer();
    incomingTransfer.active = true;
    incomingTransfer.id = transferId;
    incomingTransfer.source = packet.source;
    incomingTransfer.fragmentCount = fragmentCount;
    incomingTransfer.checksum = checksum;
  }
  if (incomingTransfer.fragmentCount != fragmentCount || incomingTransfer.checksum != checksum) {
    Serial.println(F("[Transfer] ignored inconsistent fragment metadata"));
    return;
  }

  incomingTransfer.fragments[fragmentIndex] = fields;
  incomingTransfer.receivedMask |= static_cast<uint16_t>(1U << fragmentIndex);
  incomingTransfer.lastUpdateAt = millis();
  sendTransferAcknowledgement(incomingTransfer);

  if (incomingTransfer.receivedMask != completeFragmentMask(incomingTransfer.fragmentCount)) {
    return;
  }

  String reassembled;
  reassembled.reserve(incomingTransfer.fragmentCount * FRAGMENT_DATA_LENGTH);
  for (uint8_t index = 0; index < incomingTransfer.fragmentCount; ++index) {
    reassembled += incomingTransfer.fragments[index];
  }
  if (crc16(reassembled) != incomingTransfer.checksum) {
    Serial.println(F("[Transfer] checksum mismatch; waiting for retransmission"));
    incomingTransfer.receivedMask = 0;
    return;
  }

  lastReceivedText = clipped(reassembled, MAX_BODY_LENGTH);
  Serial.print(F("[Transfer] complete from "));
  Serial.print(packet.source);
  Serial.print(F(", bytes "));
  Serial.print(reassembled.length());
  Serial.print(F(": "));
  Serial.println(reassembled);
  redrawDisplay();
}

void handleTransferAcknowledgement(const ReceivedPacket& packet) {
  String fields = packet.body;
  const uint32_t transferId = static_cast<uint32_t>(strtoul(takeDelimitedToken(fields, ',').c_str(), nullptr, 10));
  const uint16_t receivedMask = static_cast<uint16_t>(strtoul(takeDelimitedToken(fields, ',').c_str(), nullptr, 10));
  if (!outgoingTransfer.active || transferId != outgoingTransfer.id || (outgoingTransfer.destination != "*" && outgoingTransfer.destination != packet.source)) {
    return;
  }

  outgoingTransfer.destination = packet.source;
  outgoingTransfer.acknowledgedMask |= receivedMask & completeFragmentMask(outgoingTransfer.fragmentCount);
  outgoingTransfer.nextFragmentIndex = 0;
  outgoingTransfer.waitingForAcknowledgement = false;
  outgoingTransfer.lastSendAt = millis();
}

bool relayMessageSeen(const String& origin, uint32_t messageId) {
  for (const RelayRecord& record : relayHistory) {
    if (record.origin == origin && record.messageId == messageId && !timeReached(millis(), record.seenAt + RELAY_RECORD_TIMEOUT_MS)) {
      return true;
    }
  }
  return false;
}

// Relaying is intentionally bounded by both a hop count and a recently-seen
// cache so broadcast topologies cannot forward the same message indefinitely.
void rememberRelayMessage(const String& origin, uint32_t messageId) {
  relayHistory[relayHistoryNext].origin = origin;
  relayHistory[relayHistoryNext].messageId = messageId;
  relayHistory[relayHistoryNext].seenAt = millis();
  relayHistoryNext = (relayHistoryNext + 1) % MAX_RELAY_HISTORY;
}

void sendRelayedText(const String& finalDestination, uint8_t hops, const String& text) {
  if (finalDestination.length() == 0 || hops == 0 || hops > 8 || text.length() == 0) {
    Serial.println(F("Usage: relay send <node-id|*> <1-8 hops> <text>"));
    return;
  }

  const uint32_t messageId = esp_random();
  String body = String(messageId) + ',' + nodeId + ',' + finalDestination + ',' + String(hops) + ',' + clipped(text, 90);
  rememberRelayMessage(nodeId, messageId);
  if (sendPacket('L', "*", body, txSequence++)) {
    Serial.print(F("[Relay] sent message "));
    Serial.print(messageId);
    Serial.print(F(" toward "));
    Serial.println(finalDestination);
  }
}

void handleRelayedText(const ReceivedPacket& packet) {
  String fields = packet.body;
  const uint32_t messageId = static_cast<uint32_t>(strtoul(takeDelimitedToken(fields, ',').c_str(), nullptr, 10));
  const String origin = takeDelimitedToken(fields, ',');
  const String finalDestination = takeDelimitedToken(fields, ',');
  const uint8_t hopsRemaining = static_cast<uint8_t>(takeDelimitedToken(fields, ',').toInt());
  if (messageId == 0 || origin.length() == 0 || finalDestination.length() == 0 || hopsRemaining > 8 || fields.length() == 0) {
    Serial.println(F("[Relay] ignored malformed message"));
    return;
  }
  if (origin == nodeId || relayMessageSeen(origin, messageId)) {
    return;
  }

  rememberRelayMessage(origin, messageId);
  if (finalDestination == nodeId || finalDestination == "*") {
    lastReceivedText = fields;
    Serial.print(F("[Relay] text from "));
    Serial.print(origin);
    Serial.print(F(" via "));
    Serial.print(packet.source);
    Serial.print(F(": "));
    Serial.println(fields);
    redrawDisplay();
  }

  if (!relayEnabled || hopsRemaining <= 1 || finalDestination == nodeId) {
    return;
  }

  String forwardedBody = String(messageId) + ',' + origin + ',' + finalDestination + ',' + String(hopsRemaining - 1) + ',' + fields;
  if (sendPacketOrDefer('L', "*", forwardedBody, txSequence++)) {
    Serial.print(F("[Relay] forwarded message "));
    Serial.print(messageId);
    Serial.print(F(", hops remaining "));
    Serial.println(hopsRemaining - 1);
  }
}

void handleReceivedPacket(const String& raw, float rssi, float snr) {
  // All application packet types share parsing, addressing, peer accounting,
  // and per-peer duplicate detection before reaching this dispatcher.
  ReceivedPacket packet;
  if (!parsePacket(raw, packet)) {
    Serial.print(F("[LoRa] ignored malformed packet: "));
    Serial.println(raw);
    return;
  }
  if (packet.source == nodeId || !isForThisNode(packet)) {
    return;
  }

  PeerInfo* peer = rememberPeer(packet.source, rssi, snr);
  lastReceivedRssi = rssi;
  lastReceivedSnr = snr;

  const bool duplicate = peer != nullptr && peer->haveLastSequence && peer->lastSequence == packet.sequence;
  if (duplicate && peer != nullptr) {
    ++peer->duplicatePackets;
  }
  if (!duplicate) {
    if (peer != nullptr) {
      peer->lastSequence = packet.sequence;
      peer->haveLastSequence = true;
    }
    lastPacketSource = packet.source;
    lastPacketSequence = packet.sequence;
    haveLastPacket = true;
  }

  switch (packet.type) {
    case 'H':
      Serial.print(F("[LoRa] discovered peer "));
      Serial.println(packet.source);
      break;

    case 'T':
      if (!duplicate) {
        lastReceivedText = packet.body;
        Serial.print(F("[LoRa] text from "));
        Serial.print(packet.source);
        Serial.print(F(" [RSSI "));
        Serial.print(rssi, 1);
        Serial.print(F(" dBm, SNR "));
        Serial.print(snr, 1);
        Serial.print(F(" dB]: "));
        Serial.println(packet.body);
        redrawDisplay();
      }
      // ACK duplicates as well so a sender can recover from a lost ACK.
      sendAcknowledgement(packet);
      break;

    case 'P':
      Serial.print(F("[LoRa] ping from "));
      Serial.println(packet.source);
      sendPong(packet);
      break;

    case 'R':
      Serial.print(F("[LoRa] reply from "));
      Serial.print(packet.source);
      Serial.print(F(": "));
      Serial.println(packet.body);
      break;

    case 'A': {
      String expected = "T:";
      expected += String(pendingTextSequence);
      const bool fromExpectedPeer = pendingTextDestination == "*"
        || pendingTextDestination == packet.source;
      if (pendingTextActive && fromExpectedPeer && packet.body == expected) {
        pendingTextActive = false;
        pendingTextDestination = "";
        pendingTextBody = "";
        Serial.print(F("[LoRa] text acknowledged by "));
        Serial.println(packet.source);
        redrawDisplay();
      }
      break;
    }

    case 'S':
      Serial.print(F("[LoRa] telemetry from "));
      Serial.print(packet.source);
      Serial.print(F(" [RSSI "));
      Serial.print(rssi, 1);
      Serial.print(F(" dBm, SNR "));
      Serial.print(snr, 1);
      Serial.print(F(" dB]: "));
      Serial.println(packet.body);
      break;

    case 'B':
      sendPacketOrDefer('b', packet.source, packet.body, txSequence++);
      break;

    case 'b': {
      String fields = packet.body;
      const uint32_t session = static_cast<uint32_t>(strtoul(takeDelimitedToken(fields, ',').c_str(), nullptr, 10));
      takeDelimitedToken(fields, ',');
      const uint32_t sentAt = static_cast<uint32_t>(strtoul(takeDelimitedToken(fields, ',').c_str(), nullptr, 10));
      if (benchmark.active && session == benchmark.session && !duplicate && (benchmark.destination == "*" || benchmark.destination == packet.source)) {
        if (benchmark.destination == "*") {
          benchmark.destination = packet.source;
          Serial.print(F("[Benchmark] bound to responder "));
          Serial.println(packet.source);
        }
        const uint32_t rttMs = millis() - sentAt;
        ++benchmark.receivedReplies;
        benchmark.minimumRttMs = min(benchmark.minimumRttMs, rttMs);
        benchmark.maximumRttMs = max(benchmark.maximumRttMs, rttMs);
        benchmark.totalRttMs += rttMs;
        benchmark.totalRssi += rssi;
        benchmark.totalSnr += snr;
      }
      break;
    }

    case 'M': {
      String fields = packet.body;
      const String mode = takeDelimitedToken(fields, ',');
      const uint8_t profileIndex = static_cast<uint8_t>(takeDelimitedToken(fields, ',').toInt());
      const uint32_t delayMs = static_cast<uint32_t>(strtoul(takeDelimitedToken(fields, ',').c_str(), nullptr, 10));
      const String persistText = takeDelimitedToken(fields, ',');
      const bool persist = persistText.length() == 0 || persistText == "1";
      if ((mode == "L" || mode == "F") && profileIndex < LORA_PROFILE_COUNT && delayMs >= 1000 && delayMs <= 30000) {
        scheduleRadioConfiguration(mode == "L" ? RadioMode::LORA : RadioMode::FSK, profileIndex, delayMs, persist);
      } else {
        Serial.println(F("[Radio] ignored invalid configuration packet"));
      }
      break;
    }

    case 'F':
      handleIncomingFragment(packet);
      break;

    case 'K':
      handleTransferAcknowledgement(packet);
      break;

    case 'L':
      handleRelayedText(packet);
      break;

    case 'Q': {
      const uint32_t delayMs = static_cast<uint32_t>(strtoul(packet.body.c_str(), nullptr, 10));
      if (delayMs >= 1000 && delayMs <= 30000) {
        scheduleSlottedAccess(delayMs);
      }
      break;
    }

    default:
      Serial.print(F("[LoRa] unknown packet type from "));
      Serial.println(packet.source);
      break;
  }

  redrawDisplay();
}

void pollRadio() {
  if (!radioReady || !packetReceivedFlag) {
    return;
  }

  packetReceivedFlag = false;
  String raw;
  const int state = radio.readData(raw);

  if (state == RADIOLIB_ERR_NONE) {
    raw.trim();
    const float rssi = radio.getRSSI();
    const float snr = radioMode == RadioMode::LORA ? radio.getSNR() : 0.0f;
    ++totalPacketsReceived;
    handleReceivedPacket(raw, rssi, snr);
  } else if (state == RADIOLIB_ERR_CRC_MISMATCH) {
    Serial.println(F("[LoRa] CRC mismatch"));
  } else {
    printRadioError("readData", state);
  }

  // This also covers a received packet for which an ACK was deferred by the
  // transmit interval guard.
  const int receiveState = startConfiguredReceive();
  if (receiveState != RADIOLIB_ERR_NONE) {
    printRadioError("restart receive after packet", receiveState);
  }
}

void runIncomingTransferMaintenance() {
  if (incomingTransfer.active && incomingTransfer.receivedMask != completeFragmentMask(incomingTransfer.fragmentCount) && timeReached(millis(), incomingTransfer.lastUpdateAt + INCOMING_TRANSFER_TIMEOUT_MS)) {
    Serial.println(F("[Transfer] incomplete incoming transfer expired"));
    incomingTransfer = IncomingTransfer();
  }
}

void retryPendingText() {
  if (!pendingTextActive || !timeReached(millis(), pendingTextLastAttempt + ACK_TIMEOUT_MS)) {
    return;
  }

  if (pendingTextAttempts >= MAX_TEXT_ATTEMPTS) {
    Serial.println(F("[LoRa] text acknowledgement timed out"));
    pendingTextActive = false;
    pendingTextDestination = "";
    pendingTextBody = "";
    redrawDisplay();
    return;
  }

  ++pendingTextAttempts;
  pendingTextLastAttempt = millis();
  Serial.print(F("[LoRa] retrying text, attempt "));
  Serial.println(pendingTextAttempts);
  sendPacket('T', pendingTextDestination, pendingTextBody, pendingTextSequence);
}

void handleCommand(String command) {
  // Keep this command surface synchronized with the opening comment and
  // README.md so the sketch remains self-documenting in the Arduino IDE.
  command.trim();
  if (command.length() == 0) {
    return;
  }

  if (command == "?" || command == "help") {
    Serial.println(F("Commands:"));
    Serial.println(F("  t <text>  send text"));
    Serial.println(F("  p         ping peer"));
    Serial.println(F("  h         send HELLO"));
    Serial.println(F("  s         show status and send telemetry"));
    Serial.println(F("  c         run channel-activity detection"));
    Serial.println(F("  mode lora|gfsk"));
    Serial.println(F("  mode sync lora|gfsk"));
    Serial.println(F("  profiles | profile <name|number>"));
    Serial.println(F("  profile sync <name|number>"));
    Serial.println(F("  options | crc on|off | iq normal|inverted"));
    Serial.println(F("  header explicit|implicit <bytes>"));
    Serial.println(F("  ldro auto|on|off | whitening on|off"));
    Serial.println(F("  rxgain power|boosted | lowpower on|off"));
    Serial.println(F("  cad auto on|off | duty off|<percent> | slots on|off|sync"));
    Serial.println(F("  survey [startMHz endMHz stepKHz samples]"));
    Serial.println(F("  diag | diag clear | diag calibrate"));
    Serial.println(F("  sleep <seconds>"));
    Serial.println(F("  peers | peer auto|<node-id>"));
    Serial.println(F("  benchmark <packets> [bytes] | benchmark stop|report"));
    Serial.println(F("  sweep <packets> [bytes] | sweep stop"));
    Serial.println(F("  transfer <text> | transfer status|resume|cancel"));
    Serial.println(F("  relay on|off | relay send <node-id|*> <hops> <text>"));
    Serial.println(F("  config | config defaults | config restart"));
    Serial.println(F("  wifi ssid <name> | wifi password <value|open> | wifi clear"));
    Serial.println(F("  wifi address dhcp|<a.a.a.a/s> | ble name <name>"));
    return;
  }

  if (command == "config") {
    printPersistentConfiguration();
  } else if (command == "config defaults") {
    if (benchmark.active || profileSweep.active || outgoingTransfer.active || pendingRadioConfiguration.active) {
      Serial.println(F("[Config] stop the active exercise before restoring defaults"));
    } else {
      const PersistentConfiguration defaults = defaultPersistentConfiguration();
      applyPersistentConfiguration(defaults);
      if (reinitializeRadio()) {
        Serial.println(F("[Config] defaults restored; restart to apply Wi-Fi and BLE defaults"));
      }
    }
  } else if (command == "config restart") {
    Serial.println(F("[Config] restarting"));
    Serial.flush();
    ESP.restart();
  } else if (command == "wifi clear") {
    remoteWifiSsid = "";
    remoteWifiPassword = "";
    remoteWifiStaticAddress = false;
    savePersistentConfigurationIfChanged();
    Serial.println(F("[Config] station credentials cleared; restart for fixed recovery AP"));
  } else if (command.startsWith("wifi ssid ")) {
    String value = command.substring(10);
    value.trim();
    if (!value.length() || value.length() > 32) {
      Serial.println(F("[Config] Wi-Fi SSID must be 1 to 32 bytes"));
    } else {
      remoteWifiSsid = value;
      savePersistentConfigurationIfChanged();
      Serial.println(F("[Config] Wi-Fi SSID stored; restart to connect"));
    }
  } else if (command.startsWith("wifi password ")) {
    String value = command.substring(14);
    if (value == "open") {
      value = "";
    }
    if (value.length() && (value.length() < 8 || value.length() > 63)) {
      Serial.println(F("[Config] Wi-Fi password must be 8 to 63 bytes, or open"));
    } else {
      remoteWifiPassword = value;
      savePersistentConfigurationIfChanged();
      Serial.println(F("[Config] Wi-Fi password stored; restart to connect"));
    }
  } else if (command == "wifi address dhcp") {
    remoteWifiStaticAddress = false;
    savePersistentConfigurationIfChanged();
    Serial.println(F("[Config] station DHCP stored; restart to apply"));
  } else if (command.startsWith("wifi address ")) {
    IPAddress address;
    uint8_t prefixLength = 0;
    if (!parseWifiCidr(command.substring(13), address, prefixLength)) {
      Serial.println(F("Usage: wifi address dhcp|<a.a.a.a/8-30>"));
    } else {
      remoteWifiAddress = address;
      remoteWifiPrefixLength = prefixLength;
      remoteWifiStaticAddress = true;
      savePersistentConfigurationIfChanged();
      Serial.println(F("[Config] static station address stored; restart to apply"));
    }
  } else if (command.startsWith("ble name ")) {
    String value = command.substring(9);
    value.trim();
    if (!value.length() || value.length() > 24) {
      Serial.println(F("[Config] BLE name must be 1 to 24 bytes"));
    } else {
      remoteBleDeviceName = value;
      savePersistentConfigurationIfChanged();
      Serial.println(F("[Config] BLE name stored; restart to apply"));
    }
  } else if (command.startsWith("t ")) {
    sendText(command.substring(2));
  } else if (command == "p") {
    sendPing();
  } else if (command == "h") {
    sendHello();
  } else if (command == "s") {
    printStatus();
    sendTelemetry();
  } else if (command == "c") {
    runChannelActivityDetection();
  } else if (command == "profiles") {
    printProfiles();
  } else if (command.startsWith("profile sync ")) {
    const int profileIndex = findLoRaProfile(command.substring(13));
    if (profileIndex < 0) {
      Serial.println(F("[Radio] unknown profile; use profiles to list choices"));
    } else {
      requestRadioConfiguration(radioMode, static_cast<uint8_t>(profileIndex));
    }
  } else if (command.startsWith("profile ")) {
    const int profileIndex = findLoRaProfile(command.substring(8));
    if (profileIndex < 0) {
      Serial.println(F("[Radio] unknown profile; use profiles to list choices"));
    } else {
      activeProfileIndex = static_cast<uint8_t>(profileIndex);
      reinitializeRadio();
    }
  } else if (command == "mode sync lora") {
    requestRadioConfiguration(RadioMode::LORA, activeProfileIndex);
  } else if (command == "mode sync gfsk") {
    requestRadioConfiguration(RadioMode::FSK, activeProfileIndex);
  } else if (command == "mode lora") {
    radioMode = RadioMode::LORA;
    reinitializeRadio();
  } else if (command == "mode gfsk") {
    radioMode = RadioMode::FSK;
    reinitializeRadio();
  } else if (command == "options") {
    printPacketOptions();
  } else if (command.startsWith("crc ")) {
    if (parseToggle(command.substring(4), packetCrcEnabled)) {
      reinitializeRadio();
    } else {
      Serial.println(F("Usage: crc on|off"));
    }
  } else if (command == "iq normal" || command == "iq inverted") {
    invertedIqEnabled = command == "iq inverted";
    reinitializeRadio();
  } else if (command == "header explicit") {
    implicitPacketLength = 0;
    reinitializeRadio();
  } else if (command.startsWith("header implicit ")) {
    const size_t length = static_cast<size_t>(command.substring(16).toInt());
    if (length < 24 || length > MAX_PACKET_LENGTH) {
      Serial.println(F("[Radio] implicit length must be 24 to 220 bytes"));
    } else {
      implicitPacketLength = length;
      reinitializeRadio();
    }
  } else if (command.startsWith("ldro ")) {
    const String setting = command.substring(5);
    if (setting == "auto") {
      forcedLdroState = -1;
      reinitializeRadio();
    } else {
      bool enabled;
      if (parseToggle(setting, enabled)) {
        forcedLdroState = enabled ? 1 : 0;
        reinitializeRadio();
      } else {
        Serial.println(F("Usage: ldro auto|on|off"));
      }
    }
  } else if (command.startsWith("whitening ")) {
    if (parseToggle(command.substring(10), fskWhiteningEnabled)) {
      reinitializeRadio();
    } else {
      Serial.println(F("Usage: whitening on|off"));
    }
  } else if (command == "rxgain power" || command == "rxgain boosted") {
    const bool requested = command == "rxgain boosted";
    const int state = radio.setRxBoostedGainMode(requested);
    if (state != RADIOLIB_ERR_NONE) {
      printRadioError("setRxBoostedGainMode", state);
    } else {
      receiveBoostedGainEnabled = requested;
      savePersistentConfigurationIfChanged();
    }
  } else if (command.startsWith("lowpower ")) {
    bool requested;
    if (parseToggle(command.substring(9), requested)) {
      const bool previous = lowPowerReceiveEnabled;
      lowPowerReceiveEnabled = requested;
      const int state = startConfiguredReceive();
      if (state != RADIOLIB_ERR_NONE) {
        lowPowerReceiveEnabled = previous;
        printRadioError("low-power receive", state);
      } else {
        savePersistentConfigurationIfChanged();
      }
    } else {
      Serial.println(F("Usage: lowpower on|off"));
    }
  } else if (command.startsWith("cad auto ")) {
    bool requested;
    if (!parseToggle(command.substring(9), requested)) {
      Serial.println(F("Usage: cad auto on|off"));
    } else {
      listenBeforeTalkEnabled = requested;
      savePersistentConfigurationIfChanged();
    }
  } else if (command == "duty off") {
    dutyCycleLimitEnabled = false;
    savePersistentConfigurationIfChanged();
  } else if (command.startsWith("duty ")) {
    const float requestedPercent = command.substring(5).toFloat();
    if (requestedPercent <= 0.0f || requestedPercent > 100.0f) {
      Serial.println(F("[Duty] percentage must be greater than 0 and at most 100"));
    } else {
      dutyCyclePercent = requestedPercent;
      dutyCycleLimitEnabled = true;
      savePersistentConfigurationIfChanged();
    }
  } else if (command == "slots sync") {
    synchronizeSlottedAccess();
  } else if (command.startsWith("slots ")) {
    if (parseToggle(command.substring(6), slottedAccessEnabled)) {
      if (slottedAccessEnabled) {
        slotEpochAt = millis();
      }
    } else {
      Serial.println(F("Usage: slots on|off|sync"));
    }
  } else if (command == "survey") {
    const float center = activeLoRaProfile().frequencyMHz;
    runChannelSurvey(center - 0.2f, center + 0.2f, 100.0f, 3);
  } else if (command.startsWith("survey ")) {
    String arguments = command.substring(7);
    const float startMHz = takeToken(arguments).toFloat();
    const float endMHz = takeToken(arguments).toFloat();
    const float stepKHz = takeToken(arguments).toFloat();
    const uint8_t samples = static_cast<uint8_t>(takeToken(arguments).toInt());
    runChannelSurvey(startMHz, endMHz, stepKHz, samples);
  } else if (command == "diag") {
    printRadioDiagnostics();
  } else if (command == "diag clear") {
    const int state = radio.clearDeviceErrors();
    if (state != RADIOLIB_ERR_NONE) {
      printRadioError("clearDeviceErrors", state);
    }
  } else if (command == "diag calibrate") {
    const int state = radio.calibrateImage(activeLoRaProfile().frequencyMHz);
    if (state != RADIOLIB_ERR_NONE) {
      printRadioError("calibrateImage", state);
    }
  } else if (command.startsWith("sleep ")) {
    enterTimedDeepSleep(static_cast<uint32_t>(command.substring(6).toInt()));
  } else if (command == "peers") {
    printPeers();
  } else if (command == "peer auto") {
    peerSelectionLocked = false;
    peerId = "";
    Serial.println(F("[Peer] automatic selection enabled"));
  } else if (command.startsWith("peer ")) {
    const String requestedPeer = command.substring(5);
    if (findPeer(requestedPeer) == nullptr) {
      Serial.println(F("[Peer] unknown node; use peers to list discovered nodes"));
    } else {
      peerId = requestedPeer;
      peerSelectionLocked = true;
      Serial.print(F("[Peer] selected "));
      Serial.println(peerId);
    }
  } else if (command == "benchmark stop") {
    if (benchmark.active) {
      printBenchmarkReport("stopped");
      benchmark.active = false;
    }
  } else if (command == "benchmark report") {
    printBenchmarkReport(benchmark.active ? "running" : "idle");
  } else if (command.startsWith("benchmark ")) {
    String arguments = command.substring(10);
    const uint16_t packetCount = static_cast<uint16_t>(takeToken(arguments).toInt());
    const size_t payloadLength = arguments.length() ? static_cast<size_t>(takeToken(arguments).toInt()) : 64;
    startBenchmark(packetCount, payloadLength, false);
  } else if (command == "sweep stop") {
    stopProfileSweep("stopped by user");
  } else if (command.startsWith("sweep ")) {
    String arguments = command.substring(6);
    const uint16_t packetCount = static_cast<uint16_t>(takeToken(arguments).toInt());
    const size_t payloadLength = arguments.length() ? static_cast<size_t>(takeToken(arguments).toInt()) : 64;
    startProfileSweep(packetCount, payloadLength);
  } else if (command == "transfer status") {
    printTransferStatus();
  } else if (command == "transfer cancel") {
    outgoingTransfer = OutgoingTransfer();
    Serial.println(F("[Transfer] outgoing transfer cancelled"));
  } else if (command == "transfer resume") {
    if (outgoingTransfer.active || outgoingTransfer.data.length() == 0) {
      Serial.println(F("[Transfer] no paused outgoing transfer"));
    } else {
      outgoingTransfer.active = true;
      outgoingTransfer.attempts = 1;
      outgoingTransfer.nextFragmentIndex = 0;
      outgoingTransfer.waitingForAcknowledgement = false;
      outgoingTransfer.lastSendAt = 0;
      Serial.println(F("[Transfer] resumed"));
    }
  } else if (command.startsWith("transfer ")) {
    startTransfer(command.substring(9));
  } else if (command == "relay on" || command == "relay off") {
    relayEnabled = command == "relay on";
    savePersistentConfigurationIfChanged();
    Serial.print(F("[Relay] forwarding "));
    Serial.println(relayEnabled ? "enabled" : "disabled");
  } else if (command.startsWith("relay send ")) {
    String arguments = command.substring(11);
    const String destination = takeToken(arguments);
    const uint8_t hops = static_cast<uint8_t>(takeToken(arguments).toInt());
    sendRelayedText(destination, hops, arguments);
  } else {
    Serial.println(F("[LoRa] unknown command; type ? for help"));
  }
}

void pollRemoteCommandApi() {
  maintainRemoteWifi();
  if (remoteHttpReady) {
    remoteApiServer.handleClient();
  }
  if (remoteCommandQueue == nullptr) {
    return;
  }

  RemoteCommand queued = {};
  if (xQueueReceive(remoteCommandQueue, &queued, 0) != pdPASS) {
    return;
  }

  lastRemoteCommandSource = queued.source;
  lastRemoteCommandAt = millis();
  String displayedCommand = queued.value;
  String commandTokens = displayedCommand;
  if (takeToken(commandTokens) == "wifi" && takeToken(commandTokens) == "password") {
    displayedCommand = "wifi password (redacted)";
  }
  Serial.print(F("[Remote] "));
  Serial.print(queued.source);
  Serial.print(F(" command: "));
  Serial.println(displayedCommand);
  handleCommand(String(queued.value));
  setUiNotice(String(queued.source) + ": " + clipped(displayedCommand, 28));
}

void pollSerial() {
  while (Serial.available() > 0) {
    const char character = static_cast<char>(Serial.read());
    if (character == '\n' || character == '\r') {
      handleCommand(serialLine);
      serialLine = "";
    } else if (serialLine.length() < MAX_TRANSFER_LENGTH + 80) {
      serialLine += character;
    }
  }
}

bool uiPointInRect(int16_t x, int16_t y, int16_t left, int16_t top, int16_t width, int16_t height) {
  return x >= left && x < left + width && y >= top && y < top + height;
}

bool uiLongExerciseActive() {
  return benchmark.active || profileSweep.active || outgoingTransfer.active || pendingRadioConfiguration.active;
}

void requestUiRadioChange(RadioMode mode, uint8_t profileIndex) {
  if (uiLongExerciseActive()) {
    setUiNotice("Stop active exercise first");
    return;
  }

  uiPendingRadioMode = mode;
  uiPendingProfileIndex = profileIndex;
  uiConfirmationActive = true;
  redrawDisplay();
}

void applyUiRadioChange() {
  uiConfirmationActive = false;
  if (requestRadioConfiguration(uiPendingRadioMode, uiPendingProfileIndex)) {
    setUiNotice("Radio changes in 3 s");
  } else {
    setUiNotice("Radio sync was blocked");
  }
}

void stopUiTest() {
  if (profileSweep.active) {
    stopProfileSweep("stopped from touchscreen");
  } else if (benchmark.active) {
    printBenchmarkReport("stopped");
    benchmark.active = false;
  }
  uiPage = UiPage::RESULTS;
  setUiNotice("Test stopped");
}

void handleUiHomeTouch(int16_t x, int16_t y) {
  if (y >= 23 && y < 58) {
    if (x >= 3 && x < 118) {
      setUiNotice(sendHello() ? "HELLO sent" : "HELLO blocked");
    } else if (x >= 122 && x < 237) {
      setUiNotice(sendPing() ? "Ping sent" : "Ping blocked");
    }
  } else if (y >= 62 && y < 97) {
    if (x >= 3 && x < 118) {
      setUiNotice(sendText(String("Touch hello from ") + nodeId) ? "Text sent" : "Text unavailable");
    } else if (x >= 122 && x < 237) {
      setUiNotice(sendTelemetry() ? "Telemetry sent" : "Telemetry blocked");
    }
  }
}

void handleUiRadioTouch(int16_t x, int16_t y) {
  if (y >= 23 && y < 51) {
    if (x >= 3 && x < 118) {
      requestUiRadioChange(RadioMode::LORA, activeProfileIndex);
    } else if (x >= 122 && x < 237) {
      requestUiRadioChange(RadioMode::FSK, activeProfileIndex);
    }
    return;
  }
  if (y >= 55 && y < 100) {
    const int8_t row = y < 76 ? 0 : (y >= 79 ? 1 : -1);
    const int8_t column = x >= 3 && x < 118 ? 0 : (x >= 122 && x < 237 ? 1 : -1);
    if (row >= 0 && column >= 0) {
      requestUiRadioChange(radioMode, row * 2 + column);
    }
  }
}

void handleUiTestTouch(int16_t x, int16_t y) {
  if (y >= 23 && y < 58) {
    if (x >= 3 && x < 118) {
      runChannelActivityDetection();
    } else if (x >= 122 && x < 237) {
      if (benchmark.active || profileSweep.active) {
        setUiNotice("Test already running");
      } else {
        startBenchmark(10, 64, false);
        setUiNotice(benchmark.active ? "Benchmark started" : "Benchmark unavailable");
      }
    }
  } else if (y >= 62 && y < 97) {
    if (x >= 3 && x < 118) {
      if (benchmark.active || profileSweep.active) {
        setUiNotice("Test already running");
      } else {
        startProfileSweep(5, 64);
        setUiNotice(profileSweep.active ? "Profile sweep started" : "Sweep unavailable");
      }
    } else if (x >= 122 && x < 237) {
      if (benchmark.active || profileSweep.active) {
        stopUiTest();
      } else {
        uiPage = UiPage::RESULTS;
        redrawDisplay();
      }
    }
  }
}

void handleUiAccessTouch(int16_t x, int16_t y) {
  if (!((y >= 23 && y < 58) || (y >= 62 && y < 97))) {
    return;
  }

  const uint8_t row = y < 58 ? 0 : 1;
  int8_t column = -1;
  if (x >= 3 && x < 79) {
    column = 0;
  } else if (x >= 82 && x < 158) {
    column = 1;
  } else if (x >= 161 && x < 237) {
    column = 2;
  }
  if (column < 0) {
    return;
  }
  const uint8_t action = row * 3 + static_cast<uint8_t>(column);
  switch (action) {
    case 0:
      if (!listenBeforeTalkEnabled && radioMode != RadioMode::LORA) {
        setUiNotice("Automatic CAD needs LoRa");
        return;
      }
      listenBeforeTalkEnabled = !listenBeforeTalkEnabled;
      savePersistentConfigurationIfChanged();
      setUiNotice(listenBeforeTalkEnabled ? "Automatic CAD enabled" : "Automatic CAD disabled");
      break;

    case 1:
      dutyCycleLimitEnabled = !dutyCycleLimitEnabled;
      dutyCyclePercent = DEFAULT_DUTY_CYCLE_PERCENT;
      savePersistentConfigurationIfChanged();
      setUiNotice(dutyCycleLimitEnabled ? "Duty pacing at 1%" : "Duty pacing disabled");
      break;

    case 2:
      if (slottedAccessEnabled) {
        slottedAccessEnabled = false;
        setUiNotice("Slotted access disabled");
      } else {
        synchronizeSlottedAccess();
        setUiNotice(slottedAccessEnabled ? "Slots sync in 3 s" : "Slot sync was blocked");
      }
      break;

    case 3: {
      if (!lowPowerReceiveEnabled && radioMode != RadioMode::LORA) {
        setUiNotice("Low-power RX needs LoRa");
        return;
      }
      const bool previous = lowPowerReceiveEnabled;
      lowPowerReceiveEnabled = !lowPowerReceiveEnabled;
      const int state = startConfiguredReceive();
      if (state != RADIOLIB_ERR_NONE) {
        lowPowerReceiveEnabled = previous;
        printRadioError("touch low-power receive", state);
        setUiNotice("Low-power RX failed");
      } else {
        savePersistentConfigurationIfChanged();
        setUiNotice(lowPowerReceiveEnabled ? "Low-power RX enabled" : "Continuous RX enabled");
      }
      break;
    }

    case 4: {
      const bool requested = !receiveBoostedGainEnabled;
      const int state = radio.setRxBoostedGainMode(requested);
      if (state != RADIOLIB_ERR_NONE) {
        printRadioError("touch receive gain", state);
        setUiNotice("Receive gain change failed");
      } else {
        receiveBoostedGainEnabled = requested;
        savePersistentConfigurationIfChanged();
        setUiNotice(requested ? "Boosted RX gain" : "Power-saving RX gain");
      }
      break;
    }

    case 5:
      relayEnabled = !relayEnabled;
      savePersistentConfigurationIfChanged();
      setUiNotice(relayEnabled ? "Relay forwarding enabled" : "Relay forwarding disabled");
      break;
  }
}

void handleUiPeersTouch(int16_t x, int16_t y) {
  if (uiPointInRect(x, y, 3, 23, 53, 35)) {
    peerSelectionLocked = false;
    peerId = "";
    setUiNotice("Automatic peer selection");
    return;
  }
  if (uiPointInRect(x, y, 3, 62, 53, 35)) {
    const uint8_t pages = max<uint8_t>(1, static_cast<uint8_t>((peerCount() + 3) / 4));
    uiPeerPage = (uiPeerPage + 1) % pages;
    redrawDisplay();
    return;
  }

  if (x < 60 || !((y >= 23 && y < 58) || (y >= 62 && y < 97))) {
    return;
  }
  const uint8_t row = y < 58 ? 0 : 1;
  const uint8_t column = x < 150 ? 0 : 1;
  PeerInfo* peer = peerAtUiIndex(uiPeerPage * 4 + row * 2 + column);
  if (peer != nullptr) {
    peerId = peer->id;
    peerSelectionLocked = true;
    setUiNotice(String("Selected ") + peerId);
  }
}

void handleUiTouch(int16_t x, int16_t y) {
  if (uiConfirmationActive) {
    if (uiPointInRect(x, y, 18, 72, 96, 26)) {
      applyUiRadioChange();
    } else if (uiPointInRect(x, y, 126, 72, 96, 26)) {
      uiConfirmationActive = false;
      setUiNotice("Radio change cancelled");
    }
    return;
  }

  if (y >= UI_NAV_Y) {
    uiPage = static_cast<UiPage>(min<uint8_t>(4, static_cast<uint8_t>(x / 48)));
    redrawDisplay();
    return;
  }

  switch (uiPage) {
    case UiPage::HOME:
      handleUiHomeTouch(x, y);
      break;
    case UiPage::RADIO:
      handleUiRadioTouch(x, y);
      break;
    case UiPage::TEST:
      handleUiTestTouch(x, y);
      break;
    case UiPage::ACCESS:
      handleUiAccessTouch(x, y);
      break;
    case UiPage::PEERS:
      handleUiPeersTouch(x, y);
      break;
    case UiPage::RESULTS:
      break;
  }
}

void pollTouch() {
  if (!touchReady || static_cast<uint32_t>(millis() - lastTouchPollAt) < 25UL) {
    return;
  }
  lastTouchPollAt = millis();

  int16_t x = 0;
  int16_t y = 0;
  const bool pressed = nessoTouch.read(x, y);
  if (pressed && !previousTouchPressed && x >= 0 && x < UI_WIDTH && y >= 0 && y < 135) {
    handleUiTouch(x, y);
  }
  previousTouchPressed = pressed;
}

void runUiRefreshTask() {
  if (!displayReady) {
    return;
  }
  if (uiNotice.length() && timeReached(millis(), uiNoticeUntil)) {
    uiNotice = "";
    redrawDisplay();
    return;
  }

  const uint32_t refreshInterval = benchmark.active || profileSweep.active || pendingRadioConfiguration.active ? 1000UL : 15000UL;
  if (static_cast<uint32_t>(millis() - lastUiRefreshAt) >= refreshInterval) {
    redrawDisplay();
  }
}

void pollButtons() {
  const bool key1Pressed = digitalRead(KEY1) == LOW;
  const uint32_t now = millis();

  if (now - lastButtonEvent >= 250) {
    if (key1Pressed && !previousKey1Pressed) {
      lastButtonEvent = now;
      if (uiConfirmationActive) {
        applyUiRadioChange();
      } else if (uiPage == UiPage::HOME) {
        setUiNotice(sendPing() ? "Ping sent" : "Ping blocked");
      } else {
        uiPage = UiPage::HOME;
        redrawDisplay();
      }
    }
  }

  previousKey1Pressed = key1Pressed;
}

void runPeriodicTasks() {
  if (benchmark.active || profileSweep.active || pendingRadioConfiguration.active || outgoingTransfer.active) {
    return;
  }

  const uint32_t now = millis();

  if (timeReached(now, nextHelloAt)) {
    sendHello();
    nextHelloAt = now + HELLO_INTERVAL_MS + (nodeIdValue % 5000UL);
  }

  if (timeReached(now, nextTelemetryAt)) {
    sendTelemetry();
    nextTelemetryAt = now + TELEMETRY_INTERVAL_MS + (nodeIdValue % 15000UL);
  }
}

void setup() {
  Serial.begin(115200);
  Serial.setTimeout(20);

  const uint32_t serialWaitStart = millis();
  while (!Serial && millis() - serialWaitStart < 2000) {
    delay(10);
  }

  makeNodeId();
  loadPersistentConfiguration();
  setupNessoIo();
  setupDisplay();
  setupRadio();
  setupRemoteHttpApi();
  setupRemoteBleApi();

  Serial.println();
  Serial.println(F("Arduino Nesso N1 LoRa exerciser"));
  Serial.print(F("Node ID: "));
  Serial.println(nodeId);
  Serial.println(F("Type ? in the serial monitor for commands."));

  if (radioReady) {
    sendHello();
  }

  const uint32_t now = millis();
  nextHelloAt = now + HELLO_INTERVAL_MS + (nodeIdValue % 5000UL);
  nextTelemetryAt = now + 15000UL + (nodeIdValue % 15000UL);
  redrawDisplay();
}

void loop() {
  pollRadio();
  pollSerial();
  pollRemoteCommandApi();
  pollButtons();
  pollTouch();
  runDeferredPacketTask();
  retryPendingText();
  runBenchmarkTask();
  runPendingRadioConfigurationTask();
  runProfileSweepTask();
  runOutgoingTransferTask();
  runIncomingTransferMaintenance();
  runPeriodicTasks();
  runUiRefreshTask();
  delay(2);
}
