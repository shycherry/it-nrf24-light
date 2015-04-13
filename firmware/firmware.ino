#define DEBUG
#define INTERRUPT_MODE

#include <SPI.h>
#include "nRF24L01.h"
#include "RF24.h"

#ifdef DEBUG
#include "printf.h"
#endif
//
// Hardware definition
//
#define NRF_LIGHT_COLD_WHITE  A0
#define NRF_LIGHT_WARM_WHITE  A1
#define NRF_LIGHT_RED         A2
#define NRF_LIGHT_GREEN       A3
#define NRF_LIGHT_BLUE        A4

//
// Hardware configuration
//

// Set up nRF24L01 radio on SPI bus plus pins 9 & 10
RF24 radio(9,10);

//
// Topology
//

// Radio pipe addresses for the 2 nodes to communicate.
const uint64_t pipes[2] = {
  0xF0F0F0F0E1LL, 0xF0F0F0F0D2LL };

//
// Role management
//
// Set up role.  This sketch uses the same software for all the nodes
// in this system.  Doing so greatly simplifies testing.
//

// The various roles supported by this sketch
typedef enum {
  role_ping_out = 1, role_pong_back }
role_e;

// The debug-friendly names of those roles
const char* role_friendly_name[] = {
  "invalid", "Ping out", "Pong back"};

// The role of the current running sketch
role_e role = role_pong_back;

int blink = 0;
void check_radio(void);
void setup(void)
{
#ifdef DEBUG
  //
  // Print preamble
  //
  Serial.begin(57600);
  printf_begin();
  printf("\n\rRF24/examples/GettingStarted/\n\r");
  printf("ROLE: %s\n\r",role_friendly_name[role]);
  printf("*** PRESS 'T' to begin transmitting to the other node\n\r");
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
  radio.setDataRate(RF24_1MBPS);

  //radio.setAutoAck(false);

  // optionally, reduce the payload size.  seems to
  // improve reliability
  radio.setPayloadSize(4);

  //
  // Open pipes to other nodes for communication
  //

  // This simple sketch opens two pipes for these two nodes to communicate
  // back and forth.
  // Open 'our' pipe for writing
  // Open the 'other' pipe for reading, in position #1 (we can have up to 5 pipes open for reading)

  if ( role == role_ping_out )
  {
    radio.openWritingPipe(pipes[0]);
    radio.openReadingPipe(1,pipes[1]);
  }
  else
  {
    radio.openWritingPipe(pipes[1]);
    radio.openReadingPipe(1,pipes[0]);
  }

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
#ifndef INTERRUPT_MODE
  //
  // Ping out role.  Repeatedly send the current time
  //

  if (role == role_ping_out)
  {
    // First, stop listening so we can talk.
    radio.stopListening();

    // Take the time, and send it.  This will block until complete
    unsigned long time = millis();
    printf("Now sending %lu...",time);
    bool ok = radio.write( &time, sizeof(unsigned long) );

#ifdef DEBUG
    if (ok)
      printf("ok...");
    else
      printf("failed.\n\r");
#endif

    // Now, continue listening
    radio.startListening();

    // Wait here until we get a response, or timeout (250ms)
    unsigned long started_waiting_at = millis();
    bool timeout = false;
    while ( ! radio.available() && ! timeout )
      if (millis() - started_waiting_at > 100 )
        timeout = true;

    // Describe the results
    if ( timeout )
    {
#ifdef DEBUG
      printf("Failed, response timed out.\n\r");
#endif
    }
    else
    {
      // Grab the response, compare, and send to debugging spew
      unsigned long got_time;
      radio.read( &got_time, sizeof(unsigned long) );

      // blinktest
      blink = (blink+1) % 2;
      digitalWrite(NRF_LIGHT_COLD_WHITE, blink? HIGH : LOW);
      digitalWrite(NRF_LIGHT_WARM_WHITE, blink? HIGH : LOW);
      digitalWrite(NRF_LIGHT_RED, blink? HIGH : LOW);
      digitalWrite(NRF_LIGHT_GREEN, blink? HIGH : LOW);
      digitalWrite(NRF_LIGHT_BLUE, blink? HIGH : LOW);

#ifdef DEBUG
      // Spew it
      printf("Got response %lu, round-trip delay: %lu\n\r",got_time,millis()-got_time);
#endif
    }

    // Try again 1s later
    delay(100);
  }

  //
  // Pong back role.  Receive each packet, dump it out, and send it back
  //

  if ( role == role_pong_back )
  {
    // if there is data ready
    if ( radio.available() )
    {
      // Dump the payloads until we've gotten everything
      unsigned long got_time;
      bool done = false;
      while (!done)
      {
        // Fetch the payload, and see if this was the last one.
        done = radio.read( &got_time, sizeof(unsigned long) );

#ifdef DEBUG
        // Spew it
        printf("Got payload %lu...",got_time);
#endif

        // Delay just a little bit to let the other unit
        // make the transition to receiver
        delay(200);
      }

      // First, stop listening so we can talk
      radio.stopListening();

      // Send the final one back.
      radio.write( &got_time, sizeof(unsigned long) );

      // blinktest
      blink = (blink+1) % 2;
      digitalWrite(NRF_LIGHT_COLD_WHITE, blink? HIGH : LOW);
      digitalWrite(NRF_LIGHT_WARM_WHITE, blink? HIGH : LOW);
      digitalWrite(NRF_LIGHT_RED, blink? HIGH : LOW);
      digitalWrite(NRF_LIGHT_GREEN, blink? HIGH : LOW);
      digitalWrite(NRF_LIGHT_BLUE, blink? HIGH : LOW);

#ifdef DEBUG
      printf("Sent response.\n\r");
#endif

      // Now, resume listening so we catch the next packets.
      radio.startListening();
    }
  }

  //
  // Change roles
  //

#ifdef DEBUG
  if ( Serial.available() )
  {
    char c = toupper(Serial.read());
    if ( c == 'T' && role == role_pong_back )
    {
      printf("*** CHANGING TO TRANSMIT ROLE -- PRESS 'R' TO SWITCH BACK\n\r");

      // Become the primary transmitter (ping out)
      role = role_ping_out;
      radio.openWritingPipe(pipes[0]);
      radio.openReadingPipe(1,pipes[1]);
    }
    else if ( c == 'R' && role == role_ping_out )
    {
      printf("*** CHANGING TO RECEIVE ROLE -- PRESS 'T' TO SWITCH BACK\n\r");

      // Become the primary receiver (pong back)
      role = role_pong_back;
      radio.openWritingPipe(pipes[1]);
      radio.openReadingPipe(1,pipes[0]);
    }
  }
#endif

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
    if ( role == role_ping_out )
      printf("Send:OK\n\r");

    if ( role == role_pong_back )
      printf("Ack Payload:Sent\n\r");
  }

  // Have we failed to transmit?
  if ( fail )
  {
    if ( role == role_ping_out )
      printf("Send:Failed\n\r");

    if ( role == role_pong_back )
      printf("Ack Payload:Failed\n\r");
  }

  // Transmitter can power down for now, because
  // the transmission is done.
  if ( ( tx || fail ) && ( role == role_ping_out ) )
    radio.powerDown();

  // Did we receive a message?
  if ( rx )
  {
    // If we're the sender, we've received an ack payload
    if ( role == role_ping_out )
    {
      int message_count = 0;
      radio.read(&message_count,sizeof(message_count));
      printf("Ack:%lu\n\r",message_count);
    }

    // If we're the receiver, we've received a time message
    if ( role == role_pong_back )
    {
      blink = (blink+1) % 2;
      digitalWrite(NRF_LIGHT_COLD_WHITE, blink? HIGH : LOW);
      digitalWrite(NRF_LIGHT_WARM_WHITE, blink? HIGH : LOW);
      digitalWrite(NRF_LIGHT_RED, blink? HIGH : LOW);
      digitalWrite(NRF_LIGHT_GREEN, blink? HIGH : LOW);
      digitalWrite(NRF_LIGHT_BLUE, blink? HIGH : LOW);

      // Get this payload and dump it
      static unsigned long got_time;
      radio.read( &got_time, sizeof(got_time) );
      printf("Got payload %lu\n\r",got_time);
      
      // Add an ack packet for the next time around.  This is a simple
      // packet counter
      radio.writeAckPayload( 1, &blink, sizeof(blink) );
    }
  }
}

