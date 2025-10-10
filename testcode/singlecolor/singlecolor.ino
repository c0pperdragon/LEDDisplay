// board: Raspberry Pi Pico

#define PIN_CLK 0
#define PIN_LAT 1
#define PIN_E   2
#define PIN_OE  3
#define PIN_R1 13
#define PIN_G1 14
#define PIN_B1 15
#define PIN_R2 10
#define PIN_G2 11
#define PIN_B2 12
#define PIN_R3  7
#define PIN_G3  8
#define PIN_B3  9
#define PIN_R4  4
#define PIN_G4  5
#define PIN_B4  6

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
  showColor(0,0,0, 1,0,0, 0,1,0, 0,0,1, 1,1,0, 0,1,1, 1,0,1, 1,1,1);
  showColor(1,1,1, 0,0,0, 1,0,0, 0,1,0, 0,0,1, 1,1,0, 0,1,1, 1,0,1);
  showColor(1,0,1, 1,1,1, 0,0,0, 1,0,0, 0,1,0, 0,0,1, 1,1,0, 0,1,1);
  showColor(0,1,1, 1,0,1, 1,1,1, 0,0,0, 1,0,0, 0,1,0, 0,0,1, 1,1,0);
  showColor(1,1,0, 0,1,1, 1,0,1, 1,1,1, 0,0,0, 1,0,0, 0,1,0, 0,0,1);
  showColor(0,0,1, 1,1,0, 0,1,1, 1,0,1, 1,1,1, 0,0,0, 1,0,0, 0,1,0);
  showColor(0,1,0, 0,0,1, 1,1,0, 0,1,1, 1,0,1, 1,1,1, 0,0,0, 1,0,0);
  showColor(1,0,0, 0,1,0, 0,0,1, 1,1,0, 0,1,1, 1,0,1, 1,1,1, 0,0,0);
}
 
void showColor(
  int r0, int g0, int b0,
  int r1, int g1, int b1,
  int r2, int g2, int b2,
  int r3, int g3, int b3,
  int r4, int g4, int b4,
  int r5, int g5, int b5,
  int r6, int g6, int b6,
  int r7, int g7, int b7
)
{
  long int i;
  int row;

  // reset row counter
  digitalWrite(PIN_OE, LOW);
  digitalWrite(PIN_E, LOW);
  digitalWrite(PIN_E, HIGH);
  digitalWrite(PIN_OE, HIGH);
  digitalWrite(PIN_E, LOW);
  row=0;

  // clock out pixels to use for all lines
  digitalWrite(PIN_R1, r0);
  digitalWrite(PIN_R2, r1);
  digitalWrite(PIN_R3, r4);
  digitalWrite(PIN_R4, r5);
  digitalWrite(PIN_G1, g0);
  digitalWrite(PIN_G2, g1);
  digitalWrite(PIN_G3, g4);
  digitalWrite(PIN_G4, g5);
  digitalWrite(PIN_B1, b0);
  digitalWrite(PIN_B2, b1);
  digitalWrite(PIN_B3, b4);
  digitalWrite(PIN_B4, b5);
  for (i=0; i<64*5; i++)
  {
    digitalWrite(PIN_CLK, LOW);
    digitalWrite(PIN_CLK, HIGH);
  }
  digitalWrite(PIN_R1, r2);
  digitalWrite(PIN_R2, r3);
  digitalWrite(PIN_R3, r6);
  digitalWrite(PIN_R4, r7);
  digitalWrite(PIN_G1, g2);
  digitalWrite(PIN_G2, g3);
  digitalWrite(PIN_G3, g6);
  digitalWrite(PIN_G4, g7);
  digitalWrite(PIN_B1, b2);
  digitalWrite(PIN_B2, b3);
  digitalWrite(PIN_B3, b6);
  digitalWrite(PIN_B4, b7);
  for (i=0; i<64*5; i++)
  {
    digitalWrite(PIN_CLK, LOW);
    digitalWrite(PIN_CLK, HIGH);
  }
  digitalWrite(PIN_LAT, LOW);
  digitalWrite(PIN_LAT, HIGH);

  // progress vertical line
  for (i=0; i<150000; i++)
  {
    // turn on display line for short time
    digitalWrite(PIN_OE, LOW);
    delayMicroseconds(10);
    digitalWrite(PIN_OE, HIGH);
    delayMicroseconds(10);

    // progress counter register
    row=(row+1) % 32;
    if (row==0) 
    {
      digitalWrite(PIN_E, LOW);
      digitalWrite(PIN_E, HIGH);
      digitalWrite(PIN_E, LOW);
    }
    else if (row==16) 
    {
      digitalWrite(PIN_E, HIGH);
    }
    else if (row<16) 
    {
      digitalWrite(PIN_E, HIGH);
      digitalWrite(PIN_E, LOW);
    }
    else
    {
      digitalWrite(PIN_E, LOW);
      digitalWrite(PIN_E, HIGH);
    }
  }
}
