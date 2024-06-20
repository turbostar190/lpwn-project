/*---------------------------------------------------------------------------*/
#define ND_BURST 1
#define ND_SCATTER 2
/*---------------------------------------------------------------------------*/
#define EPOCH_INTERVAL_RT (RTIMER_SECOND)
#define T_SLOT (EPOCH_INTERVAL_RT / 10)
#define X_SLOT (EPOCH_INTERVAL_RT - T_SLOT) / 10 // times
#define T_DELAY (T_SLOT / 100)
#define X_DELAY (X_SLOT / 50)
/*---------------------------------------------------------------------------*/
#define MAX_NBR 64 /* Maximum number of neighbors */
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

void burst_tx(struct rtimer *t, void *ptr);
void burst_rx(struct rtimer *t, void *ptr);
void burst_off(struct rtimer *t, void *ptr);

void scatter_tx(struct rtimer *t, void *ptr);
void scatter_rx(struct rtimer *t, void *ptr);
void scatter_off(struct rtimer *t, void *ptr);
