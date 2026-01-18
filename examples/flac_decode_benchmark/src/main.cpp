/**
 * FLAC Decode Benchmark for ESP32
 *
 * Measures FLAC decoding performance including:
 * - Per-frame decode timing (min/max/avg/stddev)
 * - Total decode time
 * - Real-Time Factor (RTF)
 *
 * Audio source: Public domain music (Beethoven's Eroica from Musopen)
 */

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "esp_timer.h"
#include "flac_decoder.h"
#include "test_audio_flac.h"

using namespace esp_audio_libs::flac;

// Statistics tracking
struct BenchmarkStats {
  uint32_t frame_count = 0;
  int64_t total_time_us = 0;
  int64_t min_time_us = INT64_MAX;
  int64_t max_time_us = 0;
  double sum_squared = 0.0;  // For standard deviation calculation
};

static void print_stream_info(FLACDecoder &decoder) {
  printf("\n=== FLAC Stream Info ===\n");
  printf("Sample rate: %lu Hz\n", (unsigned long)decoder.get_sample_rate());
  printf("Channels: %lu\n", (unsigned long)decoder.get_num_channels());
  printf("Bit depth: %lu\n", (unsigned long)decoder.get_sample_depth());
  printf("Max block size: %lu samples\n", (unsigned long)decoder.get_max_block_size());

  uint64_t total_samples = decoder.get_num_samples();
  if (total_samples > 0) {
    double duration = (double)total_samples / decoder.get_sample_rate();
    printf("Total samples: %llu\n", (unsigned long long)total_samples);
    printf("Audio duration: %.2f seconds\n", duration);
  } else {
    printf("Total samples: unknown\n");
  }
  printf("\n");
}

static void print_benchmark_results(BenchmarkStats &stats, FLACDecoder &decoder) {
  printf("\n=== Benchmark Results ===\n");
  printf("Frames decoded: %lu\n", (unsigned long)stats.frame_count);
  printf("Total decode time: %.2f ms\n", stats.total_time_us / 1000.0);

  if (stats.frame_count > 0) {
    double avg_time_us = (double)stats.total_time_us / stats.frame_count;

    printf("\nPer-frame timing:\n");
    printf("  Min: %lld us\n", (long long)stats.min_time_us);
    printf("  Max: %lld us\n", (long long)stats.max_time_us);
    printf("  Avg: %.1f us\n", avg_time_us);

    // Calculate standard deviation
    if (stats.frame_count > 1) {
      double variance = (stats.sum_squared / stats.frame_count) - (avg_time_us * avg_time_us);
      if (variance > 0) {
        double stddev = sqrt(variance);
        printf("  Std: %.1f us\n", stddev);
      }
    }
  }

  // Calculate Real-Time Factor
  uint64_t total_samples = decoder.get_num_samples();
  if (total_samples > 0 && stats.total_time_us > 0) {
    double audio_duration_s = (double)total_samples / decoder.get_sample_rate();
    double decode_duration_s = stats.total_time_us / 1000000.0;
    double rtf = decode_duration_s / audio_duration_s;
    double speedup = 1.0 / rtf;

    printf("\n=== Real-Time Factor ===\n");
    printf("RTF: %.4f\n", rtf);
    if (rtf < 1.0) {
      printf("Status: FASTER than real-time (%.1fx)\n", speedup);
    } else if (rtf > 1.0) {
      printf("Status: SLOWER than real-time (%.2fx)\n", rtf);
    } else {
      printf("Status: Real-time\n");
    }
  }
}

extern "C" void app_main(void) {
  printf("\n");
  printf("========================================\n");
  printf("   FLAC Decode Benchmark for ESP32\n");
  printf("========================================\n");
  printf("\n");

  printf("Input data size: %lu bytes (%.1f KB)\n", (unsigned long)test_audio_flac_data_len,
         test_audio_flac_data_len / 1024.0);

  // Initialize decoder
  FLACDecoder decoder;

  // Disable CRC checking for maximum decode speed
  decoder.set_crc_check_enabled(false);

  // Read FLAC header
  printf("Reading FLAC header...\n");
  FLACDecoderResult result = decoder.read_header(test_audio_flac_data, test_audio_flac_data_len);
  if (result != FLAC_DECODER_SUCCESS) {
    printf("ERROR: Failed to read FLAC header (error %d)\n", result);
    return;
  }

  print_stream_info(decoder);

  // Allocate output buffer
  uint32_t output_buffer_size = decoder.get_output_buffer_size_bytes();
  printf("Allocating output buffer: %lu bytes\n", (unsigned long)output_buffer_size);

  uint8_t *output_buffer = (uint8_t *)malloc(output_buffer_size);
  if (!output_buffer) {
    printf("ERROR: Failed to allocate output buffer\n");
    return;
  }

  // Set up buffer pointers for decoding
  const uint8_t *buffer_ptr = test_audio_flac_data + decoder.get_bytes_index();
  size_t bytes_remaining = test_audio_flac_data_len - decoder.get_bytes_index();

  printf("\nStarting decode benchmark...\n");
  printf("Data to decode: %zu bytes\n", bytes_remaining);

  BenchmarkStats stats;

  // Decode all frames
  while (bytes_remaining > 0) {
    uint32_t num_samples = 0;

    // Time the decode operation
    int64_t start_time = esp_timer_get_time();
    result = decoder.decode_frame(buffer_ptr, bytes_remaining, output_buffer, &num_samples);
    int64_t end_time = esp_timer_get_time();

    if (result == FLAC_DECODER_SUCCESS) {
      int64_t frame_time = end_time - start_time;

      // Update statistics
      stats.frame_count++;
      stats.total_time_us += frame_time;
      if (frame_time < stats.min_time_us) stats.min_time_us = frame_time;
      if (frame_time > stats.max_time_us) stats.max_time_us = frame_time;
      stats.sum_squared += (double)frame_time * frame_time;

      // Advance buffer position
      size_t consumed = decoder.get_bytes_index();
      buffer_ptr += consumed;
      bytes_remaining -= consumed;

      // Progress indicator every 100 frames
      if (stats.frame_count % 100 == 0) {
        printf("  Decoded %lu frames...\n", (unsigned long)stats.frame_count);
      }
    } else if (result == FLAC_DECODER_NO_MORE_FRAMES) {
      printf("End of stream reached.\n");
      break;
    } else if (result == FLAC_DECODER_ERROR_OUT_OF_DATA) {
      printf("Out of data after %lu frames.\n", (unsigned long)stats.frame_count);
      break;
    } else {
      printf("ERROR: Decode failed with error %d at frame %lu\n", result, (unsigned long)stats.frame_count);
      break;
    }
  }

  print_benchmark_results(stats, decoder);

  // Cleanup
  free(output_buffer);

  printf("\nBenchmark complete.\n");
}
