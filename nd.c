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
  PRINTF("recv\n");
  if (!is_reception_window) return; // receiving period per limitare elaborazione dei recv
  if (packetbuf_datalen() != sizeof(struct beacon_msg)) {
    PRINTF("App: unexpected length: %d\n", packetbuf_datalen());
    return;
  }

  struct beacon_msg recv;
  memcpy(&recv, packetbuf_dataptr(), sizeof(recv));
  packetbuf_clear();

  if (recv.node_id < 1 || recv.node_id > MAX_NBR || recv.node_id == node_id) { // 1..64
    PRINTF("App: unexpected node_id: %d\n", recv.node_id);
    return;
  }

  if (!ids[recv.node_id]) {
    // new neighbour, not found yet
    PRINTF("%u\n", recv.node_id);
    ids[recv.node_id] = true;
    app_cb.nd_new_nbr(epoch_id, recv.node_id);
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
    rtimer_set(&rt, RTIMER_NOW(), 1, burst_tx, NULL);
  } else if (mode == ND_SCATTER) {
    printf("ND_SCATTER\n");
    // contare beacon perché la radio potrebbe non spegnersi mai
    rtimer_set(&rt, RTIMER_NOW(), 1, scatter_rx, NULL);
  } else {
    printf("error: invalid mode\n");
  }
}
/*---------------------------------------------------------------------------*/

static rtimer_clock_t this_epoch;
static rtimer_clock_t next_epoch;

#define BURST_NUM_TXS 50
static uint8_t burst_tx_count = 0;

#define BURST_NUM_RXS 5
static uint8_t burst_rx_count = 0;

void burst_tx(struct rtimer *t, void *ptr)
{
  is_reception_window = false;

  if (!(bool)ptr) { // ptr is null or false if it's a new epoch transmission and not a burst phase
    burst_tx_count = 0; // reset tx counter 

    // reset discovered neighbours at new epoch
    reset_epoch();

    PRINTF("%u, %u, %u, %u, %u\n", EPOCH_INTERVAL_RT, T_SLOT, X_SLOT, T_DELAY, X_DELAY);
    this_epoch = RTIMER_NOW();
    next_epoch = this_epoch + EPOCH_INTERVAL_RT;
  }

  if (burst_tx_count * T_DELAY < T_SLOT) {
    // PRINTF("%u-%u=%u,%u\n", RTIMER_NOW(), this_epoch, RTIMER_NOW() - this_epoch, T_SLOT);
    b.node_id = node_id;
    NETSTACK_RADIO.send(&b, sizeof(b));
    burst_tx_count++;

    void* p;
    bool f = false;
    p = &f; // avoid compiler warning
    rtimer_set(&rt, RTIMER_NOW() + T_DELAY, 1, burst_tx, p);
    PRINTF("->tx scheduled at %u, now:%u\n", RTIMER_NOW() + T_DELAY, RTIMER_NOW());
  } else {
    rtimer_set(&rt, RTIMER_TIME(&rt) + T_DELAY, 1, burst_rx, NULL);
    PRINTF("->rx scheduled at %u, now:%u\n", RTIMER_TIME(&rt) + T_DELAY, RTIMER_NOW());
  }
}

void burst_rx(struct rtimer *t, void *ptr)
{
  is_reception_window = true;
  PRINTF("rx, now:%u\n", RTIMER_NOW());
  NETSTACK_RADIO.on();
  
  rtimer_set(&rt, RTIMER_NOW() + X_DELAY, 1, burst_off, NULL);
  PRINTF("->off scheduled at %u, now:%u\n", RTIMER_NOW() + X_DELAY, RTIMER_NOW());
}

void burst_off(struct rtimer *t, void *ptr)
{
  PRINTF("off, now:%u\n", RTIMER_NOW());

  if (NETSTACK_RADIO.receiving_packet()) {
    PRINTF("receiving packet\n");
    packetbuf_clear();
  }

  if (NETSTACK_RADIO.pending_packet()) {
    PRINTF("pending packet\n");
    nd_recv();
  }

  is_reception_window = false;
  NETSTACK_RADIO.off();

  if (burst_rx_count < BURST_NUM_RXS) {
    burst_rx_count++;

    rtimer_set(&rt, RTIMER_NOW() + (X_SLOT - X_DELAY), 1, burst_rx, NULL);
    PRINTF("->rx scheduled at %u, now:%u\n", RTIMER_NOW() + (X_SLOT - X_DELAY), RTIMER_NOW());
  } else {
    app_cb.nd_epoch_end(epoch_id, epoch_new_nbrs);
    epoch_id++;

    burst_rx_count = 0; // reset rx counter

    rtimer_set(&rt, RTIMER_NOW() + X_DELAY, 1, burst_tx, NULL);
    PRINTF("->tx scheduled at %u, now:%u\n", RTIMER_NOW() + X_DELAY, RTIMER_NOW());
  }
}



/*---------------------------------------------------------------------------*/
// SCATTER

#define SCATTER_NUM_TXS ((EPOCH_INTERVAL_RT - T_SLOT) / (X_SLOT - X_DELAY))
static uint16_t tx_window = 0;
static bool is_epoch_zero = true;

void scatter_rx(struct rtimer *t, void *ptr) 
{
  is_reception_window = true;

  // this callback fn is called at every epoch start, 
  // but the application must be notified at epoch end
  if (!is_epoch_zero) { 
    app_cb.nd_epoch_end(epoch_id, epoch_new_nbrs);
    epoch_id++;
    tx_window = 0;
  }
  is_epoch_zero = false;

  // reset discovered neighbours at new epoch
  reset_epoch();

  // printf("%u, %u, %u, %u, %u\n", EPOCH_INTERVAL_RT, T_SLOT, X_SLOT, T_DELAY, R_DELAY);
  this_epoch = RTIMER_NOW();
  next_epoch = this_epoch + EPOCH_INTERVAL_RT;

  NETSTACK_RADIO.on();

  rtimer_set(&rt, RTIMER_NOW() + T_SLOT, 1, scatter_tx, NULL);
}

void scatter_tx(struct rtimer *t, void *ptr) 
{
  if (NETSTACK_RADIO.receiving_packet()) {
    PRINTF("receiving packet\n");
    packetbuf_clear();
  }

  if (NETSTACK_RADIO.pending_packet()) {
    PRINTF("pending packet\n");
    nd_recv();
  }

  is_reception_window = false;
  NETSTACK_RADIO.off();

  b.node_id = node_id;
  NETSTACK_RADIO.send(&b, sizeof(b));

  PRINTF("tx_w:%u, num:%u\n", tx_window, SCATTER_NUM_TXS);
  if (tx_window < SCATTER_NUM_TXS) {
    tx_window++;
    rtimer_set(&rt, RTIMER_NOW() + (X_SLOT - X_DELAY), 1, scatter_tx, NULL);
  } else {
    rtimer_set(&rt, RTIMER_TIME(&rt) + T_SLOT, 1, scatter_rx, NULL);
  }
}
