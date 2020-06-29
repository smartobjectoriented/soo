#ifndef BLUETOOTH_SL
#define BLUETOOTH_SL

#define BT_MAX_PACKET_TRANSID 0xffffff
#define BT_LAST_PACKET		(1 << 24)

#define NB_BUF 8
#define MAX_PKT_SIZE 1024


void bluetooth_init(void);

#endif /* BLUETOOTH_SL */
