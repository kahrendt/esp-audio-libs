#ifndef _dsp_platform_H_
#define _dsp_platform_H_

#ifdef ESP_PLATFORM
#include "sdkconfig.h"
#endif

#ifdef __XTENSA__
#include <xtensa/config/core-isa.h>
#include <xtensa/config/core-matmap.h>

#if ((XCHAL_HAVE_FP == 1) && (XCHAL_HAVE_LOOPS == 1))

#define dotprod_f32_ae32_enabled 1
#define dotprode_f32_ae32_enabled 1
#define dsps_add_s16_ae32_enabled 1

#endif  //

#if ((XCHAL_HAVE_LOOPS == 1) && (XCHAL_HAVE_MAC16 == 1))

#define dsps_mulc_s16_ae32_enabled 1

#endif  //

#endif  // __XTENSA__

#if CONFIG_IDF_TARGET_ESP32S3
#define dsps_dotprod_f32_aes3_enabled 1
#define dsps_add_s16_aes3_enabled 1
#endif

#endif  // _dsp_platform_H_