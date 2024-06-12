
#include "bitstream_cabac.h"


void bitstream_create_init(bitstream_t* bs)
{
  bs->read_index = 0;
  bs->write_index = 0;
  bs->data_store = 0;
  bs->data_capacity = 0;
}

void bitstream_create_new(bitstream_t* bs, uint32 size)
{
  bs->data_store = 0;

  if (size != bitstream_resize_capacity(bs, size))
  {
    evx_post_error(EVX_ERROR_EXECUTION_FAILURE);
  }
}

int bitstream_create_refer(bitstream_t* bs, uint8* source, uint32 size, BOOL flag)
{
  bs->data_store = 0;

  bitstream_clear(bs);

  uint32 byte_size = size;
  bs->data_store = source;

  if (!bs->data_store)
  {
    evx_post_error(EVX_ERROR_EXECUTION_FAILURE);
    return 0;
  }
  bs->read_index = 0;
  if (flag) {
    bs->write_index = size << 3;
  }
  else {
    bs->write_index = 0;
  }
  bs->data_capacity = byte_size;
  return byte_size << 3;
}

void bitstream_create_assign(bitstream_t* bs, void *bytes, uint32 size)
{
  bs->data_store = 0;

    if (0 != bitstream_assign2(bs, bytes, size))
    {
        evx_post_error(EVX_ERROR_EXECUTION_FAILURE);
    }
}

//~bitstream() 
//{
//    clear();
//}

const uint8 * bitstream_query_data(const bitstream_t* bs)
{
    return bs->data_store;
}

const uint32 bitstream_query_capacity(const bitstream_t* bs)
{
    return bs->data_capacity << 3;
}

const uint32 bitstream_query_occupancy(const bitstream_t* bs)
{
    return bs->write_index - bs->read_index;
}

const uint32 bitstream_query_byte_occupancy(const bitstream_t* bs)
{
    return align(bitstream_query_occupancy(bs), 8) >> 3;
}

uint32 bitstream_resize_capacity(bitstream_t* bs, uint32 size_in_bits)
{
    if (EVX_PARAM_CHECK) 
    {
        if (0 == size_in_bits) 
        {
            evx_post_error(EVX_ERROR_INVALIDARG);
            return 0;
        }
    }

    bitstream_clear(bs);

    uint32 byte_size = align(size_in_bits, 8) >> 3;
    bs->data_store = malloc(byte_size);

    if (!bs->data_store)
    {
        evx_post_error(EVX_ERROR_EXECUTION_FAILURE);
        return 0;
    }

    bs->data_capacity = byte_size;
    return size_in_bits;
}

evx_status bitstream_seek(bitstream_t* bs, uint32 bit_offset)
{
    if (bit_offset >= bs->write_index) 
    {
        bit_offset = bs->write_index;
    }
    
    bs->read_index = bit_offset;
    return 0;
}

evx_status bitstream_assign2(bitstream_t* bs, void *bytes, uint32 size)
{
    if (EVX_PARAM_CHECK) 
    {
        if (0 == size || !bytes) 
        {
            return evx_post_error(EVX_ERROR_INVALIDARG);
        }
    }

    bitstream_clear(bs);

    /* Copy the data into our own buffer and adjust our indices. */
    bs->data_store = malloc(size);

    if (!bs->data_store)
    {
        return evx_post_error(EVX_ERROR_OUTOFMEMORY);
    }

    memcpy(bs->data_store, bytes, size);

    bs->read_index  = 0;
    bs->write_index = size << 3;
    bs->data_capacity = size;

    return EVX_SUCCESS;
}

void bitstream_clear(bitstream_t* bs)
{
  bitstream_empty(bs);

  free(bs->data_store);
  bs->data_store = 0;
  bs->data_capacity = 0;
}

void bitstream_empty(bitstream_t* bs)
{
    bs->write_index = 0;
    bs->read_index = 0;
}

const uint8 bitstream_is_empty(const bitstream_t* bs)
{
    return (bs->write_index == bs->read_index);
}

const uint8 bitstream_is_full(const bitstream_t* bs)
{
    return (bs->write_index == bitstream_query_capacity(bs));
}

evx_status bitstream_write_byte(bitstream_t* bs, uint8 value)
{
    if (bs->write_index + 8 > bitstream_query_capacity(bs))
    {
        return EVX_ERROR_CAPACITY_LIMIT;
    }

    /* Determine the current byte to write. */
    uint32 dest_byte = bs->write_index >> 3;
    uint8 dest_bit = bs->write_index % 8;

    if (0 == dest_bit) 
    {
        /* This is a byte aligned write, so we perform it at byte level. */
        uint8 *data = &(bs->data_store[dest_byte]);
        *data = value;
        bs->write_index += 8;
    } 
    else 
    {
        /* Slower byte unaligned write. */
        for (uint8 i = 0; i < 8; ++i) 
        {
          bitstream_write_bit(bs, (value >> i) & 0x1);
        }
    }

    return EVX_SUCCESS;
}

evx_status bitstream_write_bit(bitstream_t* bs, uint8 value)
{
    if (bs->write_index + 1 > bitstream_query_capacity(bs))
    {
        return EVX_ERROR_CAPACITY_LIMIT;
    }

    /* Determine the current byte to write. */
    uint32 dest_byte = bs->write_index >> 3;
    uint8 dest_bit = bs->write_index % 8;

    /* Pull the correct byte from our data store, update it, and then store it.
       Note that we do not guarantee that unused buffer memory was zero filled,
       thus we safely clear the write bit. */
    uint8 *data = &(bs->data_store[dest_byte]);
    *data = ((*data) & ~(0x1 << dest_bit)) | (value & 0x1) << dest_bit;
    bs->write_index++;

    return EVX_SUCCESS;
}

evx_status bitstream_write_bits(bitstream_t* bs, void *data, uint32 bit_count)
{
    if (EVX_PARAM_CHECK) 
    {
        if (!data || 0 == bit_count) 
        {
            return evx_post_error(EVX_ERROR_INVALIDARG);
        }
    }

    if (bs->write_index + bit_count > bitstream_query_capacity(bs))
    {
        return EVX_ERROR_CAPACITY_LIMIT;
    }

    uint32 bits_copied = 0;
    uint8 *source = (uint8 *)data;

    if (0 == (bs->write_index % 8) && (bit_count >= 8))
    {
        /* We can perform a (partial) fast copy because our source and destination 
           are byte aligned. We handle any trailing bits below. */
        bits_copied = aligned_bit_copy(bs->data_store, bs->write_index, source, 0, bit_count);

        if (!bits_copied) 
        {
            return evx_post_error(EVX_ERROR_EXECUTION_FAILURE);
        }
    }

    if (bits_copied < bit_count)
    {
      /* Perform unaligned copies of our data. */
      bits_copied += unaligned_bit_copy(bs->data_store,
        bs->write_index + bits_copied,
        source,
        bits_copied,
        bit_count - bits_copied);
    }

    bs->write_index += bits_copied;

    return EVX_SUCCESS;
}

evx_status bitstream_write_bytes(bitstream_t* bs, void *data, uint32 byte_count)
{
    return bitstream_write_bits(bs, data, byte_count << 3);
}

evx_status bitstream_read_bit(bitstream_t* bs, void *data)
{
    if (EVX_PARAM_CHECK) 
    {
        if (!data) 
        {
            return evx_post_error(EVX_ERROR_INVALIDARG);
        }
    }

    if (bs->read_index >= bs->write_index)
    {
        return evx_post_error(EVX_ERROR_INVALID_RESOURCE);
    }

    /* Determine the current byte to read from. */
    uint32 source_byte = bs->read_index >> 3;
    uint8 source_bit = bs->read_index % 8;
    uint8 *dest = (uint8 *)data;

    /* Pull the correct byte from our data store. Note that we 
       preserve the high bits of *dest. */
    *dest &= 0xFE;
    *dest |= ((bs->data_store[source_byte]) >> source_bit) & 0x1;
    bs->read_index++;

    return EVX_SUCCESS;
}

evx_status bitstream_read_byte(bitstream_t* bs, void *data)
{
    if (EVX_PARAM_CHECK) 
    {
        if (!data) 
        {
            return evx_post_error(EVX_ERROR_INVALIDARG);
        }
    }

    if (bs->read_index + 8 > bs->write_index)
    {
        return EVX_ERROR_INVALID_RESOURCE;
    }

    /* Determine the current byte to read from. */
    uint32 source_byte = bs->read_index >> 3;
    uint8 source_bit = bs->read_index % 8;
    uint8 *dest = (uint8 *)data;

    if (0 == (source_bit % 8)) 
    {
        *dest = bs->data_store[source_byte];
        bs->read_index += 8;
    } 
    else
    {
        /* Slower byte unaligned read. */
        for (uint8 i = 0; i < 8; ++i) 
        {
            uint8 temp = 0;
            bitstream_read_bit(bs, &temp);
            *dest = (*dest & ~(0x1 << i)) | (temp << i);
        }
    }

    return EVX_SUCCESS;
}

evx_status bitstream_read_bits(bitstream_t* bs, void *data, uint32 *bit_count)
{
    if (EVX_PARAM_CHECK) 
    {
        if (!data || !bit_count || 0 == *bit_count) 
        {
            return evx_post_error(EVX_ERROR_INVALIDARG);
        }
    }

    /* We attempt to read *puiBitCount bits and replace it with the number
       actually read. */
    if (bs->read_index + (*bit_count) > bs->write_index)
    {
        (*bit_count) = bs->write_index - bs->read_index;
    }

    uint32 bits_copied = 0;
    uint8 *dest = (uint8 *)data;

    if (0 == (bs->read_index % 8) && ((*bit_count) >= 8 ))
    {
        /* We can perform a (partial) fast copy because our source and destination 
           are byte aligned. We handle any trailing bits below. */
        bits_copied = aligned_bit_copy(dest, 0, bs->data_store, bs->read_index, (*bit_count));

        if (!bits_copied) 
        {
            return evx_post_error(EVX_ERROR_EXECUTION_FAILURE);
        }
    }

    /* Perform unaligned copies of our data. */
    if (bits_copied < (*bit_count))
    {
      bits_copied += unaligned_bit_copy(dest,
        bits_copied,
        bs->data_store,
        bs->read_index + bits_copied,
        (*bit_count) - bits_copied);
    }

    bs->read_index += bits_copied;

    return EVX_SUCCESS;
}

evx_status bitstream_read_bytes(bitstream_t* bs, void *data, uint32 *byte_count)
{
    if (EVX_PARAM_CHECK) 
    {
        if (!byte_count || 0 == *byte_count) 
        {
            return evx_post_error(EVX_ERROR_INVALIDARG);
        }
    }

    uint32 bit_count = (*byte_count) << 3;
    evx_status result = bitstream_read_bits(bs, data, &bit_count);
    (*byte_count) = bit_count >> 3;

    return result;
}
