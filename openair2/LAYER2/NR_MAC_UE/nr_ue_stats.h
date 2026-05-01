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

  // RLC
  atomic_uint_fast32_t rlc_retx_count; /// Total number of RLC retransmissions
} nr_ue_stats_t;

void nr_ue_stats_add_rsrp(int rsrp_dBm);
void nr_ue_stats_add_sinr(float sinr_dB);
void nr_ue_stats_add_ldpc(int iterations, int max_iterations, bool success, int BG);
void nr_ue_stats_add_rlc_retx(void);
void nr_ue_stats_dump_and_reset(void);
void nr_ue_stats_tick(int frame, int slot);

#endif
