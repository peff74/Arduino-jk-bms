// Decode von hier: https://github.com/syssi/esphome-jk-bms/blob/main/components/jk_bms_NimBLE/jk_bms_NimBLE.cpp#L11


static const char *const TAG = "jk_bms_NimBLE";

static const uint8_t FRAME_VERSION_JK04 = 0x01;
static const uint8_t FRAME_VERSION_JK02 = 0x02;
static const uint8_t FRAME_VERSION_JK02_32S = 0x03;

//static const uint16_t JK_BMS_SERVICE_UUID = 0xFFE0;
//static const uint16_t JK_BMS_CHARACTERISTIC_UUID = 0xFFE1;

static const uint8_t COMMAND_CELL_INFO = 0x96;
static const uint8_t COMMAND_DEVICE_INFO = 0x97;

static const uint16_t MIN_RESPONSE_SIZE = 300;
static const uint16_t MAX_RESPONSE_SIZE = 320;

static const uint8_t ERRORS_SIZE = 16;
static const char *const ERRORS[ERRORS_SIZE] = {
  "Charge Overtemperature",               // 0000 0000 0000 0001
  "Charge Undertemperature",              // 0000 0000 0000 0010
  "Error 0x00 0x04",                      // 0000 0000 0000 0100
  "Cell Undervoltage",                    // 0000 0000 0000 1000
  "Error 0x00 0x10",                      // 0000 0000 0001 0000
  "Error 0x00 0x20",                      // 0000 0000 0010 0000
  "Error 0x00 0x40",                      // 0000 0000 0100 0000
  "Error 0x00 0x80",                      // 0000 0000 1000 0000
  "Error 0x01 0x00",                      // 0000 0001 0000 0000
  "Error 0x02 0x00",                      // 0000 0010 0000 0000
  "Cell count is not equal to settings",  // 0000 0100 0000 0000
  "Current sensor anomaly",               // 0000 1000 0000 0000
  "Cell Overvoltage",                     // 0001 0000 0000 0000
  "Error 0x20 0x00",                      // 0010 0000 0000 0000
  "Charge overcurrent protection",        // 0100 0000 0000 0000
  "Error 0x80 0x00",                      // 1000 0000 0000 0000
};

enum ProtocolVersion {
  PROTOCOL_VERSION_JK04,
  PROTOCOL_VERSION_JK02,
  PROTOCOL_VERSION_JK02_32S,
};

class JkBmsNimBLE {
public:
  char *Geraetename = "";
  bool connected = false;
  bool doConnect = false;
  bool settingsOK = false;
  unsigned long sendingtime = 0;
  unsigned long receivetime = 0;
  uint wd = 0;
  uint wdold = 0;
  NimBLERemoteCharacteristic *pRemoteCharacteristic;
  const NimBLEAdvertisedDevice *myDevice;
  ProtocolVersion protocol_version_{ PROTOCOL_VERSION_JK02_32S };
  void write_register(uint8_t address, uint32_t value, uint8_t length);
  void notifyCallback(NimBLERemoteCharacteristic *pNimBLERemoteCharacteristic, uint8_t *pData, size_t length, bool isNotify);
  void assemNimBLE_(const uint8_t *data, uint16_t length);
  void decode_(const std::vector<uint8_t> &data);
  void decode_device_info_(const std::vector<uint8_t> &data);
  void decode_jk02_cell_info_(const std::vector<uint8_t> &data);
  void decode_jk04_cell_info_(const std::vector<uint8_t> &data);
  void decode_jk02_settings_(const std::vector<uint8_t> &data);
  void decode_jk04_settings_(const std::vector<uint8_t> &data);
private:
  std::vector<uint8_t> frame_buffer_;
};



uint8_t crc(const uint8_t data[], const uint16_t len) {
  uint8_t crc = 0;
  for (uint16_t i = 0; i < len; i++) {
    crc = crc + data[i];
  }
  return crc;
}

float ieee_float_(uint32_t f) {
  static_assert(sizeof(float) == sizeof f, "`float` has a weird size.");
  float ret;
  memcpy(&ret, &f, sizeof(float));
  return ret;
}

// write_register(COMMAND_CELL_INFO, 0x00000000, 0x00);
// write_register(COMMAND_DEVICE_INFO, 0x00000000, 0x00);
void JkBmsNimBLE::write_register(uint8_t address, uint32_t value, uint8_t length) {

  // https://github.com/jblance/mpp-solar/issues/170#issuecomment-1050503970

  /*
aa5590eb 1d 04 01000000 000000000000000000 9c --> Set Charge ON (example)
|        |  |  |        |                  |
|        |  |  |        |                  +--> LSB of SUM
|        |  |  |        +---------------------> Dummy Data (usually random data)
|        |  |  +------------------------------> Value (little endian)
|        |  +---------------------------------> Size of value in byte
|        +------------------------------------> Register address to be modified
+---------------------------------------------> Header (fixed to 0xAA5590EB)

AD  # VALUE
================
1d 04 01000000  Set Charge on (off = 00000000)
1e 04 01000000  Set Disharge on (off = 00000000)
1f 04 01000000  Set Balance on (off = 00000000)
06 04 03000000  Set balance trig voltage to 0.003
21 04 42d10000  Set cal voltage to 53.57
24 04 64000000  Set cal current to 0.100
1c 04 10000000  Set cell count to 16
20 04 f0ba0400  Set battery cap to 310
04 04 420e0000  Set Cell OVP to 3.65
05 04 b0040000  Set Cell OVPR to 1.2
03 04 b0040000  Set Cell UVPR to 1.2
02 04 b0040000  Set Cell UVP to 1.2
0b 04 b0040000  Set Power Off value to 1.2
9f 04 54657374  Set User Private Data (4 bytes) to TEST
9f 0d 30313233343536373839303132 c6  Set User Private Data (4 bytes) to 0123456789012
26 04 b0040000  Set Start Balance voltage to 1.2
13 04 2c010000  Set max Balance current to 0.3
0c 04 e8030000  Set Max charge Current to 1.0
0d 04 02000000  Set Charge OCP delay to 2
0e 04 02000000  Set Charge OCPR Time to 2
0f 04 e8030000  Set Max discharge Current to 1.0
10 04 02000000  Set Discharge OCP delay to 2
11 04 02000000  Set Discharge OCPR Time to 2
25 04 0a000000  Set to SCP Delay to 10.000
12 04 02000000  Set SCPR Time to 2
14 04 2c010000  Set Charge OTP to 30
15 04 2c010000  Set Charge OTPR to 30
16 04 2c010000  Set Discharge OTP to 30
17 04 2c010000  Set Discharge OTPR to 30
18 04 00000000  Set Charge UTP to 0
19 04 00000000  Set Charge UTPR to 0
27 04 00000000  Set Con. Wire Res.01 to 0.00
...
3e 04 00000000  Set Con. Wire Res.24 to 0.00
*/


  uint8_t frame[20];
  frame[0] = 0xAA;     // start sequence
  frame[1] = 0x55;     // start sequence
  frame[2] = 0x90;     // start sequence
  frame[3] = 0xEB;     // start sequence
  frame[4] = address;  // holding register
  frame[5] = length;   // size of the value in byte
  frame[6] = value >> 0;
  frame[7] = value >> 8;
  frame[8] = value >> 16;
  frame[9] = value >> 24;
  frame[10] = 0x00;
  frame[11] = 0x00;
  frame[12] = 0x00;
  frame[13] = 0x00;
  frame[14] = 0x00;
  frame[15] = 0x00;
  frame[16] = 0x00;
  frame[17] = 0x00;
  frame[18] = 0x00;
  frame[19] = crc(frame, sizeof(frame) - 1);
  Serial.println("writeValue");
  pRemoteCharacteristic->writeValue(frame, 20);
  sendingtime = millis();
}


void JkBmsNimBLE::notifyCallback(NimBLERemoteCharacteristic *pNimBLERemoteCharacteristic, uint8_t *pData, size_t length, bool isNotify) {

  Serial.print("Notify callback for characteristic ");
  Serial.print(pNimBLERemoteCharacteristic->getUUID().toString().c_str());
  Serial.print(" of data length ");
  Serial.println(length);
  Serial.print("data: ");
  //Serial.write(pData, length);
  for (int i = 0; i < length; i++) {
    Serial.print(pData[i], HEX);
    Serial.print(", ");
  }
  Serial.println();

  this->assemNimBLE_(pData, length);
}

void JkBmsNimBLE::assemNimBLE_(const uint8_t *data, uint16_t length) {
  Serial.print("assemNimBLE_");
  if (this->frame_buffer_.size() > MAX_RESPONSE_SIZE) {
    ESP_LOGW(TAG, "Frame dropped because of invalid length");
    this->frame_buffer_.clear();
  }

  // Flush buffer on every preamNimBLE
  if (data[0] == 0x55 && data[1] == 0xAA && data[2] == 0xEB && data[3] == 0x90) {
    this->frame_buffer_.clear();
  }

  this->frame_buffer_.insert(this->frame_buffer_.end(), data, data + length);

  if (this->frame_buffer_.size() >= MIN_RESPONSE_SIZE) {
    const uint8_t *raw = &this->frame_buffer_[0];
    // Even if the frame is 320 bytes long the CRC is at position 300 in front of 0xAA 0x55 0x90 0xEB
    const uint16_t frame_size = 300;  // this->frame_buffer_.size();

    uint8_t computed_crc = crc(raw, frame_size - 1);
    uint8_t remote_crc = raw[frame_size - 1];
    if (computed_crc != remote_crc) {
      ESP_LOGW(TAG, "CRC check failed! 0x%02X != 0x%02X", computed_crc, remote_crc);
      this->frame_buffer_.clear();
      return;
    }

    std::vector<uint8_t> data2(this->frame_buffer_.begin(), this->frame_buffer_.end());

    this->decode_(data2);
    this->frame_buffer_.clear();
  }
};


void JkBmsNimBLE::decode_(const std::vector<uint8_t> &data) {
  //this->reset_online_status_tracker_();
  Serial.print("decode_");

  uint8_t frame_type = data[4];
  Serial.println(frame_type);
  switch (frame_type) {
    case 0x01:
    Serial.print("case 0x01");
      if (this->protocol_version_ == PROTOCOL_VERSION_JK04) {
        this->decode_jk04_settings_(data);
      } else {
        this->decode_jk02_settings_(data);
      }
      break;
    case 0x02:
    Serial.print("case 0x02");
      if (this->protocol_version_ == PROTOCOL_VERSION_JK04) {
        this->decode_jk04_cell_info_(data);
      } else {
        this->decode_jk02_cell_info_(data);
        Serial.print("decode_jk02_cell_info_");
      }
      break;
    case 0x03:
    Serial.print("case 0x03");
      this->decode_device_info_(data);
      break;
    default:
      Serial.printf(TAG, "Unsupported message type (0x%02X)", data[4]);
  }
}

void JkBmsNimBLE::decode_jk02_cell_info_(const std::vector<uint8_t> &data) {
  auto jk_get_16bit = [&](size_t i) -> uint16_t {
    return (uint16_t(data[i + 1]) << 8) | (uint16_t(data[i + 0]) << 0);
  };
  auto jk_get_32bit = [&](size_t i) -> uint32_t {
    return (uint32_t(jk_get_16bit(i + 2)) << 16) | (uint32_t(jk_get_16bit(i + 0)) << 0);
  };

  //const uint32_t now = millis();
  //if (now - this->last_cell_info_ < this->throttle_) {
  //   return;
  // }
  // this->last_cell_info_ = now;

  uint8_t offset = 0;
  uint8_t frame_version = FRAME_VERSION_JK02;
  if (this->protocol_version_ == PROTOCOL_VERSION_JK02) {
    // Weak assumption: The value of data[189] (JK02) or data[189+32] (JK02_32S) is 0x01, 0x02 or 0x03
    if (data[189] == 0x00 && data[189 + 32] > 0) {
      frame_version = FRAME_VERSION_JK02_32S;
      offset = 16;
      Serial.printf("You hit the unstaNimBLE auto detection of the protocol version. This feature will be removed in future!"
               "Please update your configuration to protocol version JK02_32S if you are using a JK-B2A8S20P v11+");
    }
  }

  // Override unstaNimBLE auto detection
  if (this->protocol_version_ == PROTOCOL_VERSION_JK02_32S) {
    frame_version = FRAME_VERSION_JK02_32S;
    offset = 16;
  }

  Serial.printf("Cell info frame (version %d, %d bytes) received", frame_version, data.size());
  //ESP_LOGVV(TAG, "  %s", format_hex_pretty(&data.front(), 150).c_str());
  //ESP_LOGVV(TAG, "  %s", format_hex_pretty(&data.front() + 150, data.size() - 150).c_str());

  // 6 example responses (128+128+44 = 300 bytes per frame)
  //
  //
  // 55.AA.EB.90.02.8C.FF.0C.01.0D.01.0D.FF.0C.01.0D.01.0D.FF.0C.01.0D.01.0D.01.0D.01.0D.FF.0C.01.0D.01.0D.01.0D.01.0D.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.FF.FF.00.00.00.0D.00.00.00.00.9D.01.96.01.8C.01.87.01.84.01.84.01.83.01.84.01.85.01.81.01.83.01.86.01.82.01.82.01.83.01.85.01.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.03.D0.00.00.00.00.00.00.00.00
  // 00.00.BE.00.BF.00.D2.00.00.00.00.00.00.54.8E.0B.01.00.68.3C.01.00.00.00.00.00.3D.04.00.00.64.00.79.04.CA.03.10.00.01.01.AA.06.00.00.00.00.00.00.00.00.00.00.00.00.07.00.01.00.00.00.D5.02.00.00.00.00.AE.D6.3B.40.00.00.00.00.58.AA.FD.FF.00.00.00.01.00.02.00.00.EC.E6.4F.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00
  // 00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.CD
  //
  // 55.AA.EB.90.02.8D.FF.0C.01.0D.01.0D.01.0D.01.0D.01.0D.FF.0C.01.0D.01.0D.01.0D.FF.0C.FF.0C.01.0D.01.0D.01.0D.01.0D.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.FF.FF.00.00.00.0D.00.00.00.00.9D.01.96.01.8C.01.87.01.84.01.84.01.83.01.84.01.85.01.81.01.83.01.86.01.82.01.82.01.83.01.85.01.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.04.D0.00.00.00.00.00.00.00.00
  // 00.00.BE.00.BF.00.D2.00.00.00.00.00.00.54.8E.0B.01.00.68.3C.01.00.00.00.00.00.3D.04.00.00.64.00.79.04.CA.03.10.00.01.01.AA.06.00.00.00.00.00.00.00.00.00.00.00.00.07.00.01.00.00.00.D5.02.00.00.00.00.AE.D6.3B.40.00.00.00.00.58.AA.FD.FF.00.00.00.01.00.02.00.00.F0.E6.4F.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00
  // 00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.D3
  //
  // 55.AA.EB.90.02.8E.FF.0C.01.0D.01.0D.FF.0C.01.0D.01.0D.FF.0C.01.0D.01.0D.01.0D.FF.0C.FF.0C.01.0D.01.0D.01.0D.01.0D.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.FF.FF.00.00.00.0D.00.00.00.00.9D.01.96.01.8C.01.87.01.84.01.84.01.83.01.84.01.85.01.81.01.83.01.86.01.82.01.82.01.83.01.85.01.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.04.D0.00.00.00.00.00.00.00.00
  // 00.00.BE.00.BF.00.D2.00.00.00.00.00.00.54.8E.0B.01.00.68.3C.01.00.00.00.00.00.3D.04.00.00.64.00.79.04.CA.03.10.00.01.01.AA.06.00.00.00.00.00.00.00.00.00.00.00.00.07.00.01.00.00.00.D5.02.00.00.00.00.AE.D6.3B.40.00.00.00.00.58.AA.FD.FF.00.00.00.01.00.02.00.00.F5.E6.4F.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00
  // 00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.D6
  //
  // 55.AA.EB.90.02.91.FF.0C.FF.0C.01.0D.FF.0C.01.0D.01.0D.FF.0C.01.0D.01.0D.01.0D.01.0D.FF.0C.01.0D.01.0D.01.0D.01.0D.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.FF.FF.00.00.00.0D.00.00.00.00.9D.01.96.01.8C.01.87.01.84.01.84.01.83.01.84.01.85.01.81.01.83.01.86.01.82.01.82.01.83.01.85.01.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.01.D0.00.00.00.00.00.00.00.00
  // 00.00.BF.00.C0.00.D2.00.00.00.00.00.00.54.8E.0B.01.00.68.3C.01.00.00.00.00.00.3D.04.00.00.64.00.79.04.CC.03.10.00.01.01.AA.06.00.00.00.00.00.00.00.00.00.00.00.00.07.00.01.00.00.00.D5.02.00.00.00.00.AE.D6.3B.40.00.00.00.00.58.AA.FD.FF.00.00.00.01.00.02.00.00.01.E7.4F.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00
  // 00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.E7
  //
  // 55.AA.EB.90.02.92.01.0D.01.0D.01.0D.01.0D.01.0D.01.0D.FF.0C.01.0D.01.0D.01.0D.01.0D.FF.0C.01.0D.01.0D.01.0D.01.0D.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.FF.FF.00.00.00.0D.00.00.00.00.9D.01.96.01.8C.01.87.01.84.01.84.01.83.01.84.01.85.01.81.01.83.01.86.01.82.01.82.01.83.01.85.01.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.03.D0.00.00.00.00.00.00.00.00
  // 00.00.BF.00.C0.00.D2.00.00.00.00.00.00.54.8E.0B.01.00.68.3C.01.00.00.00.00.00.3D.04.00.00.64.00.79.04.CC.03.10.00.01.01.AA.06.00.00.00.00.00.00.00.00.00.00.00.00.07.00.01.00.00.00.D5.02.00.00.00.00.AE.D6.3B.40.00.00.00.00.58.AA.FD.FF.00.00.00.01.00.02.00.00.06.E7.4F.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00
  // 00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.F8
  //
  // 55.AA.EB.90.02.93.FF.0C.01.0D.01.0D.01.0D.01.0D.01.0D.FF.0C.01.0D.01.0D.01.0D.FF.0C.FF.0C.01.0D.01.0D.01.0D.01.0D.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.FF.FF.00.00.00.0D.00.00.00.00.9D.01.96.01.8C.01.87.01.84.01.84.01.83.01.84.01.85.01.81.01.83.01.86.01.82.01.82.01.83.01.85.01.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.04.D0.00.00.00.00.00.00.00.00
  // 00.00.BE.00.C0.00.D2.00.00.00.00.00.00.54.8E.0B.01.00.68.3C.01.00.00.00.00.00.3D.04.00.00.64.00.79.04.CD.03.10.00.01.01.AA.06.00.00.00.00.00.00.00.00.00.00.00.00.07.00.01.00.00.00.D5.02.00.00.00.00.AE.D6.3B.40.00.00.00.00.58.AA.FD.FF.00.00.00.01.00.02.00.00.0A.E7.4F.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00
  // 00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.F8
  //
  // Byte Len  Payload                Content              Coeff.      Unit        Example value
  // 0     2   0x55 0xAA 0xEB 0x90    Header
  // 4     1   0x02                   Record type
  // 5     1   0x8C                   Frame counter
  // 6     2   0xFF 0x0C              Voltage cell 01       0.001        V
  // 8     2   0x01 0x0D              Voltage cell 02       0.001        V
  // 10    2   0x01 0x0D              Voltage cell 03       0.001        V
  // ...
  uint8_t cells = 24 + (offset / 2);
  float min_cell_voltage = 100.0f;
  float max_cell_voltage = -100.0f;
  for (uint8_t i = 0; i < cells; i++) {
    float cell_voltage = (float)jk_get_16bit(i * 2 + 6) * 0.001f;
    float cell_resistance = (float)jk_get_16bit(i * 2 + 64 + offset) * 0.001f;
    if (cell_voltage > 0 && cell_voltage < min_cell_voltage) {
      min_cell_voltage = cell_voltage;
    }
    if (cell_voltage > max_cell_voltage) {
      max_cell_voltage = cell_voltage;
    }
    //this->publish_state_(this->cells_[i].cell_voltage_sensor_, cell_voltage);
    //this->publish_state_(this->cells_[i].cell_resistance_sensor_, cell_resistance);
    Serial.printf("  Cell Voltage%d: %f V", i, cell_voltage);
    Serial.printf("  Cell Resistance%d: %f Ohm", i, cell_resistance);
  }
  //this->publish_state_(this->min_cell_voltage_sensor_, min_cell_voltage);
  //this->publish_state_(this->max_cell_voltage_sensor_, max_cell_voltage);
  Serial.printf("  Cell MinVoltage: %f V", min_cell_voltage);
  Serial.printf("  Cell MaxVoltage: %f V", max_cell_voltage);


  // 54    4   0xFF 0xFF 0x00 0x00    EnaNimBLEd cells bitmask
  //           0x0F 0x00 0x00 0x00    4 cells enaNimBLEd
  //           0xFF 0x00 0x00 0x00    8 cells enaNimBLEd
  //           0xFF 0x0F 0x00 0x00    12 cells enaNimBLEd
  //           0xFF 0x1F 0x00 0x00    13 cells enaNimBLEd
  //           0xFF 0xFF 0x00 0x00    16 cells enaNimBLEd
  //           0xFF 0xFF 0xFF 0x00    24 cells enaNimBLEd
  //           0xFF 0xFF 0xFF 0xFF    32 cells enaNimBLEd
  Serial.printf("EnaNimBLEd cells bitmask: 0x%02X 0x%02X 0x%02X 0x%02X", data[54 + offset], data[55 + offset],
           data[56 + offset], data[57 + offset]);

  // 58    2   0x00 0x0D              Average Cell Voltage  0.001        V
  //this->publish_state_(this->average_cell_voltage_sensor_, (float)jk_get_16bit(58 + offset) * 0.001f);

  // 60    2   0x00 0x00              Delta Cell Voltage    0.001        V
  //this->publish_state_(this->delta_cell_voltage_sensor_, (float)jk_get_16bit(60 + offset) * 0.001f);

  // 62    1   0x00                   Max voltage cell      1
  //this->publish_state_(this->max_voltage_cell_sensor_, (float)data[62 + offset] + 1);
  // 63    1   0x00                   Min voltage cell      1
  //this->publish_state_(this->min_voltage_cell_sensor_, (float)data[63 + offset] + 1);
  // 64    2   0x9D 0x01              Resistance Cell 01    0.001        Ohm
  // 66    2   0x96 0x01              Resistance Cell 02    0.001        Ohm
  // 68    2   0x8C 0x01              Resistance Cell 03    0.001        Ohm
  // ...
  // 110   2   0x00 0x00              Resistance Cell 24    0.001        Ohm

  offset = offset * 2;

  // 112   2   0x00 0x00              Unknown112
  if (frame_version == FRAME_VERSION_JK02_32S) {
    //this->publish_state_(this->power_tube_temperature_sensor_, (float)((int16_t)jk_get_16bit(112 + offset)) * 0.1f);
  } else {
    Serial.printf("Unknown112: 0x%02X 0x%02X", data[112 + offset], data[113 + offset]);
  }

  // 114   4   0x00 0x00 0x00 0x00    Wire resistance warning bitmask (each bit indicates a warning per cell / wire)
  Serial.printf("Wire resistance warning bitmask: 0x%02X 0x%02X 0x%02X 0x%02X", data[114 + offset], data[115 + offset],
           data[116 + offset], data[117 + offset]);

  // 118   4   0x03 0xD0 0x00 0x00    Battery voltage       0.001        V
  float total_voltage = (float)jk_get_32bit(118 + offset) * 0.001f;
  //this->publish_state_(this->total_voltage_sensor_, total_voltage);

  // 122   4   0x00 0x00 0x00 0x00    Battery power         0.001        W
  // 126   4   0x00 0x00 0x00 0x00    Charge current        0.001        A
  float current = (float)((int32_t)jk_get_32bit(126 + offset)) * 0.001f;
  //this->publish_state_(this->current_sensor_, (float)((int32_t)jk_get_32bit(126 + offset)) * 0.001f);

  // Don't use byte 122 because it's unsigned
  // float power = (float) ((int32_t) jk_get_32bit(122 + offset)) * 0.001f;
  float power = total_voltage * current;
  //this->publish_state_(this->power_sensor_, power);
  //this->publish_state_(this->charging_power_sensor_, std::max(0.0f, power));               // 500W vs 0W -> 500W
  //this->publish_state_(this->discharging_power_sensor_, std::abs(std::min(0.0f, power)));  // -500W vs 0W -> 500W

  // 130   2   0xBE 0x00              Temperature Sensor 1  0.1          °C
  //this->publish_state_(this->temperature_sensor_1_sensor_, (float)((int16_t)jk_get_16bit(130 + offset)) * 0.1f);

  // 132   2   0xBF 0x00              Temperature Sensor 2  0.1          °C
  //this->publish_state_(this->temperature_sensor_2_sensor_, (float)((int16_t)jk_get_16bit(132 + offset)) * 0.1f);

  // 134   2   0xD2 0x00              MOS Temperature       0.1          °C
  if (frame_version == FRAME_VERSION_JK02_32S) {
    uint16_t raw_errors_bitmask = (uint16_t(data[134 + offset]) << 8) | (uint16_t(data[134 + 1 + offset]) << 0);
    //this->publish_state_(this->errors_bitmask_sensor_, (float)raw_errors_bitmask);
    //this->publish_state_(this->errors_text_sensor_, this->error_bits_to_string_(raw_errors_bitmask));
  } else {
    //this->publish_state_(this->power_tube_temperature_sensor_, (float)((int16_t)jk_get_16bit(134 + offset)) * 0.1f);
  }

  // 136   2   0x00 0x00              System alarms
  //           0x00 0x01                Charge overtemperature               0000 0000 0000 0001
  //           0x00 0x02                Charge undertemperature              0000 0000 0000 0010
  //           0x00 0x04                                                     0000 0000 0000 0100
  //           0x00 0x08                Cell Undervoltage                    0000 0000 0000 1000
  //           0x00 0x10                                                     0000 0000 0001 0000
  //           0x00 0x20                                                     0000 0000 0010 0000
  //           0x00 0x40                                                     0000 0000 0100 0000
  //           0x00 0x80                                                     0000 0000 1000 0000
  //           0x01 0x00                                                     0000 0001 0000 0000
  //           0x02 0x00                                                     0000 0010 0000 0000
  //           0x04 0x00                Cell count is not equal to settings  0000 0100 0000 0000
  //           0x08 0x00                Current sensor anomaly               0000 1000 0000 0000
  //           0x10 0x00                Cell Over Voltage                    0001 0000 0000 0000
  //           0x20 0x00                                                     0010 0000 0000 0000
  //           0x40 0x00                                                     0100 0000 0000 0000
  //           0x80 0x00                                                     1000 0000 0000 0000
  //
  //           0x14 0x00                Cell Over Voltage +                  0001 0100 0000 0000
  //                                    Cell count is not equal to settings
  //           0x04 0x08                Cell Undervoltage +                  0000 0100 0000 1000
  //                                    Cell count is not equal to settings
  if (frame_version != FRAME_VERSION_JK02_32S) {
    uint16_t raw_errors_bitmask = (uint16_t(data[136 + offset]) << 8) | (uint16_t(data[136 + 1 + offset]) << 0);
    //this->publish_state_(this->errors_bitmask_sensor_, (float)raw_errors_bitmask);
    //this->publish_state_(this->errors_text_sensor_, this->error_bits_to_string_(raw_errors_bitmask));
  }

  // 138   2   0x00 0x00              Balance current      0.001         A
  //this->publish_state_(this->balancing_current_sensor_, (float)((int16_t)jk_get_16bit(138 + offset)) * 0.001f);

  // 140   1   0x00                   Balancing action                   0x00: Off
  //                                                                     0x01: Charging balancer
  //                                                                     0x02: Discharging balancer
  //this->publish_state_(this->balancing_binary_sensor_, (data[140 + offset] != 0x00));

  // 141   1   0x54                   State of charge in   1.0           %
  //this->publish_state_(this->state_of_charge_sensor_, (float)data[141 + offset]);

  // 142   4   0x8E 0x0B 0x01 0x00    Capacity_Remain      0.001         Ah
  //this->publish_state_(this->capacity_remaining_sensor_, (float)jk_get_32bit(142 + offset) * 0.001f);

  // 146   4   0x68 0x3C 0x01 0x00    Nominal_Capacity     0.001         Ah
  //this->publish_state_(this->total_battery_capacity_setting_sensor_, (float)jk_get_32bit(146 + offset) * 0.001f);

  // 150   4   0x00 0x00 0x00 0x00    Cycle_Count          1.0
  //this->publish_state_(this->charging_cycles_sensor_, (float)jk_get_32bit(150 + offset));

  // 154   4   0x3D 0x04 0x00 0x00    Cycle_Capacity       0.001         Ah
  //this->publish_state_(this->total_charging_cycle_capacity_sensor_, (float)jk_get_32bit(154 + offset) * 0.001f);

  // 158   2   0x64 0x00              Unknown158
  Serial.printf("Unknown158: 0x%02X 0x%02X (always 0x64 0x00?)", data[158 + offset], data[159 + offset]);

  // 160   2   0x79 0x04              Unknown160 (Cycle capacity?)
  Serial.printf("Unknown160: 0x%02X 0x%02X (always 0xC5 0x09?)", data[160 + offset], data[161 + offset]);

  // 162   4   0xCA 0x03 0x10 0x00    Total runtime in seconds           s
  //this->publish_state_(this->total_runtime_sensor_, (float)jk_get_32bit(162 + offset));
  //this->publish_state_(this->total_runtime_formatted_text_sensor_, format_total_runtime_(jk_get_32bit(162 + offset)));

  // 166   1   0x01                   Charging mosfet enaNimBLEd                      0x00: off, 0x01: on
  //this->publish_state_(this->charging_binary_sensor_, (bool)data[166 + offset]);

  // 167   1   0x01                   Discharging mosfet enaNimBLEd                   0x00: off, 0x01: on
  //this->publish_state_(this->discharging_binary_sensor_, (bool)data[167 + offset]);

  //ESP_LOGD(TAG, "Unknown168: %s",
  //         format_hex_pretty(&data.front() + 168 + offset, data.size() - (168 + offset) - 4 - 81 - 1).c_str());

  // 168   1   0xAA                   Unknown168
  // 169   2   0x06 0x00              Unknown169
  // 171   2   0x00 0x00              Unknown171
  // 173   2   0x00 0x00              Unknown173
  // 175   2   0x00 0x00              Unknown175
  // 177   2   0x00 0x00              Unknown177
  // 179   2   0x00 0x00              Unknown179
  // 181   2   0x00 0x07              Unknown181
  // 183   2   0x00 0x01              Unknown183
  // 185   2   0x00 0x00              Unknown185
  // 187   2   0x00 0xD5              Unknown187
  // 189   2   0x02 0x00              Unknown189
  Serial.printf("Unknown189: 0x%02X 0x%02X", data[189], data[190]);
  // 190   1   0x00                   Unknown190
  // 191   1   0x00                   Balancer status (working: 0x01, idle: 0x00)
  // 192   1   0x00                   Unknown192
  Serial.printf("Unknown192: 0x%02X", data[192 + offset]);
  // 193   2   0x00 0xAE              Unknown193
  Serial.printf("Unknown193: 0x%02X 0x%02X (0x00 0x8D)", data[193 + offset], data[194 + offset]);
  // 195   2   0xD6 0x3B              Unknown195
  Serial.printf("Unknown195: 0x%02X 0x%02X (0x21 0x40)", data[195 + offset], data[196 + offset]);
  // 197   10  0x40 0x00 0x00 0x00 0x00 0x58 0xAA 0xFD 0xFF 0x00
  // 207   7   0x00 0x00 0x01 0x00 0x02 0x00 0x00
  // 214   4   0xEC 0xE6 0x4F 0x00    Uptime 100ms
  //
  // 218   81  0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00
  //           0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00
  //           0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00
  //           0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00
  //           0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00
  //           0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00
  //           0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00
  //           0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00
  //           0x00
  // 299   1   0xCD                   CRC

  //this->status_notification_received_ = true;
  this->receivetime = millis();
  this->wd++;
}

void JkBmsNimBLE::decode_jk04_cell_info_(const std::vector<uint8_t> &data) {

  Serial.println("decode_jk04_cell_info");

  auto jk_get_16bit = [&](size_t i) -> uint16_t {
    return (uint16_t(data[i + 1]) << 8) | (uint16_t(data[i + 0]) << 0);
  };
  auto jk_get_32bit = [&](size_t i) -> uint32_t {
    return (uint32_t(jk_get_16bit(i + 2)) << 16) | (uint32_t(jk_get_16bit(i + 0)) << 0);
  };

  //const uint32_t now = millis();
  //if (now - this->last_cell_info_ < this->throttle_) {
  //  return;
  //}
  //this->last_cell_info_ = now;

  ESP_LOGI(TAG, "Cell info frame (%d bytes) received", data.size());
  //ESP_LOGVV(TAG, "  %s", format_hex_pretty(&data.front(), 150).c_str());
  //ESP_LOGVV(TAG, "  %s", format_hex_pretty(&data.front() + 150, data.size() - 150).c_str());

  // 0x55 0xAA 0xEB 0x90 0x02 0x4B 0xC0 0x61 0x56 0x40 0x1F 0xAA 0x56 0x40 0xFF 0x91 0x56 0x40 0xFF 0x91 0x56 0x40 0x1F
  // 0xAA 0x56 0x40 0xFF 0x91 0x56 0x40 0xFF 0x91 0x56 0x40 0xFF 0x91 0x56 0x40 0x1F 0xAA 0x56 0x40 0xFF 0x91 0x56 0x40
  // 0xFF 0x91 0x56 0x40 0xFF 0x91 0x56 0x40 0xFF 0x91 0x56 0x40 0x1F 0xAA 0x56 0x40 0xE0 0x79 0x56 0x40 0xE0 0x79 0x56
  // 0x40 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00
  // 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x7C 0x1D 0x23 0x3E 0x1B 0xEB 0x08 0x3E 0x56 0xCE 0x14 0x3E 0x4D
  // 0x9B 0x15 0x3E 0xE0 0xDB 0xCD 0x3D 0x72 0x33 0xCD 0x3D 0x94 0x88 0x01 0x3E 0x5E 0x1E 0xEA 0x3D 0xE5 0x17 0xCD 0x3D
  // 0xE3 0xBB 0xD7 0x3D 0xF5 0x44 0xD2 0x3D 0xBE 0x7C 0x01 0x3E 0x27 0xB6 0x00 0x3E 0xDA 0xB5 0xFC 0x3D 0x6B 0x51 0xF8
  // 0x3D 0xA2 0x93 0xF3 0x3D 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00
  // 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x03 0x95 0x56 0x40 0x00
  // 0xBE 0x90 0x3B 0x00 0x00 0x00 0x00 0xFF 0xFF 0x00 0x00 0x01 0x00 0x00 0x01 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00
  // 0x00 0x00 0x00 0x66 0xA0 0xD2 0x4A 0x40 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00
  // 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x01 0x01 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00
  // 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x53 0x96 0x1C 0x00 0x00 0x00 0x00 0x00 0x00 0x48 0x22 0x40 0x00
  // 0x13
  //
  // Byte Len  Payload                Content              Coeff.      Unit        Example value
  // 0     4   0x55 0xAA 0xEB 0x90    Header
  // 4     1   0x02                   Frame type
  // 5     1   0x4B                   Frame counter
  // 6     4   0xC0 0x61 0x56 0x40    Cell voltage 1                     V
  // 10    4   0x1F 0xAA 0x56 0x40    Cell voltage 2                     V
  // 14    4   0xFF 0x91 0x56 0x40    Cell voltage 3                     V
  // 18    4   0xFF 0x91 0x56 0x40    Cell voltage 4                     V
  // 22    4   0x1F 0xAA 0x56 0x40    Cell voltage 5                     V
  // 26    4   0xFF 0x91 0x56 0x40    Cell voltage 6                     V
  // 30    4   0xFF 0x91 0x56 0x40    Cell voltage 7                     V
  // 34    4   0xFF 0x91 0x56 0x40    Cell voltage 8                     V
  // 38    4   0x1F 0xAA 0x56 0x40    Cell voltage 9                     V
  // 42    4   0xFF 0x91 0x56 0x40    Cell voltage 10                    V
  // 46    4   0xFF 0x91 0x56 0x40    Cell voltage 11                    V
  // 50    4   0xFF 0x91 0x56 0x40    Cell voltage 12                    V
  // 54    4   0xFF 0x91 0x56 0x40    Cell voltage 13                    V
  // 58    4   0x1F 0xAA 0x56 0x40    Cell voltage 14                    V
  // 62    4   0xE0 0x79 0x56 0x40    Cell voltage 15                    V
  // 66    4   0xE0 0x79 0x56 0x40    Cell voltage 16                    V
  // 70    4   0x00 0x00 0x00 0x00    Cell voltage 17                    V
  // 74    4   0x00 0x00 0x00 0x00    Cell voltage 18                    V
  // 78    4   0x00 0x00 0x00 0x00    Cell voltage 19                    V
  // 82    4   0x00 0x00 0x00 0x00    Cell voltage 20                    V
  // 86    4   0x00 0x00 0x00 0x00    Cell voltage 21                    V
  // 90    4   0x00 0x00 0x00 0x00    Cell voltage 22                    V
  // 94    4   0x00 0x00 0x00 0x00    Cell voltage 23                    V
  // 98    4   0x00 0x00 0x00 0x00    Cell voltage 24                    V
  // 102   4   0x7C 0x1D 0x23 0x3E    Cell resistance 1                  Ohm
  // 106   4   0x1B 0xEB 0x08 0x3E    Cell resistance 2                  Ohm
  // 110   4   0x56 0xCE 0x14 0x3E    Cell resistance 3                  Ohm
  // 114   4   0x4D 0x9B 0x15 0x3E    Cell resistance 4                  Ohm
  // 118   4   0xE0 0xDB 0xCD 0x3D    Cell resistance 5                  Ohm
  // 122   4   0x72 0x33 0xCD 0x3D    Cell resistance 6                  Ohm
  // 126   4   0x94 0x88 0x01 0x3E    Cell resistance 7                  Ohm
  // 130   4   0x5E 0x1E 0xEA 0x3D    Cell resistance 8                  Ohm
  // 134   4   0xE5 0x17 0xCD 0x3D    Cell resistance 9                  Ohm
  // 138   4   0xE3 0xBB 0xD7 0x3D    Cell resistance 10                 Ohm
  // 142   4   0xF5 0x44 0xD2 0x3D    Cell resistance 11                 Ohm
  // 146   4   0xBE 0x7C 0x01 0x3E    Cell resistance 12                 Ohm
  // 150   4   0x27 0xB6 0x00 0x3E    Cell resistance 13                 Ohm
  // 154   4   0xDA 0xB5 0xFC 0x3D    Cell resistance 14                 Ohm
  // 158   4   0x6B 0x51 0xF8 0x3D    Cell resistance 15                 Ohm
  // 162   4   0xA2 0x93 0xF3 0x3D    Cell resistance 16                 Ohm
  // 166   4   0x00 0x00 0x00 0x00    Cell resistance 17                 Ohm
  // 170   4   0x00 0x00 0x00 0x00    Cell resistance 18                 Ohm
  // 174   4   0x00 0x00 0x00 0x00    Cell resistance 19                 Ohm
  // 178   4   0x00 0x00 0x00 0x00    Cell resistance 20                 Ohm
  // 182   4   0x00 0x00 0x00 0x00    Cell resistance 21                 Ohm
  // 186   4   0x00 0x00 0x00 0x00    Cell resistance 22                 Ohm
  // 190   4   0x00 0x00 0x00 0x00    Cell resistance 23                 Ohm
  // 194   4   0x00 0x00 0x00 0x00    Cell resistance 24                 Ohm
  // 198   4   0x00 0x00 0x00 0x00    Cell resistance 25                 Ohm
  //                                  https://github.com/jblance/mpp-solar/issues/98#issuecomment-823701486
  uint8_t cells = 24;
  float min_cell_voltage = 100.0f;
  float max_cell_voltage = -100.0f;
  float total_voltage = 0.0f;
  uint8_t min_voltage_cell = 0;
  uint8_t max_voltage_cell = 0;
  for (uint8_t i = 0; i < cells; i++) {
    float cell_voltage = (float)ieee_float_(jk_get_32bit(i * 4 + 6));
    float cell_resistance = (float)ieee_float_(jk_get_32bit(i * 4 + 102));
    total_voltage = total_voltage + cell_voltage;
    if (cell_voltage > 0 && cell_voltage < min_cell_voltage) {
      min_cell_voltage = cell_voltage;
      min_voltage_cell = i + 1;
    }
    if (cell_voltage > max_cell_voltage) {
      max_cell_voltage = cell_voltage;
      max_voltage_cell = i + 1;
    }
    //this->publish_state_(this->cells_[i].cell_voltage_sensor_, cell_voltage);
    //this->publish_state_(this->cells_[i].cell_resistance_sensor_, cell_resistance);
  }

  //this->publish_state_(this->min_cell_voltage_sensor_, min_cell_voltage);
  //this->publish_state_(this->max_cell_voltage_sensor_, max_cell_voltage);
  //this->publish_state_(this->max_voltage_cell_sensor_, (float)max_voltage_cell);
  //this->publish_state_(this->min_voltage_cell_sensor_, (float)min_voltage_cell);
  //this->publish_state_(this->total_voltage_sensor_, total_voltage);

  // 202   4   0x03 0x95 0x56 0x40    Average Cell Voltage               V
  //this->publish_state_(this->average_cell_voltage_sensor_, (float)ieee_float_(jk_get_32bit(202)));

  // 206   4   0x00 0xBE 0x90 0x3B    Delta Cell Voltage                 V
  //this->publish_state_(this->delta_cell_voltage_sensor_, (float)ieee_float_(jk_get_32bit(206)));

  // 210   4   0x00 0x00 0x00 0x00    Unknown210
  ESP_LOGD(TAG, "Unknown210: 0x%02X 0x%02X 0x%02X 0x%02X (always 0x00 0x00 0x00 0x00)", data[210], data[211], data[212],
           data[213]);

  // 214   4   0xFF 0xFF 0x00 0x00    Unknown214
  ESP_LOGD(TAG, "Unknown214: 0x%02X 0x%02X 0x%02X 0x%02X (0xFF 0xFF: 24 cells?)", data[214], data[215], data[216],
           data[217]);

  // 218   1   0x01                   Unknown218
  ESP_LOGD(TAG, "Unknown218: 0x%02X", data[218]);

  // 219   1   0x00                   Unknown219
  ESP_LOGD(TAG, "Unknown219: 0x%02X", data[219]);

  // 220   1   0x00                  Blink cells (0x00: Off, 0x01: Charging balancer, 0x02: Discharging balancer)
  bool balancing = (data[220] != 0x00);
  //this->publish_state_(this->balancing_binary_sensor_, balancing);
  //this->publish_state_(this->operation_status_text_sensor_, (balancing) ? "Balancing" : "Idle");

  // 221   1   0x01                  Unknown221
  ESP_LOGD(TAG, "Unknown221: 0x%02X", data[221]);

  // 222   4   0x00 0x00 0x00 0x00    Balancing current
  //this->publish_state_(this->balancing_current_sensor_, (float)ieee_float_(jk_get_32bit(222)));

  // 226   7   0x00 0x00 0x00 0x00 0x00 0x00 0x00    Unknown226
  ESP_LOGD(TAG, "Unknown226: 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X (always 0x00...0x00?)", data[226],
           data[227], data[228], data[229], data[230], data[231], data[232]);

  // 233   4   0x66 0xA0 0xD2 0x4A    Unknown233
  ESP_LOGD(TAG, "Unknown233: 0x%02X 0x%02X 0x%02X 0x%02X (%f)", data[233], data[234], data[235], data[236],
           ieee_float_(jk_get_32bit(233)));

  // 237   4   0x40 0x00 0x00 0x00    Unknown237
  ESP_LOGD(TAG, "Unknown237: 0x%02X 0x%02X 0x%02X 0x%02X (always 0x40 0x00 0x00 0x00?)", data[237], data[238],
           data[239], data[240]);

  // 241   45  0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00
  //           0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00
  //           0x00 0x00 0x00 0x01 0x01 0x00 0x00 0x00
  //           0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00
  //           0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00
  //           0x00 0x00 0x00 0x00 0x00
  // 286   3   0x53 0x96 0x1C 0x00        Uptime
  //this->publish_state_(this->total_runtime_sensor_, (float)jk_get_32bit(286));
  //this->publish_state_(this->total_runtime_formatted_text_sensor_, format_total_runtime_(jk_get_32bit(286)));

  // 290   4   0x00 0x00 0x00 0x00    Unknown290
  ESP_LOGD(TAG, "Unknown290: 0x%02X 0x%02X 0x%02X 0x%02X (always 0x00 0x00 0x00 0x00?)", data[290], data[291],
           data[292], data[293]);

  // 294   4   0x00 0x48 0x22 0x40    Unknown294
  ESP_LOGD(TAG, "Unknown294: 0x%02X 0x%02X 0x%02X 0x%02X", data[294], data[295], data[296], data[297]);

  // 298   1   0x00                   Unknown298
  ESP_LOGD(TAG, "Unknown298: 0x%02X", data[298]);

  // 299   1   0x13                   Checksm

  //status_notification_received_ = true;
  this->receivetime = millis();
  this->wd++;
}

void JkBmsNimBLE::decode_jk02_settings_(const std::vector<uint8_t> &data) {
  auto jk_get_16bit = [&](size_t i) -> uint16_t {
    return (uint16_t(data[i + 1]) << 8) | (uint16_t(data[i + 0]) << 0);
  };
  auto jk_get_32bit = [&](size_t i) -> uint32_t {
    return (uint32_t(jk_get_16bit(i + 2)) << 16) | (uint32_t(jk_get_16bit(i + 0)) << 0);
  };

  ESP_LOGI(TAG, "Settings frame (%d bytes) received", data.size());
  //ESP_LOGVV(TAG, "  %s", format_hex_pretty(&data.front(), 160).c_str());
  //ESP_LOGVV(TAG, "  %s", format_hex_pretty(&data.front() + 160, data.size() - 160).c_str());

  // JK02 response example:
  //
  // 0x55 0xAA 0xEB 0x90 0x01 0x4F 0x58 0x02 0x00 0x00 0x54 0x0B 0x00 0x00 0x80 0x0C 0x00 0x00 0xCC 0x10 0x00 0x00 0x68
  // 0x10 0x00 0x00 0x0A 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00
  // 0xF0 0x0A 0x00 0x00 0xA8 0x61 0x00 0x00 0x1E 0x00 0x00 0x00 0x3C 0x00 0x00 0x00 0xF0 0x49 0x02 0x00 0x2C 0x01 0x00
  // 0x00 0x3C 0x00 0x00 0x00 0x3C 0x00 0x00 0x00 0xD0 0x07 0x00 0x00 0xBC 0x02 0x00 0x00 0x58 0x02 0x00 0x00 0xBC 0x02
  // 0x00 0x00 0x58 0x02 0x00 0x00 0x38 0xFF 0xFF 0xFF 0x9C 0xFF 0xFF 0xFF 0x84 0x03 0x00 0x00 0xBC 0x02 0x00 0x00 0x0D
  // 0x00 0x00 0x00 0x01 0x00 0x00 0x00 0x01 0x00 0x00 0x00 0x01 0x00 0x00 0x00 0x88 0x13 0x00 0x00 0xDC 0x05 0x00 0x00
  // 0xE4 0x0C 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00
  // 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00
  // 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00
  // 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00
  // 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00
  // 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00
  // 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00
  // 0x40

  // Byte Len  Payload                Content              Coeff.      Unit        Example value
  // 0     4   0x55 0xAA 0xEB 0x90    Header
  // 4     1   0x01                   Frame type
  // 5     1   0x4F                   Frame counter
  // 6     4   0x58 0x02 0x00 0x00    Unknown6
  ESP_LOGD(TAG, "  Unknown6: %f", (float)jk_get_32bit(6) * 0.001f);
  // 10    4   0x54 0x0B 0x00 0x00    Cell UVP
  ESP_LOGI(TAG, "  Cell UVP: %f V", (float)jk_get_32bit(10) * 0.001f);
  //this->publish_state_(this->cell_voltage_undervoltage_protection_number_, (float)jk_get_32bit(10) * 0.001f);

  // 14    4   0x80 0x0C 0x00 0x00    Cell OVP Recovery
  ESP_LOGI(TAG, "  Cell UVPR: %f V", (float)jk_get_32bit(14) * 0.001f);
  //this->publish_state_(this->cell_voltage_undervoltage_recovery_number_, (float)jk_get_32bit(14) * 0.001f);

  // 18    4   0xCC 0x10 0x00 0x00    Cell OVP
  ESP_LOGI(TAG, "  Cell OVP: %f V", (float)jk_get_32bit(18) * 0.001f);
  //this->publish_state_(this->cell_voltage_overvoltage_protection_number_, (float)jk_get_32bit(18) * 0.001f);

  // 22    4   0x68 0x10 0x00 0x00    Cell OVP Recovery
  ESP_LOGI(TAG, "  Cell OVPR: %f V", (float)jk_get_32bit(22) * 0.001f);
  //this->publish_state_(this->cell_voltage_overvoltage_recovery_number_, (float)jk_get_32bit(22) * 0.001f);

  // 26    4   0x0A 0x00 0x00 0x00    Balance trigger voltage
  ESP_LOGI(TAG, "  Balance trigger voltage: %f V", (float)jk_get_32bit(26) * 0.001f);
  //this->publish_state_(this->balance_trigger_voltage_number_, (float)jk_get_32bit(26) * 0.001f);

  // 30    4   0x00 0x00 0x00 0x00    Unknown30
  // 34    4   0x00 0x00 0x00 0x00    Unknown34
  // 38    4   0x00 0x00 0x00 0x00    Unknown38
  // 42    4   0x00 0x00 0x00 0x00    Unknown42
  // 46    4   0xF0 0x0A 0x00 0x00    Power off voltage
  ESP_LOGI(TAG, "  Power off voltage: %f V", (float)jk_get_32bit(46) * 0.001f);
  //this->publish_state_(this->power_off_voltage_number_, (float)jk_get_32bit(46) * 0.001f);

  // 50    4   0xA8 0x61 0x00 0x00    Max. charge current
  ESP_LOGI(TAG, "  Max. charge current: %f A", (float)jk_get_32bit(50) * 0.001f);
  //this->publish_state_(this->max_charge_current_number_, (float)jk_get_32bit(50) * 0.001f);

  // 54    4   0x1E 0x00 0x00 0x00    Charge OCP delay
  ESP_LOGI(TAG, "  Charge OCP delay: %f s", (float)jk_get_32bit(54));
  // 58    4   0x3C 0x00 0x00 0x00    Charge OCP recovery delay
  ESP_LOGI(TAG, "  Charge OCP recovery delay: %f s", (float)jk_get_32bit(58));
  // 62    4   0xF0 0x49 0x02 0x00    Max. discharge current
  ESP_LOGI(TAG, "  Max. discharge current: %f A", (float)jk_get_32bit(62) * 0.001f);
  //this->publish_state_(this->max_discharge_current_number_, (float)jk_get_32bit(62) * 0.001f);

  // 66    4   0x2C 0x01 0x00 0x00    Discharge OCP delay
  ESP_LOGI(TAG, "  Discharge OCP recovery delay: %f s", (float)jk_get_32bit(66));
  // 70    4   0x3C 0x00 0x00 0x00    Discharge OCP recovery delay
  ESP_LOGI(TAG, "  Discharge OCP recovery delay: %f s", (float)jk_get_32bit(70));
  // 74    4   0x3C 0x00 0x00 0x00    SCPR time
  ESP_LOGI(TAG, "  SCP recovery time: %f s", (float)jk_get_32bit(74));
  // 78    4   0xD0 0x07 0x00 0x00    Max balance current
  ESP_LOGI(TAG, "  Max. balance current: %f A", (float)jk_get_32bit(78) * 0.001f);
  //this->publish_state_(this->max_balance_current_number_, (float)jk_get_32bit(78) * 0.001f);

  // 82    4   0xBC 0x02 0x00 0x00    Charge OTP
  ESP_LOGI(TAG, "  Charge OTP: %f °C", (float)jk_get_32bit(82) * 0.1f);
  // 86    4   0x58 0x02 0x00 0x00    Charge OTP Recovery
  ESP_LOGI(TAG, "  Charge OTP recovery: %f °C", (float)jk_get_32bit(86) * 0.1f);
  // 90    4   0xBC 0x02 0x00 0x00    Discharge OTP
  ESP_LOGI(TAG, "  Discharge OTP: %f °C", (float)jk_get_32bit(90) * 0.1f);
  // 94    4   0x58 0x02 0x00 0x00    Discharge OTP Recovery
  ESP_LOGI(TAG, "  Discharge OTP recovery: %f °C", (float)jk_get_32bit(94) * 0.1f);
  // 98    4   0x38 0xFF 0xFF 0xFF    Charge UTP
  ESP_LOGI(TAG, "  Charge UTP: %f °C", (float)((int32_t)jk_get_32bit(98)) * 0.1f);
  // 102   4   0x9C 0xFF 0xFF 0xFF    Charge UTP Recovery
  ESP_LOGI(TAG, "  Charge UTP recovery: %f °C", (float)((int32_t)jk_get_32bit(102)) * 0.1f);
  // 106   4   0x84 0x03 0x00 0x00    MOS OTP
  ESP_LOGI(TAG, "  MOS OTP: %f °C", (float)((int32_t)jk_get_32bit(106)) * 0.1f);
  // 110   4   0xBC 0x02 0x00 0x00    MOS OTP Recovery
  ESP_LOGI(TAG, "  MOS OTP recovery: %f °C", (float)((int32_t)jk_get_32bit(110)) * 0.1f);
  // 114   4   0x0D 0x00 0x00 0x00    Cell count
  ESP_LOGI(TAG, "  Cell count: %f", (float)jk_get_32bit(114));
  //this->publish_state_(this->cell_count_number_, (float)data[114]);

  // 118   4   0x01 0x00 0x00 0x00    Charge switch
  ESP_LOGI(TAG, "  Charge switch: %s", ((bool)data[118]) ? "on" : "off");
  //this->publish_state_(this->charging_switch_, (bool)data[118]);

  // 122   4   0x01 0x00 0x00 0x00    Discharge switch
  ESP_LOGI(TAG, "  Discharge switch: %s", ((bool)data[122]) ? "on" : "off");
  //this->publish_state_(this->discharging_switch_, (bool)data[122]);

  // 126   4   0x01 0x00 0x00 0x00    Balancer switch
  ESP_LOGI(TAG, "  Balancer switch: %s", ((bool)data[126]) ? "on" : "off");
  //this->publish_state_(this->balancer_switch_, (bool)(data[126]));

  // 130   4   0x88 0x13 0x00 0x00    Nominal battery capacity
  ESP_LOGI(TAG, "  Nominal battery capacity: %f Ah", (float)jk_get_32bit(130) * 0.001f);
  //this->publish_state_(this->total_battery_capacity_number_, (float)jk_get_32bit(130) * 0.001f);

  // 134   4   0xDC 0x05 0x00 0x00    Unknown134
  ESP_LOGD(TAG, "  Unknown134: %f", (float)jk_get_32bit(134) * 0.001f);
  // 138   4   0xE4 0x0C 0x00 0x00    Start balance voltage
  ESP_LOGI(TAG, "  Start balance voltage: %f V", (float)jk_get_32bit(138) * 0.001f);
  //this->publish_state_(this->balance_starting_voltage_number_, (float)jk_get_32bit(138) * 0.001f);

  // 142   4   0x00 0x00 0x00 0x00
  // 146   4   0x00 0x00 0x00 0x00
  // 150   4   0x00 0x00 0x00 0x00
  // 154   4   0x00 0x00 0x00 0x00
  // 158   4   0x00 0x00 0x00 0x00    Con. wire resistance 1
  // 162   4   0x00 0x00 0x00 0x00    Con. wire resistance 2
  // 166   4   0x00 0x00 0x00 0x00    Con. wire resistance 3
  // 170   4   0x00 0x00 0x00 0x00    Con. wire resistance 4
  // 174   4   0x00 0x00 0x00 0x00    Con. wire resistance 5
  // 178   4   0x00 0x00 0x00 0x00    Con. wire resistance 6
  // 182   4   0x00 0x00 0x00 0x00    Con. wire resistance 7
  // 186   4   0x00 0x00 0x00 0x00    Con. wire resistance 8
  // 190   4   0x00 0x00 0x00 0x00    Con. wire resistance 9
  // 194   4   0x00 0x00 0x00 0x00    Con. wire resistance 10
  // 198   4   0x00 0x00 0x00 0x00    Con. wire resistance 11
  // 202   4   0x00 0x00 0x00 0x00    Con. wire resistance 12
  // 206   4   0x00 0x00 0x00 0x00    Con. wire resistance 13
  // 210   4   0x00 0x00 0x00 0x00    Con. wire resistance 14
  // 214   4   0x00 0x00 0x00 0x00    Con. wire resistance 15
  // 218   4   0x00 0x00 0x00 0x00    Con. wire resistance 16
  // 222   4   0x00 0x00 0x00 0x00    Con. wire resistance 17
  // 226   4   0x00 0x00 0x00 0x00    Con. wire resistance 18
  // 230   4   0x00 0x00 0x00 0x00    Con. wire resistance 19
  // 234   4   0x00 0x00 0x00 0x00    Con. wire resistance 20
  // 238   4   0x00 0x00 0x00 0x00    Con. wire resistance 21
  // 242   4   0x00 0x00 0x00 0x00    Con. wire resistance 22
  // 246   4   0x00 0x00 0x00 0x00    Con. wire resistance 23
  // 250   4   0x00 0x00 0x00 0x00    Con. wire resistance 24
  for (uint8_t i = 0; i < 24; i++) {
    ESP_LOGD(TAG, "  Con. wire resistance %d: %f Ohm", i + 1, (float)jk_get_32bit(i * 4 + 158) * 0.001f);
  }

  // 254   4   0x00 0x00 0x00 0x00
  // 258   4   0x00 0x00 0x00 0x00
  // 262   4   0x00 0x00 0x00 0x00
  // 266   4   0x00 0x00 0x00 0x00
  // 270   4   0x00 0x00 0x00 0x00
  // 274   4   0x00 0x00 0x00 0x00
  // 278   4   0x00 0x00 0x00 0x00
  // 282   1   0x00                   New controls bitmask
  //this->publish_state_(this->disaNimBLE_temperature_sensors_switch_, check_bit_(data[282], 2));
  //this->publish_state_(this->display_always_on_switch_, check_bit_(data[282], 16));

  // 283   3   0x00 0x00 0x00
  // 286   4   0x00 0x00 0x00 0x00
  // 290   4   0x00 0x00 0x00 0x00
  // 294   4   0x00 0x00 0x00 0x00
  // 298   1   0x00
  // 299   1   0x40                   CRC
  this->settingsOK = true;
}

void JkBmsNimBLE::decode_jk04_settings_(const std::vector<uint8_t> &data) {
  auto jk_get_16bit = [&](size_t i) -> uint16_t {
    return (uint16_t(data[i + 1]) << 8) | (uint16_t(data[i + 0]) << 0);
  };
  auto jk_get_32bit = [&](size_t i) -> uint32_t {
    return (uint32_t(jk_get_16bit(i + 2)) << 16) | (uint32_t(jk_get_16bit(i + 0)) << 0);
  };

  ESP_LOGI(TAG, "Settings frame (%d bytes) received", data.size());
  //ESP_LOGVV(TAG, "  %s", format_hex_pretty(&data.front(), 160).c_str());
  //ESP_LOGVV(TAG, "  %s", format_hex_pretty(&data.front() + 160, data.size() - 160).c_str());

  // JK04 (JK-B2A16S v3) response example:
  //
  // 0x55 0xAA 0xEB 0x90 0x01 0x50 0x00 0x00 0x80 0x3F 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00
  // 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x10 0x00 0x00 0x00 0x00 0x00 0x40 0x40 0x00 0x00 0x00 0x00
  // 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00
  // 0x00 0x00 0x00 0x00 0x00 0xA3 0xFD 0x40 0x40 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00
  // 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x88 0x40 0x9A 0x99 0x59 0x40 0x0A 0xD7 0xA3 0x3B 0x00 0x00 0x00 0x40 0x01
  // 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00
  // 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00
  // 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00
  // 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00
  // 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00
  // 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00
  // 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00
  // 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00
  // 0xCE

  // Byte Len  Payload                Content              Coeff.      Unit        Example value
  // 0     4   0x55 0xAA 0xEB 0x90    Header
  // 4     1   0x01                   Frame type
  // 5     1   0x50                   Frame counter
  // 6     4   0x00 0x00 0x80 0x3F
  ESP_LOGD(TAG, "  Unknown6: 0x%02X 0x%02X 0x%02X 0x%02X (%f)", data[6], data[7], data[8], data[9],
           (float)ieee_float_(jk_get_32bit(6)));

  // 10    4   0x00 0x00 0x00 0x00
  // 14    4   0x00 0x00 0x00 0x00
  // 18    4   0x00 0x00 0x00 0x00
  // 22    4   0x00 0x00 0x00 0x00
  // 26    4   0x00 0x00 0x00 0x00
  // 30    4   0x00 0x00 0x00 0x00
  // 34    4   0x10 0x00 0x00 0x00    Cell count
  ESP_LOGI(TAG, "  Cell count: %d", data[34]);

  // 38    4   0x00 0x00 0x40 0x40    Power off voltage
  ESP_LOGI(TAG, "  Power off voltage: %f V", (float)ieee_float_(jk_get_32bit(38)));

  // 42    4   0x00 0x00 0x00 0x00
  // 46    4   0x00 0x00 0x00 0x00
  // 50    4   0x00 0x00 0x00 0x00
  // 54    4   0x00 0x00 0x00 0x00
  // 58    4   0x00 0x00 0x00 0x00
  // 62    4   0x00 0x00 0x00 0x00
  // 66    4   0x00 0x00 0x00 0x00
  // 70    4   0x00 0x00 0x00 0x00
  // 74    4   0xA3 0xFD 0x40 0x40
  ESP_LOGD(TAG, "  Unknown74: %f", (float)ieee_float_(jk_get_32bit(74)));

  // 78    4   0x00 0x00 0x00 0x00
  // 82    4   0x00 0x00 0x00 0x00
  // 86    4   0x00 0x00 0x00 0x00
  // 90    4   0x00 0x00 0x00 0x00
  // 94    4   0x00 0x00 0x00 0x00
  // 98    4   0x00 0x00 0x88 0x40    Start balance voltage
  ESP_LOGI(TAG, "  Start balance voltage: %f V", (float)ieee_float_(jk_get_32bit(98)));

  // 102   4   0x9A 0x99 0x59 0x40
  ESP_LOGD(TAG, "  Unknown102: %f", (float)ieee_float_(jk_get_32bit(102)));

  // 106   4   0x0A 0xD7 0xA3 0x3B    Trigger delta voltage
  ESP_LOGI(TAG, "  Trigger Delta Voltage: %f V", (float)ieee_float_(jk_get_32bit(106)));

  // 110   4   0x00 0x00 0x00 0x40    Max. balance current
  ESP_LOGI(TAG, "  Max. balance current: %f A", (float)ieee_float_(jk_get_32bit(110)));
  // 114   4   0x01 0x00 0x00 0x00    Balancer switch
  //this->publish_state_(this->balancer_switch_, (bool)(data[114]));

  ESP_LOGI(TAG, "  ADC Vref: unknown V");  // 53.67 V?

  // 118  20   0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00
  // 138  20   0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00
  // 158  20   0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00
  // 178  20   0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00
  // 198  20   0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00
  // 218  20   0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00
  // 238  20   0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00
  // 258  20   0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00
  // 278  20   0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00
  // 298   2   0x00 0xCE
  this->settingsOK = true;
}

void JkBmsNimBLE::decode_device_info_(const std::vector<uint8_t> &data) {
  auto jk_get_16bit = [&](size_t i) -> uint16_t {
    return (uint16_t(data[i + 1]) << 8) | (uint16_t(data[i + 0]) << 0);
  };
  auto jk_get_32bit = [&](size_t i) -> uint32_t {
    return (uint32_t(jk_get_16bit(i + 2)) << 16) | (uint32_t(jk_get_16bit(i + 0)) << 0);
  };

  ESP_LOGI(TAG, "Device info frame (%d bytes) received", data.size());
  //ESP_LOGVV(TAG, "  %s", format_hex_pretty(&data.front(), 160).c_str());
  //ESP_LOGVV(TAG, "  %s", format_hex_pretty(&data.front() + 160, data.size() - 160).c_str());

  // JK04 (JK-B2A16S v3) response example:
  //
  // 0x55 0xAA 0xEB 0x90 0x03 0xE7 0x4A 0x4B 0x2D 0x42 0x32 0x41 0x31 0x36 0x53 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x33
  // 0x2E 0x30 0x00 0x00 0x00 0x00 0x00 0x33 0x2E 0x33 0x2E 0x30 0x00 0x00 0x00 0x10 0x8E 0x32 0x02 0x13 0x00 0x00 0x00
  // 0x42 0x4D 0x53 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x31 0x32 0x33 0x34 0x00 0x00 0x00
  // 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00
  // 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00
  // 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00
  // 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00
  // 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00
  // 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00
  // 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00
  // 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00
  // 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00
  // 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00
  // 0xA9
  //
  // Device info frame (300 bytes):
  //   Vendor ID: JK-B2A16S
  //   Hardware version: 3.0
  //   Software version: 3.3.0
  //   Uptime: 36867600 s
  //   Power on count: 19
  //   Device name: BMS
  //   Device passcode: 1234
  //   Manufacturing date:
  //   Serial number:
  //   Passcode:
  //   User data:
  //   Setup passcode:

  // JK02 response example:
  //
  // 0x55 0xAA 0xEB 0x90 0x03 0x9F 0x4A 0x4B 0x2D 0x42 0x32 0x41 0x32 0x34 0x53 0x31 0x35 0x50 0x00 0x00 0x00 0x00 0x31
  // 0x30 0x2E 0x58 0x57 0x00 0x00 0x00 0x31 0x30 0x2E 0x30 0x37 0x00 0x00 0x00 0x40 0xAF 0x01 0x00 0x06 0x00 0x00 0x00
  // 0x4A 0x4B 0x2D 0x42 0x32 0x41 0x32 0x34 0x53 0x31 0x35 0x50 0x00 0x00 0x00 0x00 0x31 0x32 0x33 0x34 0x00 0x00 0x00
  // 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x32 0x32 0x30 0x34 0x30 0x37 0x00 0x00 0x32 0x30 0x32 0x31 0x36 0x30
  // 0x32 0x30 0x39 0x36 0x00 0x30 0x30 0x30 0x30 0x00 0x49 0x6E 0x70 0x75 0x74 0x20 0x55 0x73 0x65 0x72 0x64 0x61 0x74
  // 0x61 0x00 0x00 0x31 0x32 0x33 0x34 0x35 0x36 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00
  // 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00
  // 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00
  // 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00
  // 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00
  // 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00
  // 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00
  // 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00
  // 0x65

  ESP_LOGI(TAG, "  Vendor ID: %s", std::string(data.begin() + 6, data.begin() + 6 + 16).c_str());
  ESP_LOGI(TAG, "  Hardware version: %s", std::string(data.begin() + 22, data.begin() + 22 + 8).c_str());
  ESP_LOGI(TAG, "  Software version: %s", std::string(data.begin() + 30, data.begin() + 30 + 8).c_str());
  ESP_LOGI(TAG, "  Uptime: %d s", jk_get_32bit(38));
  ESP_LOGI(TAG, "  Power on count: %d", jk_get_32bit(42));
  ESP_LOGI(TAG, "  Device name: %s", std::string(data.begin() + 46, data.begin() + 46 + 16).c_str());
  ESP_LOGI(TAG, "  Device passcode: %s", std::string(data.begin() + 62, data.begin() + 62 + 16).c_str());
  ESP_LOGI(TAG, "  Manufacturing date: %s", std::string(data.begin() + 78, data.begin() + 78 + 8).c_str());
  ESP_LOGI(TAG, "  Serial number: %s", std::string(data.begin() + 86, data.begin() + 86 + 11).c_str());
  ESP_LOGI(TAG, "  Passcode: %s", std::string(data.begin() + 97, data.begin() + 97 + 5).c_str());
  ESP_LOGI(TAG, "  User data: %s", std::string(data.begin() + 102, data.begin() + 102 + 16).c_str());
  ESP_LOGI(TAG, "  Setup passcode: %s", std::string(data.begin() + 118, data.begin() + 118 + 16).c_str());
}
