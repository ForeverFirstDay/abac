
/*
//
// Copyright (c) 2002-2015 Joe Bertolami. All Right Reserved.
//
// bitstream.h
//
//   Redistribution and use in source and binary forms, with or without
//   modification, are permitted provided that the following conditions are met:
//
//   * Redistributions of source code must retain the above copyright notice, this
//     list of conditions and the following disclaimer.
//
//   * Redistributions in binary form must reproduce the above copyright notice,
//     this list of conditions and the following disclaimer in the documentation
//     and/or other materials provided with the distribution.
//
//   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
//   AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
//   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
//   DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
//   FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
//   DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
//   SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
//   CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
//   OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
//   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// Additional Information:
//
//   For more information, visit http://www.bertolami.com.
//
*/

#ifndef __EVX_BIT_STREAM_H__	
#define __EVX_BIT_STREAM_H__	

#include "base.h"
#include "memory.h"
#include "evx_math.h"

#define EVX_READ_BIT(source, bit)           (((source) >> (bit)) & 0x1)
#define EVX_WRITE_BIT(dest, bit, value)     (dest) = (((dest) & ~(0x1 << (bit))) | \
                                            (((value) & 0x1) << (bit)))

typedef struct
{
  uint32 read_index;
  uint32 write_index;
  uint32 data_capacity;
  uint8* data_store;
} bitstream_t;

void bitstream_create1(bitstream_t* bs);
void bitstream_create2(bitstream_t* bs, uint32 size);
int bitstream_create3(bitstream_t* bs, uint8* source, uint32 size);
void bitstream_create4(bitstream_t* bs, void *bytes, uint32 size);
//virtual ~bitstream();

const uint8 * bitstream_query_data(const bitstream_t* bs);
const uint32 bitstream_query_capacity(const bitstream_t* bs);
const uint32 bitstream_query_occupancy(const bitstream_t* bs);
const uint32 bitstream_query_byte_occupancy(const bitstream_t* bs);
uint32 bitstream_resize_capacity(bitstream_t* bs, uint32 size_in_bits);

/* seek will only adjust the read index. there is purposely 
    no way to adjust the write index. */
evx_status bitstream_seek(bitstream_t* bs, uint32 bit_offset);
evx_status bitstream_assign1(bitstream_t* bs, const bitstream_t* rvalue);
evx_status bitstream_assign2(bitstream_t* bs, void *bytes, uint32 size);

void bitstream_clear(bitstream_t* bs);
void bitstream_empty(bitstream_t* bs);

const uint8 bitstream_is_empty(const bitstream_t* bs) ;
const uint8 bitstream_is_full(const bitstream_t* bs);

evx_status bitstream_write_byte(bitstream_t* bs, uint8 value);
evx_status bitstream_write_bit(bitstream_t* bs, uint8 value);
evx_status bitstream_write_bytes(bitstream_t* bs, void *data, uint32 byte_count);
evx_status bitstream_write_bits(bitstream_t* bs, void *data, uint32 bit_count);

evx_status bitstream_read_byte(bitstream_t* bs, void *data);
evx_status bitstream_read_bit(bitstream_t* bs, void *data);
evx_status bitstream_read_bytes(bitstream_t* bs, void *data, uint32 *byte_count);
evx_status bitstream_read_bits(bitstream_t* bs, void *data, uint32 *bit_count);

 
#endif // __EVX_BIT_STREAM_H__
