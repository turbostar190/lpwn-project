#define RANDOM_SEED_NUMBER 15

#include "contiki.h"
#include "dev/radio.h"
#include "lib/random.h"
#include "net/netstack.h"
#include <stdio.h>

#include "node-id.h"

#if CONTIKI_TARGET_ZOUL
#include "deployment.h"
#endif

#include "simple-energest.h"

/*---------------------------------------------------------------------------*/
#include "nd.h"
/*---------------------------------------------------------------------------*/
static void
nd_new_nbr_cb(uint16_t epoch, uint8_t nbr_id)
{
  printf("App: Epoch %u New NBR %u\n",
    epoch, nbr_id);
}
/*---------------------------------------------------------------------------*/
static void
nd_epoch_end_cb(uint16_t epoch, uint8_t num_nbr)
{
  printf("App: Epoch %u finished Num NBR %u\n",
    epoch, num_nbr);
}
/*---------------------------------------------------------------------------*/
struct nd_callbacks rcb = {
  .nd_new_nbr = nd_new_nbr_cb,
  .nd_epoch_end = nd_epoch_end_cb};
/*---------------------------------------------------------------------------*/
PROCESS(app_process, "Application process");
AUTOSTART_PROCESSES(&app_process);
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(app_process, ev, data)
{
  static struct etimer et;

  PROCESS_BEGIN();

#if CONTIKI_TARGET_ZOUL
  deployment_set_node_id_ieee_addr();
  deployment_print_id_info();
#endif

  simple_energest_start();

  /* Initialization */
  printf("Node ID: %u\n", node_id);
  printf("RTIMER_SECOND: %u\n", RTIMER_SECOND);
  printf("RANDOM_SEED_NUMBER: %u\n", RANDOM_SEED_NUMBER);

  printf("BURST_T_SLOT: %ldms\n", RTIMERTICKS_TO_US_64(BURST_T_SLOT)/1000);
  printf("BURST_T_DELAY: %ldms\n", RTIMERTICKS_TO_US_64(BURST_T_DELAY)/1000);
  printf("BURST_X_SLOT: %ldms\n", RTIMERTICKS_TO_US_64(BURST_X_SLOT)/1000);
  printf("BURST_X_DUR: %ldms\n", RTIMERTICKS_TO_US_64(BURST_X_DUR)/1000);
  printf("BURST_NUM_RXS: %d\n", BURST_NUM_RXS);
  printf("BURST_NUM_TXS: %d\n", BURST_NUM_TXS);

  printf("SCATTER_T_SLOT: %ldms\n", RTIMERTICKS_TO_US_64(SCATTER_T_SLOT)/1000);
  printf("SCATTER_X_SLOT: %ldms\n", RTIMERTICKS_TO_US_64(SCATTER_X_SLOT)/1000);
  printf("SCATTER_NUM_TXS: %d\n", SCATTER_NUM_TXS);
  
  /* Begin with radio off */
  NETSTACK_RADIO.off();
  
  /* Configure radio filtering */
  NETSTACK_RADIO.set_value(RADIO_PARAM_RX_MODE, 0);

  /* Wait at the beginning a random time to de-synchronize node start */
  random_init(node_id * RANDOM_SEED_NUMBER);

  etimer_set(&et, random_rand() % CLOCK_SECOND);
  PROCESS_WAIT_UNTIL(etimer_expired(&et));
  
  /* Start ND Primitive */
  nd_start(ND_BURST, &rcb);
  // nd_start(ND_SCATTER, &rcb);

  /* Do nothing else */
  while (1) {
    PROCESS_WAIT_EVENT();
  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
