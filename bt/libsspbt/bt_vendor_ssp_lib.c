#include "algo_api.h"
#include "algo_utils.h"
#include "bt_vendor_ssp_lib.h"

// Entry point of DLib
const bt_vendor_ssp_interface_t BLUETOOTH_VENDOR_SSP_LIB_INTERFACE = {
    sizeof(bt_vendor_ssp_interface_t),

    algo_p256_generate_public_key,
    algo_p192_generate_public_key,

    algo_p256_generate_private_key,
    algo_p192_generate_private_key,

    algo_p256_generate_dhkey,
    algo_p192_generate_dhkey,

    alogo_init,

    big2nd,
    dump_hex,
    char2bytes,
    char2bytes_bg,
    dump_hex_bg,
};