import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[1]
SOURCE = (ROOT / "NessoN1LoRaExerciser.ino").read_text(encoding="utf-8")


def extract(pattern):
    match = re.search(pattern, SOURCE, re.MULTILINE | re.DOTALL)
    if not match:
        raise AssertionError(f"Sketch definition not found: {pattern}")
    return match.group(0)


def function(name):
    return extract(r"^[\w:*& ]+\b" + re.escape(name) + r"\([^\n]*\) \{\n.*?^\}")


PRELUDE = r'''
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <limits>
#include <sstream>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>
using std::min;
using std::max;
using std::isfinite;
#define F(text) text

class String {
public:
  std::string value;
  String() = default;
  String(const char* text) : value(text) {}
  String(const std::string& text) : value(text) {}
  String(char character) : value(1, character) {}
  template<class Number, std::enable_if_t<std::is_arithmetic_v<Number>, int> = 0>
  String(Number number) : value(std::to_string(number)) {}
  String(double number, int precision) {
    std::ostringstream stream;
    stream.precision(precision);
    stream << std::fixed << number;
    value = stream.str();
  }
  size_t length() const { return value.length(); }
  const char* c_str() const { return value.c_str(); }
  void reserve(size_t size) { value.reserve(size); }
  String substring(size_t start, size_t end = std::string::npos) const {
    start = min(start, value.length());
    end = min(end, value.length());
    return value.substr(start, end > start ? end - start : 0);
  }
  int indexOf(char delimiter, size_t start = 0) const {
    const auto found = value.find(delimiter, start);
    return found == std::string::npos ? -1 : static_cast<int>(found);
  }
  bool startsWith(const char* prefix) const { return value.rfind(prefix, 0) == 0; }
  char charAt(size_t index) const { return index < value.length() ? value[index] : '\0'; }
  long toInt() const { return strtol(value.c_str(), nullptr, 10); }
  void replace(char before, char after) { std::replace(value.begin(), value.end(), before, after); }
  void trim() {
    const auto first = value.find_first_not_of(" \r\n\t");
    value = first == std::string::npos ? "" : value.substr(first, value.find_last_not_of(" \r\n\t") - first + 1);
  }
  String& operator+=(const String& other) { value += other.value; return *this; }
  friend String operator+(String first, const String& second) { return first += second; }
  friend bool operator==(const String& first, const String& second) { return first.value == second.value; }
  friend bool operator!=(const String& first, const String& second) { return !(first == second); }
};

struct SerialMock {
  std::deque<char> input;
  template<class Value> void print(const Value&, int = 0) {}
  template<class Value> void println(const Value&) {}
  void println() {}
  void printf(const char*, ...) {}
  int available() const { return static_cast<int>(input.size()); }
  int read() { const char value = input.front(); input.pop_front(); return value; }
  void feed(const std::string& text) { input.insert(input.end(), text.begin(), text.end()); }
} Serial;

constexpr int RADIOLIB_ERR_NONE = 0;
constexpr int RADIOLIB_ERR_TX_TIMEOUT = -5;
constexpr int RADIOLIB_ERR_RX_TIMEOUT = -6;
constexpr int RADIOLIB_ERR_CRC_MISMATCH = -7;
constexpr int RADIOLIB_CHANNEL_FREE = -15;
constexpr int RADIOLIB_PREAMBLE_DETECTED = -14;
constexpr int RADIOLIB_LORA_DETECTED = -702;
constexpr uint16_t RADIOLIB_SX126X_IRQ_TX_DONE = 1;
constexpr uint16_t RADIOLIB_SX126X_IRQ_RX_DONE = 2;
constexpr uint16_t RADIOLIB_SX126X_IRQ_CAD_DONE = 128;
constexpr int LORA_IRQ = 15;
constexpr int HIGH = 1;
constexpr int LOW = 0;
constexpr int OUTPUT = 1;
constexpr int WIFI_OFF = 0;
constexpr int ESP_SLEEP_WAKEUP_ALL = 255;
constexpr int MC_DATUM = 4;
constexpr int LCD_BACKLIGHT = 100;
constexpr int LED_BUILTIN = 101;
constexpr int LORA_LNA_ENABLE = 102;
constexpr int LORA_ANTENNA_SWITCH = 103;
constexpr int LORA_ENABLE = 104;
constexpr int POWEROFF = 105;
constexpr int HEX = 16;
uint32_t testNow = 1000;
uint32_t millis() { return testNow; }
void delay(uint32_t elapsed) { testNow += elapsed; }
uint32_t random(uint32_t) { return 0; }
uint32_t esp_random() { return 12345; }

struct NessoBattery {
  static constexpr uint8_t BQ27220_I2C_ADDR = 0x55;
  static constexpr uint8_t AW32001_I2C_ADDR = 0x49;
  static constexpr uint8_t BQ27220_VOLTAGE = 0x08;
  static constexpr uint8_t AW3200_SYS_STATUS = 0x08;
  enum ChargeStatus { NOT_CHARGING, PRE_CHARGE, CHARGING, FULL_CHARGE };
};
'''

GLOBALS = r'''
bool radioReady = true;
bool radioOperationBusy = false;
bool channelSurveyActive = false;
bool pendingTextActive = false;
bool touchReady = true;
bool displayReady = true;
bool remoteHttpReady = true;
bool remoteBleReady = true;
bool previousTouchPressed = false;
bool touchHandled = false;
uint32_t touchPressedAt = 0;
uint32_t lastTouchPollAt = 0;
bool packetReceivedFlag = false;
bool listenBeforeTalkEnabled = false;
bool dutyCycleLimitEnabled = false;
bool slottedAccessEnabled = false;
uint32_t lastTxAt = 0;
uint32_t nextDutyCycleTransmitAt = 0;
uint32_t slotEpochAt = 0;
uint32_t nodeIdValue = 0;
uint32_t accessDeferrals = 0;
uint32_t cadBusyDetections = 0;
uint32_t transmitFailures = 0;
uint32_t txSequence = 1;
uint64_t totalTransmitAirtimeUs = 0;
uint64_t totalPacketsSent = 0;
uint64_t totalPacketsReceived = 0;
float dutyCyclePercent = 1.0f;
size_t implicitPacketLength = 0;
uint8_t activeProfileIndex = 0;
RadioMode radioMode = RadioMode::LORA;
BenchmarkState benchmark;
ProfileSweepState profileSweep;
PendingRadioConfiguration pendingRadioConfiguration;
IncomingTransfer incomingTransfer;
OutgoingTransfer outgoingTransfer;
ChannelSurveyState channelSurvey;
String lastReceivedText;
String serialLine;
const char* nodeId = "12345678";
uint16_t batteryChargeLevel = 0;
float batteryVoltage = 0;
NessoBattery::ChargeStatus batteryChargeStatus = NessoBattery::NOT_CHARGING;
uint32_t lastBatteryStatusAt = 0;
uint32_t lastBatterySuccessAt = 0;
bool batteryStatusReady = false;
bool batteryReadSucceeded = false;
bool batteryChargeStatusReady = false;
bool gaugeOk = true;
bool chargerOk = true;
uint16_t gaugeVoltage = 4120;
uint16_t gaugePercent = 12;
unsigned gaugeReads = 0;
unsigned buttonPolls = 0;
unsigned screenPolls = 0;
unsigned httpPolls = 0;
bool reenterTouchHandler = false;
std::vector<std::pair<int16_t, int16_t>> touches;
std::vector<uint16_t> acknowledgements;
std::vector<String> queuedCommands;
std::vector<String> executedCommands;
String receivedPayload;

struct PowerPinWrite { int pin; int level; uint32_t at; };
std::vector<PowerPinWrite> powerPinWrites;
bool powerOutputEnabled = false;
bool allWakeSourcesDisabled = false;
bool deepSleepEntered = false;
struct PowerOffFallback {};
void digitalWrite(int pin, int level) { powerPinWrites.push_back({pin, level, millis()}); }
void pinMode(int pin, int mode) {
  assert(pin == POWEROFF && mode == OUTPUT);
  assert(!powerPinWrites.empty() && powerPinWrites.back().pin == POWEROFF && powerPinWrites.back().level == LOW);
  powerOutputEnabled = true;
}
void esp_sleep_disable_wakeup_source(int source) {
  assert(source == ESP_SLEEP_WAKEUP_ALL);
  allWakeSourcesDisabled = true;
}
void esp_deep_sleep_start() {
  assert(allWakeSourcesDisabled);
  deepSleepEntered = true;
  throw PowerOffFallback();
}
struct WifiMock { bool stopped = false; void mode(int requested) { assert(requested == WIFI_OFF); stopped = true; } } WiFi;
struct HttpMock { bool stopped = false; void stop() { stopped = true; } } remoteApiServer;
struct BLEDevice { inline static bool stopped = false; static void deinit(bool) { stopped = true; } };
struct DisplayMock { void fillScreen(uint16_t) {} } nessoDisplay;
void drawUiText(const String&, int16_t, int16_t, uint16_t, int, uint8_t) {}

struct TouchMock {
  bool pressed = false;
  int16_t rawX = 0;
  int16_t rawY = 0;
  bool read(int16_t& horizontal, int16_t& vertical) {
    horizontal = rawX; vertical = rawY; return pressed;
  }
} nessoTouch;

struct RadioMock {
  uint16_t irq = 0;
  uint16_t scheduledIrq = 0;
  uint32_t scheduledAt = 0;
  bool completeTx = true;
  bool completeCad = true;
  int scanResult = RADIOLIB_CHANNEL_FREE;
  int reads = 0;
  int receives = 0;
  int scans = 0;
  int transmits = 0;
  int finishedTx = 0;
  bool interruptDetached = false;
  bool sleeping = false;
  float frequency = 0;
  String raw;
  std::string transmitted;
  uint16_t getIrqFlags() const {
    return irq | (static_cast<int32_t>(testNow - scheduledAt) >= 0 ? scheduledIrq : 0);
  }
  int standby() { irq = scheduledIrq = 0; return 0; }
  int startReceive() { ++receives; irq = scheduledIrq = 0; return 0; }
  int setFrequency(float requested) { frequency = requested; return 0; }
  int startChannelScan() {
    ++scans; irq = 0; scheduledAt = testNow + 5;
    scheduledIrq = completeCad ? RADIOLIB_SX126X_IRQ_CAD_DONE : 0;
    return 0;
  }
  int getChannelScanResult() { irq = scheduledIrq = 0; return scanResult; }
  uint32_t getTimeOnAir(size_t length) { return static_cast<uint32_t>(length * 1000); }
  int startTransmit(const uint8_t* data, size_t length) {
    ++transmits; transmitted.assign(reinterpret_cast<const char*>(data), length);
    irq = 0; scheduledAt = testNow + 5;
    scheduledIrq = completeTx ? RADIOLIB_SX126X_IRQ_TX_DONE : 0;
    return 0;
  }
  int finishTransmit() { ++finishedTx; irq = scheduledIrq = 0; return 0; }
  void clearPacketReceivedAction() { interruptDetached = true; }
  int sleep(bool) { sleeping = true; return 0; }
  int readData(String& data) { ++reads; data = raw; irq = 0; return 0; }
  float getRSSI(bool = true) { return -75.0f; }
  float getSNR() { return 7.0f; }
} radio;

int digitalRead(int) { return radio.getIrqFlags() != 0 ? HIGH : 0; }
const LoRaProfile& activeLoRaProfile() { return LORA_PROFILES[activeProfileIndex]; }
void printRadioError(const char*, int) {}
void redrawDisplay() {}
void setUiNotice(const String&) {}
void setTransmitting(bool) {}
int startConfiguredReceive() { return radio.startReceive(); }
String targetPeerOrBroadcast() { return "87654321"; }
void pollTouch();
void handleUiTouch(int16_t horizontal, int16_t vertical) {
  touches.emplace_back(horizontal, vertical);
  if (reenterTouchHandler) {
    reenterTouchHandler = false;
    delay(INPUT_POLL_INTERVAL_MS);
    pollTouch();
  }
}
void handleReceivedPacket(const String& raw, float, float) { receivedPayload = raw; }
void pollButtons() { ++buttonPolls; }
void runUiRefreshTask() { ++screenPolls; }
void pollRemoteCommandApi() { ++httpPolls; }
bool readI2cWord(uint8_t, uint8_t address, uint16_t& value) {
  ++gaugeReads; value = address == 0x08 ? gaugeVoltage : gaugePercent; return gaugeOk;
}
bool readI2cRegister(uint8_t, uint8_t, uint8_t* value, size_t) { *value = 2 << 3; return chargerOk; }
bool sendPacketOrDefer(char, const String&, const String& body, uint32_t) {
  acknowledgements.push_back(static_cast<uint16_t>(strtoul(body.c_str() + body.indexOf(',') + 1, nullptr, 10)));
  return true;
}
RemoteEnqueueResult enqueueRemoteCommand(const String& command, const char*) {
  if (!command.length()) { return RemoteEnqueueResult::EMPTY; }
  queuedCommands.push_back(command); return RemoteEnqueueResult::QUEUED;
}
void handleCommand(String command);
'''

TESTS = r'''
void handleCommand(String command) {
  if (!command.length()) { return; }
  executedCommands.push_back(command);
  if (command == "p") {
    RadioOperationScope operation;
    waitForRadioEvent(RADIOLIB_SX126X_IRQ_TX_DONE, 5);
  }
}

void resetState() {
  testNow = 1000;
  radioReady = true;
  radioOperationBusy = channelSurveyActive = pendingTextActive = false;
  benchmark = BenchmarkState(); profileSweep = ProfileSweepState();
  pendingRadioConfiguration = PendingRadioConfiguration();
  incomingTransfer = IncomingTransfer(); outgoingTransfer = OutgoingTransfer();
  channelSurvey = ChannelSurveyState(); radio = RadioMock();
  previousTouchPressed = touchHandled = false; touchPressedAt = lastTouchPollAt = 0;
  nessoTouch = TouchMock(); touches.clear(); acknowledgements.clear();
  queuedCommands.clear(); executedCommands.clear(); Serial.input.clear(); serialLine = "";
  receivedPayload = ""; packetReceivedFlag = false;
  lastTxAt = 0; implicitPacketLength = 0;
  listenBeforeTalkEnabled = dutyCycleLimitEnabled = slottedAccessEnabled = false;
  totalPacketsSent = totalPacketsReceived = totalTransmitAirtimeUs = 0;
  buttonPolls = screenPolls = httpPolls = 0;
  reenterTouchHandler = false;
  batteryStatusReady = batteryReadSucceeded = batteryChargeStatusReady = false;
  lastBatteryStatusAt = lastBatterySuccessAt = 0;
  gaugeOk = chargerOk = true; gaugeVoltage = 4120; gaugePercent = 12; gaugeReads = 0;
  displayReady = remoteHttpReady = remoteBleReady = true;
  powerPinWrites.clear(); powerOutputEnabled = allWakeSourcesDisabled = deepSleepEntered = false;
  WiFi.stopped = remoteApiServer.stopped = BLEDevice::stopped = false;
}

ReceivedPacket fragment(uint8_t index, uint16_t checksum, const String& data) {
  ReceivedPacket packet;
  packet.type = 'F'; packet.source = "87654321";
  packet.body = String(12345) + ',' + String(index) + ",2," + String(checksum) + ',' + data;
  return packet;
}

void testTouch() {
  const std::vector<std::pair<int16_t, int16_t>> rawPoints = {{0, 0}, {134, 239}, {10, 72}, {10, 120}, {10, 216}};
  for (const auto& point : rawPoints) {
    resetState(); nessoTouch.pressed = true; nessoTouch.rawX = point.first; nessoTouch.rawY = point.second;
    pollTouch(); assert(touches.empty());
    delay(INPUT_POLL_INTERVAL_MS); pollTouch();
    assert(touches.size() == 1);
    assert(touches[0].first == point.second && touches[0].second == 134 - point.first);
    delay(INPUT_POLL_INTERVAL_MS); pollTouch(); assert(touches.size() == 1);
    nessoTouch.pressed = false; delay(INPUT_POLL_INTERVAL_MS); pollTouch();
    nessoTouch.pressed = true; delay(INPUT_POLL_INTERVAL_MS); pollTouch();
    delay(INPUT_POLL_INTERVAL_MS); pollTouch(); assert(touches.size() == 2);
  }
  resetState(); nessoTouch.pressed = true; nessoTouch.rawX = 135; nessoTouch.rawY = 50;
  pollTouch(); delay(INPUT_POLL_INTERVAL_MS); pollTouch(); assert(touches.empty());
  resetState(); nessoTouch.pressed = true; nessoTouch.rawX = 10; nessoTouch.rawY = 72;
  pollTouch(); delay(INPUT_POLL_INTERVAL_MS); reenterTouchHandler = true; pollTouch();
  assert(touches.size() == 1);
  puts("PASS touch rotation, bounds and one-action-per-contact");
}

void testIrqAndFraming() {
  resetState(); radio.irq = RADIOLIB_SX126X_IRQ_TX_DONE; packetReceivedFlag = true;
  pollRadio(); assert(radio.reads == 0 && totalPacketsReceived == 0);
  radio.irq = RADIOLIB_SX126X_IRQ_CAD_DONE; packetReceivedFlag = true;
  pollRadio(); assert(radio.reads == 0);
  radio.irq = RADIOLIB_SX126X_IRQ_RX_DONE; radio.raw = "N1L|F|87654321|12345678|1|12345,0,2,1,tail  ";
  packetReceivedFlag = true; pollRadio(); assert(radio.reads == 1 && receivedPayload == radio.raw);
  assert(totalPacketsReceived == 1);
  resetState(); implicitPacketLength = 100;
  assert(sendPacket('F', "87654321", "12345,0,2,1,tail \n ", 1));
  assert(radio.transmitted.size() == 100 && radio.transmitted.back() == '\0');
  assert(radio.transmitted.find("tail \n \0", 0, 8) != std::string::npos);
  assert(buttonPolls > 0 && screenPolls > 0 && httpPolls > 0);
  resetState(); radio.completeTx = false;
  assert(!sendPacket('P', "87654321", "ping", 1));
  assert(radio.finishedTx == 1 && radio.receives > 0 && !radioOperationBusy);
  assert(testNow < 2000 && buttonPolls > 0);
  puts("PASS IRQ classification, payload padding and responsive TX timeout");
}

void testTransfer() {
  resetState();
  const String first = std::string(71, 'A') + " ";
  const String second = "tail \n ";
  const String message = first + second;
  const uint16_t checksum = crc16(message);
  handleIncomingFragment(fragment(0, checksum, first));
  assert(acknowledgements.back() == 1);
  handleIncomingFragment(fragment(1, checksum, second));
  assert(acknowledgements.back() == 3 && lastReceivedText == message);
  handleIncomingFragment(fragment(1, checksum, second));
  assert(acknowledgements.back() == 3);

  resetState(); handleIncomingFragment(fragment(0, checksum, "damaged"));
  handleIncomingFragment(fragment(1, checksum, second));
  assert(acknowledgements.back() == 0 && incomingTransfer.receivedMask == 0);
  assert(std::find(acknowledgements.begin(), acknowledgements.end(), 3) == acknowledgements.end());
  handleIncomingFragment(fragment(0, checksum, first));
  handleIncomingFragment(fragment(1, checksum, second));
  assert(acknowledgements.back() == 3);

  outgoingTransfer.active = true; outgoingTransfer.id = 12345;
  outgoingTransfer.fragmentCount = 2; outgoingTransfer.destination = "87654321";
  ReceivedPacket acknowledgement; acknowledgement.source = "87654321";
  acknowledgement.sequence = 10; acknowledgement.body = "12345,1";
  handleTransferAcknowledgement(acknowledgement); assert(outgoingTransfer.acknowledgedMask == 1);
  acknowledgement.sequence = 11; acknowledgement.body = "12345,0";
  handleTransferAcknowledgement(acknowledgement); assert(outgoingTransfer.acknowledgedMask == 0);
  acknowledgement.sequence = 10; acknowledgement.body = "12345,3";
  handleTransferAcknowledgement(acknowledgement); assert(outgoingTransfer.acknowledgedMask == 0);
  acknowledgement.sequence = 12;
  handleTransferAcknowledgement(acknowledgement); assert(outgoingTransfer.acknowledgedMask == 3);
  puts("PASS transfer CRC gate, whitespace, reset and reordered ACKs");
}

void testGuards() {
  const std::vector<UserOperation> operations = {UserOperation::TRANSMIT, UserOperation::CONFIGURE, UserOperation::BENCHMARK, UserOperation::SWEEP, UserOperation::TRANSFER, UserOperation::SCAN, UserOperation::SURVEY};
  for (auto operation : operations) {
    resetState(); assert(operationAllowed(operation));
    for (bool* blocker : {&radioOperationBusy, &channelSurveyActive, &benchmark.active, &outgoingTransfer.active, &pendingRadioConfiguration.active, &pendingTextActive, &profileSweep.active}) {
      *blocker = true; assert(!operationAllowed(operation)); *blocker = false;
    }
    incomingTransfer.active = true; incomingTransfer.fragmentCount = 2; incomingTransfer.lastUpdateAt = millis();
    assert(!operationAllowed(operation));
    incomingTransfer.receivedMask = 3; assert(operationAllowed(operation));
  }
  resetState(); profileSweep.active = true;
  assert(operationAllowed(UserOperation::CONFIGURE, true));
  assert(operationAllowed(UserOperation::BENCHMARK, true));
  assert(!operationAllowed(UserOperation::TRANSFER, true));
  resetState(); radioReady = false;
  assert(operationAllowed(UserOperation::CONFIGURE)); assert(!operationAllowed(UserOperation::TRANSMIT));
  puts("PASS shared operation exclusions and sweep-owned transitions");
}

void testBattery() {
  resetState(); assert(std::string(batteryDataState()) == "unavailable");
  assert(updateBatteryStatus(true) && batteryStatusFresh());
  assert(batteryChargeLevel == 12 && batteryVoltage > 4.11f);
  const auto reads = gaugeReads; delay(1000); assert(!updateBatteryStatus()); assert(gaugeReads == reads);
  gaugeOk = false; delay(BATTERY_STATUS_INTERVAL_MS); assert(updateBatteryStatus());
  assert(std::string(batteryDataState()) == "stale" && batteryChargeLevel == 12);
  gaugeOk = true; gaugePercent = 13; assert(updateBatteryStatus(true));
  assert(batteryStatusFresh() && batteryChargeLevel == 13);
  delay(BATTERY_STALE_AFTER_MS); assert(!batteryStatusFresh());
  gaugePercent = 101; assert(updateBatteryStatus(true)); assert(!batteryStatusFresh());
  gaugePercent = 50; gaugeVoltage = 65535; assert(updateBatteryStatus(true)); assert(!batteryStatusFresh());
  resetState(); testNow = UINT32_MAX - 100; assert(updateBatteryStatus(true)); delay(200);
  assert(batteryStatusFresh());
  puts("PASS battery refresh cadence, stale failures, recovery and clock rollover");
}

void testSurveyAndWaits() {
  resetState(); radio.completeCad = false;
  assert(scanChannelResponsive() == RADIOLIB_ERR_RX_TIMEOUT);
  assert(buttonPolls > 0 && testNow < 3000 && !radioOperationBusy);
  resetState(); runChannelSurvey(868.0f, 868.1f, 100.0f, 2);
  assert(channelSurveyActive && radio.scans == 0);
  for (unsigned tick = 0; tick < 2000 && channelSurveyActive; ++tick) { runChannelSurveyTask(); delay(1); }
  assert(!channelSurveyActive && radio.scans == 4 && radio.frequency == activeLoRaProfile().frequencyMHz);
  resetState(); runChannelSurvey(868.0f, 868.2f, 100.0f, 2); runChannelSurveyTask();
  finishChannelSurvey("stopped"); assert(!channelSurveyActive && radio.receives > 0);
  resetState(); radio.completeCad = false; runChannelSurvey(868.0f, 868.0f, 100.0f, 1);
  for (unsigned tick = 0; tick < 2000 && channelSurveyActive; ++tick) { runChannelSurveyTask(); delay(1); }
  assert(!channelSurveyActive && radio.frequency == activeLoRaProfile().frequencyMHz);
  resetState(); radio.scanResult = RADIOLIB_LORA_DETECTED;
  runChannelSurvey(868.0f, 868.0f, 100.0f, 2);
  for (unsigned tick = 0; tick < 2000 && channelSurveyActive; ++tick) { runChannelSurveyTask(); delay(1); }
  assert(!channelSurveyActive && radio.scans == 2 && channelSurvey.detections == 2);
  resetState(); radio.scanResult = RADIOLIB_LORA_DETECTED; listenBeforeTalkEnabled = true;
  const auto previousDetections = cadBusyDetections;
  assert(!sendPacket('P', "87654321", "ping", 1));
  assert(cadBusyDetections - previousDetections == CSMA_MAX_ATTEMPTS && radio.transmits == 0);
  puts("PASS incremental survey completion, cancellation and CAD timeout");
}

void testSerialReentry() {
  resetState(); Serial.feed("p\r\nh\r\n"); pollSerial();
  assert(executedCommands.size() == 1 && executedCommands[0] == "p");
  assert(queuedCommands.size() == 1 && queuedCommands[0] == "h" && serialLine.length() == 0);
  puts("PASS serial command isolation during cooperative radio waits");
}

void testPowerOff() {
  for (bool peripheralsReady : {true, false}) {
    resetState();
    displayReady = remoteHttpReady = remoteBleReady = radioReady = peripheralsReady;
    const uint32_t startedAt = millis();
    try { powerOffNesso(); assert(false); } catch (const PowerOffFallback&) {}
    assert(WiFi.stopped && !remoteHttpReady && !remoteBleReady && !radioReady);
    assert(remoteApiServer.stopped == peripheralsReady && BLEDevice::stopped == peripheralsReady);
    assert(radio.interruptDetached == peripheralsReady && radio.sleeping == peripheralsReady);
    assert(powerOutputEnabled && allWakeSourcesDisabled && deepSleepEntered);
    std::vector<PowerPinWrite> pulses;
    for (const auto& write : powerPinWrites) {
      if (write.pin == POWEROFF) { pulses.push_back(write); }
    }
    assert(pulses.size() == POWER_OFF_PULSE_STEPS + 1);
    assert(pulses[0].level == LOW);
    for (size_t step = 0; step < POWER_OFF_PULSE_STEPS; ++step) {
      assert(pulses[step + 1].level == ((step & 1) ? HIGH : LOW));
      assert(pulses[step + 1].at == startedAt + (peripheralsReady ? 500 : 0) + step * POWER_OFF_PULSE_MS);
    }
    assert(millis() - pulses[1].at == POWER_OFF_PULSE_STEPS * POWER_OFF_PULSE_MS);
  }
  puts("PASS shutdown pulses, peripheral teardown and no-wakeup fallback");
}

int main() {
  testTouch(); testIrqAndFraming(); testTransfer(); testGuards(); testBattery(); testSurveyAndWaits(); testSerialReentry();
  testPowerOff();
  for (const auto& profile : LORA_PROFILES) {
    assert(profile.frequencyMHz == LORA_FREQUENCY_MHZ && profile.txPowerDbm <= LORA_TX_POWER_DBM);
  }
  assert(LORA_PROFILES[0].bandwidthKHz == LORA_BANDWIDTH_KHZ);
  assert(LORA_PROFILES[0].spreadingFactor == LORA_SPREADING_FACTOR);
  assert(LORA_PROFILES[0].codingRate == LORA_CODING_RATE);
  assert(LORA_PROFILES[0].preambleSymbols == LORA_PREAMBLE_SYMBOLS);
  puts("PASS configured radio profiles");
}
'''


def main():
    compiler = os.environ.get("CXX") or shutil.which("g++") or shutil.which("clang++")
    if not compiler:
        raise SystemExit("A host C++17 compiler (g++ or clang++) is required; set CXX if needed.")

    constants = "\n".join(re.findall(r"^constexpr (?:uint\d+_t|int\d+_t|size_t|float) [A-Z0-9_]+ = [^;]+;", SOURCE, re.MULTILINE))
    type_names = ["RadioMode", "UserOperation", "RemoteEnqueueResult", "LoRaProfile", "BenchmarkState", "OutgoingTransfer", "IncomingTransfer", "PendingRadioConfiguration", "SweepPhase", "ProfileSweepState", "SurveyPhase", "ChannelSurveyState", "ReceivedPacket"]
    types = "\n".join(extract(r"^(?:struct|enum class) " + name + r"\b[^\n]*\{\n.*?^\};") for name in type_names)
    profiles = extract(r"^constexpr LoRaProfile LORA_PROFILES\[\] = \{\n.*?^\};")
    constants = "\n".join(line for line in constants.splitlines() if "LORA_PROFILE_COUNT" not in line)
    names = ["timeReached", "completeFragmentMask", "crc16", "takeDelimitedToken", "clipped", "operationAllowed", "sendTransferAcknowledgement", "handleIncomingFragment", "handleTransferAcknowledgement", "pollTouch", "pollRadio", "batteryStatusFresh", "batteryDataState", "updateBatteryStatus", "waitForRadioEvent", "channelScanTimeoutMs", "scanChannelResponsive", "radioBackoff", "waitForTransmitAccess", "recordTransmitAirtime", "sendPacket", "runChannelSurvey", "finishChannelSurvey", "runChannelSurveyTask", "serviceRadioWait", "pollSerial", "powerOffNesso"]
    definitions = []
    declarations = []
    for name in names:
        header, body = function(name).split("{", 1)
        declarations.append(header.strip() + ";")
        definitions.append(re.sub(r" = (?:false|true|3000)(?=[,)])", "", header) + "{" + body)

    for name, guard in [("startBenchmark", "UserOperation::BENCHMARK"), ("startProfileSweep", "UserOperation::SWEEP"), ("startTransfer", "UserOperation::TRANSFER"), ("scheduleRadioConfiguration", "UserOperation::CONFIGURE"), ("requestRadioConfiguration", "UserOperation::CONFIGURE"), ("handleUiAccessTouch", "UserOperation::CONFIGURE"), ("handleCommand", "changesRadio && !operationAllowed(UserOperation::CONFIGURE)")]:
        assert guard in function(name), f"Missing shared operation guard in {name}"
    assert "radio.scanChannel()" not in SOURCE
    assert "radio.transmit(" not in SOURCE

    program = "\n".join([PRELUDE, constants, types, profiles, GLOBALS, extract(r"^class RadioOperationScope \{\n.*?^\};"), *declarations, *definitions, TESTS])
    with tempfile.TemporaryDirectory(prefix="nesso-regression-") as directory:
        executable = Path(directory) / ("regressions.exe" if os.name == "nt" else "regressions")
        subprocess.run([compiler, "-std=c++17", "-Wall", "-Wextra", "-Werror", "-x", "c++", "-", "-o", str(executable)], input=program, text=True, check=True)
        subprocess.run([str(executable)], check=True)


if __name__ == "__main__":
    main()