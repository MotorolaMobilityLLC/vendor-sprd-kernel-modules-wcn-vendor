#ifndef BT_SPRD_SNOOP_H
#define BT_SPRD_SNOOP_H

void bt_snoop_packet_cap(int type, const uint8_t *packet, bool is_received);
void bt_snoop_thread_start(void);
void bt_snoop_thread_stop(void);

#endif