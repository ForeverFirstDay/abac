
#include "cabac.h"
//#include "math.h"

#define EVX_ENTROPY_PRECISION					(16)
#define EVX_ENTROPY_PRECISION_MAX				(((uint32)0x1 << EVX_ENTROPY_PRECISION) - 1)
#define EVX_ENTROPY_PRECISION_MASK				(((uint32)0x1 << EVX_ENTROPY_PRECISION) - 1)
#define EVX_ENTROPY_HALF_RANGE					((EVX_ENTROPY_PRECISION_MAX >> 1))
#define EVX_ENTROPY_QTR_RANGE					(EVX_ENTROPY_HALF_RANGE >> 1)
#define EVX_ENTROPY_3QTR_RANGE					(3 * EVX_ENTROPY_QTR_RANGE)
#define EVX_ENTROPY_MSB_MASK					((uint64)0x1 << (EVX_ENTROPY_PRECISION - 1))
#define EVX_ENTROPY_SMSB_MASK					(EVX_ENTROPY_MSB_MASK >> 1)

#if (EVX_ENTROPY_PRECISION > 32)
  #error "EVX_ENTROPY_PRECISION must be <= 32"
#endif


/* 
// ABAC Ranging
//
// + Our range for 0 is [0, mid]			(inclusive)
// + Our range for 1 is [mid+1, high]		(inclusive)
//
// Thus, when encoding a zero, low should remain the same, high becomes mid.
// When encoding a one, low should be set to mid + 1, high remains the same. 
*/

void entropy_coder_init1(entropy_coder_t* coder)
{
  coder->history[0] = 1;
  coder->history[1] = 1;

  coder->e3_count = 0;
  coder->adaptive = 1;
  coder->model = EVX_ENTROPY_HALF_RANGE;
  coder->value = 0;

  coder->low = 0;
  coder->high = EVX_ENTROPY_PRECISION_MAX;
  coder->mid = EVX_ENTROPY_HALF_RANGE;
}

void entropy_coder_init2(entropy_coder_t* coder, uint32 input_model)
{
  coder->history[0] = 0;
  coder->history[1] = 0;

  coder->model = input_model;
  coder->e3_count = 0;
  coder->adaptive = 0;
  coder->value = 0;

  coder->low	= 0;
  coder->high = EVX_ENTROPY_PRECISION_MAX;
  coder->mid = coder->model;
}

void entropy_coder_clear(entropy_coder_t* coder)
{
  coder->low	= 0;
  coder->value = 0;
  coder->e3_count = 0;
  
    if (coder->adaptive)
    {
      coder->history[0] = 1;
      coder->history[1] = 1;
      coder->high = EVX_ENTROPY_PRECISION_MAX;
      coder->mid	= EVX_ENTROPY_HALF_RANGE;
    } 
    else 
    {
      coder->high = EVX_ENTROPY_PRECISION_MAX;
      coder->mid = coder->model;
    }
}

void entropy_coder_resolve_model(entropy_coder_t* coder)
{
    uint64 mid_range = 0; 
    uint64 range = coder->high - coder->low;
    
    if (coder->adaptive)
    {
        mid_range = range * coder->history[0] / (coder->history[0] + coder->history[1]);
    } 
    else 
    {
        mid_range = range * coder->model / EVX_ENTROPY_PRECISION_MAX;
    }

    coder->mid = coder->low + mid_range;
}

evx_status entropy_coder_encode_symbol(entropy_coder_t* coder, uint8 value)
{
    /* We only encode the first 2 GB instances of each symbol. */
    if (coder->history[value] >= (2 * EVX_GB))
    {
        return evx_post_error(EVX_ERROR_INVALID_RESOURCE);
    }

    /* Adapt our model with knowledge of our recently processed value. */
    entropy_coder_resolve_model(coder);

    /* Encode our bit. */
    value = value & 0x1;

    if (value) 
    {
      coder->low = coder->mid + 1;
    } 
    else 
    {
      coder->high = coder->mid;
    }

    coder->history[value]++;

    return EVX_SUCCESS;
}

evx_status entropy_coder_decode_symbol(entropy_coder_t* coder, uint32 value, bitstream_t *dest)
{
    if (EVX_PARAM_CHECK) 
    {
        if (!dest) 
        {
            return evx_post_error(EVX_ERROR_INVALIDARG);
        }
    }

    /* Adapt our model with knowledge of our recently processed value. */
    entropy_coder_resolve_model(coder);

    /* Decode our bit. */
    if (value >= coder->low && value <= coder->mid)
    {
      coder->high = coder->mid;
      coder->history[0]++;
      bitstream_write_bit(dest, 0);
    } 
    else if (value > coder->mid && value <= coder->high)
    {
      coder->low = coder->mid + 1;
      coder->history[1]++;
      bitstream_write_bit(dest, 1);
    }

    return EVX_SUCCESS;
}

evx_status entropy_coder_flush_inverse_bits(entropy_coder_t* coder, uint8 value, bitstream_t *dest)
{
    if (EVX_PARAM_CHECK) 
    {
        if (!dest) 
        {
            return evx_post_error(EVX_ERROR_INVALIDARG);
        }
    }

    value = !value;

    for (uint32 i = 0; i < coder->e3_count; ++i)
    {
        if (EVX_SUCCESS != bitstream_write_bit(dest, value)) 
        {
            return evx_post_error(EVX_ERROR_EXECUTION_FAILURE);
        }
    }

    coder->e3_count = 0;

    return EVX_SUCCESS;
}

evx_status entropy_coder_resolve_encode_scaling(entropy_coder_t* coder, bitstream_t *dest)
{
    if (EVX_PARAM_CHECK) 
    {
        if (!dest) 
        {
            return evx_post_error(EVX_ERROR_INVALIDARG);
        }
    }

    while (1) 
    {
        if ((coder->high & EVX_ENTROPY_MSB_MASK) == (coder->low & EVX_ENTROPY_MSB_MASK))
        {
            /* E1/E2 scaling violation. */
            uint8 msb = (coder->high & EVX_ENTROPY_MSB_MASK) >> (EVX_ENTROPY_PRECISION - 1);
            coder->low -= EVX_ENTROPY_HALF_RANGE * msb + msb;
            coder->high -= EVX_ENTROPY_HALF_RANGE * msb + msb;

            if (EVX_SUCCESS != bitstream_write_bit(dest, msb))
            {
                return evx_post_error(EVX_ERROR_INVALID_RESOURCE);
            }

            if (EVX_SUCCESS != entropy_coder_flush_inverse_bits(coder, msb, dest))
            {
                return evx_post_error(EVX_ERROR_INVALID_RESOURCE);
            }
        } 
        else if (coder->high <= EVX_ENTROPY_3QTR_RANGE && coder->low > EVX_ENTROPY_QTR_RANGE)
        {
            /* E3 scaling violation. */
          coder->high -= EVX_ENTROPY_QTR_RANGE + 1;
          coder->low	-= EVX_ENTROPY_QTR_RANGE + 1;
          coder->e3_count += 1;
        } 
        else 
        {
            break;
        }

        coder->high = ((coder->high << 0x1) & EVX_ENTROPY_PRECISION_MAX) | 0x1;
        coder->low = ((coder->low  << 0x1) & EVX_ENTROPY_PRECISION_MAX) | 0x0;
    }

    return EVX_SUCCESS;
}

evx_status entropy_coder_resolve_decode_scaling(entropy_coder_t* coder, uint32 *value, bitstream_t *source, bitstream_t *dest)
{
    if (EVX_PARAM_CHECK) 
    {
        if (!value || !source || !dest) 
        {
            return evx_post_error(EVX_ERROR_INVALIDARG);
        }
    }

    uint8 bit = 0;

    while (1) 
    {
        if (coder->high <= EVX_ENTROPY_HALF_RANGE)
        {
            /* If our high value is less than half we do nothing (but
               prevent the loop from exiting). */
        } 
        else if (coder->low > EVX_ENTROPY_HALF_RANGE)
        {
          coder->high -= (EVX_ENTROPY_HALF_RANGE + 1);
          coder->low	-= (EVX_ENTROPY_HALF_RANGE + 1);
            *value -= (EVX_ENTROPY_HALF_RANGE + 1);
        }	
        else if (coder->high <= EVX_ENTROPY_3QTR_RANGE && coder->low > EVX_ENTROPY_QTR_RANGE)
        {
            /* E3 scaling violation. */
          coder->high -= EVX_ENTROPY_QTR_RANGE + 1;
          coder->low	-= EVX_ENTROPY_QTR_RANGE + 1;
            *value -= EVX_ENTROPY_QTR_RANGE + 1;
        } 
        else
        {
            break;
        }   

        if (!bitstream_is_empty(source)) 
        {
            if (EVX_SUCCESS != bitstream_read_bit(source, &bit)) 
            {
                return evx_post_error(EVX_ERROR_EXECUTION_FAILURE);
            }
        }

        coder->high = ((coder->high << 0x1) & EVX_ENTROPY_PRECISION_MAX) | 0x1;
        coder->low = ((coder->low  << 0x1) & EVX_ENTROPY_PRECISION_MAX) | 0x0;
        *value = ((*value << 0x1) & EVX_ENTROPY_PRECISION_MAX) | bit;
    }

    return EVX_SUCCESS;
}

evx_status entropy_coder_flush_encoder(entropy_coder_t* coder, bitstream_t *dest)
{
    if (EVX_PARAM_CHECK) 
    {
        if (!dest) 
        {
            return evx_post_error(EVX_ERROR_INVALIDARG);
        }
    }

    coder->e3_count++;

    if (coder->low < EVX_ENTROPY_QTR_RANGE)
    {
        if (EVX_SUCCESS != bitstream_write_bit(dest, 0) || 
            EVX_SUCCESS != entropy_coder_flush_inverse_bits(coder, 0, dest)) 
        {
            return evx_post_error(EVX_ERROR_EXECUTION_FAILURE);
        }
    } 
    else 
    {
        if (EVX_SUCCESS != bitstream_write_bit(dest, 1) ||
            EVX_SUCCESS != entropy_coder_flush_inverse_bits(coder, 1, dest)) 
        {
            return evx_post_error(EVX_ERROR_EXECUTION_FAILURE);
        }
    }

    entropy_coder_clear(coder);

    return EVX_SUCCESS;
}

evx_status entropy_coder_encode(entropy_coder_t* coder, bitstream_t *source, bitstream_t *dest)
{
    if (EVX_PARAM_CHECK) 
    {
        if (!source || !dest) 
        {
            return evx_post_error(EVX_ERROR_INVALIDARG);
        }
    }

    uint8 value = 0;

    while (!bitstream_is_empty(source)) 
    {
        if (EVX_SUCCESS != bitstream_read_bit(source, &value) ||
            EVX_SUCCESS != entropy_coder_encode_symbol(coder, value) ||
            EVX_SUCCESS != entropy_coder_resolve_encode_scaling(coder, dest)) 
        {
            return evx_post_error(EVX_ERROR_INVALID_RESOURCE);
        }
    }

    //if (auto_finish) 
    //{
        /* We close out the tab here in order to remain consistent with the decode
           behavior. If stream support is required, this will require an update. */
        if (EVX_SUCCESS != entropy_coder_flush_encoder(coder, dest)) 
        {
            return evx_post_error(EVX_ERROR_EXECUTION_FAILURE);
        }

        entropy_coder_clear(coder);
    //}

    return EVX_SUCCESS;
}

evx_status entropy_coder_decode(entropy_coder_t* coder, uint32 symbol_count, bitstream_t *source, bitstream_t *dest)
{
    if (EVX_PARAM_CHECK) 
    {
        if (0 == symbol_count || !source || !dest ) 
        {
            return evx_post_error(EVX_ERROR_INVALIDARG);
        }
    }

    //if (auto_start) 
    //{
        uint8 bit = 0;
        coder->value = 0;

        entropy_coder_clear(coder);

        /* We read in our initial bits with padded tailing zeroes. */
        for (uint32 i = 0; i < EVX_ENTROPY_PRECISION; ++i) 
        {
            if (!bitstream_is_empty(source))
            {
                if (EVX_SUCCESS != bitstream_read_bit(source, &bit))
                {
                    return evx_post_error(EVX_ERROR_INVALID_RESOURCE);
                }
            }

            coder->value <<= 0x1;
            coder->value |= bit;
        }
    //}

    /* Begin decoding the sequence. */
    for (uint32 i = 0; i < symbol_count; ++i) 
    {
        if (EVX_SUCCESS != entropy_coder_decode_symbol(coder, coder->value, dest) ||
            EVX_SUCCESS != entropy_coder_resolve_decode_scaling(coder, &(coder->value), source, dest))
        {
            return evx_post_error(EVX_ERROR_EXECUTION_FAILURE);
        }
    }

    return EVX_SUCCESS;
}

evx_status entropy_coder_start_decode(entropy_coder_t* coder, bitstream_t *source)
{
    uint8 bit = 0;
    coder->value = 0;

    entropy_coder_clear(coder);

    /* We read in our initial bits with padded tailing zeroes. */
    for (uint32 i = 0; i < EVX_ENTROPY_PRECISION; ++i) 
    {
        if (!bitstream_is_empty(source))
        {
            if (EVX_SUCCESS != bitstream_read_bit(source, &bit))
            {
                return evx_post_error(EVX_ERROR_INVALID_RESOURCE);
            }
        }

        coder->value <<= 0x1;
        coder->value |= bit;
    }

    return EVX_SUCCESS;
}

evx_status entropy_coder_finish_encode(entropy_coder_t* coder, bitstream_t *dest)
{
    if (EVX_SUCCESS != entropy_coder_flush_encoder(coder, dest))
    {
        return evx_post_error(EVX_ERROR_EXECUTION_FAILURE);
    }

    entropy_coder_clear(coder);

    return EVX_SUCCESS;
}
