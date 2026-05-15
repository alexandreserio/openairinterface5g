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

// Bucket an LDPC decode latency in microseconds into NR_UE_STATS_LAT_BUCKETS bins.
// Buckets (us): [<10, 10-19, 20-49, 50-99, 100-199, 200-499, 500-999, >=1000]
static inline unsigned bucket_decode_us(uint32_t us)
{
  if (us < 10) return 0;
  if (us < 20) return 1;
  if (us < 50) return 2;
  if (us < 100) return 3;
  if (us < 200) return 4;
  if (us < 500) return 5;
  if (us < 1000) return 6;
  return 7;
}

// Bucket a deadline-miss severity in microseconds.
// Buckets (us): [<10, 10-99, 100-999, >=1000]
static inline unsigned bucket_deadline_us(uint32_t us)
{
  if (us < 10) return 0;
  if (us < 100) return 1;
  if (us < 1000) return 2;
  return 3;
}

// CAS-loop max for atomic_uint_fast32_t.
static inline void atomic_max_u32(atomic_uint_fast32_t *target, uint_fast32_t value)
{
  uint_fast32_t current = atomic_load_explicit(target, memory_order_relaxed);
  while (value > current
         && !atomic_compare_exchange_weak_explicit(target,
                                                   &current,
                                                   value,
                                                   memory_order_relaxed,
                                                   memory_order_relaxed)) {
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
          "ldpc_count,ldpc_failures,ldpc_fail_rate,"
          "ldpc_avg_iter,"
          "ldpc_bg1_count,ldpc_bg2_count,"
          "ldpc_bg1_r13,ldpc_bg1_r23,ldpc_bg1_r89,"
          "ldpc_bg2_r15,ldpc_bg2_r13,ldpc_bg2_r23,"
          "ldpc_iter_hist,"
          "ldpc_decode_us_avg,ldpc_decode_us_max,ldpc_decode_us_hist,"
          "ldpc_rv_count,ldpc_rv_fail,"
          "ldpc_Z_avg,ldpc_Z_max,ldpc_C_avg,ldpc_C_max,"
          "rlc_retx_count,"
          "deadline_miss_count,deadline_miss_us_avg,deadline_miss_us_max,deadline_miss_hist,"
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

void nr_ue_stats_add_ldpc(int iterations,
                          int max_iterations,
                          bool success,
                          int BG,
                          int R,
                          int rv_index,
                          int Z,
                          int C,
                          uint32_t decode_us)
{
  atomic_fetch_add_explicit(&g_stats.ldpc_count, 1, memory_order_relaxed);
  if (!success) {
    atomic_fetch_add_explicit(&g_stats.ldpc_failures, 1, memory_order_relaxed);
  }
  if (BG == 1) {
    atomic_fetch_add_explicit(&g_stats.ldpc_bg1_count, 1, memory_order_relaxed);
    if (R == 13)
      atomic_fetch_add_explicit(&g_stats.ldpc_bg1_r13_count, 1, memory_order_relaxed);
    else if (R == 23)
      atomic_fetch_add_explicit(&g_stats.ldpc_bg1_r23_count, 1, memory_order_relaxed);
    else if (R == 89)
      atomic_fetch_add_explicit(&g_stats.ldpc_bg1_r89_count, 1, memory_order_relaxed);
  } else if (BG == 2) {
    atomic_fetch_add_explicit(&g_stats.ldpc_bg2_count, 1, memory_order_relaxed);
    if (R == 15)
      atomic_fetch_add_explicit(&g_stats.ldpc_bg2_r15_count, 1, memory_order_relaxed);
    else if (R == 13)
      atomic_fetch_add_explicit(&g_stats.ldpc_bg2_r13_count, 1, memory_order_relaxed);
    else if (R == 23)
      atomic_fetch_add_explicit(&g_stats.ldpc_bg2_r23_count, 1, memory_order_relaxed);
  }

  // Iteration histogram. Clip into [0, NR_UE_STATS_ITER_BUCKETS-1].
  int iter_bin = iterations;
  if (iter_bin < 0)
    iter_bin = 0;
  if (iter_bin >= NR_UE_STATS_ITER_BUCKETS)
    iter_bin = NR_UE_STATS_ITER_BUCKETS - 1;
  atomic_fetch_add_explicit(&g_stats.ldpc_iter_hist[iter_bin], 1, memory_order_relaxed);

  // Decode time aggregates and histogram.
  atomic_fetch_add_explicit(&g_stats.ldpc_decode_us_sum, decode_us, memory_order_relaxed);
  atomic_max_u32(&g_stats.ldpc_decode_us_max, decode_us);
  atomic_fetch_add_explicit(&g_stats.ldpc_decode_us_hist[bucket_decode_us(decode_us)], 1, memory_order_relaxed);

  // Per-RV bookkeeping. rv_index outside {0..3} folded into 0 (defensive).
  unsigned rv_bin = (rv_index >= 0 && rv_index < NR_UE_STATS_RV_BUCKETS) ? (unsigned)rv_index : 0;
  atomic_fetch_add_explicit(&g_stats.ldpc_rv_count[rv_bin], 1, memory_order_relaxed);
  if (!success)
    atomic_fetch_add_explicit(&g_stats.ldpc_rv_fail[rv_bin], 1, memory_order_relaxed);

  // Z / C aggregates.
  if (Z > 0) {
    atomic_fetch_add_explicit(&g_stats.ldpc_Z_sum, (uint_fast64_t)Z, memory_order_relaxed);
    atomic_max_u32(&g_stats.ldpc_Z_max, (uint_fast32_t)Z);
  }
  if (C > 0) {
    atomic_fetch_add_explicit(&g_stats.ldpc_C_sum, (uint_fast64_t)C, memory_order_relaxed);
    atomic_max_u32(&g_stats.ldpc_C_max, (uint_fast32_t)C);
  }
  (void)max_iterations;
}

void nr_ue_stats_add_rlc_retx(void)
{
  atomic_fetch_add_explicit(&g_stats.rlc_retx_count, 1, memory_order_relaxed);
}

void nr_ue_stats_add_deadline_miss(uint32_t missed_by_us)
{
  atomic_fetch_add_explicit(&g_stats.deadline_miss_count, 1, memory_order_relaxed);
  atomic_fetch_add_explicit(&g_stats.deadline_miss_us_sum, missed_by_us, memory_order_relaxed);
  atomic_max_u32(&g_stats.deadline_miss_us_max, missed_by_us);
  atomic_fetch_add_explicit(&g_stats.deadline_miss_hist[bucket_deadline_us(missed_by_us)], 1, memory_order_relaxed);
}

// Snapshot+reset all atomic histogram entries into a caller-provided buffer.
static void snapshot_reset_hist(atomic_uint_fast32_t *src, uint32_t *dst, size_t n)
{
  for (size_t i = 0; i < n; i++)
    dst[i] = (uint32_t)atomic_exchange_explicit(&src[i], 0, memory_order_relaxed);
}

// Write a histogram as a pipe-separated single-column value (parses cleanly in pandas
// with df[col].str.split('|', expand=True).astype(int)).
static void fprintf_hist(FILE *f, const uint32_t *h, size_t n)
{
  for (size_t i = 0; i < n; i++)
    fprintf(f, "%s%u", i == 0 ? "" : "|", h[i]);
  fputc(',', f);
}

void nr_ue_stats_dump_and_reset(void)
{
  int64_t rsrp_sum = g_stats.rsrp_sum;
  uint32_t rsrp_cnt = g_stats.rsrp_count;
  double sinr_sum = g_stats.sinr_sum;
  uint32_t sinr_cnt = g_stats.sinr_count;
  uint_fast32_t ldpc_cnt = atomic_exchange_explicit(&g_stats.ldpc_count, 0, memory_order_relaxed);
  uint_fast32_t ldpc_fail = atomic_exchange_explicit(&g_stats.ldpc_failures, 0, memory_order_relaxed);
  uint_fast32_t ldpc_bg1 = atomic_exchange_explicit(&g_stats.ldpc_bg1_count, 0, memory_order_relaxed);
  uint_fast32_t ldpc_bg2 = atomic_exchange_explicit(&g_stats.ldpc_bg2_count, 0, memory_order_relaxed);
  uint_fast32_t ldpc_bg1_r13 = atomic_exchange_explicit(&g_stats.ldpc_bg1_r13_count, 0, memory_order_relaxed);
  uint_fast32_t ldpc_bg1_r23 = atomic_exchange_explicit(&g_stats.ldpc_bg1_r23_count, 0, memory_order_relaxed);
  uint_fast32_t ldpc_bg1_r89 = atomic_exchange_explicit(&g_stats.ldpc_bg1_r89_count, 0, memory_order_relaxed);
  uint_fast32_t ldpc_bg2_r15 = atomic_exchange_explicit(&g_stats.ldpc_bg2_r15_count, 0, memory_order_relaxed);
  uint_fast32_t ldpc_bg2_r13 = atomic_exchange_explicit(&g_stats.ldpc_bg2_r13_count, 0, memory_order_relaxed);
  uint_fast32_t ldpc_bg2_r23 = atomic_exchange_explicit(&g_stats.ldpc_bg2_r23_count, 0, memory_order_relaxed);

  uint32_t iter_hist[NR_UE_STATS_ITER_BUCKETS];
  uint32_t lat_hist[NR_UE_STATS_LAT_BUCKETS];
  uint32_t rv_cnt[NR_UE_STATS_RV_BUCKETS];
  uint32_t rv_fail[NR_UE_STATS_RV_BUCKETS];
  uint32_t dl_miss_hist[NR_UE_STATS_DL_MISS_BUCKETS];
  snapshot_reset_hist(g_stats.ldpc_iter_hist, iter_hist, NR_UE_STATS_ITER_BUCKETS);
  snapshot_reset_hist(g_stats.ldpc_decode_us_hist, lat_hist, NR_UE_STATS_LAT_BUCKETS);
  snapshot_reset_hist(g_stats.ldpc_rv_count, rv_cnt, NR_UE_STATS_RV_BUCKETS);
  snapshot_reset_hist(g_stats.ldpc_rv_fail, rv_fail, NR_UE_STATS_RV_BUCKETS);
  snapshot_reset_hist(g_stats.deadline_miss_hist, dl_miss_hist, NR_UE_STATS_DL_MISS_BUCKETS);

  uint_fast64_t ldpc_decode_us_sum = atomic_exchange_explicit(&g_stats.ldpc_decode_us_sum, 0, memory_order_relaxed);
  uint_fast32_t ldpc_decode_us_max = atomic_exchange_explicit(&g_stats.ldpc_decode_us_max, 0, memory_order_relaxed);
  uint_fast64_t ldpc_Z_sum = atomic_exchange_explicit(&g_stats.ldpc_Z_sum, 0, memory_order_relaxed);
  uint_fast32_t ldpc_Z_max = atomic_exchange_explicit(&g_stats.ldpc_Z_max, 0, memory_order_relaxed);
  uint_fast64_t ldpc_C_sum = atomic_exchange_explicit(&g_stats.ldpc_C_sum, 0, memory_order_relaxed);
  uint_fast32_t ldpc_C_max = atomic_exchange_explicit(&g_stats.ldpc_C_max, 0, memory_order_relaxed);

  uint_fast32_t rlc_retx = atomic_exchange_explicit(&g_stats.rlc_retx_count, 0, memory_order_relaxed);
  uint_fast32_t deadline_miss = atomic_exchange_explicit(&g_stats.deadline_miss_count, 0, memory_order_relaxed);
  uint_fast64_t deadline_miss_us_sum = atomic_exchange_explicit(&g_stats.deadline_miss_us_sum, 0, memory_order_relaxed);
  uint_fast32_t deadline_miss_us_max = atomic_exchange_explicit(&g_stats.deadline_miss_us_max, 0, memory_order_relaxed);

  uint64_t late_packets_cnt = g_callbacks.total_getter ? g_callbacks.total_getter() : 0;
  uint64_t underflow_cnt = g_callbacks.underflow_getter ? g_callbacks.underflow_getter() : 0;

  g_stats.rsrp_sum = 0;
  g_stats.rsrp_count = 0;
  g_stats.sinr_sum = 0.0;
  g_stats.sinr_count = 0;
  if (g_callbacks.resetter)
    g_callbacks.resetter();

  if (rsrp_cnt == 0 && sinr_cnt == 0 && ldpc_cnt == 0 && rlc_retx == 0 && deadline_miss == 0)
    return;

  // Derive iter mean from histogram (lossless: Σ i*hist[i] / Σ hist[i]).
  uint64_t iter_weighted_sum = 0;
  uint64_t iter_total = 0;
  for (unsigned i = 0; i < NR_UE_STATS_ITER_BUCKETS; i++) {
    iter_weighted_sum += (uint64_t)i * iter_hist[i];
    iter_total += iter_hist[i];
  }

  FILE *csv = open_csv();
  if (csv) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    double elapsed = (now.tv_sec - g_start_ts.tv_sec) + (now.tv_nsec - g_start_ts.tv_nsec) / 1e9;

    fprintf(csv, "%" PRIu64 ",%.3f,", g_period_idx, elapsed);
    if (rsrp_cnt > 0)
      fprintf(csv, "%.3f,%u,", (double)rsrp_sum / rsrp_cnt, rsrp_cnt);
    else
      fprintf(csv, ",0,");

    if (sinr_cnt > 0)
      fprintf(csv, "%.3f,%u,", sinr_sum / sinr_cnt, sinr_cnt);
    else
      fprintf(csv, ",0,");

    // ldpc_count, ldpc_failures, ldpc_fail_rate, ldpc_avg_iter
    if (ldpc_cnt > 0) {
      fprintf(csv,
              "%u,%u,%.6f,%.4f,",
              (uint32_t)ldpc_cnt,
              (uint32_t)ldpc_fail,
              (double)ldpc_fail / ldpc_cnt,
              iter_total > 0 ? (double)iter_weighted_sum / iter_total : 0.0);
    } else {
      fprintf(csv, "0,0,,,");
    }

    fprintf(csv, "%u,%u,", (uint32_t)ldpc_bg1, (uint32_t)ldpc_bg2);
    fprintf(csv,
            "%u,%u,%u,%u,%u,%u,",
            (uint32_t)ldpc_bg1_r13,
            (uint32_t)ldpc_bg1_r23,
            (uint32_t)ldpc_bg1_r89,
            (uint32_t)ldpc_bg2_r15,
            (uint32_t)ldpc_bg2_r13,
            (uint32_t)ldpc_bg2_r23);

    fprintf_hist(csv, iter_hist, NR_UE_STATS_ITER_BUCKETS);

    // ldpc_decode_us_avg, ldpc_decode_us_max, ldpc_decode_us_hist
    if (ldpc_cnt > 0)
      fprintf(csv, "%.3f,%u,", (double)ldpc_decode_us_sum / ldpc_cnt, (uint32_t)ldpc_decode_us_max);
    else
      fprintf(csv, ",0,");
    fprintf_hist(csv, lat_hist, NR_UE_STATS_LAT_BUCKETS);

    // ldpc_rv_count, ldpc_rv_fail
    fprintf_hist(csv, rv_cnt, NR_UE_STATS_RV_BUCKETS);
    fprintf_hist(csv, rv_fail, NR_UE_STATS_RV_BUCKETS);

    // ldpc_Z_avg, ldpc_Z_max, ldpc_C_avg, ldpc_C_max
    if (ldpc_cnt > 0)
      fprintf(csv,
              "%.3f,%u,%.3f,%u,",
              (double)ldpc_Z_sum / ldpc_cnt,
              (uint32_t)ldpc_Z_max,
              (double)ldpc_C_sum / ldpc_cnt,
              (uint32_t)ldpc_C_max);
    else
      fprintf(csv, ",0,,0,");

    fprintf(csv, "%u,", (uint32_t)rlc_retx);

    // deadline_miss_count, _us_avg, _us_max, _hist
    if (deadline_miss > 0)
      fprintf(csv,
              "%u,%.3f,%u,",
              (uint32_t)deadline_miss,
              (double)deadline_miss_us_sum / deadline_miss,
              (uint32_t)deadline_miss_us_max);
    else
      fprintf(csv, "0,,0,");
    fprintf_hist(csv, dl_miss_hist, NR_UE_STATS_DL_MISS_BUCKETS);

    fprintf(csv, "%u,%u\n", (uint32_t)late_packets_cnt, (uint32_t)underflow_cnt);
  }
  g_period_idx++;

  LOG_I(NR_MAC, "UE stats (last %d frames):\n", PRINT_PERIOD_FRAMES);
  if (rsrp_cnt > 0)
    LOG_I(NR_MAC, "  SS-RSRP: avg %.1f dBm (%u meas)\n", (double)rsrp_sum / rsrp_cnt, rsrp_cnt);
  if (sinr_cnt > 0)
    LOG_I(NR_MAC, "  SS-SINR: avg %.2f dB (%u meas)\n", sinr_sum / sinr_cnt, sinr_cnt);
  if (ldpc_cnt > 0) {
    double avg_iter = iter_total > 0 ? (double)iter_weighted_sum / iter_total : 0.0;
    double fail_rate = (double)ldpc_fail / ldpc_cnt;
    LOG_I(NR_MAC,
          "  LDPC:    avg %.2f iter, fail rate %.5f (%u decodes, %u failures)\n",
          avg_iter,
          fail_rate,
          (uint32_t)ldpc_cnt,
          (uint32_t)ldpc_fail);
    LOG_I(NR_MAC,
          "  LDPC dt: avg %.1f us, max %u us (Z avg %.1f max %u, C avg %.2f max %u)\n",
          (double)ldpc_decode_us_sum / ldpc_cnt,
          (uint32_t)ldpc_decode_us_max,
          (double)ldpc_Z_sum / ldpc_cnt,
          (uint32_t)ldpc_Z_max,
          (double)ldpc_C_sum / ldpc_cnt,
          (uint32_t)ldpc_C_max);
    LOG_I(NR_MAC,
          "  LDPC rv: cnt %u/%u/%u/%u fail %u/%u/%u/%u\n",
          rv_cnt[0], rv_cnt[1], rv_cnt[2], rv_cnt[3],
          rv_fail[0], rv_fail[1], rv_fail[2], rv_fail[3]);
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
    if (ldpc_bg1 > 0) {
      LOG_I(NR_MAC,
            "  LDPC BG1 rates: 1/3 %u (%.1f%%), 2/3 %u (%.1f%%), 8/9 %u (%.1f%%)\n",
            (uint32_t)ldpc_bg1_r13,
            100.0 * ldpc_bg1_r13 / ldpc_bg1,
            (uint32_t)ldpc_bg1_r23,
            100.0 * ldpc_bg1_r23 / ldpc_bg1,
            (uint32_t)ldpc_bg1_r89,
            100.0 * ldpc_bg1_r89 / ldpc_bg1);
    }
    if (ldpc_bg2 > 0) {
      LOG_I(NR_MAC,
            "  LDPC BG2 rates: 1/5 %u (%.1f%%), 1/3 %u (%.1f%%), 2/3 %u (%.1f%%)\n",
            (uint32_t)ldpc_bg2_r15,
            100.0 * ldpc_bg2_r15 / ldpc_bg2,
            (uint32_t)ldpc_bg2_r13,
            100.0 * ldpc_bg2_r13 / ldpc_bg2,
            (uint32_t)ldpc_bg2_r23,
            100.0 * ldpc_bg2_r23 / ldpc_bg2);
    }
  }
  if (rlc_retx > 0)
    LOG_I(NR_MAC, "  RLC:     %u retransmissions\n", (uint32_t)rlc_retx);
  if (deadline_miss > 0)
    LOG_I(NR_MAC,
          "  TX:      %u deadline misses (avg %.1f us, max %u us; <10/<100/<1k/>=1k = %u/%u/%u/%u)\n",
          (uint32_t)deadline_miss,
          (double)deadline_miss_us_sum / deadline_miss,
          (uint32_t)deadline_miss_us_max,
          dl_miss_hist[0], dl_miss_hist[1], dl_miss_hist[2], dl_miss_hist[3]);
  if (late_packets_cnt || underflow_cnt) {
    // LOG_I(NR_MAC, "  Late Packets: %" PRIu64 " total, Tx %" PRIu64 ", Rx %" PRIu64 ", Async %" PRIu64 "\n",
    // g_callbacks.total_getter(), g_callbacks.rx_getter(), g_callbacks.tx_getter(), g_callbacks.async_getter());
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
