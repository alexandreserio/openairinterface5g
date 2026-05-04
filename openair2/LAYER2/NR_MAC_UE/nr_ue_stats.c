#include "nr_ue_stats.h"
#include "common/utils/LOG/log.h"
#include <stdbool.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define PRINT_PERIOD_FRAMES 128
#define DEFAULT_CSV_PATH "nr_ue_stats.csv"

static nr_ue_stats_t g_stats;
static FILE *g_csv = NULL;
static uint64_t g_period_idx = 0;
static struct timespec g_start_ts;
static nr_ue_stats_radio_callbacks_t g_callbacks = {0};

static void close_csv(void)
{
  if (g_csv) {
    fclose(g_csv);
    g_csv = NULL;
  }
}

static FILE *open_csv(void)
{
  if (g_csv) {
    return g_csv;
  }

  const char *path = getenv("NR_UE_STATS_CSV");
  if (!path || path[0] == '\0') {
    path = DEFAULT_CSV_PATH;
  }

  g_csv = fopen(path, "w");
  if (!g_csv) {
    LOG_E(NR_MAC, "nr_ue_stats: could not open CSV file '%s'\n", path);
    return NULL;
  }

  setvbuf(g_csv, NULL, _IOLBF, 0);
  fprintf(g_csv,
          "period,elapsed_s,"
          "rsrp_avg_dBm,rsrp_count,"
          "sinr_avg_dB,sinr_count,"
          "ldpc_avg_iter,ldpc_fail_rate,ldpc_count,ldpc_failures,"
          "ldpc_bg1_count,ldpc_bg2_count,"
          "rlc_retx_count,"
          "late_packet_count,underflow_count\n");
  clock_gettime(CLOCK_MONOTONIC, &g_start_ts);
  atexit(close_csv);
  LOG_I(NR_MAC, "nr_ue_stats: writing CSV to '%s'\n", path);
  return g_csv;
}

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

void nr_ue_stats_add_ldpc(int iterations, int max_iterations, bool success, int BG)
{
  atomic_fetch_add_explicit(&g_stats.ldpc_iter_sum, (uint_fast64_t)iterations, memory_order_relaxed);
  atomic_fetch_add_explicit(&g_stats.ldpc_count, 1, memory_order_relaxed);
  if (!success)
    atomic_fetch_add_explicit(&g_stats.ldpc_failures, 1, memory_order_relaxed);
  if (BG == 1)
    atomic_fetch_add_explicit(&g_stats.ldpc_bg1_count, 1, memory_order_relaxed);
  else if (BG == 2)
    atomic_fetch_add_explicit(&g_stats.ldpc_bg2_count, 1, memory_order_relaxed);
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
  uint_fast32_t ldpc_bg1 = atomic_exchange_explicit(&g_stats.ldpc_bg1_count, 0, memory_order_relaxed);
  uint_fast32_t ldpc_bg2 = atomic_exchange_explicit(&g_stats.ldpc_bg2_count, 0, memory_order_relaxed);
  uint_fast32_t rlc_retx = atomic_exchange_explicit(&g_stats.rlc_retx_count, 0, memory_order_relaxed);
  uint64_t late_packets_cnt = g_callbacks.total_getter();
  uint64_t underflow_cnt = g_callbacks.underflow_getter();

  g_stats.rsrp_sum = 0;
  g_stats.rsrp_count = 0;
  g_stats.sinr_sum = 0.0;
  g_stats.sinr_count = 0;
  g_callbacks.resetter();

  if (rsrp_cnt == 0 && sinr_cnt == 0 && ldpc_cnt == 0 && rlc_retx == 0)
    return;

  FILE *csv = open_csv();
  if (csv) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    double elapsed = (now.tv_sec - g_start_ts.tv_sec) + (now.tv_nsec - g_start_ts.tv_nsec) / 1e9;

    fprintf(csv, "%" PRIu64 ",%.3f,", g_period_idx, elapsed);
    if (rsrp_cnt > 0) {
      fprintf(csv, "%.3f,%u,", (double)rsrp_sum / rsrp_cnt, rsrp_cnt);
    }
    else {
      fprintf(csv, ",0,");
    }

    if (sinr_cnt > 0) {
      fprintf(csv, "%.3f,%u,", sinr_sum / sinr_cnt, sinr_cnt);
    }
    else {
      fprintf(csv, ",0,");
    }

    if (ldpc_cnt > 0) {
      fprintf(csv,
              "%.4f,%.6f,%u,%u,",
              (double)ldpc_iter / ldpc_cnt,
              (double)ldpc_fail / ldpc_cnt,
              (uint32_t)ldpc_cnt,
              (uint32_t)ldpc_fail);
    }
    else {
      fprintf(csv, ",,0,0,");
    }

    fprintf(csv, "%u,%u,%u,", (uint32_t)ldpc_bg1, (uint32_t)ldpc_bg2, (uint32_t)rlc_retx);

    fprintf(csv, "%u,%u\n", (uint32_t)late_packets_cnt, (uint32_t)underflow_cnt);
  }
  g_period_idx++;

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
    uint_fast32_t bg_total = ldpc_bg1 + ldpc_bg2;
    if (bg_total > 0) {
      LOG_I(NR_MAC,
            "  LDPC BG: BG1 %u/%u (%.1f%%), BG2 %u/%u (%.1f%%)\n",
            (uint32_t)ldpc_bg1,
            (uint32_t)bg_total,
            100.0 * ldpc_bg1 / bg_total,
            (uint32_t)ldpc_bg2,
            (uint32_t)bg_total,
            100.0 * ldpc_bg2 / bg_total);
    }
  }
  if (rlc_retx > 0)
    LOG_I(NR_MAC, "  RLC:     %u retransmissions\n", (uint32_t)rlc_retx);
  if (late_packets_cnt || underflow_cnt) {
    // LOG_I(NR_MAC, "  Late Packets: %" PRIu64 " total, Tx %" PRIu64 ", Rx %" PRIu64 ", Async %" PRIu64 "\n", g_callbacks.total_getter(), g_callbacks.rx_getter(), g_callbacks.tx_getter(), g_callbacks.async_getter());
    LOG_I(NR_MAC, "  Late Packets: %" PRIu64 ", Underflows: %" PRIu64 "\n", late_packets_cnt, underflow_cnt);
  }
}

void nr_ue_stats_register_late_count_callbacks(uint64_t (*async_getter)(),
                                               uint64_t (*rx_getter)(),
                                               uint64_t (*tx_getter)(),
                                               uint64_t (*total_getter)(),
                                               uint64_t (*underflow_getter)(),
                                               void (*resetter)())
{
  g_callbacks.async_getter = async_getter;
  g_callbacks.rx_getter = rx_getter;
  g_callbacks.tx_getter = tx_getter;
  g_callbacks.total_getter = total_getter;
  g_callbacks.underflow_getter = underflow_getter;
  g_callbacks.resetter = resetter;
}

void nr_ue_stats_tick(int frame, int slot)
{
  if (slot != 0)
    return;
  if ((frame % PRINT_PERIOD_FRAMES) != 0)
    return;
  nr_ue_stats_dump_and_reset();
}
