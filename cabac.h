
/*
//
// Copyright (c) 2002-2015 Joe Bertolami. All Right Reserved.
//
// cabac.h
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

#ifndef __EV_CABAC_H__
#define __EV_CABAC_H__

#include "bitstream_cabac.h"
 
/*
// Entropy Stream Interface
//
// There are two ways to use this interface:
//
//  o: Stream coding
//
//     To code an entire stream at once, call Encode()/Decode(). This will instruct
//     the coder to initialize itself, perform the coding operation, perform a flush
//     if necessary, and clear its internal state.
//
//  o: Incremental coding
//
//     To incrementally code one or more symbols at a time, you must pass FALSE as the 
//     optional parameter to Encode and Decode. Additionally, you must call FinishEncode()
//     after all encode operations are complete, and StartDecode prior to calling the first
//     Decode(). This process allows the coder to properly initialize, flush, and reset itself.
*/

#define EVX_KB                  ((uint32) 1024)
#define EVX_MB                  (EVX_KB * EVX_KB)
#define EVX_GB                  (EVX_MB * EVX_KB)


typedef struct
{
  uint8 adaptive;
  uint32 e3_count;
  uint32 history[2];
  uint32 value;

  uint32 model;
  uint32 low;
  uint32 high;
  uint32 mid;
} entropy_coder_t;


void entropy_coder_resolve_model(entropy_coder_t* coder);

evx_status entropy_coder_flush_encoder(entropy_coder_t* coder, bitstream_t *dest);
evx_status entropy_coder_flush_inverse_bits(entropy_coder_t* coder, uint8 value, bitstream_t *dest);

evx_status entropy_coder_encode_symbol(entropy_coder_t* coder, uint8 value);
evx_status entropy_coder_decode_symbol(entropy_coder_t* coder, uint32 value, bitstream_t *dest);

evx_status entropy_coder_resolve_encode_scaling(entropy_coder_t* coder, bitstream_t *dest);
evx_status entropy_coder_resolve_decode_scaling(entropy_coder_t* coder, uint32 *value, bitstream_t *source, bitstream_t *dest);


void entropy_coder_init1(entropy_coder_t* coder);
void entropy_coder_init2(entropy_coder_t* coder, uint32 input_model);
void entropy_coder_clear(entropy_coder_t* coder);

evx_status entropy_coder_encode(entropy_coder_t* coder, bitstream_t *source, bitstream_t* dest);
evx_status entropy_coder_decode(entropy_coder_t* coder, uint32 symbol_count, bitstream_t *source, bitstream_t *dest);

evx_status entropy_coder_start_decode(entropy_coder_t* coder, bitstream_t *source);
evx_status entropy_coder_finish_encode(entropy_coder_t* coder, bitstream_t *dest);



#endif // __EVX_CABAC_H__