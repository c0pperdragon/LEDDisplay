#include "hardware/timer.h"
#include "hardware/dma.h"
#include "hardware/spi.h"
#include "hardware/irq.h"
#include "hardware/gpio.h"
#include "outshifter.pio.h"
#include "image.h"

#define SCREENBUFFER_LEN        (64 * 64 * 20 * 2)   //2 Byte pro pixel anstatt 12 bit, da der DMA nur 8/16/32 Bit übertragen kann
#define BITBLOCK_LEN            (SCREENBUFFER_LEN / 4)
#define SHIFTBLOCK_LEN          (64 * 10 * 2)        //16 bit * anzahl shifts
#define SHIFTBLOCK_TRANSFER_LEN (SHIFTBLOCK_LEN / 4) //Transfercount des DMA bei 4 byte Blöcken

#define ROW_TIMING          130 //130 microsec for 1 row on/off sequence
#define ROW_TIME_MULTIPLIER 14  //Multiplier in microsecs for row on/off sequence

#define PIN_CLK 0
#define PIN_LAT 1
#define PIN_E 2
#define PIN_OE 3
#define PIN_RGB 4  //Pins GPIO4 - GPIO15

uint8_t  screenbuffer[SCREENBUFFER_LEN];
uint32_t blockoffs = 0;
uint8_t  row = 0;
uint8_t  rowTimeMult = 1;


PIO pioout;       //pioblock for outputshifter
int sm0;          //statemachine for outputshifter
int out_dma_chan; //dma for outputshifter


void clearScreen() {
  memset( screenbuffer, 0, sizeof(screenbuffer) );
}

void writePixel(int x, int y, int r, int g, int b) {
  uint8_t mask;
  int shift;
  int offs;
  uint8_t col;

  if ( y < 32 ) {
    shift = 3;
    offs = x * 2;
  } else if ( y < 64 ) {
    offs = x * 2;
    shift = 0;    
  } else if ( y < 96 ) {
    offs = (SHIFTBLOCK_LEN / 2) + (x * 2);
    shift = 3;
  } else if ( y < 128 ) {
    offs = (SHIFTBLOCK_LEN / 2) + (x * 2);
    shift = 0;
  } else if ( y < 160 ) {
    offs = 1 + x * 2;
    shift = 3;
  } else if ( y < 192 ) {
    offs = 1 + x * 2;
    shift = 0;    
  } else if ( y < 224 ) {
    offs = ((SHIFTBLOCK_LEN / 2) + 1) + (x * 2);
    shift = 3;
  } else if ( y < 256 ) {
    offs = ((SHIFTBLOCK_LEN / 2) + 1) + (x * 2);
    shift = 0;    
  } else return;

  int shiftred = shift;
  int shiftgre = shift + 1;
  int shiftblu = shift + 2;

  mask = 0x7 << shift;

  for (int i=0; i<4;i++) {
    col = screenbuffer[offs] & mask;
    col |= ( 
      (((r>>i) & 0x01) << shiftred) |
      (((g>>i) & 0x01) << shiftgre) |
      (((b>>i) & 0x01) << shiftblu) 
    );
    offs += BITBLOCK_LEN;
  }
}

void drawDemoImage() {
  clearScreen();

  int imgoffs = 0;  
  for (int x=0; x<320;x++)
    for (int y=0; y<256;y++) {
      uint32_t rgba = image[imgoffs];      
      imgoffs++;

      int r = ((rgba & 0x00FF0000) >> 16);
      int g = ((rgba & 0x0000FF00) >> 8);
      int b = ((rgba & 0x000000FF) >> 0);

      writePixel(x,y, r,g,b);
    }
}


void setup() {
  //Prepare screenbuffer with Demoimage from image.h
  drawDemoImage();
  
  //Setup pio
  pioout = pio0;
  uint offset = pio_add_program(pioout, &outshifter_program);
  sm0 = pio_claim_unused_sm(pioout, true);
  outshifter_program_init(pioout, sm0, offset, PIN_RGB, PIN_CLK);

  //Setup dma
  out_dma_chan = dma_claim_unused_channel(true);
  dma_channel_config c = dma_channel_get_default_config(out_dma_chan);
  channel_config_set_transfer_data_size(&c, DMA_SIZE_32);
  channel_config_set_read_increment(&c, true);
  channel_config_set_dreq(&c, DREQ_PIO0_TX0);

  dma_channel_configure(
    out_dma_chan,
    &c,
    &pioout->txf[0],    // Write address (only need to set this once)
    NULL,               // Don't provide a read address yet
    SHIFTBLOCK_TRANSFER_LEN,
    false               // Don't start yet
  );


  //write first row of screenbuffer to output
  dma_channel_set_read_addr(out_dma_chan, screenbuffer + blockoffs, true);
  //calculate the offset for the next output
  blockoffs = (blockoffs + SHIFTBLOCK_LEN) % SCREENBUFFER_LEN;
  
  //make sure dma is finished
  delayMicroseconds(ROW_TIMING);
}

void loop() {  
  uint64_t displayStartTime;
  uint64_t targetTime;

  //Latch the pixels
  digitalWrite( PIN_LAT, 1);  
  digitalWrite( PIN_LAT, 0);

  //Start dma to output the next line of pixels, dma should have ended by now or the timings are off
  dma_channel_set_read_addr(out_dma_chan, screenbuffer + blockoffs, true);

  //calculate the offset for the next output
  blockoffs = (blockoffs + SHIFTBLOCK_LEN) % SCREENBUFFER_LEN;
  
  //display current rows with correct timing
  displayStartTime = time_us_64() + 2;
  while ( time_us_64() < displayStartTime );  

  //Turn row on
  digitalWrite(PIN_OE, LOW);

  targetTime = displayStartTime + ROW_TIME_MULTIPLIER * rowTimeMult;
  while ( time_us_64() < targetTime );
  
  //Turn row off
  digitalWrite(PIN_OE, HIGH);
  
  row=(row+1) % 32;
  if (row==0)
  {
    digitalWrite(PIN_E, LOW);
    digitalWrite(PIN_E, HIGH);
    digitalWrite(PIN_E, LOW);


    rowTimeMult <<= 1;
    if ( rowTimeMult > 8 ) rowTimeMult = 1;
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

  targetTime = displayStartTime + ROW_TIMING;
  while ( time_us_64() < targetTime );

}
