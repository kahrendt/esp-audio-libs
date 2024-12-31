// A C++ class wrapper for the ART resampler

#pragma once

#include "art_resampler.h"
#include "art_biquad.h"

#include <algorithm>
#include <esp_heap_caps.h>

namespace resampler {

// Class that wraps the ART resampler for straightforward use

struct ResamplerResults {
  size_t frames_used;
  size_t frames_generated;
  size_t predicted_frames_used;
  uint32_t clipped_samples;
};

class Resampler {
 public:
  Resampler(size_t input_buffer_samples, size_t output_buffer_samples)
      : input_buffer_samples_(input_buffer_samples), output_buffer_samples_(output_buffer_samples) {}
  ~Resampler();

  /// @brief Initializes the resampler
  /// @param target_sample_rate Output sample rate
  /// @param source_sample_rate Input sample rate
  /// @param input_bits Number of bits in the incoming audio samples
  /// @param output_bits Number of bits in the output audio samples
  /// @param channels Source audio's number of channels
  /// @param number_of_taps Number of taps per filter. Must be a multiple of 4.
  /// @param number_of_filters Number of windowed-sinc filters. Must be greater than 1.
  /// @param use_pre_post_filter Enable a cascading biquad filter before or after the FIR filter
  /// @param subsample_interpolate Linearly interpolate the output sample with the two nearest sinc filters
  /// @return True if buffers were allocated succesfully, false otherwise.
  bool initialize(float target_sample_rate, float source_sample_rate, uint8_t input_bits, uint8_t output_bits,
                           uint8_t channels, uint16_t number_of_taps, uint16_t number_of_filters,
                           bool use_pre_post_filter, bool subsample_interpolate) ;

  /// @brief Resamples the input samples to the initalized sample rate
  /// @param input Pointer to source samples as a uint8_t buffer
  /// @param output Pointer to write resampled samples as a uint8_t buffer
  /// @param input_frames_available Frames available at the input source pointer
  /// @param output_frames_free Frames free at the output sink pointer
  /// @param gain Gain (in dB) to apply before resampling
  /// @return (ResamplerResults) Information about the number of frames used and  generated
  ResamplerResults resample(const uint8_t *input_buffer, uint8_t *output_buffer, size_t input_frames_available,
                         size_t output_frames_free, float gain);

 protected:
  float *float_input_buffer_{nullptr};
  size_t input_buffer_samples_;

  float *float_output_buffer_{nullptr};
  size_t output_buffer_samples_;

  Resample *resampler_{nullptr};

  Biquad lowpass_[2][2];
  BiquadCoefficients lowpass_coeff_;

  uint16_t number_of_taps_;
  uint16_t number_of_filters_;

  float sample_ratio_{1.0};
  float lowpass_ratio_{1.0};

  bool pre_filter_{false};
  bool post_filter_{false};

  uint8_t input_bits_;
  uint8_t output_bits_;
  uint8_t channels_;
};
}  // namespace resampler
