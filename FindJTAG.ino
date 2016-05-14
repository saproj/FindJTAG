/*
JTAG IDCODE reader which tries all possible pin assignments.
*/

uint8_t TDI_PIN;
uint8_t TDO_PIN;
uint8_t TMS_PIN;
uint8_t TCK_PIN;
bool found;

/*
#define A2 8
#define A3 9
#define A4 10
#define A5 11
*/

// Uses pins A2-A5, but they can be changed to any other digital or analog pins.
uint8_t permutations[2*3*4][4] = {
  {A2, A3, A4, A5},
  {A2, A3, A5, A4},
  {A2, A4, A3, A5},
  {A2, A4, A5, A3},
  {A2, A5, A3, A4},
  {A2, A5, A4, A3},
  {A3, A2, A4, A5},
  {A3, A2, A5, A4},
  {A3, A4, A2, A5},
  {A3, A4, A5, A2},
  {A3, A5, A2, A4},
  {A3, A5, A4, A2},
  {A4, A2, A3, A5},
  {A4, A2, A5, A3},
  {A4, A3, A2, A5},
  {A4, A3, A5, A2},
  {A4, A5, A2, A3},
  {A4, A5, A3, A2},
  {A5, A2, A3, A4},
  {A5, A2, A4, A3},
  {A5, A3, A2, A4},
  {A5, A3, A4, A2},
  {A5, A4, A2, A3},
  {A5, A4, A3, A2},
};


enum signals {
  // Change value to 0 to make signal inverted (may be needed for transistor-based connection schematics).
  TDI_HIGH = 1,
  TDO_HIGH = 1,
  TCK_HIGH = 1,
  TMS_HIGH = 1,

  // low signals always have opposite value
  TDI_LOW = !TDI_HIGH,
  TDO_LOW = !TDO_HIGH,
  TCK_LOW = !TCK_HIGH,
  TMS_LOW = !TMS_HIGH,
};

void setup() {
  Serial.begin(9600);
  while (!Serial)
    ;
}

int clock(int tms, int tdi = TDI_LOW) {
  // read last output
  digitalWrite(TCK_PIN, TCK_LOW);
  const int r = digitalRead(TDO_PIN) == TDO_HIGH;
  
  // set new input
  digitalWrite(TDI_PIN, tdi);
  digitalWrite(TMS_PIN, tms);
  digitalWrite(TCK_PIN, TCK_HIGH);

  return r;
}

inline int TMS() { return clock(TMS_HIGH); }
inline int tms() { return clock(TMS_LOW); }
inline int TMS_TDI() { return clock(TMS_HIGH, TDI_HIGH); }
inline int tms_TDI() { return clock(TMS_LOW, TDI_HIGH); }
inline int tms_tdi() { return clock(TMS_LOW, TDI_LOW); }

bool detect_chain(unsigned &ilen, unsigned &dlen) {
  const unsigned MAX_CHAIN_BITS = 1234;
  unsigned i;
  
  // reset and go to Shift-IR
  TMS(); TMS(); TMS(); TMS(); TMS();
  tms(); TMS(); TMS(); tms(); tms();
  
  // fill IR with zeroes
  for (i = 0; i < MAX_CHAIN_BITS; i++)
    tms_tdi();

  // send ones until we get one back
  for (i = 0; i < MAX_CHAIN_BITS && !tms_TDI(); i++)
    ;

  // process response
  if (i >= MAX_CHAIN_BITS) {
    Serial.println("Unable to determine chain length.");
    return false;
  }
  ilen = i;

  // go to Shift-DR sending one last TDI
  TMS_TDI(); TMS(); TMS(); tms(); tms();

  // fill DR with zeroes
  for (i = 0; i < ilen; i++)
    tms_tdi();

  // send ones until we get one back
  for (i = 0; i <= ilen && !tms_TDI(); i++)
    ;

  // process response
  if (i > ilen) {
    Serial.println("Unable to determine number of devices.");
    return false;
  }
  dlen = i;

  return true;
}

uint32_t read(unsigned n) {
  uint32_t r = 0;
  for (unsigned i = 0; i < n; i++)
    r |= uint32_t(tms_tdi()) << i;
  return r;
}

void read_idcode() {
    Serial.println("IDCODE:");
    
    // read and output 32 bits of IDCODE
#if 1
    Serial.print("\tone: ");
    Serial.println(read(1), BIN);
    Serial.print("\tmanufacturer: 0x");
    Serial.println(read(11), HEX);
    Serial.print("\tpart: 0x");
    Serial.println(read(16), HEX);
    Serial.print("\tver: 0x");
    Serial.println(read(4), HEX);
#else
    for (unsigned j = 0; j < 32; j++)
      Serial.print(tms_tdi(), DEC);
    Serial.println();
#endif
}

void permutation(unsigned n) {
  unsigned ilen, dlen;

  Serial.print("Starting JTAG chain detection at permutation ");
  Serial.println(n, DEC);

  // assign pins according to permutation number
  TDI_PIN = permutations[n][0];
  TDO_PIN = permutations[n][1];
  TMS_PIN = permutations[n][2];
  TCK_PIN = permutations[n][3];

  pinMode(TDI_PIN, OUTPUT);
  pinMode(TDO_PIN, INPUT);
  pinMode(TCK_PIN, OUTPUT);
  pinMode(TMS_PIN, OUTPUT);
  digitalWrite(TCK_PIN, TCK_HIGH);

  if (!detect_chain(ilen, dlen))
    return;
  Serial.print("Chain length: ");
  Serial.println(ilen, DEC);
  Serial.print("Number of devices: ");
  Serial.println(dlen, DEC);
  
  // reset (also loads IDCODE into DR)
  TMS(); TMS(); TMS(); TMS(); TMS();

  // go to Shift-DR
  tms(); TMS(); tms(); tms();

  for (unsigned i = 0; i < dlen; i++) {
    read_idcode();
    Serial.println("We have found a working permutation!");
    found = true;
  }
}

void loop() {
  if (found) return;
  for (unsigned n = 0; n < 2*3*4; n++)
    permutation(n);
  delay(3000);
  Serial.println();
}

