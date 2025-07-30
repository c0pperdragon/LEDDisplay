// board: Raspberry Pi Pico

#define PIN_G2 15
#define PIN_G1 14
#define PIN_R1 13
#define PIN_B1 12
#define PIN_R2 11
#define PIN_B2 10
#define PIN_OE  9
#define PIN_E   8
#define PIN_G4  7
#define PIN_G3  6
#define PIN_R3  5
#define PIN_B3  4
#define PIN_R4  3
#define PIN_B4  2
#define PIN_LAT 1
#define PIN_CLK 0


void setup() 
{
  pinMode(PIN_G2, OUTPUT);
  pinMode(PIN_G1, OUTPUT);
  pinMode(PIN_R1, OUTPUT);
  pinMode(PIN_B1, OUTPUT);
  pinMode(PIN_R2, OUTPUT);
  pinMode(PIN_B2, OUTPUT);
  pinMode(PIN_OE, OUTPUT);
  pinMode(PIN_E, OUTPUT);
  pinMode(PIN_G4, OUTPUT);
  pinMode(PIN_G3, OUTPUT);
  pinMode(PIN_R3, OUTPUT);
  pinMode(PIN_B3, OUTPUT);
  pinMode(PIN_R4, OUTPUT);
  pinMode(PIN_B4, OUTPUT);
  pinMode(PIN_LAT, OUTPUT);
  pinMode(PIN_CLK, OUTPUT);

  digitalWrite(PIN_E, HIGH);
  digitalWrite(PIN_OE, HIGH);
  digitalWrite(PIN_LAT, HIGH);
  digitalWrite(PIN_CLK, HIGH);
}

void loop() 
{
  showColor(1,0,0);
  showColor(0,1,0);
  showColor(0,0,1);
  showColor(1,1,0);
  showColor(0,1,1);
  showColor(1,0,1);
  showColor(1,1,1);
}
 
void showColor(int r, int g, int b)
{
  long int i;
  int row;

  // reset row counter
  digitalWrite(PIN_CLK, LOW);
  digitalWrite(PIN_LAT, LOW);  
  digitalWrite(PIN_LAT, HIGH);
  digitalWrite(PIN_E, LOW);
  row=0;

  // clock out pixels to use for all lines
  digitalWrite(PIN_R1, r);
  digitalWrite(PIN_R2, r);
  digitalWrite(PIN_R3, r);
  digitalWrite(PIN_R4, r);
  digitalWrite(PIN_G1, g);
  digitalWrite(PIN_G2, g);
  digitalWrite(PIN_G3, g);
  digitalWrite(PIN_G4, g);
  digitalWrite(PIN_B1, b);
  digitalWrite(PIN_B2, b);
  digitalWrite(PIN_B3, b);
  digitalWrite(PIN_B4, b);
  for (i=0; i<64*10; i++)
  {
    digitalWrite(PIN_CLK, LOW);
    digitalWrite(PIN_CLK, HIGH);
  }

  // progress vertical line
  for (i=0; i<150000; i++)
  {
    // set highest line counter bit manually
    digitalWrite(PIN_E, row>=16 ? HIGH:LOW);

    // turn on display line for short time
    digitalWrite(PIN_OE, LOW);
    delayMicroseconds(10);
    digitalWrite(PIN_OE, HIGH);
    delayMicroseconds(10);

    // progress counter register
    digitalWrite(PIN_LAT, LOW);
    digitalWrite(PIN_LAT, HIGH);
    row=(row+1) % 32;
  }
}
