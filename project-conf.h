/*---------------------------------------------------------------------------*/
#ifndef PROJECT_CONF_H_
#define PROJECT_CONF_H_

/* Enable energy estimation */
#define ENERGEST_CONF_ON              1
#define IEEE802154_CONF_PANID            0xABCD

/*---------------------------------------------------------------------------*/
/* Enable the ROM bootloader */
#define ROM_BOOTLOADER_ENABLE                 1

/*---------------------------------------------------------------------------*/
/* Disable button shutdown functionality */
#define BUTTON_SENSOR_CONF_ENABLE_SHUTDOWN    0
/*---------------------------------------------------------------------------*/
/* Change to match your configuration */
#define RF_CORE_CONF_CHANNEL                 26
#define RF_BLE_CONF_ENABLED                   0

#if CONTIKI_TARGET_ZOUL
/* System clock runs at 32 MHz */
#define SYS_CTRL_CONF_SYS_DIV         SYS_CTRL_CLOCK_CTRL_SYS_DIV_32MHZ

/* IO clock runs at 32 MHz */
#define SYS_CTRL_CONF_IO_DIV          SYS_CTRL_CLOCK_CTRL_IO_DIV_32MHZ

#define NETSTACK_CONF_WITH_IPV6       0

#define CC2538_RF_CONF_CHANNEL        26

#define COFFEE_CONF_SIZE              0

#define LPM_CONF_MAX_PM               LPM_PM0
#else
#endif

/*---------------------------------------------------------------------------*/
#undef NETSTACK_CONF_RDC
#define NETSTACK_CONF_RDC      nd_rdc_driver
/*---------------------------------------------------------------------------*/
#undef NETSTACK_CONF_NETWORK
#define NETSTACK_CONF_NETWORK	nd_driver
/*---------------------------------------------------------------------------*/
#endif /* PROJECT_CONF_H_ */
/*---------------------------------------------------------------------------*/
