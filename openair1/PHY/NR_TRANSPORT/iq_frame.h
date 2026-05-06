// header file created for IQ frame format definition used in NR ULSCH demodulation IQ dumping - ADRIANO COSTA

#ifndef IQ_FRAME_H
#define IQ_FRAME_H

#include <stdint.h>

#define IQ_FRAME_MAGIC 0x31465149 

typedef enum {
  IQ_FMT_INT16 = 1
} iq_format_t;

typedef struct __attribute__((packed)) {
  uint32_t magic;
  uint8_t  version;
  uint8_t  format;
  uint16_t reserved;
  uint64_t frame_id;
  uint64_t timestamp_ns;
  uint32_t sample_count;
  uint16_t rnti;
  uint16_t reserved2;
} iq_frame_hdr_t;

#endif

