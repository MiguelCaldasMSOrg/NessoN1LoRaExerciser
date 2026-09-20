/*
  Arduino Nesso N1 - two-node SX1262 LoRa exerciser

  Flash this same sketch to two Nesso N1 boards. Each board derives a short
  node ID from its ESP32 eFuse MAC address, discovers the other node with
  HELLO packets, and can exchange text, pings, acknowledgements, and
  telemetry.

  Required libraries:
    - RadioLib
    - Arduino_Nesso_N1

  Serial monitor: 115200 baud, newline enabled.

  Commands:
    t <text>  Send text to the discovered peer, or broadcast if no peer is known
    p         Ping the peer
    h         Send a discovery HELLO
    s         Print local status and send telemetry
    c         Run a LoRa channel-activity detection scan
    ?         Print this help

  Buttons:
    KEY1      Send a text message
    KEY2      Send telemetry

  The default profile is a common EU868 test frequency. Change the radio
  settings below on both boards, and use only frequencies and power levels
  permitted in your country.
*/

#include <RadioLib.h>
#include <Arduino_Nesso_N1.h>

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
NessoBattery nessoBattery;

constexpr uint16_t COLOR_BLACK = 0x0000;
constexpr uint16_t COLOR_WHITE = 0xFFFF;
constexpr uint16_t COLOR_CYAN = 0x07FF;
constexpr uint16_t COLOR_GREEN = 0x07E0;
constexpr uint16_t COLOR_YELLOW = 0xFFE0;
constexpr uint16_t COLOR_RED = 0xF800;

volatile bool packetReceivedFlag = false;
bool radioReady = false;
bool displayReady = false;

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
bool previousKey2Pressed = false;

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

String clipped(const String& value, size_t maxLength) {
  if (value.length() <= maxLength) {
    return value;
  }
  if (maxLength < 4) {
    return value.substring(0, maxLength);
  }
  return value.substring(0, maxLength - 3) + "...";
}

void drawDisplayLine(const String& text, int32_t y, uint16_t color = COLOR_WHITE) {
  if (!displayReady) {
    return;
  }
  String line = clipped(text, 38);
  nessoDisplay.setTextColor(color);
  nessoDisplay.drawString(line.c_str(), 0, y);
}

void redrawDisplay() {
  if (!displayReady) {
    return;
  }

  nessoDisplay.fillScreen(COLOR_BLACK);
  nessoDisplay.setTextSize(2);
  drawDisplayLine("N1 LoRa", 0, COLOR_CYAN);
  nessoDisplay.setTextSize(1);
  drawDisplayLine(String("ID: ") + nodeId, 25, COLOR_WHITE);
  drawDisplayLine(String("Peer: ") + (peerId.length() ? peerId : "searching"), 39, COLOR_YELLOW);
  drawDisplayLine(String("RX: ") + (lastReceivedText.length() ? lastReceivedText : "(none)"), 57, COLOR_GREEN);
  drawDisplayLine(String("RSSI ") + String(lastReceivedRssi, 1) + " SNR " + String(lastReceivedSnr, 1), 75, COLOR_WHITE);
  drawDisplayLine("A=text  B=telemetry", 105, COLOR_CYAN);
}

void printRadioError(const char* operation, int state) {
  Serial.print(F("[LoRa] "));
  Serial.print(operation);
  Serial.print(F(" failed, code "));
  Serial.println(state);
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

  nessoDisplay.setRotation(1);
  nessoDisplay.setTextWrap(false);
  redrawDisplay();
}

void setupRadio() {
  // Nesso N1 uses a 3.0 V TCXO and the board support uses the SX1262 LDO
  // regulator configuration.
  radio.tcxoVoltage = LORA_TCXO_VOLTAGE;
  radio.useRegulatorLDO = true;

  Serial.print(F("[LoRa] Initializing SX1262 at "));
  Serial.print(LORA_FREQUENCY_MHZ, 3);
  Serial.println(F(" MHz"));

  int state = radio.begin(LORA_FREQUENCY_MHZ, LORA_BANDWIDTH_KHZ, LORA_SPREADING_FACTOR, LORA_CODING_RATE, LORA_SYNC_WORD, LORA_TX_POWER_DBM, LORA_PREAMBLE_SYMBOLS, LORA_TCXO_VOLTAGE, true);

  if (state != RADIOLIB_ERR_NONE) {
    printRadioError("begin", state);
    return;
  }

  radio.setPacketReceivedAction(onRadioPacketReceived);
  state = radio.startReceive();
  if (state != RADIOLIB_ERR_NONE) {
    printRadioError("startReceive", state);
    return;
  }

  radioReady = true;
  Serial.println(F("[LoRa] ready and listening"));
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

  const uint32_t now = millis();
  if (lastTxAt != 0 && static_cast<uint32_t>(now - lastTxAt) < TX_MIN_INTERVAL_MS) {
    Serial.println(F("[LoRa] transmit deferred by duty-cycle guard"));
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

  int state = radio.standby();
  if (state != RADIOLIB_ERR_NONE) {
    printRadioError("standby before transmit", state);
    return false;
  }

  setTransmitting(true);
  state = radio.transmit(payload);
  setTransmitting(false);
  lastTxAt = millis();

  const int receiveState = radio.startReceive();
  if (state != RADIOLIB_ERR_NONE) {
    printRadioError("transmit", state);
  }
  if (receiveState != RADIOLIB_ERR_NONE) {
    printRadioError("restart receive", receiveState);
  }

  return state == RADIOLIB_ERR_NONE && receiveState == RADIOLIB_ERR_NONE;
}

String targetPeerOrBroadcast() {
  return peerId.length() ? peerId : "*";
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

void sendHello() {
  sendPacket('H', "*", "hello", txSequence++);
}

void sendPing() {
  const uint32_t sequence = txSequence++;
  sendPacket('P', targetPeerOrBroadcast(), "ping", sequence);
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

void sendTelemetry() {
  const uint32_t sequence = txSequence++;
  if (sendPacket('S', targetPeerOrBroadcast(), localTelemetry(), sequence)) {
    Serial.println(F("[LoRa] telemetry sent"));
  }
}

void sendAcknowledgement(const ReceivedPacket& packet) {
  String body = "T:";
  body += String(packet.sequence);
  sendPacket('A', packet.source, body, txSequence++);
}

void sendPong(const ReceivedPacket& packet) {
  String body = "P:";
  body += String(packet.sequence);
  body += ";pong";
  sendPacket('R', packet.source, body, txSequence++);
}

void printStatus() {
  Serial.println();
  Serial.println(F("--- Nesso N1 LoRa status ---"));
  Serial.print(F("Node:        "));
  Serial.println(nodeId);
  Serial.print(F("Peer:        "));
  Serial.println(peerId.length() ? peerId : "(not discovered)");
  Serial.print(F("Frequency:   "));
  Serial.print(LORA_FREQUENCY_MHZ, 3);
  Serial.println(F(" MHz"));
  Serial.print(F("Modulation:  BW "));
  Serial.print(LORA_BANDWIDTH_KHZ, 1);
  Serial.print(F(" kHz, SF"));
  Serial.print(LORA_SPREADING_FACTOR);
  Serial.print(F(", CR 4/"));
  Serial.println(LORA_CODING_RATE);
  Serial.print(F("TX power:    "));
  Serial.print(LORA_TX_POWER_DBM);
  Serial.println(F(" dBm"));
  Serial.print(F("Radio:       "));
  Serial.println(radioReady ? "ready" : "not ready");
  Serial.print(F("Battery:     "));
  Serial.print(nessoBattery.getVoltage(), 2);
  Serial.print(F(" V, "));
  Serial.print(nessoBattery.getChargeLevel());
  Serial.println(F("%"));
  Serial.println(F("----------------------------"));
}

void runChannelActivityDetection() {
  if (!radioReady) {
    Serial.println(F("[LoRa] CAD skipped: radio is not ready"));
    return;
  }

  int state = radio.standby();
  if (state != RADIOLIB_ERR_NONE) {
    printRadioError("standby before CAD", state);
    return;
  }

  const int scanState = radio.scanChannel();
  if (scanState == RADIOLIB_CHANNEL_FREE) {
    Serial.println(F("[LoRa] CAD: channel free"));
  } else if (scanState == RADIOLIB_PREAMBLE_DETECTED) {
    Serial.println(F("[LoRa] CAD: LoRa preamble detected"));
  } else {
    printRadioError("CAD", scanState);
  }

  state = radio.startReceive();
  if (state != RADIOLIB_ERR_NONE) {
    printRadioError("restart receive after CAD", state);
  }
}

void handleReceivedPacket(const String& raw, float rssi, float snr) {
  ReceivedPacket packet;
  if (!parsePacket(raw, packet)) {
    Serial.print(F("[LoRa] ignored malformed packet: "));
    Serial.println(raw);
    return;
  }
  if (packet.source == nodeId || !isForThisNode(packet)) {
    return;
  }

  peerId = packet.source;
  lastReceivedRssi = rssi;
  lastReceivedSnr = snr;

  const bool duplicate = haveLastPacket && packet.source == lastPacketSource && packet.sequence == lastPacketSequence;
  if (!duplicate) {
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
    const float rssi = radio.getRSSI();
    const float snr = radio.getSNR();
    handleReceivedPacket(raw, rssi, snr);
  } else if (state == RADIOLIB_ERR_CRC_MISMATCH) {
    Serial.println(F("[LoRa] CRC mismatch"));
  } else {
    printRadioError("readData", state);
  }

  // This also covers a received packet for which an ACK was deferred by the
  // transmit interval guard.
  const int receiveState = radio.startReceive();
  if (receiveState != RADIOLIB_ERR_NONE) {
    printRadioError("restart receive after packet", receiveState);
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
    return;
  }

  if (command.startsWith("t ")) {
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
  } else {
    Serial.println(F("[LoRa] unknown command; type ? for help"));
  }
}

void pollSerial() {
  while (Serial.available() > 0) {
    const char character = static_cast<char>(Serial.read());
    if (character == '\n' || character == '\r') {
      handleCommand(serialLine);
      serialLine = "";
    } else if (serialLine.length() < 180) {
      serialLine += character;
    }
  }
}

void pollButtons() {
  const bool key1Pressed = digitalRead(KEY1) == LOW;
  const bool key2Pressed = digitalRead(KEY2) == LOW;
  const uint32_t now = millis();

  if (now - lastButtonEvent >= 250) {
    if (key1Pressed && !previousKey1Pressed) {
      lastButtonEvent = now;
      sendText(String("Button A from ") + nodeId);
    } else if (key2Pressed && !previousKey2Pressed) {
      lastButtonEvent = now;
      sendTelemetry();
    }
  }

  previousKey1Pressed = key1Pressed;
  previousKey2Pressed = key2Pressed;
}

void runPeriodicTasks() {
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
  setupNessoIo();
  setupDisplay();
  setupRadio();

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
  pollButtons();
  retryPendingText();
  runPeriodicTasks();
  delay(2);
}
