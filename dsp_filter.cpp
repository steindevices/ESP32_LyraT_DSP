#include "dsp_process.h"

static float Biquad_Buff_F32[ DSP_MAX_SAMPLES ];  // Single channel input buffer for biquad function


//------------------------------------------------------------------------------------
// Send DSP information for all channels to serial output
//------------------------------------------------------------------------------------

esp_err_t dsp_filter_info( dsp_channel_t* channels ) {

  dsp_channel_t*  channel;

  for( int channel_id=0; channel_id < DSP_NUM_CHANNELS ; ++channel_id ) {
    channel = &channels[channel_id];

    SERIAL.printf( "I-DSP: Channel: %s\r\n", channel->name );
    SERIAL.printf( "I-DSP:   Sampling freq = %d\r\n", DSP_SAMPLE_RATE );
    SERIAL.printf( "I-DSP:   Gain = %f dB\r\n", channel->gain_dB );
    SERIAL.printf( "I-DSP:   Scaling factor = %f\r\n", channel->buffers->scaling_factor );
    SERIAL.printf( "I-DSP:   Delay = %d millis\r\n", channel->delay_millis );
    SERIAL.printf( "I-DSP:   Delay samples = %d\r\n", channel->buffers->delay_samples );
    SERIAL.printf( "I-DSP:   Clipping count = %d\r\n", channel->buffers->clipping_count );
    SERIAL.printf( "I-DSP:   Biquad filters = %d\r\n", channel->num_filters );

    for( int i=0; i < channel->num_filters; ++i ) {
      SERIAL.printf( "I-DSP:   Filter %d coeffs = %8.6e %8.6e %8.6e %8.6e %8.6e\r\n",
        i, channel->coeffs[i][0], channel->coeffs[i][1], channel->coeffs[i][2], channel->coeffs[i][3], channel->coeffs[i][4] );
    }
  }

  return( ESP_OK );
}


//------------------------------------------------------------------------------------
// Initialize the DSP filters based on the channel configs
//------------------------------------------------------------------------------------

esp_err_t dsp_filter_init( dsp_channel_t* channels ) {

  int             delay_samples;
  dsp_channel_t*  channel;

  for( int channel_id=0; channel_id < DSP_NUM_CHANNELS ; ++channel_id ) {

    channel = &channels[channel_id];

    // Check if filter count is within limits
    if( channel->num_filters < 0 || channel->num_filters > DSP_MAX_FILTERS ) {
      SERIAL.printf( "E-DSP: ERROR: Invalid number of filters specified '%d'", channel->num_filters );
      return( ESP_FAIL );
    }

    // Check if specified gain is within limits
    if( channel->gain_dB < -DSP_MAX_GAIN || channel->gain_dB > DSP_MAX_GAIN ) {
      SERIAL.printf( "E-DSP: ERROR: Invalid gain setting for channel '%s'", channel->name );
      return( ESP_FAIL );
    }

    // Check if specified delay is within limits
    if( channel->delay_millis < 0 || channel->delay_millis > DSP_MAX_DELAY_MILLIS ) {
      SERIAL.printf( "E-DSP: Invalid delay setting for channel '%s'", channel->name );
      return( ESP_FAIL );
    }

    // Allocate the necessary data buffers for delay and biquad calculations
    channel->buffers = (dsp_buffer_t*) malloc( sizeof( dsp_buffer_t ) );

    if( channel->buffers == NULL ) {
      SERIAL.printf( "E-DSP: Unable to allocate data structure for channel '%s'", channel->name );
      return( ESP_FAIL );
    }

    // Set scaling factor
    channel->buffers->scaling_factor = exp10( channel->gain_dB/20.0 );

    // Initialize biquad delay values for each filter
    for( int filter_id = 0; filter_id < DSP_MAX_FILTERS; ++filter_id ) {
      channel->buffers->biquad_w[filter_id][0] = 0.0;
      channel->buffers->biquad_w[filter_id][1] = 0.0;
    }

    // Set clipping count
    channel->buffers->clipping_count = 0;

    // Calculate number of delay samples required
    delay_samples = DSP_SAMPLE_RATE*channel->delay_millis/1000;

    if( delay_samples > 0 ) {
      // Set up the delay buffer
      channel->buffers->delay_samples = delay_samples;
      channel->buffers->delay_offset = 0;
      memset( channel->buffers->delay_buff, 0, DSP_MAX_DELAY_SAMPLES*sizeof( sample_t ) );
    } else {
      // No delay buffer
      channel->buffers->delay_samples = 0;
      channel->buffers->delay_offset = 0;
    }
  }

  return( ESP_OK );
};


//------------------------------------------------------------------------------------
// Process the audio buffer by cascading the biquad filters and applying delay/gain
//------------------------------------------------------------------------------------

esp_err_t dsp_filter( dsp_channel_t* channels, sample_t* input_buffer, int buffer_len, bool* clip_flag ) {

  esp_err_t        res;
  dsp_channel_t*  channel;
  int              input_samples;
  float            sample_value;
  float            prev_value;
  float            scaling_factor;
  int              delay_samples;
  int              delay_offset;
  sample_t*        delay_buff;

  // Check if input sample count exceeded
  input_samples = buffer_len/sizeof( sample_t )/2;

  if( input_samples > DSP_MAX_SAMPLES ) {
    SERIAL.printf( "E-DSP: Too many input samples = '%d'", input_samples );
    return( ESP_FAIL );
  }

  // Reset the clipping flag
  *clip_flag = false;

  for( int channel_id=0; channel_id < DSP_NUM_CHANNELS ; ++channel_id ) {

    channel = &channels[channel_id];
    delay_samples = channel->buffers->delay_samples;
    delay_offset = channel->buffers->delay_offset;
    delay_buff = &channel->buffers->delay_buff[0];

    if( delay_samples > 0 ) {
      for( int i = 0; i < input_samples; ++i ) {
        // Output the delayed samples from the delay buffer
        Biquad_Buff_F32[i] = delay_buff[delay_offset];

        // Replace the delay buffer sample with the next sample from the input stream
        delay_buff[delay_offset] = input_buffer[i*DSP_NUM_CHANNELS  + channel_id];

        // Increment the delay buffer pointer and wrap it when at end of delay buffer
        ++ delay_offset;
        if( delay_offset == delay_samples ) {
          delay_offset = 0;
        }
      }

      // Update the buffer pointer
      channel->buffers->delay_offset = delay_offset;
    } else {
      for( int i = 0; i < input_samples; ++ i ) {
        // Output the delayed samples from the input buffer
        Biquad_Buff_F32[i] = input_buffer[i*DSP_NUM_CHANNELS  + channel_id];
      }
    }

    // Process each biquad filter in the channel
    if( channel->num_filters > 0 ) {
      int filter_id = 0;
      while( true ) {
        res = dsps_biquad_f32_ae32( Biquad_Buff_F32,  Biquad_Buff_F32, input_samples, channel->coeffs[filter_id], channel->buffers->biquad_w[filter_id] );

        if( res != ESP_OK ) {
          SERIAL.printf( "E-DSP: ERROR: Failure during biquad processing = '%d'", res );
          return( res );
        }

        ++ filter_id;
        if( filter_id == channel->num_filters ) {
          break;
        }
      }
    }

    scaling_factor = channel->buffers->scaling_factor;

    // Copy results of filter processing back to the input buffer
    prev_value = 0;
    for( int i=0; i < input_samples; ++i ) {
      sample_value = Biquad_Buff_F32[i]*scaling_factor;

      // Check if value out of range
      if( sample_value < -DSP_MAX_SAMPLE_VALUE || sample_value > DSP_MAX_SAMPLE_VALUE ) {

        SERIAL.printf( "I-DSP:  Clipping in channel '%s' with value '%f'\r\n", channel->name, sample_value );

        // Set clipping flag
        *clip_flag = true;
        ++channel->buffers->clipping_count;

        // Set sample to limit audible distortion
        sample_value = ( ( DSP_MAX_SAMPLE_VALUE*( sample_value < 0 ? -1 : 1 ) ) + prev_value)/2;
      }

      input_buffer[i*DSP_NUM_CHANNELS  + channel_id] = sample_value;
      prev_value = sample_value;
    }

  }
  return( ESP_OK );
}