/*---------------------------------------------------------------------------*/
#define ND_BURST 1
#define ND_SCATTER 2
/*---------------------------------------------------------------------------*/

#define EPOCH_INTERVAL_RT (RTIMER_SECOND)

#define BURST_T_SLOT ((EPOCH_INTERVAL_RT / 10) * 3) // 200ms // 300ms
#define BURST_T_DELAY (BURST_T_SLOT / 10) // 20ms // 30ms
#define BURST_X_SLOT (((EPOCH_INTERVAL_RT - BURST_T_SLOT) / 100) * 5) // 40ms // 35ms
#define BURST_X_DUR (BURST_X_SLOT / 5) // 8ms // 7ms
#define BURST_NUM_RXS ((EPOCH_INTERVAL_RT - BURST_T_SLOT) / BURST_X_SLOT) // 20 times
#define BURST_NUM_TXS (BURST_T_SLOT / BURST_T_DELAY) // 10 times

#define SCATTER_T_SLOT ((EPOCH_INTERVAL_RT / 10) * 2) // 200ms
#define SCATTER_X_SLOT ((EPOCH_INTERVAL_RT / 100) * 8) // 80ms
#define SCATTER_NUM_TXS ((EPOCH_INTERVAL_RT - SCATTER_T_SLOT) / SCATTER_X_SLOT) // 10 times

/*---------------------------------------------------------------------------*/
#define MAX_NBR 156 // 64 /* Maximum number of neighbors, 156 on testbed */
/*---------------------------------------------------------------------------*/
void nd_recv(void); /* Called by lower layers when a message is received */
/*---------------------------------------------------------------------------*/
/* ND callbacks:
 * 	nd_new_nbr: inform the application when a new neighbor is discovered
 *	nd_epoch_end: report to the application the number of neighbors discovered
 *				  at the end of the epoch
 */
struct nd_callbacks {
  void (* nd_new_nbr)(uint16_t epoch, uint8_t nbr_id);
  void (* nd_epoch_end)(uint16_t epoch, uint8_t num_nbr);
};
/*---------------------------------------------------------------------------*/
/* Start selected ND primitive (ND_BURST or ND_SCATTER) */
void nd_start(uint8_t mode, const struct nd_callbacks *cb);
/*---------------------------------------------------------------------------*/

struct beacon_msg
{
  uint16_t node_id;
  uint8_t dummy;
} __attribute__((packed));

void reset_epoch();

void burst_tx(struct rtimer *t, void *ptr);
void burst_rx(struct rtimer *t, void *ptr);
void burst_off(struct rtimer *t, void *ptr);

void scatter_tx(struct rtimer *t, void *ptr);
void scatter_rx(struct rtimer *t, void *ptr);
