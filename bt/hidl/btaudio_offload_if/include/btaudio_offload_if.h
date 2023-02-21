namespace aidl {
namespace android {
namespace hardware {
namespace bluetooth {
namespace audio {

typedef enum {
    ENC_CODEC_TYPE_INVALID =  0xFFFFFFFF,
    ENC_CODEC_TYPE_AAC = 0x04000000,
    ENC_CODEC_TYPE_SBC = 0x1F000000,
    ENC_CODEC_TYPE_APTX = 0x20000000,
    ENC_CODEC_TYPE_APTX_HD = 0x21000000,
    ENC_CODEC_TYPE_LDAC = 0x23000000,
    ENC_CODEC_TYPE_PCM = 0x1,
} enc_codec_t;

//in packages/modules/Bluetooth/system/audio_a2dp_hw/src/audio_a2dp_hw.cc
//vendor/audio don't has a2dp_state_t
typedef enum {
  AUDIO_A2DP_STATE_STARTING,
  AUDIO_A2DP_STATE_STARTED,
  AUDIO_A2DP_STATE_STOPPING,
  AUDIO_A2DP_STATE_STOPPED,
  /* need explicit set param call to resume (suspend=false) */
  AUDIO_A2DP_STATE_SUSPENDED,
  AUDIO_A2DP_STATE_STANDBY /* allows write to autoresume */
} a2dp_state_t;

/* data type for the SBC Codec Information Element */
typedef struct {
  uint8_t num_subbands; /* Number of subbands */
  uint8_t block_len;    /* Block length */
  uint8_t samp_freq;    /* Sampling frequency */
  uint8_t ch_mode;      /* Channel mode */
  uint8_t alloc_method; /* Allocation method */
  uint8_t min_bitpool;  /* Minimum bitpool */
  uint8_t max_bitpool;  /* Maximum bitpool */
  uint8_t bits_per_sample;
  //btav_a2dp_codec_bits_per_sample_t bits_per_sample;
} tA2DP_SBC_CIE;

/*temp test for a2dp offload para*/
/*
typedef enum {
  BTAV_A2DP_CODEC_BITS_PER_SAMPLE_NONE = 0x0,
  BTAV_A2DP_CODEC_BITS_PER_SAMPLE_16 = 0x1 << 0,
  BTAV_A2DP_CODEC_BITS_PER_SAMPLE_24 = 0x1 << 1,
  BTAV_A2DP_CODEC_BITS_PER_SAMPLE_32 = 0x1 << 2
} btav_a2dp_codec_bits_per_sample_t;
*/
}  // namespace audio
}  // namespace bluetooth
}  // namespace hardware
}  // namespace android
}  // namespace aidl
