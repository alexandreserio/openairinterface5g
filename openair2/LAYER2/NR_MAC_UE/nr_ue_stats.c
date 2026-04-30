#include "nr_ue_stats.h"
#include "common/utils/LOG/log.h"
#include <stdbool.h>

#define PRINT_PERIOD_FRAMES 128

static nr_ue_stats_t g_stats;

void nr_ue_stats_add_rsrp(int rsrp_dBm)
{
  g_stats.rsrp_sum += rsrp_dBm;
  g_stats.rsrp_count++;
}

void nr_ue_stats_add_sinr(float sinr_dB)
{
  g_stats.sinr_sum += sinr_dB;
  g_stats.sinr_count++;
}

void nr_ue_stats_add_ldpc(int iterations, int max_iterations, bool success)
{
  atomic_fetch_add_explicit(&g_stats.ldpc_iter_sum, (uint_fast64_t)iterations, memory_order_relaxed);
  atomic_fetch_add_explicit(&g_stats.ldpc_count, 1, memory_order_relaxed);
  if (!success) 
    atomic_fetch_add_explicit(&g_stats.ldpc_failures, 1, memory_order_relaxed);
  (void)max_iterations;
}

void nr_ue_stats_add_rlc_retx(void)
{
  atomic_fetch_add_explicit(&g_stats.rlc_retx_count, 1, memory_order_relaxed);
}

void nr_ue_stats_dump_and_reset(void)
{
  int64_t rsrp_sum = g_stats.rsrp_sum;
  uint32_t rsrp_cnt = g_stats.rsrp_count;
  double sinr_sum = g_stats.sinr_sum;
  uint32_t sinr_cnt = g_stats.sinr_count;
  uint_fast64_t ldpc_iter = atomic_exchange_explicit(&g_stats.ldpc_iter_sum, 0, memory_order_relaxed);
  uint_fast32_t ldpc_cnt = atomic_exchange_explicit(&g_stats.ldpc_count, 0, memory_order_relaxed);
  uint_fast32_t ldpc_fail = atomic_exchange_explicit(&g_stats.ldpc_failures, 0, memory_order_relaxed);
  uint_fast32_t rlc_retx = atomic_exchange_explicit(&g_stats.rlc_retx_count, 0, memory_order_relaxed);

  g_stats.rsrp_sum = 0;
  g_stats.rsrp_count = 0;
  g_stats.sinr_sum = 0.0;
  g_stats.sinr_count = 0;

  if (rsrp_cnt == 0 && sinr_cnt == 0 && ldpc_cnt == 0 && rlc_retx == 0)
    return;

  LOG_I(NR_MAC, "UE stats (last %d frames):\n", PRINT_PERIOD_FRAMES);
  if (rsrp_cnt > 0)
    LOG_I(NR_MAC, "  SS-RSRP: avg %.1f dBm (%u meas)\n", (double)rsrp_sum / rsrp_cnt, rsrp_cnt);
  if (sinr_cnt > 0)
    LOG_I(NR_MAC, "  SS-SINR: avg %.2f dB (%u meas)\n", sinr_sum / sinr_cnt, sinr_cnt);
  if (ldpc_cnt > 0) {
    double avg_iter = (double)ldpc_iter / ldpc_cnt;
    double fail_rate = (double)ldpc_fail / ldpc_cnt;
    LOG_I(NR_MAC,
          "  LDPC:    avg %.2f iter, fail rate %.5f (%u decodes, %u failures)\n",
          avg_iter,
          fail_rate,
          (uint32_t)ldpc_cnt,
          (uint32_t)ldpc_fail);
  }
  if (rlc_retx > 0)
    LOG_I(NR_MAC, "  RLC:     %u retransmissions\n", (uint32_t)rlc_retx);
}

void nr_ue_stats_tick(int frame, int slot)
{
  if (slot != 0)
    return;
  if ((frame % PRINT_PERIOD_FRAMES) != 0)
    return;
  nr_ue_stats_dump_and_reset();
}
