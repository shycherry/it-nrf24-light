#define DEBUG
#define NRF24BRIDGE_TEST

#include <SPI.h>
#include "RF24.h"

#ifdef DEBUG
#include "printf.h"
#endif
//
// Hardware definition
//
#define GLOWING_RATE_HZ       240
#define NB_LIGHTS             5
#define NRF_LIGHT_COLD_WHITE  A0
#define NRF_LIGHT_WARM_WHITE  A1
#define NRF_LIGHT_RED         A2
#define NRF_LIGHT_GREEN       A3
#define NRF_LIGHT_BLUE        A4

//
// Hardware configuration
//
const int lights[NB_LIGHTS] = {
  NRF_LIGHT_COLD_WHITE,
  NRF_LIGHT_WARM_WHITE,
  NRF_LIGHT_RED,
  NRF_LIGHT_GREEN,
  NRF_LIGHT_BLUE
};
//
int brightnesses[NB_LIGHTS] = {128, 128, 128, 128, 128};

// Set up nRF24L01 radio on SPI bus plus pins 9 & 10
RF24 radio(9,10);

//
// Topology
//

// Radio pipe addresses for the 2 nodes to communicate.
const uint64_t pipes[2] = {
  0xF0F0F0F0E1LL, 0xF0F0F0F0D2LL };

bool isGlowTestActive = true;
int brightness = 0;
unsigned long time = 0;

void check_radio(void);

void refresh_lights()
{
  int iterLight = 0;
  int iterBright = 0;
  for(iterBright = 0; iterBright < 255; iterBright++)
  {
    for(iterLight = 0; iterLight < NB_LIGHTS; iterLight++)
    {
      if(iterBright > brightnesses[iterLight])
        digitalWrite(lights[iterLight], LOW);
      else
        digitalWrite(lights[iterLight], HIGH);
    }
  }
}

bool fadeIn[NB_LIGHTS] = {1,1,1,1,1};

void update_glow_test()
{
  if(millis() > time+(1000/GLOWING_RATE_HZ))
  {
    int iter = 0;
    // for(iter = 0; iter < NB_LIGHTS; iter++)
    {
      brightnesses[time%NB_LIGHTS] = (brightnesses[time%NB_LIGHTS] + ((fadeIn[time%NB_LIGHTS])? 1 : -1) );
      if(brightnesses[time%NB_LIGHTS] == 255)
        fadeIn[time%NB_LIGHTS] = 0;
      else if(brightnesses[time%NB_LIGHTS] == 0)
        fadeIn[time%NB_LIGHTS] = 1;
    }
    time = millis();
  }
}

void setup(void)
{
#ifdef DEBUG
  //
  // Print preamble
  //
  Serial.begin(57600);
  printf_begin();
  printf("\n\rRF24/examples/GettingStarted/\n\r");
#endif

  //
  // Setup and configure Analog pins
  //
  pinMode(NRF_LIGHT_COLD_WHITE, OUTPUT);
  pinMode(NRF_LIGHT_WARM_WHITE, OUTPUT);
  pinMode(NRF_LIGHT_RED, OUTPUT);
  pinMode(NRF_LIGHT_GREEN, OUTPUT);
  pinMode(NRF_LIGHT_BLUE, OUTPUT);

  //
  // Setup and configure rf radio
  //

  radio.begin();

  // optionally, increse the delay between retries & # of retries
  radio.setRetries(500,15);
  radio.enableDynamicPayloads();
  radio.setAutoAck(false);
  radio.setDataRate(RF24_2MBPS);

  //radio.setAutoAck(false);

  // optionally, reduce the payload size.  seems to
  // improve reliability
  radio.setPayloadSize(8);

  //
  // Open pipes to other nodes for communication
  //

  // This simple sketch opens two pipes for these two nodes to communicate
  // back and forth.
  // Open 'our' pipe for writing
  // Open the 'other' pipe for reading, in position #1 (we can have up to 5 pipes open for reading)

#ifdef NRF24BRIDGE_TEST
    radio.openWritingPipe(pipes[0]);
    radio.openReadingPipe(1,pipes[1]);
#else
    radio.openWritingPipe(pipes[1]);
    radio.openReadingPipe(1,pipes[0]);
#endif

  //
  // Start listening
  //
  radio.startListening();

#ifdef DEBUG
  //
  // Dump the configuration of the rf unit for debugging
  //
  radio.printDetails();
#endif
  attachInterrupt(0, check_radio, FALLING);
}

void loop(void)
{
#ifdef NRF24BRIDGE_TEST
  radio.stopListening();
  unsigned long time = millis();
  printf("Now sending %lu...", time);
  bool ok = radio.write( &time, sizeof(unsigned long));
  radio.startListening();
  delay(1000);
#else
  refresh_lights();

  if(isGlowTestActive)
    update_glow_test();
#endif
}

void check_radio(void)
{
  // What happened?
  bool tx,fail,rx;
  radio.whatHappened(tx,fail,rx);

  // Have we successfully transmitted?
  if ( tx )
  {
#ifdef NRF24BRIDGE_TEST
    printf("Send:OK\n\r");
#else
    printf("Ack Payload:Sent\n\r");
#endif
  }

  // Have we failed to transmit?
  if ( fail )
  {
#ifdef NRF24BRIDGE_TEST
    printf("Send:Failed\n\r");
#else
    printf("Ack Payload:Failed\n\r");
#endif
  }

  // Transmitter can power down for now, because
  // the transmission is done.
#ifdef NRF24BRIDGE_TEST
  if ( ( tx || fail ))
    radio.powerDown();
#endif

  // Did we receive a message?
  if ( rx )
  {
    // If we're the sender, we've received an ack payload
#ifdef NRF24BRIDGE_TEST
    unsigned long got_time = 0;
    radio.read(&got_time,sizeof(unsigned long));
    printf("Ack:%lu\n\r",got_time);
#else
    // If we're the receiver, we've received a time message
    // Get this payload and dump it
    unsigned long got_time;
    radio.read( &got_time, sizeof(unsigned long) );
    printf("Got payload %lu\n\r",got_time);

    isGlowTestActive = !isGlowTestActive;

    // Add an ack packet for the next time around.  This is a simple
    // packet counter
    radio.writeAckPayload( 1, &got_time, sizeof(unsigned long) );
#endif
  }
}

