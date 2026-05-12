#ifndef __NR_UE_STATS_H__
#define __NR_UE_STATS_H__

#include <stdint.h>
#include <stdatomic.h>
#include <stdbool.h>

/// Central structure to gather different parameters from OAI
typedef struct {
  // RSRP
  int64_t rsrp_sum; /// Sum of all RSRP values
  uint32_t rsrp_count; /// Total number of RSRP values

  // SINR
  double sinr_sum; /// Sum of all SINR values
  uint32_t sinr_count; /// Total number of SINR values

  // LDPC
  atomic_uint_fast64_t ldpc_iter_sum; /// Sum of all LDPC iterations
  atomic_uint_fast32_t ldpc_count; /// Total number LDPC runs
  atomic_uint_fast32_t ldpc_failures; /// Total number of LDPC failures
  atomic_uint_fast32_t ldpc_bg1_count; /// Number of decodes using BG1
  atomic_uint_fast32_t ldpc_bg2_count; /// Number of decodes using BG2
  atomic_uint_fast32_t ldpc_bg1_r13_count; /// Number of decodes using BG1 1/3
  atomic_uint_fast32_t ldpc_bg1_r23_count; /// Number of decodes using BG1 2/3
  atomic_uint_fast32_t ldpc_bg1_r89_count; /// Number of decodes using BG1 8/9
  atomic_uint_fast32_t ldpc_bg2_r15_count; /// Number of decodes using BG2 1/5
  atomic_uint_fast32_t ldpc_bg2_r13_count; /// Number of decodes using BG2 1/3
  atomic_uint_fast32_t ldpc_bg2_r23_count; /// Number of decodes using BG2 2/3

  // RLC
  atomic_uint_fast32_t rlc_retx_count; /// Total number of RLC retransmissions

  // TX deadline misses (radio TX slot deadline missed in nr-ue.c)
  atomic_uint_fast32_t deadline_miss_count;
} nr_ue_stats_t;

/// Struct for USRP late packets / underflows getters callbacks
typedef struct {
  uint64_t (*async_getter)();
  uint64_t (*rx_getter)();
  uint64_t (*tx_getter)();
  uint64_t (*total_getter)();
  uint64_t (*underflow_getter)();
  void (*resetter)();
} nr_ue_stats_radio_callbacks_t;

void nr_ue_stats_add_rsrp(int rsrp_dBm);
void nr_ue_stats_add_sinr(float sinr_dB);
void nr_ue_stats_add_ldpc(int iterations, int max_iterations, bool success, int BG, int R);
void nr_ue_stats_add_rlc_retx(void);
void nr_ue_stats_add_deadline_miss(void);
void nr_ue_stats_dump_and_reset(void);
void nr_ue_stats_register_late_count_callbacks(uint64_t (*async_getter)(),
                                               uint64_t (*rx_getter)(),
                                               uint64_t (*tx_getter)(),
                                               uint64_t (*total_getter)(),
                                               uint64_t (*underflow_getter)(),
                                               void (*resetter)());
void nr_ue_stats_tick(int frame, int slot);

#endif
