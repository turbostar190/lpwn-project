/*---------------------------------------------------------------------------*/
#include "contiki.h"
#include "lib/random.h"
#include "sys/rtimer.h"
#include "dev/radio.h"
#include "net/netstack.h"
#include "net/packetbuf.h"
#include "node-id.h"
/*---------------------------------------------------------------------------*/
#include <stdbool.h>
#include <stdio.h>
/*---------------------------------------------------------------------------*/
#include "nd.h"
/*---------------------------------------------------------------------------*/
#define DEBUG 0
#if DEBUG
#define PRINTF(...) printf(__VA_ARGS__)
#else
#define PRINTF(...)
#endif
/*---------------------------------------------------------------------------*/
struct nd_callbacks app_cb = {
  .nd_new_nbr = NULL,
  .nd_epoch_end = NULL
  };
/*---------------------------------------------------------------------------*/

struct beacon_msg b = {
  .node_id = 0
};

static struct rtimer rt;

static uint16_t epoch_id = 0;
static bool ids[MAX_NBR+1] = {false}; // neighbour list to tracks discovery
static uint8_t epoch_new_nbrs = 0;
static bool is_reception_window;

void reset_epoch() {
  PRINTF("ID %u: reset epoch\n", node_id);
  int i = 0;
  for (i = 0; i < MAX_NBR+1; i++) {
    ids[i] = false;
  }

  epoch_new_nbrs = 0;
}

void
nd_recv(void)
{
  /* New packet received
   * 1. Read packet from packetbuf---packetbuf_dataptr()
   * 2. If a new neighbor is discovered within the epoch, notify the application
   * NOTE: The testbed's firefly nodes can receive packets only if they are at 
   * least 3 bytes long (5 considering the CRC). 
   * If while you are testing you receive nothing make sure your packet is long enough
   */
  // PRINTF("recv\n");
  if (!is_reception_window) { // limit packet elaboration to rx windows only
    PRINTF("not reception window\n");
    packetbuf_clear();
    return;
  }
  if (packetbuf_datalen() != sizeof(struct beacon_msg)) {
    PRINTF("unexpected length: %d\n", packetbuf_datalen());
    packetbuf_clear();
    return;
  }

  struct beacon_msg recv;
  memcpy(&recv, packetbuf_dataptr(), sizeof(recv));
  packetbuf_clear();

  uint16_t recv_nid = recv.node_id;

  if (recv_nid < 1 || recv_nid > MAX_NBR || recv_nid == node_id) { // 1..MAX_NBR
    PRINTF("unexpected node_id: %d\n", recv_nid);
    return;
  }

  PRINTF("recv.node_id: %u\n", recv_nid);

  if (!ids[recv_nid]) {
    // new neighbour, not seen yet
    PRINTF("ids[%u] is now true\n", recv_nid);
    ids[recv_nid] = true;
    app_cb.nd_new_nbr(epoch_id, recv_nid);
    epoch_new_nbrs++;
  }
}
/*---------------------------------------------------------------------------*/
void
nd_start(uint8_t mode, const struct nd_callbacks *cb)
{ 
  app_cb.nd_epoch_end = cb->nd_epoch_end;
  app_cb.nd_new_nbr = cb->nd_new_nbr;

  reset_epoch();

  if (mode == ND_BURST) {
    printf("ND_BURST\n");
    burst_tx(&rt, NULL);
  } else if (mode == ND_SCATTER) {
    printf("ND_SCATTER\n");
    scatter_rx(&rt, NULL);
  } else {
    printf("error: invalid mode\n");
  }
}
/*---------------------------------------------------------------------------*/

static uint8_t burst_tx_count = 0;
static uint8_t burst_rx_count = 0;

void burst_tx(struct rtimer *t, void *ptr)
{
  is_reception_window = false;

  if (!(bool)ptr) { // ptr is null or false if it's a new epoch transmission and not a burst phase
    burst_tx_count = 0; // reset tx counter 

    // reset discovered neighbours at new epoch
    reset_epoch();
  }

  if (burst_tx_count < BURST_NUM_TXS) {
    b.node_id = node_id;
    NETSTACK_RADIO.send(&b, sizeof(b));
    burst_tx_count++;

    void* p;
    bool f = true;
    p = &f; // avoid compiler warning
    unsigned short random_us = random_rand() % (3 * 1000); // up to 3 milliseconds
    rtimer_set(&rt, RTIMER_NOW() + BURST_T_DELAY - (unsigned)US_TO_RTIMERTICKS(random_us), 1, burst_tx, p);
  } else {
    burst_rx(&rt, NULL);
  }
}

void burst_rx(struct rtimer *t, void *ptr)
{
  is_reception_window = true;
  NETSTACK_RADIO.on();
  
  rtimer_set(&rt, RTIMER_NOW() + BURST_X_DUR, 1, burst_off, NULL);
}

void burst_off(struct rtimer *t, void *ptr)
{
  if (NETSTACK_RADIO.receiving_packet()) {
    PRINTF("receiving packet\n");
    // packetbuf_clear();
    rtimer_set(&rt, RTIMER_NOW() + (unsigned)US_TO_RTIMERTICKS(500), 1, burst_off, NULL);
    return;
  }

  is_reception_window = false;
  NETSTACK_RADIO.off();

  if (NETSTACK_RADIO.pending_packet()) {
    PRINTF("pending packet\n");
    NETSTACK_RADIO.read(packetbuf_dataptr(), PACKETBUF_SIZE);
  }

  if (burst_rx_count < BURST_NUM_RXS-1) {
    burst_rx_count++;

    rtimer_set(&rt, RTIMER_TIME(&rt) + (BURST_X_SLOT - BURST_X_DUR), 1, burst_rx, NULL);
  } else {
    app_cb.nd_epoch_end(epoch_id, epoch_new_nbrs);
    epoch_id++;

    burst_rx_count = 0; // reset rx counter

    rtimer_set(&rt, RTIMER_TIME(&rt) + (BURST_X_SLOT - BURST_X_DUR), 1, burst_tx, NULL);
  }
}



/*---------------------------------------------------------------------------*/
// SCATTER

static uint16_t scatter_tx_count = 0;
static bool is_epoch_zero = true;

void scatter_rx(struct rtimer *t, void *ptr) 
{
  // this callback fn is called at every epoch start, 
  // but the application must be notified at epoch end
  if (!is_epoch_zero) { 
    app_cb.nd_epoch_end(epoch_id, epoch_new_nbrs);
    epoch_id++;
    scatter_tx_count = 0;
  }
  is_epoch_zero = false;

  // reset discovered neighbours at new epoch
  reset_epoch();

  is_reception_window = true;
  NETSTACK_RADIO.on();

  rtimer_set(&rt, RTIMER_NOW() + SCATTER_T_SLOT, 1, scatter_tx, NULL);
}

void scatter_tx(struct rtimer *t, void *ptr) 
{
  /*if (NETSTACK_RADIO.receiving_packet()) {
    PRINTF("receiving packet\n");
    rtimer_set(&rt, RTIMER_NOW() + (unsigned)US_TO_RTIMERTICKS(500), 1, scatter_tx, NULL);
    return;
  }*/
  if (NETSTACK_RADIO.pending_packet()) {
    PRINTF("pending packet\n");
    NETSTACK_RADIO.read(packetbuf_dataptr(), PACKETBUF_SIZE);
  }

  is_reception_window = false;
  NETSTACK_RADIO.off();
  packetbuf_clear();

  radio_value_t radio_status;
  NETSTACK_RADIO.get_value(RADIO_PARAM_POWER_MODE, &radio_status);
  if (radio_status == RADIO_POWER_MODE_ON) {
    printf("status: %d\n", radio_status);
    // NETSTACK_RADIO.off(); // try again?...
  }

  unsigned short random_us = random_rand() % (3 * 1000); // up to 3 milliseconds

  /*if (!NETSTACK_RADIO.channel_clear()) {
    rtimer_set(&rt, RTIMER_NOW() + (BURST_X_SLOT - BURST_X_DELAY) - (unsigned)US_TO_RTIMERTICKS(random_us), 1, scatter_tx, NULL);
    return;
  }*/

  b.node_id = node_id;
  int send_status = NETSTACK_RADIO.send(&b, sizeof(b));
  if (send_status == RADIO_TX_COLLISION) {
    PRINTF("collision\n");
    rtimer_set(&rt, RTIMER_TIME(&rt) + SCATTER_X_SLOT - (unsigned)US_TO_RTIMERTICKS(random_us), 1, scatter_tx, NULL);
    return;
  }

  if (scatter_tx_count < SCATTER_NUM_TXS-1) {
    scatter_tx_count++;
    rtimer_set(&rt, RTIMER_TIME(&rt) + SCATTER_X_SLOT, 1, scatter_tx, NULL);
  } else {
    rtimer_set(&rt, RTIMER_TIME(&rt) + SCATTER_X_SLOT, 1, scatter_rx, NULL);
  }
}
