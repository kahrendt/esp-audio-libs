#include "resampler.h"

#include "esp_random.h"

namespace resampler {

Resampler::~Resampler() {
  if (this->resampler_ != nullptr) {
    resampleFree(this->resampler_);
  }

  if (this->float_input_buffer_ != nullptr) {
    free(this->float_input_buffer_);
  }
  if (this->float_output_buffer_ != nullptr) {
    free(this->float_output_buffer_);
  }

  if (this->tpdf_generators_ != nullptr) {
    free(this->tpdf_generators_);
  }

  if (this->error_ != nullptr) {
    free(this->error_);
  }
};

bool Resampler::initialize(float target_sample_rate, float source_sample_rate, uint8_t input_bits, uint8_t output_bits,
                           uint8_t channels, uint16_t number_of_taps, uint16_t number_of_filters,
                           bool use_pre_post_filter, bool subsample_interpolate) {
  this->input_bits_ = input_bits;
  this->output_bits_ = output_bits;
  this->channels_ = channels;
  this->number_of_taps_ = number_of_taps;
  this->number_of_filters_ = number_of_filters;

  this->error_ = (float *) malloc(channels * sizeof(float));
  memset(this->error_, 0, channels * sizeof(float));

  this->tpdf_dither_init_(channels);

  this->float_input_buffer_ =
      (float *) heap_caps_malloc(this->input_buffer_samples_ * sizeof(float), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

  this->float_output_buffer_ =
      (float *) heap_caps_malloc(this->output_buffer_samples_ * sizeof(float), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

  if ((this->float_input_buffer_ == nullptr) || (this->float_output_buffer_ == nullptr)) {
    return false;
  }

  int flags = 0;

  if (subsample_interpolate) {
    flags |= SUBSAMPLE_INTERPOLATE;
  }

  this->sample_ratio_ = target_sample_rate / source_sample_rate;

  if (this->sample_ratio_ < 1.0) {
    this->lowpass_ratio_ -= (10.24 / this->number_of_taps_);

    if (this->lowpass_ratio_ < 0.84) {
      this->lowpass_ratio_ = 0.84;
    }

    if (this->lowpass_ratio_ < this->sample_ratio_) {
      // avoid discontinuities near unity sample ratios
      this->lowpass_ratio_ = this->sample_ratio_;
    }
  }
  if (this->lowpass_ratio_ * this->sample_ratio_ < 0.98 && use_pre_post_filter) {
    float cutoff = this->lowpass_ratio_ * this->sample_ratio_ / 2.0;
    biquad_lowpass(&this->lowpass_coeff_, cutoff);
    this->pre_filter_ = true;
  }

  if (this->lowpass_ratio_ / this->sample_ratio_ < 0.98 && use_pre_post_filter && !this->pre_filter_) {
    float cutoff = this->lowpass_ratio_ / this->sample_ratio_ / 2.0;
    biquad_lowpass(&this->lowpass_coeff_, cutoff);
    this->post_filter_ = true;
  }

  if (this->pre_filter_ || this->post_filter_) {
    for (int i = 0; i < this->channels_; ++i) {
      biquad_init(&this->lowpass_[i][0], &this->lowpass_coeff_, 1.0);
      biquad_init(&this->lowpass_[i][1], &this->lowpass_coeff_, 1.0);
    }
  }

  if (this->sample_ratio_ < 1.0) {
    this->resampler_ = resampleInit(this->channels_, this->number_of_taps_, this->number_of_filters_,
                                    this->sample_ratio_ * this->lowpass_ratio_, flags | INCLUDE_LOWPASS);
  } else if (this->lowpass_ratio_ < 1.0) {
    this->resampler_ = resampleInit(this->channels_, this->number_of_taps_, this->number_of_filters_,
                                    this->lowpass_ratio_, flags | INCLUDE_LOWPASS);
  } else {
    this->resampler_ = resampleInit(this->channels_, this->number_of_taps_, this->number_of_filters_, 1.0, flags);
  }

  resampleAdvancePosition(this->resampler_, this->number_of_taps_ / 2.0);

  return true;
}

ResamplerResults Resampler::resample(const uint8_t *input_buffer, uint8_t *output_buffer, size_t input_frames_available,
                                     size_t output_frames_free,
                                     float gain) {  //, size_t &frames_used, size_t &frames_generated,
                                                    // uint32_t &clipped_samples) {
  unsigned int necessary_frames = resampleGetRequiredSamples(this->resampler_, output_frames_free, this->sample_ratio_);

  unsigned int frames_to_process = std::min(input_frames_available, necessary_frames);

  gain = pow(10.0, gain / 20.0);

  // if (this->input_bits_ <= 8) {
  //   float gain_factor = gain / 128.0;

  //   for (unsigned int i = 0; i < frames_to_process * this->channels_; ++i) {
  //     this->float_input_buffer_[i] = ((int) input_buffer[i] - 128) * gain_factor;
  //   }

  // } else if (this->input_bits_ <= 16) {
  //   float gain_factor = gain / 32768.0;
  //   unsigned int i, j;

  //   for (i = j = 0; i < frames_to_process * this->channels_; ++i) {
  //     int16_t value = input_buffer[j++];
  //     value += input_buffer[j++] << 8;
  //     this->float_input_buffer_[i] = value * gain_factor;
  //   }
  // } else if (this->input_bits_ <= 24) {
  //   float gain_factor = gain / 8388608.0;
  //   unsigned int i, j;

  //   for (i = j = 0; i < frames_to_process * this->channels_; ++i) {
  //     int32_t value = input_buffer[j++];
  //     value += input_buffer[j++] << 8;
  //     value += (int32_t) (signed char) input_buffer[j++] << 16;
  //     this->float_input_buffer_[i] = value * gain_factor;
  //   }
  // } else if (this->input_bits_ <= 32) {
  //   float gain_factor = gain / 2147483648.0;
  //   unsigned int i, j;

  //   for (i = j = 0; i < frames_to_process * this->channels_; ++i) {
  //     int32_t value = input_buffer[j++];
  //     value += input_buffer[j++] << 8;
  //     value += (int32_t) (signed char) input_buffer[j++] << 16;
  //     value += (int32_t) (signed char) input_buffer[j++] << 24;
  //     this->float_input_buffer_[i] = value * gain_factor;
  //   }
  // }

  for (unsigned int i = 0; i < frames_to_process * this->channels_; ++i) {
    //   float divisor = 1;
    //   if (this->input_bits_ <= 8) {
    //     divisor = 128.0;
    //   } else if (this->input_bits_ <= 16) {
    //     divisor = 32768.0;
    //   } else if (this->input_bits_ <= 24) {
    //     divisor = 8388608.0
    //   }
    this->float_input_buffer_[i] = static_cast<float>(reinterpret_cast<const int16_t *>(input_buffer)[i]) / 32768.0f;
  }

  if (this->pre_filter_) {
    for (int i = 0; i < this->channels_; ++i) {
      biquad_apply_buffer(&this->lowpass_[i][0], this->float_input_buffer_ + i, frames_to_process, this->channels_);
      biquad_apply_buffer(&this->lowpass_[i][1], this->float_input_buffer_ + i, frames_to_process, this->channels_);
    }
  }

  ResampleResult res = resampleProcessInterleaved(this->resampler_, this->float_input_buffer_, frames_to_process,
                                                  this->float_output_buffer_, output_frames_free, this->sample_ratio_);

  size_t frames_generated = res.output_generated;

  if (this->post_filter_) {
    for (int i = 0; i < this->channels_; ++i) {
      biquad_apply_buffer(&this->lowpass_[i][0], this->float_output_buffer_ + i, frames_generated, this->channels_);
      biquad_apply_buffer(&this->lowpass_[i][1], this->float_output_buffer_ + i, frames_generated, this->channels_);
    }
  }

  size_t frames_used = res.input_used;

  const size_t samples_generated = frames_generated * this->channels_;

  // const uint8_t out_bits = 16;

  float scaler = (1 << this->output_bits_) / 2.0;
  int32_t offset = (this->output_bits_ <= 8) * 128;
  int32_t high_clip = (1 << (this->output_bits_ - 1)) - 1;
  int32_t low_clip = ~high_clip;
  int left_shift = (32 - this->output_bits_) % 8;
  size_t i, j;
  uint32_t clipped_samples = 0;

  // uint8_t *temp_buffer = (uint8_t *) (output);

  for (i = j = 0; i < samples_generated; ++i) {
    uint8_t chan = i % this->channels_;
    int32_t output =
        floor((this->float_output_buffer_[i] *= scaler) - this->error_[chan] + this->tpdf_dither_(chan, -1) + 0.5);
    if (output > high_clip) {
      ++clipped_samples;
      output = high_clip;
    } else if (output < low_clip) {
      ++clipped_samples;
      output = low_clip;
    }

    this->error_[chan] += output - this->float_output_buffer_[i];
    output = (output << left_shift) + offset;
    output_buffer[j++] = output = (output << left_shift) + offset;
    if (this->output_bits_ > 8) {
      output_buffer[j++] = output >> 8;

      if (this->output_bits_ > 16) {
        output_buffer[j++] = output >> 16;
      }
      if (this->output_bits_ > 24) {
        output_buffer[j++] = output >> 24;
      }
    }
  }

  ResamplerResults results = {
      .frames_used = frames_used, .frames_generated = frames_generated, .clipped_samples = clipped_samples};
  return results;
}

void Resampler::tpdf_dither_init_(int num_channels) {
  int generator_bytes = num_channels * sizeof(uint32_t);
  uint8_t *seed = (uint8_t *) malloc(generator_bytes);

  uint32_t random_seed = 0x31415926; //esp_random();

  this->tpdf_generators_ = (uint32_t *) seed;

  while (generator_bytes--) {
    *seed++ = random_seed >> 24;
    random_seed = ((random_seed << 4) - random_seed) ^ 1;
    random_seed = ((random_seed << 4) - random_seed) ^ 1;
    random_seed = ((random_seed << 4) - random_seed) ^ 1;
  }
}

float Resampler::tpdf_dither_(int channel, int type) {
  uint32_t random = this->tpdf_generators_[channel];
  random = ((random << 4) - random) ^ 1;
  random = ((random << 4) - random) ^ 1;
  uint32_t first = type ? this->tpdf_generators_[channel] ^ ((int32_t) type >> 31) : ~random;
  random = ((random << 4) - random) ^ 1;
  random = ((random << 4) - random) ^ 1;
  random = ((random << 4) - random) ^ 1;
  this->tpdf_generators_[channel] = random;
  return (((first >> 1) + (random >> 1)) / 2147483648.0) - 1.0;
}

}  // namespace resampler