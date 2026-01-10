// Needs to run with 200Mhz to have necessary performance.
// Use highest optimization level, tool

#include "hardware/timer.h"
#include "hardware/dma.h"
#include "hardware/spi.h"
#include "hardware/irq.h"
#include "hardware/gpio.h"
#include "hardware/pio.h"
#define PICO_NO_HARDWARE 0
#include "outshifter.pio.h"
#include "lightswitcher.pio.h"
#include "pixelreader.pio.h"
#include "delayedinterrupt.pio.h"
#include "image.h"

#define PIN_CLK 0
#define PIN_LAT 1
#define PIN_E 2
#define PIN_OE 3
#define PIN_RGB 4   //Pins GPIO4 - GPIO15

#define PIN_DATA 16 // Pins GIPO16 - GPIO22
#define PIN_DEBUG 28
#define PIN_SEGMENTSELECTOR 27

#define SCREENBUFFER_LEN        ((320/2)*4*32)   // screen buffer size in uint32

PIO pioout;       //pioblock for outputshifter
int out_dma;      //dma for outputshifter
PIO pioin;        //pioblock for data reader
int in_dma;      //dma for pixel data receiver
int trigger_dma;  // dma just for tiggering delayed interrupt

int top;                // first line of whole screen to be shown by this program

int currentTotalLine;   // line number of the original whole screen
uint32_t screenbuffer[SCREENBUFFER_LEN];  // screen buffer to drive output 
int currentreadbuffer;
uint32_t readlinebuffer[60*(384/2)];   // hold 60 blocks of input data, which is enough for 72 lines on screen

int segment;  // currently processed display segment (a 32th of the whole display)
int row_latch; // what is currently latched in the external counter
volatile int requestedLineToProcess;
int warmup;

void setup() 
{
  // set pin modes and idle values
  digitalWrite(PIN_OE, HIGH);
  pinMode(PIN_OE, OUTPUT);
  digitalWrite(PIN_OE, HIGH);
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
  pinMode(PIN_E, OUTPUT);
  digitalWrite(PIN_E, HIGH);
  pinMode(PIN_DEBUG, OUTPUT);
  digitalWrite(PIN_DEBUG, LOW);
  pinMode(PIN_SEGMENTSELECTOR, INPUT_PULLUP);
  top = digitalRead(PIN_SEGMENTSELECTOR) ? 0 : 128;

  // startup values for various counters
  memset(screenbuffer, 0, SCREENBUFFER_LEN*4);
  currentTotalLine = 0;
  currentreadbuffer = 0;
  segment = 0;
  requestedLineToProcess = -1;

  // reset the row counter latch to 0
  digitalWrite(PIN_E, LOW);
  digitalWrite(PIN_OE, LOW);
  digitalWrite(PIN_E, HIGH);
  digitalWrite(PIN_OE, HIGH);
  digitalWrite(PIN_E, LOW);
  row_latch = 0;

  //Setup pio block and state machines for driving the LEDs
  pioout = pio0;
  pio_gpio_init(pioout, PIN_CLK);
  pio_gpio_init(pioout, PIN_LAT);
  pio_gpio_init(pioout, PIN_OE);
  for (int i=0;i<12;i++) { pio_gpio_init(pioout, PIN_RGB+i); }  
  {
    int sm = pio_claim_unused_sm(pioout, true);
    int o = pio_add_program(pioout, &outshifter_program);
    pio_sm_config c = outshifter_program_get_default_config(o);
    sm_config_set_out_pins(&c, PIN_RGB, 12);
    sm_config_set_sideset_pins(&c, PIN_CLK);
    sm_config_set_clkdiv(&c, 1.0);
    sm_config_set_in_shift(&c, false, false, 0);
    sm_config_set_out_shift(&c, false, false, 32);
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX);
    pio_sm_set_consecutive_pindirs(pioout, sm, PIN_CLK, 2, true);
	  pio_sm_set_consecutive_pindirs(pioout, sm, PIN_RGB, 12, true);	
    pio_sm_init(pioout, sm, o, &c);
	  pio_sm_set_enabled(pioout, sm, true);
  }
  {
    int sm = pio_claim_unused_sm(pioout, true);
    int o = pio_add_program(pioout, &lightswitcher_program);
    pio_sm_config c = lightswitcher_program_get_default_config(o);
    sm_config_set_sideset_pins(&c, PIN_OE);
    sm_config_set_clkdiv(&c, 1.0);
    sm_config_set_in_shift(&c, false, false, 0);
    sm_config_set_out_shift(&c, false, false, 0);
    pio_sm_set_consecutive_pindirs(pioout, sm, PIN_OE, 1, true);
    pio_sm_init(pioout, sm, o, &c);
	  pio_sm_set_enabled(pioout, sm, true);
  }
  //Setup dma channel for driving LEDs
  out_dma = dma_claim_unused_channel(true);
  {
    dma_channel_config c = dma_channel_get_default_config(out_dma);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_32);
    channel_config_set_read_increment(&c, true);
    channel_config_set_write_increment(&c, false);
    channel_config_set_dreq(&c, DREQ_PIO0_TX0);
    dma_channel_configure
    (
      out_dma,
      &c,
      &pioout->txf[0],     // Write address (only need to set this once)
      NULL,                // Don't provide a read address yet
      SCREENBUFFER_LEN/32, // always write 1 of 32 lines in each segment
      false                // Don't start yet
    );
  }

  pioin = pio1;  
  // set up CPU interrupt for incomming scanlines of data
  irq_set_exclusive_handler(PIO_IRQ_NUM(pioin, 0), lineFinishInterrupt);
  irq_set_enabled(PIO_IRQ_NUM(pioin, 0), true);
  pio_set_irq0_source_enabled(pioin, pis_interrupt0, true);
  // set up CPU interrupt for delayed setting of the row latch
  irq_set_exclusive_handler(PIO_IRQ_NUM(pioin, 1), updateRowLatchInterrupt);
  irq_set_enabled(PIO_IRQ_NUM(pioin, 1), true);
  pio_set_irq1_source_enabled(pioin, pis_interrupt1, true);

  //Setup dma channel for receiving bit data
  in_dma = dma_claim_unused_channel(true);
  {
    dma_channel_config c = dma_channel_get_default_config(in_dma);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_32);
    channel_config_set_read_increment(&c, false);
    channel_config_set_write_increment(&c, true);
    channel_config_set_dreq(&c, DREQ_PIO1_RX0);
    dma_channel_configure
    (
      in_dma,
      &c,
      readlinebuffer,      
      &pioin->rxf[0],      // Read address (only need to set this once)
      384/2,               // always read 384 pixels (16 bit per pixel), no matter if they will be used
      true                 // Start immediately, waiting for data
    );
  }
  //Setup dma channel for triggering delayed interrupt
  trigger_dma = dma_claim_unused_channel(true);
  {
    dma_channel_config c = dma_channel_get_default_config(trigger_dma);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_32);
    channel_config_set_read_increment(&c, true);
    channel_config_set_write_increment(&c, false);
    channel_config_set_dreq(&c, DREQ_PIO1_TX1);
    dma_channel_configure
    (
      trigger_dma,
      &c,
      &pioin->txf[1],     // Write address (only need to set this once)
      NULL,                // Don't provide a read address yet
      1,                   // just use 1 int for signaling
      false                // Don't start yet
    );
  }
  //Set up pio block and state machines for reading input data and creating a delayed interrupt
  for (int i=0;i<7;i++) { pio_gpio_init(pioin, PIN_DATA+i); }  
  {
    int sm = pio_claim_unused_sm(pioin, true);
    int o = pio_add_program(pioin, &pixelreader_program);
    pio_sm_config c = pixelreader_program_get_default_config(o);
    sm_config_set_in_pins(&c, PIN_DATA);
    sm_config_set_clkdiv(&c, 1.0);
    sm_config_set_in_shift(&c, false, false, 32);
    sm_config_set_jmp_pin(&c, PIN_DATA+6);
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_RX);
    pio_sm_set_consecutive_pindirs(pioin, sm, PIN_DATA, 7, false);
    pio_sm_init(pioin, sm, o, &c);
	  pio_sm_set_enabled(pioin, sm, true);
  }
  {
    int sm = pio_claim_unused_sm(pioin, true);
    int o = pio_add_program(pioin, &delayedinterrupt_program);
    pio_sm_config c = delayedinterrupt_program_get_default_config(o);
    sm_config_set_clkdiv(&c, 1.0);
    sm_config_set_in_shift(&c, false, false, 32);
    pio_sm_init(pioin, sm, o, &c);
	  pio_sm_set_enabled(pioin, sm, true);
  }
}

void lineFinishInterrupt()
{ 
    // which rows to show next to have very small delay, but still avoid access conflict
    segment = ((currentTotalLine+28)%32);

    // Start dma to output the pixels of all the rows of current segment (row latch will be set by delayed interrupt)
    dma_channel_set_read_addr(out_dma, screenbuffer+(SCREENBUFFER_LEN/32)*segment, true);
    // Write one int to the trigger DMA to get a later interrupt
    dma_channel_set_read_addr(trigger_dma, screenbuffer, true);

    // request that the main loop processed the line that was just received
    requestedLineToProcess = currentTotalLine;

    // check if this was the last row of the whole screen
    if (digitalRead(PIN_DATA)==LOW)
    {
      currentreadbuffer = 0;
      currentTotalLine = 0;
    }
    else
    {
      currentreadbuffer = (currentreadbuffer+1) % 60;
      currentTotalLine++;
    }

    // prepare dma to read the next data block
    dma_channel_set_write_addr(in_dma, readlinebuffer+(currentreadbuffer*(384/2)), true);
}

void updateRowLatchInterrupt()
{
  static int warmup = 0;
  if (warmup<100)
  {
    warmup++;
    return;
  }
  
  int i;
//  digitalWrite(PIN_DEBUG, HIGH);
  for (i=0; (i<6) & (row_latch != segment); i++)
  {
    if (row_latch<16) 
    {
        digitalWrite(PIN_E, HIGH);
        row_latch = ((row_latch+1) & 15) + 16;
    }
    else
    {
        digitalWrite(PIN_E, LOW);
        row_latch = row_latch-16;
    }
  }
//  digitalWrite(PIN_DEBUG, LOW);
}


// Use the CPU to rearrange the incomming data from a pixel-by pixel format to the
// output buffer where all the bits for one LED segment (0 - 7) are grouped together 
// The input format is 16 bits for each pixel, with the left pixel of each pair in the higher 16 bits of each word.

void distributeLineData(int line, uint32_t* data)
{
  int shift = 9 - (line/32) * 3;
  uint32_t use_mask = 0x00070007u<<shift;
  uint32_t cut_mask = ~use_mask;
  uint32_t* outbuf3 = screenbuffer + (line%32) * (SCREENBUFFER_LEN/32);
  uint32_t* outbuf2 = outbuf3 + 160;
  uint32_t* outbuf1 = outbuf2 + 160;
  uint32_t* outbuf0 = outbuf1 + 160;
  for (int i=0; i<160; i++)
  {                        
    uint32_t d = *(data++);
    uint32_t x,y; 
    x = (*outbuf3) & cut_mask;        // bit 3
    y = ((d>>9) << shift) & use_mask;
    *outbuf3 = x | y;
    outbuf3++;
    x = (*outbuf2) & cut_mask;        // bit 2
    y = ((d>>6) << shift) & use_mask;
    *outbuf2 = x | y;
    outbuf2++;
    x = (*outbuf1) & cut_mask;        // bit 1
    y = ((d>>3) << shift) & use_mask;
    *outbuf1 = x | y;
    outbuf1++;
    x = (*outbuf0) & cut_mask;        // bit 0
    y = ((d) << shift) & use_mask;
    *outbuf0 = x | y;
    outbuf0++;
  } 
}

// asynchronious processing that converts the input buffer data to the screen buffer format when needed
void loop() 
{  
  int p = -1;
  for (;;)
  {
    int l = requestedLineToProcess;   // check if some work is scheduled
    if (l==p) continue;               // nothing to do yet
    if (l>=top && l<top+128)          // only process lines that are actually shown by this controller
    {
      distributeLineData(l-top, readlinebuffer+(320/2)*(l%72) );
    }
    p=l;
  }
}

