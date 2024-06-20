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
  .node_id = NULL
};

static struct rtimer rt;

static uint16_t epoch_id = 0;
static bool ids[MAX_NBR+1] = {false}; // vettore di booleani (bitmap) come struttura dati per salvare i vicini. Max dim: MAX_NBR
static uint16_t new_nbrs = 0;
static bool is_reception_window;

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
  printf("recv\n");
  if (!is_reception_window) return; // receiving period per limitare elaborazione dei recv
  if (packetbuf_datalen() != sizeof(struct beacon_msg)) {
    printf("App: unexpected length: %d\n", packetbuf_datalen());
    return;
  }

  struct beacon_msg recv;
  memcpy(&recv, packetbuf_dataptr(), sizeof(recv));
  packetbuf_clear();

  if (recv.node_id < 1 || recv.node_id > MAX_NBR || recv.node_id == node_id) { // 1..64
    printf("App: unexpected node_id: %d\n", recv.node_id);
    return;
  }

  if (!ids[recv.node_id]) {
    printf("%u\n", recv.node_id);
    ids[recv.node_id] = true;
    app_cb.nd_new_nbr(epoch_id, recv.node_id);
    new_nbrs++;
  }
}
/*---------------------------------------------------------------------------*/
void
nd_start(uint8_t mode, const struct nd_callbacks *cb)
{
  // int random_seed_number = 1;
  // random_init(node_id+random_seed_number);
  
  app_cb.nd_epoch_end = cb->nd_epoch_end;
  app_cb.nd_new_nbr = cb->nd_new_nbr;

  int i = 0;
  for (i = 0; i < MAX_NBR+1; i++) {
    ids[i] = false;
  }
  new_nbrs = 0;

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

#define NUM_TXS 50
static int tx_count = 0;

#define NUM_RXS 5
static int rx_count = 0;

void burst_tx(struct rtimer *t, void *ptr)
{
  is_reception_window = false;

  if (!(bool)ptr) { // ptr is null or false if it's a new epoch transmission and not a burst phase

    tx_count = 0;

    // reset discovered neighbours at new epoch
    int i = 0;
    for (i = 0; i < MAX_NBR+1; i++) {
      ids[i] = false;
    }
    new_nbrs = 0;

    PRINTF("%u, %u, %u, %u, %u\n", EPOCH_INTERVAL_RT, T_SLOT, X_SLOT, T_DELAY, X_DELAY);
    this_epoch = RTIMER_NOW();
    next_epoch = this_epoch + EPOCH_INTERVAL_RT;
  }

  b.node_id = node_id;

  // printf("now:%u\n", RTIMER_NOW());
  if (tx_count * T_DELAY < T_SLOT) {
    // PRINTF("%u-%u=%u,%u\n", RTIMER_NOW(), this_epoch, RTIMER_NOW() - this_epoch, T_SLOT);
    NETSTACK_RADIO.send(&b, sizeof(b));
    tx_count++;

    rtimer_set(&rt, RTIMER_NOW() + T_DELAY, 1, burst_tx, true);
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
    printf("receiving packet\n");
    packetbuf_clear();
  }

  if (NETSTACK_RADIO.pending_packet()) {
    printf("pending packet\n");
    nd_recv();
  }

  is_reception_window = false;
  NETSTACK_RADIO.off();

  PRINTF("now:%u,time:%u,next:%u\n", RTIMER_NOW(), RTIMER_TIME(&rt), next_epoch);
  if (rx_count < NUM_RXS) {
    rx_count++;
    rtimer_set(&rt, RTIMER_NOW() + (X_SLOT - X_DELAY), 1, burst_rx, NULL);
    PRINTF("->rx scheduled at %u, now:%u\n", RTIMER_NOW() + (X_SLOT - X_DELAY), RTIMER_NOW());

  } else {
    app_cb.nd_epoch_end(epoch_id, new_nbrs);
    epoch_id++;

    rx_count = 0;

    rtimer_set(&rt, RTIMER_NOW() + X_DELAY, 1, burst_tx, NULL);
    PRINTF("->tx scheduled at %u, now:%u\n", RTIMER_NOW() + X_DELAY, RTIMER_NOW());
  }
}

uint num_tx_windows = (EPOCH_INTERVAL_RT - X_SLOT) / T_SLOT;
uint tx_window = 0;

void scatter_tx(struct rtimer *t, void *ptr) {
  NETSTACK_RADIO.off();

  struct beacon_msg b = {
    .node_id = node_id
  };
  NETSTACK_RADIO.send(&b, sizeof(b));

  printf("tx_w:%u, num:%u\n", tx_window, num_tx_windows);
  if (tx_window < num_tx_windows) {
    rtimer_set(&rt, RTIMER_NOW() + X_DELAY, 1, scatter_tx, NULL);
    tx_window++;
  } else {
    rtimer_set(&rt, RTIMER_NOW(), 1, scatter_off, NULL);
  }
}

void scatter_rx(struct rtimer *t, void *ptr) {
  is_reception_window = true;

  // printf("scatter:%d\n", (int)ptr);
  if (!(bool)ptr) { // ptr is null or false if it's a new epoch transmission and not a scatter phase

    // reset discovered neighbours at new epoch
    int i = 0;
    for (i = 0; i < MAX_NBR+1; i++) {
      ids[i] = false;
    }
    new_nbrs = 0;

    // printf("%u, %u, %u, %u, %u\n", EPOCH_INTERVAL_RT, T_SLOT, X_SLOT, T_DELAY, R_DELAY);
    this_epoch = RTIMER_TIME(&rt);
    next_epoch = this_epoch + EPOCH_INTERVAL_RT;
  }

  NETSTACK_RADIO.on();

  rtimer_set(&rt, RTIMER_NOW() + T_SLOT, 1, scatter_tx, NULL);
}

void scatter_off(struct rtimer *t, void *ptr) {
  
  app_cb.nd_epoch_end(epoch_id, new_nbrs);
  epoch_id++;
  tx_window = 0;
  rtimer_set(&rt, RTIMER_NOW() + X_SLOT, 1, scatter_rx, NULL);
}