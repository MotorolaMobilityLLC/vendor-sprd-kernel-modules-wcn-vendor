#ifndef __BT_VENDOR_SSP_LIB_H__
#define __BT_VENDOR_SSP_LIB_H__

/*
* Bluetooth Host SSP VENDOR Interface
*/

typedef struct {
    /** Set to sizeof(bt_vendor_ssp_interface_t) */
    size_t size;
    void (*algo_p256_generate_public_key)(uint8_t *own_private_key, uint8_t *own_public_key_x, uint8_t *own_public_key_y);
    void (*algo_p192_generate_public_key)(uint8_t *own_private_key, uint8_t *own_public_key_x, uint8_t *own_public_key_y);

    void (*algo_p256_generate_private_key)(uint8_t *private_key);
    void (*algo_p192_generate_private_key)(uint8_t *private_key);

    void (*algo_p256_generate_dhkey)(uint8_t *own_private_key, uint8_t *peer_public_key_x, uint8_t *peer_public_key_y, uint8_t *ecdh_key);
    void (*algo_p192_generate_dhkey)(uint8_t *own_private_key, uint8_t *peer_public_key_x, uint8_t *peer_public_key_y, uint8_t *ecdh_key);

    void (*alogo_init)(void);

    void (*big2nd)(unsigned char *src,  size_t size);
    void (*dump_hex)(const char *tag, unsigned char *dst,  size_t size);
    void (*char2bytes)(char* src, unsigned char *dst,  size_t size);
    void (*char2bytes_bg)(char* src, unsigned char *dst,  size_t size);
    void (*dump_hex_bg)(const char *tag, unsigned char *dst,  size_t size);

}bt_vendor_ssp_interface_t;

extern const bt_vendor_ssp_interface_t BLUETOOTH_VENDOR_SSP_LIB_INTERFACE;

#endif