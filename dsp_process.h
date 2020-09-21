#include <string.h>
#include <math.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/i2s.h>
#include <driver/i2c.h>
#include <TelnetSpy.h>
#include "es8388_registers.h"


//------------------------------------------------------------------------------------
// Constant definitions
//------------------------------------------------------------------------------------

#define DSP_NUM_CHANNELS       2                 // Number of channels
#define DSP_MAX_FILTERS        10                // Max number of biquad filters
#define DSP_SAMPLE_RATE        44100             // The sample rate
#define DSP_MAX_GAIN           24                // Maximum gain for the channel
#define DSP_MAX_SAMPLES        512               // Maximum number of samples per channel each loop
#define DSP_MAX_DELAY_MILLIS   250               // Maximum delay allowed in milliseconds
#define DSP_MAX_DELAY_SAMPLES  ((DSP_MAX_DELAY_MILLIS*DSP_SAMPLE_RATE)/1000+1)

typedef  int16_t    sample_t;                    // Type defined for each sample input from the DAC
#define DSP_BITS_PER_SAMPLE                      (i2s_bits_per_sample_t) (sizeof( sample_t )*8)
#define DSP_MAX_SAMPLE_VALUE                     ((1 << (DSP_BITS_PER_SAMPLE-1)) - 1)


//------------------------------------------------------------------------------------
// Type definitions
//------------------------------------------------------------------------------------

typedef struct dsp_buffer_t {
  float        scaling_factor;                   // Factor used to scale values for specified gain
  float        biquad_w[DSP_MAX_FILTERS][2];     // Array of historic W values for each biquad filter
  int          delay_samples;                    // Number of calculated samples delayed in buffer
  int          delay_offset;                     // Offset within the delay buffer for storing next set of input values
  int         clipping_count;                    // Number of times audio clipped per channel
  sample_t    delay_buff[DSP_MAX_DELAY_SAMPLES];
                                                 // Sample delay buffer
} dsp_buffer_t;

typedef struct dsp_channel_t {
  char*        name;                             // Name of the channel
  float        gain_dB;                          // The amount of gain added to the channel
  int          delay_millis;                     // The delay (in millseconds) introduced into the channel
  int          num_filters;                      // The number of biquad filters used in the channel
  float        coeffs[DSP_MAX_FILTERS][5];       // The biquad coefficients for each of the filters
  dsp_buffer_t*  buffers;                        // Data buffer for the channel
} dsp_channel_t;


//------------------------------------------------------------------------------------
// Global variables
//------------------------------------------------------------------------------------

extern TelnetSpy      SerialAndTelnet;
#define SERIAL        SerialAndTelnet
//#define SERIAL        Serial


//------------------------------------------------------------------------------------
// Global functions (C++)
//------------------------------------------------------------------------------------

esp_err_t dsp_init();
esp_err_t dsp_loop();
esp_err_t dsp_command( char command );
esp_err_t dsp_filter_init( dsp_channel_t* channels );
esp_err_t dsp_filter_info( dsp_channel_t* channels );
esp_err_t dsp_filter( dsp_channel_t* channels, sample_t* dsp_buffer, int buffer_len, bool* clip_flag );
esp_err_t dsp_plot( dsp_channel_t* channels );

extern "C" {
  esp_err_t dsps_biquad_f32_ae32(const float* input, float* output, int len, float* coef, float* w);
}