/* Proof of concept of closed- and open-addressing
 * unordered associative containers.
 *
 * Copyright 2022 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 */
 
#ifndef FOA_UNORDERED_RC_HPP
#define FOA_UNORDERED_RC_HPP
 
#include <boost/config.hpp>
#include <boost/container_hash/hash.hpp>
#include <boost/core/bit.hpp>
#include <boost/iterator/iterator_facade.hpp>
#include <boost/predef.h>
#include <cassert>
#include <climits>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <tuple>
#include <type_traits>
#include <vector>
#include "fxa_common.hpp"

#if defined(__ARM_NEON)
#include <arm_neon.h>
#endif

namespace fxa_unordered{

namespace rc{

#ifdef FXA_UNORDERED_SSE2

struct group16b
{
  static constexpr int N=16;

  inline void set(std::size_t pos,std::size_t hash)
  {
    if(pos==0)hash&=0xFEu;
    reinterpret_cast<unsigned char*>(&mask)[pos]|=hash|0x80u;
  }

  inline void set_sentinel()
  {
    reinterpret_cast<unsigned char*>(&mask)[N-1]=0x01u;
  }

  inline bool is_sentinel(std::size_t pos)const
  {
    return pos==N-1&&
      reinterpret_cast<const unsigned char*>(&mask)[N-1]==0x01u;
  }

  inline void reset(std::size_t pos)
  {
    assert(pos<N);
    reinterpret_cast<unsigned char*>(&mask)[pos]&=pos==0?0x01u:0;
  }

  inline int match(unsigned char hash)const
  {
    const auto one=_mm_setr_epi8(1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0);
    auto m=_mm_set1_epi8(hash|0x80u);
    return _mm_movemask_epi8(_mm_cmpeq_epi8(_mm_or_si128(mask,one),_mm_or_si128(m,one)));
  }

  inline auto is_not_overflowed(std::size_t /* hash */)const
  {
    return !(reinterpret_cast<const unsigned char*>(&mask)[0]&0x01u);
  }

  inline void mark_overflow(std::size_t /* hash */)
  {
    reinterpret_cast<unsigned char*>(&mask)[0]|=0x01u;
  }

  inline int match_available()const
  {
    return
      _mm_movemask_epi8(_mm_cmpeq_epi8(mask,_mm_setzero_si128()))|
      (reinterpret_cast<const unsigned char*>(&mask)[0]==0x01u);    
  }

  inline int match_occupied()const
  {
    return (~match_available())&0xFFFF;
  }

  inline int match_really_occupied()const // excluding sentinel
  {
    return _mm_movemask_epi8(mask);
  }

private:
  __m128i mask=_mm_setzero_si128();
};

#endif /* FXA_UNORDERED_SSE2 */

#ifdef FXA_UNORDERED_SSE2

struct group16
{
  static constexpr int N=16;

  inline void set(std::size_t pos,std::size_t hash)
  {
    reinterpret_cast<unsigned char*>(&mask)[pos]=hash&0x7Fu;
  }

  inline void set_sentinel()
  {
    reinterpret_cast<unsigned char*>(&mask)[N-1]=sentinel_;
  }

  inline bool is_sentinel(std::size_t pos)const
  {
    return pos==N-1&&
      reinterpret_cast<const unsigned char*>(&mask)[N-1]==(unsigned char)sentinel_;
  }

  inline void reset(std::size_t pos)
  {
    assert(pos<N);
    reinterpret_cast<unsigned char*>(&mask)[pos]=deleted_;
  }

  static void reset(unsigned char* pc)
  {
    *pc=deleted_;
  }

  inline int match(std::size_t hash)const
  {
    auto m=_mm_set1_epi8(hash&0x7Fu);
    return _mm_movemask_epi8(_mm_cmpeq_epi8(mask,m));
  }

  inline int is_not_overflowed(std::size_t /* hash */)const
  {
    auto m=_mm_set1_epi8(empty_);
    return _mm_movemask_epi8(_mm_cmpeq_epi8(mask,m));
  }

  inline void mark_overflow(std::size_t /* hash */){}

  inline int match_available()const
  {
    auto m=_mm_set1_epi8(sentinel_);
    return _mm_movemask_epi8(_mm_cmpgt_epi8_fixed(m,mask));    
  }

  inline int match_occupied()const
  {
    return (~match_available())&0xFFFFul;
  }

  inline int match_really_occupied()const // excluding sentinel
  {
    return (~_mm_movemask_epi8(mask))&0xFFFFul;
  }

protected:
  // exact values as per Abseil rationale
  static constexpr int8_t empty_=-128,
                          deleted_=-2,
                          sentinel_=-1;

  //ripped from Abseil raw_hash_set.h
  static inline __m128i _mm_cmpgt_epi8_fixed(__m128i a, __m128i b) {
#if defined(__GNUC__) && !defined(__clang__)
    if (std::is_unsigned<char>::value) {
      const __m128i mask = _mm_set1_epi8(0x80);
      const __m128i diff = _mm_subs_epi8(b, a);
      return _mm_cmpeq_epi8(_mm_and_si128(diff, mask), mask);
    }
#endif
    return _mm_cmpgt_epi8(a, b);
  }

  __m128i   mask=_mm_set1_epi8(empty_);
};

#elif defined(__ARM_NEON)
// TODO: check for little endianness

struct group16
{
  static constexpr int N=16;

  inline void set(std::size_t pos,std::size_t hash)
  {
    reinterpret_cast<unsigned char*>(&mask)[pos]=hash&0x7Fu;
  }

  inline void set_sentinel()
  {
    reinterpret_cast<unsigned char*>(&mask)[N-1]=sentinel_;
  }

  inline bool is_sentinel(std::size_t pos)const
  {
    return pos==N-1&&
      reinterpret_cast<const unsigned char*>(&mask)[N-1]==(unsigned char)sentinel_;
  }

  inline void reset(std::size_t pos)
  {
    assert(pos<N);
    reinterpret_cast<unsigned char*>(&mask)[pos]=deleted_;
  }

  static void reset(unsigned char* pc)
  {
    *pc=deleted_;
  }

  inline int match(std::size_t hash)const
  {
    auto m=vdupq_n_s8(hash&0x7Fu);
    return simde_mm_movemask_epi8(vceqq_s8(mask,m));
  }

  inline int is_not_overflowed(std::size_t /* hash */)const
  {
    auto m=vdupq_n_s8(empty_);
    return simde_mm_movemask_epi8(vceqq_s8(mask,m));
  }

  inline void mark_overflow(std::size_t /* hash */){}

  inline int match_available()const
  {
    auto m=vdupq_n_s8(sentinel_);
    return simde_mm_movemask_epi8(vcgtq_s8(m,mask));    
  }

  inline int match_occupied()const
  {
    /* ~match_available() */
    auto m=vdupq_n_s8(sentinel_);
    return simde_mm_movemask_epi8(vcleq_s8(m,mask));    
  }

  inline int match_really_occupied()const // excluding sentinel
  {
    return (~_mm_movemask_aarch64(reinterpret_cast<uint8x16_t>(mask)))&0xFFFFul;
  }

private:
  // exact values as per Abseil rationale
  static constexpr int8_t empty_=-128,
                          deleted_=-2,
                          sentinel_=-1;

  // https://github.com/simd-everywhere/simde/blob/e1bc968696e6533d6b0bf8dddb0614737c983479/simde/x86/sse2.h#L3755
  // (adapted)
  static inline int simde_mm_movemask_epi8(uint8x16_t a)
  {
    /* https://github.com/WebAssembly/simd/pull/201#issue-380682845 */
    static const uint8_t md[16] = {
    1 << 0, 1 << 1, 1 << 2, 1 << 3,
    1 << 4, 1 << 5, 1 << 6, 1 << 7,
    1 << 0, 1 << 1, 1 << 2, 1 << 3,
    1 << 4, 1 << 5, 1 << 6, 1 << 7,
    };

    /* Extend sign bit over entire lane */
    // we omit this op as lanes are 00/FF 
    // uint8x16_t extended = vreinterpretq_u8_s8(vshrq_n_s8(a, 7));
    /* Clear all but the bit we're interested in. */
    uint8x16_t masked = vandq_u8(vld1q_u8(md), a);
    /* Alternate bytes from low half and high half */
    uint8x8x2_t tmp = vzip_u8(vget_low_u8(masked), vget_high_u8(masked));
    uint16x8_t x = vreinterpretq_u16_u8(vcombine_u8(tmp.val[0], tmp.val[1]));
    return vaddvq_u16(x);
  }

  // https://stackoverflow.com/a/68694558/213114
  static inline int _mm_movemask_aarch64(uint8x16_t input)
  {   
      const uint8_t __attribute__ ((aligned (16))) ucShift[] = {(uint8_t)-7,(uint8_t)-6,(uint8_t)-5,(uint8_t)-4,(uint8_t)-3,(uint8_t)-2,(uint8_t)-1,0,(uint8_t)-7,(uint8_t)-6,(uint8_t)-5,(uint8_t)-4,(uint8_t)-3,(uint8_t)-2,(uint8_t)-1,0};
      uint8x16_t vshift = vld1q_u8(ucShift);
      uint8x16_t vmask = vandq_u8(input, vdupq_n_u8(0x80));
      int out;
    
      vmask = vshlq_u8(vmask, vshift);
      out = vaddv_u8(vget_low_u8(vmask));
      out += (vaddv_u8(vget_high_u8(vmask)) << 8);
    
      return out;
  }

  int8x16_t mask=vdupq_n_s8(empty_);
};

#else

struct group16
{
  static constexpr int N=16;

  inline void set(std::size_t pos,std::size_t hash)
  {
    set_impl(pos,hash&0x7Fu);
  }

  inline void set_sentinel()
  {
    uint64_ops::set(himask,N-1,sentinel_);
  }

  inline bool is_sentinel(std::size_t pos)const
  {
    /* precondition: pos is occupied */
    return pos==N-1&&(himask & uint64_t(0x8000000000000000ull));
  }

  inline void reset(std::size_t pos)
  {
    uint64_ops::set(himask,pos,deleted_);
  }

  static void reset(unsigned char* pc)
  {
    std::size_t pos=reinterpret_cast<uintptr_t>(pc)%sizeof(group16);
    pc-=pos;
    reinterpret_cast<group16*>(pc)->reset(pos);
  }

  inline int match(std::size_t hash)const
  {
    return match_impl(hash&0x7Fu);
  }

  inline int is_not_overflowed(std::size_t /* hash */)const
  {
    return match_empty();
  }

  inline void mark_overflow(std::size_t /* hash */){}

  inline int match_empty()const
  {
    auto m=himask>>32;
    return m&(m>>16);
  }

  inline int match_available()const
  {
    auto m=himask>>16;
    return m&(m>>32);
  }

  inline int match_occupied()const
  {
    // ~match_available()
    auto m=(~himask)>>16;
    return (m|(m>>32))&0xFFFF;
  }

  inline int match_really_occupied()const // excluding sentinel
  {
    return
      (~himask)>>48;
  }

private:
  static constexpr int empty_=   0xE, // 1110
                       deleted_= 0xA, // 1010
                       sentinel_=0x8; // 1000

  inline void set_impl(std::size_t pos,std::size_t m)
  {
    uint64_ops::set(lomask,pos,m&0xFu);
    uint64_ops::set(himask,pos,m>>4);
  }
  
  inline int match_impl(std::size_t m)const
  {
    return uint64_ops::match(lomask,himask,m);
  }

  uint64_t lomask=0,himask=0xFFFFFFFFFFFF0000ull;
};

#endif /* FXA_UNORDERED_SSE2 */

constexpr unsigned char adjust_hash_table[]={
  2,3,2,3,4,5,6,7,8,9,10,11,12,13,14,15,
  16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,
  32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,
  48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63,
  64,65,66,67,68,69,70,71,72,73,74,75,76,77,78,79,
  80,81,82,83,84,85,86,87,88,89,90,91,92,93,94,95,
  96,97,98,99,100,101,102,103,104,105,106,107,108,109,110,111,
  112,113,114,115,116,117,118,119,120,121,122,123,124,125,126,127,
  128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,
  144,145,146,147,148,149,150,151,152,153,154,155,156,157,158,159,
  160,161,162,163,164,165,166,167,168,169,170,171,172,173,174,175,
  176,177,178,179,180,181,182,183,184,185,186,187,188,189,190,191,
  192,193,194,195,196,197,198,199,200,201,202,203,204,205,206,207,
  208,209,210,211,212,213,214,215,216,217,218,219,220,221,222,223,
  224,225,226,227,228,229,230,231,232,233,234,235,236,237,238,239,
  240,241,242,243,244,245,246,247,248,249,250,251,252,253,254,255,
};

#ifdef FXA_UNORDERED_SSE2

constexpr __m128i match_table[]=
{
#if defined(BOOST_MSVC)
  {2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2},{3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3},{2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2},{3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3},
  {4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4},{5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5},{6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6},{7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7},
  {8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8},{9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9},{10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10},{11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11},
  {12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12},{13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13},{14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14},{15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15},
  {16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16},{17,17,17,17,17,17,17,17,17,17,17,17,17,17,17,17},{18,18,18,18,18,18,18,18,18,18,18,18,18,18,18,18},{19,19,19,19,19,19,19,19,19,19,19,19,19,19,19,19},
  {20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20},{21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21},{22,22,22,22,22,22,22,22,22,22,22,22,22,22,22,22},{23,23,23,23,23,23,23,23,23,23,23,23,23,23,23,23},
  {24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24},{25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25},{26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26},{27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27},
  {28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28},{29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29},{30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30},{31,31,31,31,31,31,31,31,31,31,31,31,31,31,31,31},
  {32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32},{33,33,33,33,33,33,33,33,33,33,33,33,33,33,33,33},{34,34,34,34,34,34,34,34,34,34,34,34,34,34,34,34},{35,35,35,35,35,35,35,35,35,35,35,35,35,35,35,35},
  {36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36},{37,37,37,37,37,37,37,37,37,37,37,37,37,37,37,37},{38,38,38,38,38,38,38,38,38,38,38,38,38,38,38,38},{39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39},
  {40,40,40,40,40,40,40,40,40,40,40,40,40,40,40,40},{41,41,41,41,41,41,41,41,41,41,41,41,41,41,41,41},{42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42},{43,43,43,43,43,43,43,43,43,43,43,43,43,43,43,43},
  {44,44,44,44,44,44,44,44,44,44,44,44,44,44,44,44},{45,45,45,45,45,45,45,45,45,45,45,45,45,45,45,45},{46,46,46,46,46,46,46,46,46,46,46,46,46,46,46,46},{47,47,47,47,47,47,47,47,47,47,47,47,47,47,47,47},
  {48,48,48,48,48,48,48,48,48,48,48,48,48,48,48,48},{49,49,49,49,49,49,49,49,49,49,49,49,49,49,49,49},{50,50,50,50,50,50,50,50,50,50,50,50,50,50,50,50},{51,51,51,51,51,51,51,51,51,51,51,51,51,51,51,51},
  {52,52,52,52,52,52,52,52,52,52,52,52,52,52,52,52},{53,53,53,53,53,53,53,53,53,53,53,53,53,53,53,53},{54,54,54,54,54,54,54,54,54,54,54,54,54,54,54,54},{55,55,55,55,55,55,55,55,55,55,55,55,55,55,55,55},
  {56,56,56,56,56,56,56,56,56,56,56,56,56,56,56,56},{57,57,57,57,57,57,57,57,57,57,57,57,57,57,57,57},{58,58,58,58,58,58,58,58,58,58,58,58,58,58,58,58},{59,59,59,59,59,59,59,59,59,59,59,59,59,59,59,59},
  {60,60,60,60,60,60,60,60,60,60,60,60,60,60,60,60},{61,61,61,61,61,61,61,61,61,61,61,61,61,61,61,61},{62,62,62,62,62,62,62,62,62,62,62,62,62,62,62,62},{63,63,63,63,63,63,63,63,63,63,63,63,63,63,63,63},
  {64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64},{65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65},{66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66},{67,67,67,67,67,67,67,67,67,67,67,67,67,67,67,67},
  {68,68,68,68,68,68,68,68,68,68,68,68,68,68,68,68},{69,69,69,69,69,69,69,69,69,69,69,69,69,69,69,69},{70,70,70,70,70,70,70,70,70,70,70,70,70,70,70,70},{71,71,71,71,71,71,71,71,71,71,71,71,71,71,71,71},
  {72,72,72,72,72,72,72,72,72,72,72,72,72,72,72,72},{73,73,73,73,73,73,73,73,73,73,73,73,73,73,73,73},{74,74,74,74,74,74,74,74,74,74,74,74,74,74,74,74},{75,75,75,75,75,75,75,75,75,75,75,75,75,75,75,75},
  {76,76,76,76,76,76,76,76,76,76,76,76,76,76,76,76},{77,77,77,77,77,77,77,77,77,77,77,77,77,77,77,77},{78,78,78,78,78,78,78,78,78,78,78,78,78,78,78,78},{79,79,79,79,79,79,79,79,79,79,79,79,79,79,79,79},
  {80,80,80,80,80,80,80,80,80,80,80,80,80,80,80,80},{81,81,81,81,81,81,81,81,81,81,81,81,81,81,81,81},{82,82,82,82,82,82,82,82,82,82,82,82,82,82,82,82},{83,83,83,83,83,83,83,83,83,83,83,83,83,83,83,83},
  {84,84,84,84,84,84,84,84,84,84,84,84,84,84,84,84},{85,85,85,85,85,85,85,85,85,85,85,85,85,85,85,85},{86,86,86,86,86,86,86,86,86,86,86,86,86,86,86,86},{87,87,87,87,87,87,87,87,87,87,87,87,87,87,87,87},
  {88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88},{89,89,89,89,89,89,89,89,89,89,89,89,89,89,89,89},{90,90,90,90,90,90,90,90,90,90,90,90,90,90,90,90},{91,91,91,91,91,91,91,91,91,91,91,91,91,91,91,91},
  {92,92,92,92,92,92,92,92,92,92,92,92,92,92,92,92},{93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93},{94,94,94,94,94,94,94,94,94,94,94,94,94,94,94,94},{95,95,95,95,95,95,95,95,95,95,95,95,95,95,95,95},
  {96,96,96,96,96,96,96,96,96,96,96,96,96,96,96,96},{97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97},{98,98,98,98,98,98,98,98,98,98,98,98,98,98,98,98},{99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99},
  {100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100},{101,101,101,101,101,101,101,101,101,101,101,101,101,101,101,101},{102,102,102,102,102,102,102,102,102,102,102,102,102,102,102,102},{103,103,103,103,103,103,103,103,103,103,103,103,103,103,103,103},
  {104,104,104,104,104,104,104,104,104,104,104,104,104,104,104,104},{105,105,105,105,105,105,105,105,105,105,105,105,105,105,105,105},{106,106,106,106,106,106,106,106,106,106,106,106,106,106,106,106},{107,107,107,107,107,107,107,107,107,107,107,107,107,107,107,107},
  {108,108,108,108,108,108,108,108,108,108,108,108,108,108,108,108},{109,109,109,109,109,109,109,109,109,109,109,109,109,109,109,109},{110,110,110,110,110,110,110,110,110,110,110,110,110,110,110,110},{111,111,111,111,111,111,111,111,111,111,111,111,111,111,111,111},
  {112,112,112,112,112,112,112,112,112,112,112,112,112,112,112,112},{113,113,113,113,113,113,113,113,113,113,113,113,113,113,113,113},{114,114,114,114,114,114,114,114,114,114,114,114,114,114,114,114},{115,115,115,115,115,115,115,115,115,115,115,115,115,115,115,115},
  {116,116,116,116,116,116,116,116,116,116,116,116,116,116,116,116},{117,117,117,117,117,117,117,117,117,117,117,117,117,117,117,117},{118,118,118,118,118,118,118,118,118,118,118,118,118,118,118,118},{119,119,119,119,119,119,119,119,119,119,119,119,119,119,119,119},
  {120,120,120,120,120,120,120,120,120,120,120,120,120,120,120,120},{121,121,121,121,121,121,121,121,121,121,121,121,121,121,121,121},{122,122,122,122,122,122,122,122,122,122,122,122,122,122,122,122},{123,123,123,123,123,123,123,123,123,123,123,123,123,123,123,123},
  {124,124,124,124,124,124,124,124,124,124,124,124,124,124,124,124},{125,125,125,125,125,125,125,125,125,125,125,125,125,125,125,125},{126,126,126,126,126,126,126,126,126,126,126,126,126,126,126,126},{127,127,127,127,127,127,127,127,127,127,127,127,127,127,127,127},
  {-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128},{-127,-127,-127,-127,-127,-127,-127,-127,-127,-127,-127,-127,-127,-127,-127,-127},{-126,-126,-126,-126,-126,-126,-126,-126,-126,-126,-126,-126,-126,-126,-126,-126},{-125,-125,-125,-125,-125,-125,-125,-125,-125,-125,-125,-125,-125,-125,-125,-125},
  {-124,-124,-124,-124,-124,-124,-124,-124,-124,-124,-124,-124,-124,-124,-124,-124},{-123,-123,-123,-123,-123,-123,-123,-123,-123,-123,-123,-123,-123,-123,-123,-123},{-122,-122,-122,-122,-122,-122,-122,-122,-122,-122,-122,-122,-122,-122,-122,-122},{-121,-121,-121,-121,-121,-121,-121,-121,-121,-121,-121,-121,-121,-121,-121,-121},
  {-120,-120,-120,-120,-120,-120,-120,-120,-120,-120,-120,-120,-120,-120,-120,-120},{-119,-119,-119,-119,-119,-119,-119,-119,-119,-119,-119,-119,-119,-119,-119,-119},{-118,-118,-118,-118,-118,-118,-118,-118,-118,-118,-118,-118,-118,-118,-118,-118},{-117,-117,-117,-117,-117,-117,-117,-117,-117,-117,-117,-117,-117,-117,-117,-117},
  {-116,-116,-116,-116,-116,-116,-116,-116,-116,-116,-116,-116,-116,-116,-116,-116},{-115,-115,-115,-115,-115,-115,-115,-115,-115,-115,-115,-115,-115,-115,-115,-115},{-114,-114,-114,-114,-114,-114,-114,-114,-114,-114,-114,-114,-114,-114,-114,-114},{-113,-113,-113,-113,-113,-113,-113,-113,-113,-113,-113,-113,-113,-113,-113,-113},
  {-112,-112,-112,-112,-112,-112,-112,-112,-112,-112,-112,-112,-112,-112,-112,-112},{-111,-111,-111,-111,-111,-111,-111,-111,-111,-111,-111,-111,-111,-111,-111,-111},{-110,-110,-110,-110,-110,-110,-110,-110,-110,-110,-110,-110,-110,-110,-110,-110},{-109,-109,-109,-109,-109,-109,-109,-109,-109,-109,-109,-109,-109,-109,-109,-109},
  {-108,-108,-108,-108,-108,-108,-108,-108,-108,-108,-108,-108,-108,-108,-108,-108},{-107,-107,-107,-107,-107,-107,-107,-107,-107,-107,-107,-107,-107,-107,-107,-107},{-106,-106,-106,-106,-106,-106,-106,-106,-106,-106,-106,-106,-106,-106,-106,-106},{-105,-105,-105,-105,-105,-105,-105,-105,-105,-105,-105,-105,-105,-105,-105,-105},
  {-104,-104,-104,-104,-104,-104,-104,-104,-104,-104,-104,-104,-104,-104,-104,-104},{-103,-103,-103,-103,-103,-103,-103,-103,-103,-103,-103,-103,-103,-103,-103,-103},{-102,-102,-102,-102,-102,-102,-102,-102,-102,-102,-102,-102,-102,-102,-102,-102},{-101,-101,-101,-101,-101,-101,-101,-101,-101,-101,-101,-101,-101,-101,-101,-101},
  {-100,-100,-100,-100,-100,-100,-100,-100,-100,-100,-100,-100,-100,-100,-100,-100},{-99,-99,-99,-99,-99,-99,-99,-99,-99,-99,-99,-99,-99,-99,-99,-99},{-98,-98,-98,-98,-98,-98,-98,-98,-98,-98,-98,-98,-98,-98,-98,-98},{-97,-97,-97,-97,-97,-97,-97,-97,-97,-97,-97,-97,-97,-97,-97,-97},
  {-96,-96,-96,-96,-96,-96,-96,-96,-96,-96,-96,-96,-96,-96,-96,-96},{-95,-95,-95,-95,-95,-95,-95,-95,-95,-95,-95,-95,-95,-95,-95,-95},{-94,-94,-94,-94,-94,-94,-94,-94,-94,-94,-94,-94,-94,-94,-94,-94},{-93,-93,-93,-93,-93,-93,-93,-93,-93,-93,-93,-93,-93,-93,-93,-93},
  {-92,-92,-92,-92,-92,-92,-92,-92,-92,-92,-92,-92,-92,-92,-92,-92},{-91,-91,-91,-91,-91,-91,-91,-91,-91,-91,-91,-91,-91,-91,-91,-91},{-90,-90,-90,-90,-90,-90,-90,-90,-90,-90,-90,-90,-90,-90,-90,-90},{-89,-89,-89,-89,-89,-89,-89,-89,-89,-89,-89,-89,-89,-89,-89,-89},
  {-88,-88,-88,-88,-88,-88,-88,-88,-88,-88,-88,-88,-88,-88,-88,-88},{-87,-87,-87,-87,-87,-87,-87,-87,-87,-87,-87,-87,-87,-87,-87,-87},{-86,-86,-86,-86,-86,-86,-86,-86,-86,-86,-86,-86,-86,-86,-86,-86},{-85,-85,-85,-85,-85,-85,-85,-85,-85,-85,-85,-85,-85,-85,-85,-85},
  {-84,-84,-84,-84,-84,-84,-84,-84,-84,-84,-84,-84,-84,-84,-84,-84},{-83,-83,-83,-83,-83,-83,-83,-83,-83,-83,-83,-83,-83,-83,-83,-83},{-82,-82,-82,-82,-82,-82,-82,-82,-82,-82,-82,-82,-82,-82,-82,-82},{-81,-81,-81,-81,-81,-81,-81,-81,-81,-81,-81,-81,-81,-81,-81,-81},
  {-80,-80,-80,-80,-80,-80,-80,-80,-80,-80,-80,-80,-80,-80,-80,-80},{-79,-79,-79,-79,-79,-79,-79,-79,-79,-79,-79,-79,-79,-79,-79,-79},{-78,-78,-78,-78,-78,-78,-78,-78,-78,-78,-78,-78,-78,-78,-78,-78},{-77,-77,-77,-77,-77,-77,-77,-77,-77,-77,-77,-77,-77,-77,-77,-77},
  {-76,-76,-76,-76,-76,-76,-76,-76,-76,-76,-76,-76,-76,-76,-76,-76},{-75,-75,-75,-75,-75,-75,-75,-75,-75,-75,-75,-75,-75,-75,-75,-75},{-74,-74,-74,-74,-74,-74,-74,-74,-74,-74,-74,-74,-74,-74,-74,-74},{-73,-73,-73,-73,-73,-73,-73,-73,-73,-73,-73,-73,-73,-73,-73,-73},
  {-72,-72,-72,-72,-72,-72,-72,-72,-72,-72,-72,-72,-72,-72,-72,-72},{-71,-71,-71,-71,-71,-71,-71,-71,-71,-71,-71,-71,-71,-71,-71,-71},{-70,-70,-70,-70,-70,-70,-70,-70,-70,-70,-70,-70,-70,-70,-70,-70},{-69,-69,-69,-69,-69,-69,-69,-69,-69,-69,-69,-69,-69,-69,-69,-69},
  {-68,-68,-68,-68,-68,-68,-68,-68,-68,-68,-68,-68,-68,-68,-68,-68},{-67,-67,-67,-67,-67,-67,-67,-67,-67,-67,-67,-67,-67,-67,-67,-67},{-66,-66,-66,-66,-66,-66,-66,-66,-66,-66,-66,-66,-66,-66,-66,-66},{-65,-65,-65,-65,-65,-65,-65,-65,-65,-65,-65,-65,-65,-65,-65,-65},
  {-64,-64,-64,-64,-64,-64,-64,-64,-64,-64,-64,-64,-64,-64,-64,-64},{-63,-63,-63,-63,-63,-63,-63,-63,-63,-63,-63,-63,-63,-63,-63,-63},{-62,-62,-62,-62,-62,-62,-62,-62,-62,-62,-62,-62,-62,-62,-62,-62},{-61,-61,-61,-61,-61,-61,-61,-61,-61,-61,-61,-61,-61,-61,-61,-61},
  {-60,-60,-60,-60,-60,-60,-60,-60,-60,-60,-60,-60,-60,-60,-60,-60},{-59,-59,-59,-59,-59,-59,-59,-59,-59,-59,-59,-59,-59,-59,-59,-59},{-58,-58,-58,-58,-58,-58,-58,-58,-58,-58,-58,-58,-58,-58,-58,-58},{-57,-57,-57,-57,-57,-57,-57,-57,-57,-57,-57,-57,-57,-57,-57,-57},
  {-56,-56,-56,-56,-56,-56,-56,-56,-56,-56,-56,-56,-56,-56,-56,-56},{-55,-55,-55,-55,-55,-55,-55,-55,-55,-55,-55,-55,-55,-55,-55,-55},{-54,-54,-54,-54,-54,-54,-54,-54,-54,-54,-54,-54,-54,-54,-54,-54},{-53,-53,-53,-53,-53,-53,-53,-53,-53,-53,-53,-53,-53,-53,-53,-53},
  {-52,-52,-52,-52,-52,-52,-52,-52,-52,-52,-52,-52,-52,-52,-52,-52},{-51,-51,-51,-51,-51,-51,-51,-51,-51,-51,-51,-51,-51,-51,-51,-51},{-50,-50,-50,-50,-50,-50,-50,-50,-50,-50,-50,-50,-50,-50,-50,-50},{-49,-49,-49,-49,-49,-49,-49,-49,-49,-49,-49,-49,-49,-49,-49,-49},
  {-48,-48,-48,-48,-48,-48,-48,-48,-48,-48,-48,-48,-48,-48,-48,-48},{-47,-47,-47,-47,-47,-47,-47,-47,-47,-47,-47,-47,-47,-47,-47,-47},{-46,-46,-46,-46,-46,-46,-46,-46,-46,-46,-46,-46,-46,-46,-46,-46},{-45,-45,-45,-45,-45,-45,-45,-45,-45,-45,-45,-45,-45,-45,-45,-45},
  {-44,-44,-44,-44,-44,-44,-44,-44,-44,-44,-44,-44,-44,-44,-44,-44},{-43,-43,-43,-43,-43,-43,-43,-43,-43,-43,-43,-43,-43,-43,-43,-43},{-42,-42,-42,-42,-42,-42,-42,-42,-42,-42,-42,-42,-42,-42,-42,-42},{-41,-41,-41,-41,-41,-41,-41,-41,-41,-41,-41,-41,-41,-41,-41,-41},
  {-40,-40,-40,-40,-40,-40,-40,-40,-40,-40,-40,-40,-40,-40,-40,-40},{-39,-39,-39,-39,-39,-39,-39,-39,-39,-39,-39,-39,-39,-39,-39,-39},{-38,-38,-38,-38,-38,-38,-38,-38,-38,-38,-38,-38,-38,-38,-38,-38},{-37,-37,-37,-37,-37,-37,-37,-37,-37,-37,-37,-37,-37,-37,-37,-37},
  {-36,-36,-36,-36,-36,-36,-36,-36,-36,-36,-36,-36,-36,-36,-36,-36},{-35,-35,-35,-35,-35,-35,-35,-35,-35,-35,-35,-35,-35,-35,-35,-35},{-34,-34,-34,-34,-34,-34,-34,-34,-34,-34,-34,-34,-34,-34,-34,-34},{-33,-33,-33,-33,-33,-33,-33,-33,-33,-33,-33,-33,-33,-33,-33,-33},
  {-32,-32,-32,-32,-32,-32,-32,-32,-32,-32,-32,-32,-32,-32,-32,-32},{-31,-31,-31,-31,-31,-31,-31,-31,-31,-31,-31,-31,-31,-31,-31,-31},{-30,-30,-30,-30,-30,-30,-30,-30,-30,-30,-30,-30,-30,-30,-30,-30},{-29,-29,-29,-29,-29,-29,-29,-29,-29,-29,-29,-29,-29,-29,-29,-29},
  {-28,-28,-28,-28,-28,-28,-28,-28,-28,-28,-28,-28,-28,-28,-28,-28},{-27,-27,-27,-27,-27,-27,-27,-27,-27,-27,-27,-27,-27,-27,-27,-27},{-26,-26,-26,-26,-26,-26,-26,-26,-26,-26,-26,-26,-26,-26,-26,-26},{-25,-25,-25,-25,-25,-25,-25,-25,-25,-25,-25,-25,-25,-25,-25,-25},
  {-24,-24,-24,-24,-24,-24,-24,-24,-24,-24,-24,-24,-24,-24,-24,-24},{-23,-23,-23,-23,-23,-23,-23,-23,-23,-23,-23,-23,-23,-23,-23,-23},{-22,-22,-22,-22,-22,-22,-22,-22,-22,-22,-22,-22,-22,-22,-22,-22},{-21,-21,-21,-21,-21,-21,-21,-21,-21,-21,-21,-21,-21,-21,-21,-21},
  {-20,-20,-20,-20,-20,-20,-20,-20,-20,-20,-20,-20,-20,-20,-20,-20},{-19,-19,-19,-19,-19,-19,-19,-19,-19,-19,-19,-19,-19,-19,-19,-19},{-18,-18,-18,-18,-18,-18,-18,-18,-18,-18,-18,-18,-18,-18,-18,-18},{-17,-17,-17,-17,-17,-17,-17,-17,-17,-17,-17,-17,-17,-17,-17,-17},
  {-16,-16,-16,-16,-16,-16,-16,-16,-16,-16,-16,-16,-16,-16,-16,-16},{-15,-15,-15,-15,-15,-15,-15,-15,-15,-15,-15,-15,-15,-15,-15,-15},{-14,-14,-14,-14,-14,-14,-14,-14,-14,-14,-14,-14,-14,-14,-14,-14},{-13,-13,-13,-13,-13,-13,-13,-13,-13,-13,-13,-13,-13,-13,-13,-13},
  {-12,-12,-12,-12,-12,-12,-12,-12,-12,-12,-12,-12,-12,-12,-12,-12},{-11,-11,-11,-11,-11,-11,-11,-11,-11,-11,-11,-11,-11,-11,-11,-11},{-10,-10,-10,-10,-10,-10,-10,-10,-10,-10,-10,-10,-10,-10,-10,-10},{-9,-9,-9,-9,-9,-9,-9,-9,-9,-9,-9,-9,-9,-9,-9,-9},
  {-8,-8,-8,-8,-8,-8,-8,-8,-8,-8,-8,-8,-8,-8,-8,-8},{-7,-7,-7,-7,-7,-7,-7,-7,-7,-7,-7,-7,-7,-7,-7,-7},{-6,-6,-6,-6,-6,-6,-6,-6,-6,-6,-6,-6,-6,-6,-6,-6},{-5,-5,-5,-5,-5,-5,-5,-5,-5,-5,-5,-5,-5,-5,-5,-5},
  {-4,-4,-4,-4,-4,-4,-4,-4,-4,-4,-4,-4,-4,-4,-4,-4},{-3,-3,-3,-3,-3,-3,-3,-3,-3,-3,-3,-3,-3,-3,-3,-3},{-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2},{-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
#else
  {(long long int)0x0000000000000000ULL,(long long int)0x0000000000000000ULL},{(long long int)0x0101010101010101ULL,(long long int)0x0101010101010101ULL},{(long long int)0x0202020202020202ULL,(long long int)0x0202020202020202ULL},{(long long int)0x0303030303030303ULL,(long long int)0x0303030303030303ULL},
  {(long long int)0x0404040404040404ULL,(long long int)0x0404040404040404ULL},{(long long int)0x0505050505050505ULL,(long long int)0x0505050505050505ULL},{(long long int)0x0606060606060606ULL,(long long int)0x0606060606060606ULL},{(long long int)0x0707070707070707ULL,(long long int)0x0707070707070707ULL},
  {(long long int)0x0808080808080808ULL,(long long int)0x0808080808080808ULL},{(long long int)0x0909090909090909ULL,(long long int)0x0909090909090909ULL},{(long long int)0x0A0A0A0A0A0A0A0AULL,(long long int)0x0A0A0A0A0A0A0A0AULL},{(long long int)0x0B0B0B0B0B0B0B0BULL,(long long int)0x0B0B0B0B0B0B0B0BULL},
  {(long long int)0x0C0C0C0C0C0C0C0CULL,(long long int)0x0C0C0C0C0C0C0C0CULL},{(long long int)0x0D0D0D0D0D0D0D0DULL,(long long int)0x0D0D0D0D0D0D0D0DULL},{(long long int)0x0E0E0E0E0E0E0E0EULL,(long long int)0x0E0E0E0E0E0E0E0EULL},{(long long int)0x0F0F0F0F0F0F0F0FULL,(long long int)0x0F0F0F0F0F0F0F0FULL},
  {(long long int)0x1010101010101010ULL,(long long int)0x1010101010101010ULL},{(long long int)0x1111111111111111ULL,(long long int)0x1111111111111111ULL},{(long long int)0x1212121212121212ULL,(long long int)0x1212121212121212ULL},{(long long int)0x1313131313131313ULL,(long long int)0x1313131313131313ULL},
  {(long long int)0x1414141414141414ULL,(long long int)0x1414141414141414ULL},{(long long int)0x1515151515151515ULL,(long long int)0x1515151515151515ULL},{(long long int)0x1616161616161616ULL,(long long int)0x1616161616161616ULL},{(long long int)0x1717171717171717ULL,(long long int)0x1717171717171717ULL},
  {(long long int)0x1818181818181818ULL,(long long int)0x1818181818181818ULL},{(long long int)0x1919191919191919ULL,(long long int)0x1919191919191919ULL},{(long long int)0x1A1A1A1A1A1A1A1AULL,(long long int)0x1A1A1A1A1A1A1A1AULL},{(long long int)0x1B1B1B1B1B1B1B1BULL,(long long int)0x1B1B1B1B1B1B1B1BULL},
  {(long long int)0x1C1C1C1C1C1C1C1CULL,(long long int)0x1C1C1C1C1C1C1C1CULL},{(long long int)0x1D1D1D1D1D1D1D1DULL,(long long int)0x1D1D1D1D1D1D1D1DULL},{(long long int)0x1E1E1E1E1E1E1E1EULL,(long long int)0x1E1E1E1E1E1E1E1EULL},{(long long int)0x1F1F1F1F1F1F1F1FULL,(long long int)0x1F1F1F1F1F1F1F1FULL},
  {(long long int)0x2020202020202020ULL,(long long int)0x2020202020202020ULL},{(long long int)0x2121212121212121ULL,(long long int)0x2121212121212121ULL},{(long long int)0x2222222222222222ULL,(long long int)0x2222222222222222ULL},{(long long int)0x2323232323232323ULL,(long long int)0x2323232323232323ULL},
  {(long long int)0x2424242424242424ULL,(long long int)0x2424242424242424ULL},{(long long int)0x2525252525252525ULL,(long long int)0x2525252525252525ULL},{(long long int)0x2626262626262626ULL,(long long int)0x2626262626262626ULL},{(long long int)0x2727272727272727ULL,(long long int)0x2727272727272727ULL},
  {(long long int)0x2828282828282828ULL,(long long int)0x2828282828282828ULL},{(long long int)0x2929292929292929ULL,(long long int)0x2929292929292929ULL},{(long long int)0x2A2A2A2A2A2A2A2AULL,(long long int)0x2A2A2A2A2A2A2A2AULL},{(long long int)0x2B2B2B2B2B2B2B2BULL,(long long int)0x2B2B2B2B2B2B2B2BULL},
  {(long long int)0x2C2C2C2C2C2C2C2CULL,(long long int)0x2C2C2C2C2C2C2C2CULL},{(long long int)0x2D2D2D2D2D2D2D2DULL,(long long int)0x2D2D2D2D2D2D2D2DULL},{(long long int)0x2E2E2E2E2E2E2E2EULL,(long long int)0x2E2E2E2E2E2E2E2EULL},{(long long int)0x2F2F2F2F2F2F2F2FULL,(long long int)0x2F2F2F2F2F2F2F2FULL},
  {(long long int)0x3030303030303030ULL,(long long int)0x3030303030303030ULL},{(long long int)0x3131313131313131ULL,(long long int)0x3131313131313131ULL},{(long long int)0x3232323232323232ULL,(long long int)0x3232323232323232ULL},{(long long int)0x3333333333333333ULL,(long long int)0x3333333333333333ULL},
  {(long long int)0x3434343434343434ULL,(long long int)0x3434343434343434ULL},{(long long int)0x3535353535353535ULL,(long long int)0x3535353535353535ULL},{(long long int)0x3636363636363636ULL,(long long int)0x3636363636363636ULL},{(long long int)0x3737373737373737ULL,(long long int)0x3737373737373737ULL},
  {(long long int)0x3838383838383838ULL,(long long int)0x3838383838383838ULL},{(long long int)0x3939393939393939ULL,(long long int)0x3939393939393939ULL},{(long long int)0x3A3A3A3A3A3A3A3AULL,(long long int)0x3A3A3A3A3A3A3A3AULL},{(long long int)0x3B3B3B3B3B3B3B3BULL,(long long int)0x3B3B3B3B3B3B3B3BULL},
  {(long long int)0x3C3C3C3C3C3C3C3CULL,(long long int)0x3C3C3C3C3C3C3C3CULL},{(long long int)0x3D3D3D3D3D3D3D3DULL,(long long int)0x3D3D3D3D3D3D3D3DULL},{(long long int)0x3E3E3E3E3E3E3E3EULL,(long long int)0x3E3E3E3E3E3E3E3EULL},{(long long int)0x3F3F3F3F3F3F3F3FULL,(long long int)0x3F3F3F3F3F3F3F3FULL},
  {(long long int)0x4040404040404040ULL,(long long int)0x4040404040404040ULL},{(long long int)0x4141414141414141ULL,(long long int)0x4141414141414141ULL},{(long long int)0x4242424242424242ULL,(long long int)0x4242424242424242ULL},{(long long int)0x4343434343434343ULL,(long long int)0x4343434343434343ULL},
  {(long long int)0x4444444444444444ULL,(long long int)0x4444444444444444ULL},{(long long int)0x4545454545454545ULL,(long long int)0x4545454545454545ULL},{(long long int)0x4646464646464646ULL,(long long int)0x4646464646464646ULL},{(long long int)0x4747474747474747ULL,(long long int)0x4747474747474747ULL},
  {(long long int)0x4848484848484848ULL,(long long int)0x4848484848484848ULL},{(long long int)0x4949494949494949ULL,(long long int)0x4949494949494949ULL},{(long long int)0x4A4A4A4A4A4A4A4AULL,(long long int)0x4A4A4A4A4A4A4A4AULL},{(long long int)0x4B4B4B4B4B4B4B4BULL,(long long int)0x4B4B4B4B4B4B4B4BULL},
  {(long long int)0x4C4C4C4C4C4C4C4CULL,(long long int)0x4C4C4C4C4C4C4C4CULL},{(long long int)0x4D4D4D4D4D4D4D4DULL,(long long int)0x4D4D4D4D4D4D4D4DULL},{(long long int)0x4E4E4E4E4E4E4E4EULL,(long long int)0x4E4E4E4E4E4E4E4EULL},{(long long int)0x4F4F4F4F4F4F4F4FULL,(long long int)0x4F4F4F4F4F4F4F4FULL},
  {(long long int)0x5050505050505050ULL,(long long int)0x5050505050505050ULL},{(long long int)0x5151515151515151ULL,(long long int)0x5151515151515151ULL},{(long long int)0x5252525252525252ULL,(long long int)0x5252525252525252ULL},{(long long int)0x5353535353535353ULL,(long long int)0x5353535353535353ULL},
  {(long long int)0x5454545454545454ULL,(long long int)0x5454545454545454ULL},{(long long int)0x5555555555555555ULL,(long long int)0x5555555555555555ULL},{(long long int)0x5656565656565656ULL,(long long int)0x5656565656565656ULL},{(long long int)0x5757575757575757ULL,(long long int)0x5757575757575757ULL},
  {(long long int)0x5858585858585858ULL,(long long int)0x5858585858585858ULL},{(long long int)0x5959595959595959ULL,(long long int)0x5959595959595959ULL},{(long long int)0x5A5A5A5A5A5A5A5AULL,(long long int)0x5A5A5A5A5A5A5A5AULL},{(long long int)0x5B5B5B5B5B5B5B5BULL,(long long int)0x5B5B5B5B5B5B5B5BULL},
  {(long long int)0x5C5C5C5C5C5C5C5CULL,(long long int)0x5C5C5C5C5C5C5C5CULL},{(long long int)0x5D5D5D5D5D5D5D5DULL,(long long int)0x5D5D5D5D5D5D5D5DULL},{(long long int)0x5E5E5E5E5E5E5E5EULL,(long long int)0x5E5E5E5E5E5E5E5EULL},{(long long int)0x5F5F5F5F5F5F5F5FULL,(long long int)0x5F5F5F5F5F5F5F5FULL},
  {(long long int)0x6060606060606060ULL,(long long int)0x6060606060606060ULL},{(long long int)0x6161616161616161ULL,(long long int)0x6161616161616161ULL},{(long long int)0x6262626262626262ULL,(long long int)0x6262626262626262ULL},{(long long int)0x6363636363636363ULL,(long long int)0x6363636363636363ULL},
  {(long long int)0x6464646464646464ULL,(long long int)0x6464646464646464ULL},{(long long int)0x6565656565656565ULL,(long long int)0x6565656565656565ULL},{(long long int)0x6666666666666666ULL,(long long int)0x6666666666666666ULL},{(long long int)0x6767676767676767ULL,(long long int)0x6767676767676767ULL},
  {(long long int)0x6868686868686868ULL,(long long int)0x6868686868686868ULL},{(long long int)0x6969696969696969ULL,(long long int)0x6969696969696969ULL},{(long long int)0x6A6A6A6A6A6A6A6AULL,(long long int)0x6A6A6A6A6A6A6A6AULL},{(long long int)0x6B6B6B6B6B6B6B6BULL,(long long int)0x6B6B6B6B6B6B6B6BULL},
  {(long long int)0x6C6C6C6C6C6C6C6CULL,(long long int)0x6C6C6C6C6C6C6C6CULL},{(long long int)0x6D6D6D6D6D6D6D6DULL,(long long int)0x6D6D6D6D6D6D6D6DULL},{(long long int)0x6E6E6E6E6E6E6E6EULL,(long long int)0x6E6E6E6E6E6E6E6EULL},{(long long int)0x6F6F6F6F6F6F6F6FULL,(long long int)0x6F6F6F6F6F6F6F6FULL},
  {(long long int)0x7070707070707070ULL,(long long int)0x7070707070707070ULL},{(long long int)0x7171717171717171ULL,(long long int)0x7171717171717171ULL},{(long long int)0x7272727272727272ULL,(long long int)0x7272727272727272ULL},{(long long int)0x7373737373737373ULL,(long long int)0x7373737373737373ULL},
  {(long long int)0x7474747474747474ULL,(long long int)0x7474747474747474ULL},{(long long int)0x7575757575757575ULL,(long long int)0x7575757575757575ULL},{(long long int)0x7676767676767676ULL,(long long int)0x7676767676767676ULL},{(long long int)0x7777777777777777ULL,(long long int)0x7777777777777777ULL},
  {(long long int)0x7878787878787878ULL,(long long int)0x7878787878787878ULL},{(long long int)0x7979797979797979ULL,(long long int)0x7979797979797979ULL},{(long long int)0x7A7A7A7A7A7A7A7AULL,(long long int)0x7A7A7A7A7A7A7A7AULL},{(long long int)0x7B7B7B7B7B7B7B7BULL,(long long int)0x7B7B7B7B7B7B7B7BULL},
  {(long long int)0x7C7C7C7C7C7C7C7CULL,(long long int)0x7C7C7C7C7C7C7C7CULL},{(long long int)0x7D7D7D7D7D7D7D7DULL,(long long int)0x7D7D7D7D7D7D7D7DULL},{(long long int)0x7E7E7E7E7E7E7E7EULL,(long long int)0x7E7E7E7E7E7E7E7EULL},{(long long int)0x7F7F7F7F7F7F7F7FULL,(long long int)0x7F7F7F7F7F7F7F7FULL},
  {(long long int)0x8080808080808080ULL,(long long int)0x8080808080808080ULL},{(long long int)0x8181818181818181ULL,(long long int)0x8181818181818181ULL},{(long long int)0x8282828282828282ULL,(long long int)0x8282828282828282ULL},{(long long int)0x8383838383838383ULL,(long long int)0x8383838383838383ULL},
  {(long long int)0x8484848484848484ULL,(long long int)0x8484848484848484ULL},{(long long int)0x8585858585858585ULL,(long long int)0x8585858585858585ULL},{(long long int)0x8686868686868686ULL,(long long int)0x8686868686868686ULL},{(long long int)0x8787878787878787ULL,(long long int)0x8787878787878787ULL},
  {(long long int)0x8888888888888888ULL,(long long int)0x8888888888888888ULL},{(long long int)0x8989898989898989ULL,(long long int)0x8989898989898989ULL},{(long long int)0x8A8A8A8A8A8A8A8AULL,(long long int)0x8A8A8A8A8A8A8A8AULL},{(long long int)0x8B8B8B8B8B8B8B8BULL,(long long int)0x8B8B8B8B8B8B8B8BULL},
  {(long long int)0x8C8C8C8C8C8C8C8CULL,(long long int)0x8C8C8C8C8C8C8C8CULL},{(long long int)0x8D8D8D8D8D8D8D8DULL,(long long int)0x8D8D8D8D8D8D8D8DULL},{(long long int)0x8E8E8E8E8E8E8E8EULL,(long long int)0x8E8E8E8E8E8E8E8EULL},{(long long int)0x8F8F8F8F8F8F8F8FULL,(long long int)0x8F8F8F8F8F8F8F8FULL},
  {(long long int)0x9090909090909090ULL,(long long int)0x9090909090909090ULL},{(long long int)0x9191919191919191ULL,(long long int)0x9191919191919191ULL},{(long long int)0x9292929292929292ULL,(long long int)0x9292929292929292ULL},{(long long int)0x9393939393939393ULL,(long long int)0x9393939393939393ULL},
  {(long long int)0x9494949494949494ULL,(long long int)0x9494949494949494ULL},{(long long int)0x9595959595959595ULL,(long long int)0x9595959595959595ULL},{(long long int)0x9696969696969696ULL,(long long int)0x9696969696969696ULL},{(long long int)0x9797979797979797ULL,(long long int)0x9797979797979797ULL},
  {(long long int)0x9898989898989898ULL,(long long int)0x9898989898989898ULL},{(long long int)0x9999999999999999ULL,(long long int)0x9999999999999999ULL},{(long long int)0x9A9A9A9A9A9A9A9AULL,(long long int)0x9A9A9A9A9A9A9A9AULL},{(long long int)0x9B9B9B9B9B9B9B9BULL,(long long int)0x9B9B9B9B9B9B9B9BULL},
  {(long long int)0x9C9C9C9C9C9C9C9CULL,(long long int)0x9C9C9C9C9C9C9C9CULL},{(long long int)0x9D9D9D9D9D9D9D9DULL,(long long int)0x9D9D9D9D9D9D9D9DULL},{(long long int)0x9E9E9E9E9E9E9E9EULL,(long long int)0x9E9E9E9E9E9E9E9EULL},{(long long int)0x9F9F9F9F9F9F9F9FULL,(long long int)0x9F9F9F9F9F9F9F9FULL},
  {(long long int)0xA0A0A0A0A0A0A0A0ULL,(long long int)0xA0A0A0A0A0A0A0A0ULL},{(long long int)0xA1A1A1A1A1A1A1A1ULL,(long long int)0xA1A1A1A1A1A1A1A1ULL},{(long long int)0xA2A2A2A2A2A2A2A2ULL,(long long int)0xA2A2A2A2A2A2A2A2ULL},{(long long int)0xA3A3A3A3A3A3A3A3ULL,(long long int)0xA3A3A3A3A3A3A3A3ULL},
  {(long long int)0xA4A4A4A4A4A4A4A4ULL,(long long int)0xA4A4A4A4A4A4A4A4ULL},{(long long int)0xA5A5A5A5A5A5A5A5ULL,(long long int)0xA5A5A5A5A5A5A5A5ULL},{(long long int)0xA6A6A6A6A6A6A6A6ULL,(long long int)0xA6A6A6A6A6A6A6A6ULL},{(long long int)0xA7A7A7A7A7A7A7A7ULL,(long long int)0xA7A7A7A7A7A7A7A7ULL},
  {(long long int)0xA8A8A8A8A8A8A8A8ULL,(long long int)0xA8A8A8A8A8A8A8A8ULL},{(long long int)0xA9A9A9A9A9A9A9A9ULL,(long long int)0xA9A9A9A9A9A9A9A9ULL},{(long long int)0xAAAAAAAAAAAAAAAAULL,(long long int)0xAAAAAAAAAAAAAAAAULL},{(long long int)0xABABABABABABABABULL,(long long int)0xABABABABABABABABULL},
  {(long long int)0xACACACACACACACACULL,(long long int)0xACACACACACACACACULL},{(long long int)0xADADADADADADADADULL,(long long int)0xADADADADADADADADULL},{(long long int)0xAEAEAEAEAEAEAEAEULL,(long long int)0xAEAEAEAEAEAEAEAEULL},{(long long int)0xAFAFAFAFAFAFAFAFULL,(long long int)0xAFAFAFAFAFAFAFAFULL},
  {(long long int)0xB0B0B0B0B0B0B0B0ULL,(long long int)0xB0B0B0B0B0B0B0B0ULL},{(long long int)0xB1B1B1B1B1B1B1B1ULL,(long long int)0xB1B1B1B1B1B1B1B1ULL},{(long long int)0xB2B2B2B2B2B2B2B2ULL,(long long int)0xB2B2B2B2B2B2B2B2ULL},{(long long int)0xB3B3B3B3B3B3B3B3ULL,(long long int)0xB3B3B3B3B3B3B3B3ULL},
  {(long long int)0xB4B4B4B4B4B4B4B4ULL,(long long int)0xB4B4B4B4B4B4B4B4ULL},{(long long int)0xB5B5B5B5B5B5B5B5ULL,(long long int)0xB5B5B5B5B5B5B5B5ULL},{(long long int)0xB6B6B6B6B6B6B6B6ULL,(long long int)0xB6B6B6B6B6B6B6B6ULL},{(long long int)0xB7B7B7B7B7B7B7B7ULL,(long long int)0xB7B7B7B7B7B7B7B7ULL},
  {(long long int)0xB8B8B8B8B8B8B8B8ULL,(long long int)0xB8B8B8B8B8B8B8B8ULL},{(long long int)0xB9B9B9B9B9B9B9B9ULL,(long long int)0xB9B9B9B9B9B9B9B9ULL},{(long long int)0xBABABABABABABABAULL,(long long int)0xBABABABABABABABAULL},{(long long int)0xBBBBBBBBBBBBBBBBULL,(long long int)0xBBBBBBBBBBBBBBBBULL},
  {(long long int)0xBCBCBCBCBCBCBCBCULL,(long long int)0xBCBCBCBCBCBCBCBCULL},{(long long int)0xBDBDBDBDBDBDBDBDULL,(long long int)0xBDBDBDBDBDBDBDBDULL},{(long long int)0xBEBEBEBEBEBEBEBEULL,(long long int)0xBEBEBEBEBEBEBEBEULL},{(long long int)0xBFBFBFBFBFBFBFBFULL,(long long int)0xBFBFBFBFBFBFBFBFULL},
  {(long long int)0xC0C0C0C0C0C0C0C0ULL,(long long int)0xC0C0C0C0C0C0C0C0ULL},{(long long int)0xC1C1C1C1C1C1C1C1ULL,(long long int)0xC1C1C1C1C1C1C1C1ULL},{(long long int)0xC2C2C2C2C2C2C2C2ULL,(long long int)0xC2C2C2C2C2C2C2C2ULL},{(long long int)0xC3C3C3C3C3C3C3C3ULL,(long long int)0xC3C3C3C3C3C3C3C3ULL},
  {(long long int)0xC4C4C4C4C4C4C4C4ULL,(long long int)0xC4C4C4C4C4C4C4C4ULL},{(long long int)0xC5C5C5C5C5C5C5C5ULL,(long long int)0xC5C5C5C5C5C5C5C5ULL},{(long long int)0xC6C6C6C6C6C6C6C6ULL,(long long int)0xC6C6C6C6C6C6C6C6ULL},{(long long int)0xC7C7C7C7C7C7C7C7ULL,(long long int)0xC7C7C7C7C7C7C7C7ULL},
  {(long long int)0xC8C8C8C8C8C8C8C8ULL,(long long int)0xC8C8C8C8C8C8C8C8ULL},{(long long int)0xC9C9C9C9C9C9C9C9ULL,(long long int)0xC9C9C9C9C9C9C9C9ULL},{(long long int)0xCACACACACACACACAULL,(long long int)0xCACACACACACACACAULL},{(long long int)0xCBCBCBCBCBCBCBCBULL,(long long int)0xCBCBCBCBCBCBCBCBULL},
  {(long long int)0xCCCCCCCCCCCCCCCCULL,(long long int)0xCCCCCCCCCCCCCCCCULL},{(long long int)0xCDCDCDCDCDCDCDCDULL,(long long int)0xCDCDCDCDCDCDCDCDULL},{(long long int)0xCECECECECECECECEULL,(long long int)0xCECECECECECECECEULL},{(long long int)0xCFCFCFCFCFCFCFCFULL,(long long int)0xCFCFCFCFCFCFCFCFULL},
  {(long long int)0xD0D0D0D0D0D0D0D0ULL,(long long int)0xD0D0D0D0D0D0D0D0ULL},{(long long int)0xD1D1D1D1D1D1D1D1ULL,(long long int)0xD1D1D1D1D1D1D1D1ULL},{(long long int)0xD2D2D2D2D2D2D2D2ULL,(long long int)0xD2D2D2D2D2D2D2D2ULL},{(long long int)0xD3D3D3D3D3D3D3D3ULL,(long long int)0xD3D3D3D3D3D3D3D3ULL},
  {(long long int)0xD4D4D4D4D4D4D4D4ULL,(long long int)0xD4D4D4D4D4D4D4D4ULL},{(long long int)0xD5D5D5D5D5D5D5D5ULL,(long long int)0xD5D5D5D5D5D5D5D5ULL},{(long long int)0xD6D6D6D6D6D6D6D6ULL,(long long int)0xD6D6D6D6D6D6D6D6ULL},{(long long int)0xD7D7D7D7D7D7D7D7ULL,(long long int)0xD7D7D7D7D7D7D7D7ULL},
  {(long long int)0xD8D8D8D8D8D8D8D8ULL,(long long int)0xD8D8D8D8D8D8D8D8ULL},{(long long int)0xD9D9D9D9D9D9D9D9ULL,(long long int)0xD9D9D9D9D9D9D9D9ULL},{(long long int)0xDADADADADADADADAULL,(long long int)0xDADADADADADADADAULL},{(long long int)0xDBDBDBDBDBDBDBDBULL,(long long int)0xDBDBDBDBDBDBDBDBULL},
  {(long long int)0xDCDCDCDCDCDCDCDCULL,(long long int)0xDCDCDCDCDCDCDCDCULL},{(long long int)0xDDDDDDDDDDDDDDDDULL,(long long int)0xDDDDDDDDDDDDDDDDULL},{(long long int)0xDEDEDEDEDEDEDEDEULL,(long long int)0xDEDEDEDEDEDEDEDEULL},{(long long int)0xDFDFDFDFDFDFDFDFULL,(long long int)0xDFDFDFDFDFDFDFDFULL},
  {(long long int)0xE0E0E0E0E0E0E0E0ULL,(long long int)0xE0E0E0E0E0E0E0E0ULL},{(long long int)0xE1E1E1E1E1E1E1E1ULL,(long long int)0xE1E1E1E1E1E1E1E1ULL},{(long long int)0xE2E2E2E2E2E2E2E2ULL,(long long int)0xE2E2E2E2E2E2E2E2ULL},{(long long int)0xE3E3E3E3E3E3E3E3ULL,(long long int)0xE3E3E3E3E3E3E3E3ULL},
  {(long long int)0xE4E4E4E4E4E4E4E4ULL,(long long int)0xE4E4E4E4E4E4E4E4ULL},{(long long int)0xE5E5E5E5E5E5E5E5ULL,(long long int)0xE5E5E5E5E5E5E5E5ULL},{(long long int)0xE6E6E6E6E6E6E6E6ULL,(long long int)0xE6E6E6E6E6E6E6E6ULL},{(long long int)0xE7E7E7E7E7E7E7E7ULL,(long long int)0xE7E7E7E7E7E7E7E7ULL},
  {(long long int)0xE8E8E8E8E8E8E8E8ULL,(long long int)0xE8E8E8E8E8E8E8E8ULL},{(long long int)0xE9E9E9E9E9E9E9E9ULL,(long long int)0xE9E9E9E9E9E9E9E9ULL},{(long long int)0xEAEAEAEAEAEAEAEAULL,(long long int)0xEAEAEAEAEAEAEAEAULL},{(long long int)0xEBEBEBEBEBEBEBEBULL,(long long int)0xEBEBEBEBEBEBEBEBULL},
  {(long long int)0xECECECECECECECECULL,(long long int)0xECECECECECECECECULL},{(long long int)0xEDEDEDEDEDEDEDEDULL,(long long int)0xEDEDEDEDEDEDEDEDULL},{(long long int)0xEEEEEEEEEEEEEEEEULL,(long long int)0xEEEEEEEEEEEEEEEEULL},{(long long int)0xEFEFEFEFEFEFEFEFULL,(long long int)0xEFEFEFEFEFEFEFEFULL},
  {(long long int)0xF0F0F0F0F0F0F0F0ULL,(long long int)0xF0F0F0F0F0F0F0F0ULL},{(long long int)0xF1F1F1F1F1F1F1F1ULL,(long long int)0xF1F1F1F1F1F1F1F1ULL},{(long long int)0xF2F2F2F2F2F2F2F2ULL,(long long int)0xF2F2F2F2F2F2F2F2ULL},{(long long int)0xF3F3F3F3F3F3F3F3ULL,(long long int)0xF3F3F3F3F3F3F3F3ULL},
  {(long long int)0xF4F4F4F4F4F4F4F4ULL,(long long int)0xF4F4F4F4F4F4F4F4ULL},{(long long int)0xF5F5F5F5F5F5F5F5ULL,(long long int)0xF5F5F5F5F5F5F5F5ULL},{(long long int)0xF6F6F6F6F6F6F6F6ULL,(long long int)0xF6F6F6F6F6F6F6F6ULL},{(long long int)0xF7F7F7F7F7F7F7F7ULL,(long long int)0xF7F7F7F7F7F7F7F7ULL},
  {(long long int)0xF8F8F8F8F8F8F8F8ULL,(long long int)0xF8F8F8F8F8F8F8F8ULL},{(long long int)0xF9F9F9F9F9F9F9F9ULL,(long long int)0xF9F9F9F9F9F9F9F9ULL},{(long long int)0xFAFAFAFAFAFAFAFAULL,(long long int)0xFAFAFAFAFAFAFAFAULL},{(long long int)0xFBFBFBFBFBFBFBFBULL,(long long int)0xFBFBFBFBFBFBFBFBULL},
  {(long long int)0xFCFCFCFCFCFCFCFCULL,(long long int)0xFCFCFCFCFCFCFCFCULL},{(long long int)0xFDFDFDFDFDFDFDFDULL,(long long int)0xFDFDFDFDFDFDFDFDULL},{(long long int)0xFEFEFEFEFEFEFEFEULL,(long long int)0xFEFEFEFEFEFEFEFEULL},{(long long int)0xFFFFFFFFFFFFFFFFULL,(long long int)0xFFFFFFFFFFFFFFFFULL},
#endif
};

struct group15
{
  static constexpr int N=15;
  
  inline void set(std::size_t pos,std::size_t hash)
  {
    assert(pos<N);
    reinterpret_cast<unsigned char*>(&mask)[pos]=adjust_hash(hash);
  }

  inline void set_sentinel()
  {
    reinterpret_cast<unsigned char*>(&mask)[N-1]=0x01; // occupied
  }

  inline bool is_sentinel(std::size_t pos)const
  {
    return reinterpret_cast<const unsigned char*>(&mask)[pos]==0x01;
  }

  inline void reset(std::size_t pos)
  {
    assert(pos<N);
    reinterpret_cast<unsigned char*>(&mask)[pos]=0u;
  }

  static void reset(unsigned char* pc)
  {
    *pc=0u;
  }

  inline int match(std::size_t hash)const
  {
    //auto m=_mm_set1_epi8(adjust_hash(hash));
    //return _mm_movemask_epi8(_mm_cmpeq_epi8(mask,m))&0x7FFF;
    return _mm_movemask_epi8(_mm_cmpeq_epi8(mask,match_table[(unsigned char)hash]))&0x7FFF;
  }

  inline auto is_not_overflowed(std::size_t hash)const
  {
#if defined(BOOST_MSVC)
    return BOOST_LIKELY(!overflow())||!(overflow()&(1u<<(hash%8)));
#else
    return !(overflow()&(1u<<(hash%8)));
#endif
  }

  inline void mark_overflow(std::size_t hash)
  {
    overflow()|=1u<<(hash%8);
  }

  inline int match_available()const
  {
    return _mm_movemask_epi8(_mm_cmpeq_epi8(mask,_mm_setzero_si128()))&0x7FFF;
  }

  inline int match_occupied()const
  {
    return (~match_available())&0x7FFF;
  }

  inline int match_really_occupied()const // excluding sentinel
  {
    return reinterpret_cast<const unsigned char*>(&mask)[N-1]==0x01?
      match_occupied()&0x3FFF:match_occupied();
  }

private:
  inline static unsigned char adjust_hash(unsigned char hash)
  {
    return adjust_hash_table[hash];
  }

  inline unsigned char& overflow()
  {
    return reinterpret_cast<unsigned char*>(&mask)[N];
  }

  inline unsigned char overflow()const
  {
    return reinterpret_cast<const unsigned char*>(&mask)[N];
  }

  __m128i mask=_mm_setzero_si128();
};

#elif defined(__ARM_NEON)
// TODO: check for little endianness

struct group15
{
  static constexpr int N=15;
  
  inline void set(std::size_t pos,std::size_t hash)
  {
    assert(pos<N);
    reinterpret_cast<unsigned char*>(&mask)[pos]=adjust_hash(hash);
  }

  inline void set_sentinel()
  {
    reinterpret_cast<unsigned char*>(&mask)[N-1]=0x01; // occupied
  }

  inline bool is_sentinel(std::size_t pos)const
  {
    return pos==N-1&&
      reinterpret_cast<const unsigned char*>(&mask)[N-1]==0x01;
  }

  inline void reset(std::size_t pos)
  {
    assert(pos<N);
    reinterpret_cast<unsigned char*>(&mask)[pos]=0u;
  }

  static void reset(unsigned char* pc)
  {
    *pc=0u;
  }

  inline int match(std::size_t hash)const
  {
    auto m=vdupq_n_s8(adjust_hash(hash));
    return simde_mm_movemask_epi8(vceqq_s8(mask,m))&0x7FFF;
  }

  inline auto is_not_overflowed(std::size_t hash)const
  {
#if defined(BOOST_MSVC)
    return BOOST_LIKELY(!overflow())||!(overflow()&(1u<<(hash%8)));
#else
    return !(overflow()&(1u<<(hash%8)));
#endif
  }

  inline void mark_overflow(std::size_t hash)
  {
    overflow()|=1u<<(hash%8);
  }

  inline int match_available()const
  {
    return simde_mm_movemask_epi8(vceqq_s8(mask,vdupq_n_s8(0)))&0x7FFF;
  }

  inline int match_occupied()const
  {
    /* ~match_available() */
    return simde_mm_movemask_epi8(
      vcgtq_u8(vreinterpretq_u8_s8(mask),vdupq_n_u8(0)))&0x7FFF;
  }

  inline int match_really_occupied()const // excluding sentinel
  {
    return reinterpret_cast<const unsigned char*>(&mask)[N-1]==0x01?
      match_occupied()&0x3FFF:match_occupied();
  }

private:
  // https://github.com/simd-everywhere/simde/blob/e1bc968696e6533d6b0bf8dddb0614737c983479/simde/x86/sse2.h#L3755
  // (adapted)
  static inline int simde_mm_movemask_epi8(uint8x16_t a)
  {
    /* https://github.com/WebAssembly/simd/pull/201#issue-380682845 */
    static const uint8_t md[16] = {
    1 << 0, 1 << 1, 1 << 2, 1 << 3,
    1 << 4, 1 << 5, 1 << 6, 1 << 7,
    1 << 0, 1 << 1, 1 << 2, 1 << 3,
    1 << 4, 1 << 5, 1 << 6, 1 << 7,
    };

    /* Extend sign bit over entire lane */
    // we omit this op as lanes are 00/FF 
    // uint8x16_t extended = vreinterpretq_u8_s8(vshrq_n_s8(a, 7));
    /* Clear all but the bit we're interested in. */
    uint8x16_t masked = vandq_u8(vld1q_u8(md), a);
    /* Alternate bytes from low half and high half */
    uint8x8x2_t tmp = vzip_u8(vget_low_u8(masked), vget_high_u8(masked));
    uint16x8_t x = vreinterpretq_u16_u8(vcombine_u8(tmp.val[0], tmp.val[1]));
    return vaddvq_u16(x);
  }

  inline static unsigned char adjust_hash(unsigned char hash)
  {
    return adjust_hash_table[hash];
  }

  inline unsigned char& overflow()
  {
    return reinterpret_cast<unsigned char*>(&mask)[N];
  }

  inline unsigned char overflow()const
  {
    return reinterpret_cast<const unsigned char*>(&mask)[N];
  }

  int8x16_t mask=vdupq_n_s8(0);
};

#else

struct group15
{
  static constexpr int N=15;

  inline void set(std::size_t pos,std::size_t hash)
  {
    set_impl(pos,adjust_hash(hash));
  }

  inline void set_sentinel()
  {
    set_impl(N-1,1);
  }

  inline bool is_sentinel(std::size_t pos)const
  {
    return pos==N-1&&
           (mask[0] & uint64_t(0x4000400040004000ull))==uint64_t(0x4000ull)&&
           (mask[1] & uint64_t(0x4000400040004000ull))==0;
  }

  inline void reset(std::size_t pos)
  {
    set_impl(pos,0);
  }

  static void reset(unsigned char* pc)
  {
    std::size_t pos=reinterpret_cast<uintptr_t>(pc)%sizeof(group15);
    pc-=pos;
    reinterpret_cast<group15*>(pc)->reset(pos);
  }

  inline int match(std::size_t hash)const
  {
    return match_impl(adjust_hash(hash));
  }

  inline bool is_not_overflowed(std::size_t hash)const
  {
    return !(reinterpret_cast<const uint16_t*>(mask)[hash%8] & 0x8000u);
  }

  inline void mark_overflow(std::size_t hash)
  {
    reinterpret_cast<uint16_t*>(mask)[hash%8]|=0x8000u;
  }

  inline int match_available()const
  {
    auto x=~(mask[0]|mask[1]);
    uint32_t y=x&(x>>32);
    y&=y>>16;
    return y&0x7FFF;
  }

  inline int match_occupied()const
  {
    // ~match_available()
    auto x=mask[0]|mask[1];
    uint32_t y=x|(x>>32);
    y|=y>>16;
    return y&0x7FFF;
  }

  inline int match_really_occupied()const // excluding sentinel
  {
    return ~(match_impl(0)|match_impl(1))&0x7FFF;
  }

protected:
  inline static unsigned char adjust_hash(unsigned char hash)
  {
    return adjust_hash_table[hash];
  }

  inline void set_impl(std::size_t pos,std::size_t m)
  {
    uint64_ops::set(mask[0],pos,m&0xFu);
    uint64_ops::set(mask[1],pos,m>>4);
  }
  
  inline int match_impl(std::size_t m)const
  {
    return uint64_ops::match(mask[0],mask[1],m)&0x7FFF;
  }

  uint64_t mask[2]={0,0};
};

#endif /* FXA_UNORDERED_SSE2 */

template<typename T>
struct element
{
  T*       data(){return reinterpret_cast<T*>(&storage);}
  const T* data()const{return reinterpret_cast<const T*>(&storage);}
  T&       value(){return *data();}
  const T& value()const{return *data();}


private:
  std::aligned_storage_t<sizeof(T),alignof(T)> storage;
};

struct pow2_prober
{
  pow2_prober(std::size_t pos):pos{pos}{}

  std::size_t get()const{return pos;}

  bool next(std::size_t size)
  {
    step+=1;
    pos=(pos+step)&(size-1);
    return step<size;
  }

private:
  std::size_t pos,step=0;
};

struct nonpow2_prober
{
  nonpow2_prober(std::size_t pos):pos{pos}{}

  std::size_t get()const{return pos;}

  bool next(std::size_t size)
  {
    auto ceil = boost::core::bit_ceil(size);
    for(;;){
      step+=1;
      pos=(pos+step)&(ceil-1);
      if(pos<size)break;
    }
    return step<size;
  }

private:
  std::size_t pos,step=0;
};

inline int unchecked_countr_zero(unsigned int x)
{
#if !defined(USE_BOOST_CORE_COUNTR_ZERO)&&defined(_MSC_VER)&&!defined(__clang__)
  unsigned long r;
  _BitScanForward(&r,x);
  return (int)r;
#else
  FXA_ASSUME(x);
  return boost::core::countr_zero(x);
#endif
}

template<typename T>
inline const T& extract_key(const T& x){return x;}

template<class Key,class Value>
inline const Key& extract_key(const map_value_adaptor<Key,Value>& x)
{return x.first;}

template<typename T,typename Allocator>
void clean_without_destruction(std::vector<T,Allocator>& x)
{
  using vector_type=std::vector<T,Allocator>;
  using alloc_traits=std::allocator_traits<Allocator>;
  
  std::aligned_storage_t<sizeof(vector_type),alignof(vector_type)> storage;
  auto pv=::new (reinterpret_cast<vector_type*>(&storage))
    vector_type(std::move(x));
  auto a=pv->get_allocator();
  alloc_traits::deallocate(a,pv->data(),pv->capacity());
}

template<
  typename T,typename Hash=boost::hash<T>,typename Pred=std::equal_to<T>,
  typename Allocator=std::allocator<T>,
  typename Group=group15,
  typename SizePolicy=pow2_size,
  typename Prober=pow2_prober, // must match growing policy
  typename HashSplitPolicy=shift_hash<0>
>
class foa_unordered_rc_set 
{
  using size_policy=SizePolicy;
  using prober=Prober;
  using hash_split_policy=HashSplitPolicy;
  using alloc_traits=std::allocator_traits<Allocator>;
  using group_type=Group;
  using element_type=element<T>;
  static constexpr auto N=group_type::N;

public:
  using key_type=T;
  using value_type=T;
  using size_type=std::size_t;
  class const_iterator:public boost::iterator_facade<
    const_iterator,const value_type,boost::forward_traversal_tag>
  {
  public:
    const_iterator()=default;

  private:
    friend class foa_unordered_rc_set;
    friend class boost::iterator_core_access;

    const_iterator(const group_type* pg,std::size_t n,const element_type* pe):
      pc{reinterpret_cast<unsigned char*>(const_cast<group_type*>(pg))+n},
      pe{const_cast<element_type*>(pe)}
    {
      assert(pg==group());
      assert(n==offset());
    }

    group_type* group()const
    {
      return reinterpret_cast<group_type*>(
        reinterpret_cast<uintptr_t>(pc)/sizeof(group_type)*sizeof(group_type));
    }

    std::size_t offset()const
    {
      return static_cast<std::size_t>(
        pc-reinterpret_cast<unsigned char*>(group()));
    }

    std::size_t rebase()
    {
      std::size_t off=reinterpret_cast<uintptr_t>(pc)%sizeof(group_type);
      pc-=off;
      return off;
    }

    const value_type& dereference()const noexcept
    {
      return pe->value();
    }

    bool equal(const const_iterator& x)const noexcept
    {
      return pe==x.pe;
    }

    void increment()noexcept
    {
      std::size_t n0=rebase();

      auto mask=reinterpret_cast<group_type*>(pc)->match_occupied()&
                reset_first_bits(n0+1);
      if(!mask){
        do{
          pc+=sizeof(group_type);
          pe+=N;
        }
        while(!(mask=reinterpret_cast<group_type*>(pc)->match_occupied()));
      }

      auto n=unchecked_countr_zero((unsigned int)mask);
      if(BOOST_UNLIKELY(reinterpret_cast<group_type*>(pc)->is_sentinel(n))){
        pe=nullptr;
      }
      else{
        pc+=n;
        pe+=n-n0;
      }
    }

    unsigned char *pc=nullptr;
    element_type  *pe=nullptr;
  };
  using iterator=const_iterator;

  foa_unordered_rc_set()
  {
    groups.back().set_sentinel();
  }

  foa_unordered_rc_set(const foa_unordered_rc_set&)=default;
  foa_unordered_rc_set(foa_unordered_rc_set&&)=default;

  ~foa_unordered_rc_set()
  {
    if(!groups.empty()){
     for(auto first=begin(),last=end();first!=last;)erase(first++);
    }
  }
  
  const_iterator begin()const noexcept
  {
    auto pg=groups.data();
    const_iterator it{pg,0,elements.data()};
    if(pg&&!(pg->match_really_occupied()&0x1u))++it;
    return it;
  }
  
  const_iterator end()const noexcept
  {
    return {};
  }

  size_type size()const noexcept{return size_;};

  BOOST_FORCEINLINE auto insert(const T& x){return insert_impl(x);}
  BOOST_FORCEINLINE auto insert(T&& x){return insert_impl(std::move(x));}

  void erase(const_iterator pos)
  {
    destroy_element(pos.pe->data());
    group_type::reset(pos.pc);
    --size_;
  }

  template<typename Key>
  size_type erase(const Key& x)
  {
    auto it=find(x);
    if(it!=end()){
      erase(it);
      return 1;
    }
    else return 0;
  }
  
  template<typename Key>
//#if defined(BOOST_MSVC)
  BOOST_FORCEINLINE 
//#endif
  iterator find(const Key& x)const
  {
    auto   hash=h(x);
    return find_impl(
      x,
      position_for(hash_split_policy::long_hash(hash)),
      hash_split_policy::short_hash(hash));
  }

  void rehash(std::size_t nb)
  {
    std::size_t n=static_cast<std::size_t>(1.0f+static_cast<float>(nb)*mlf);
    if(n>ml)unchecked_reserve(n);
  }

  float max_load_factor()const{return mlf;}

private:
  // used only on unchecked_reserve
  foa_unordered_rc_set(std::size_t n,Allocator al):
    al{al},size_{n}
  {
    groups.back().set_sentinel(); // we should wrap groups in its own class for this
  }

  template<typename Value>
  void construct_element(Value&& x,value_type* p)
  {
    alloc_traits::construct(al,p,std::forward<Value>(x));
  }

  void destroy_element(value_type* p)
  {
    alloc_traits::destroy(al,p);
  }

  template<typename IntegralConstant>
  static void prefetch(const void* p,IntegralConstant)
  {
#if defined(BOOST_GCC)||defined(BOOST_CLANG)
    __builtin_prefetch((const char*)p,IntegralConstant::value);
#elif defined(FXA_UNORDERED_SSE2)
    if constexpr(!IntegralConstant::value){ /* read access*/
      _mm_prefetch((const char*)p,_MM_HINT_T0);
    }
    else{
      _m_prefetchw((const char*)p);
    }
#endif    
  }

  template<typename IntegralConstant=std::false_type>
  static void prefetch_elements(const element_type* pe,IntegralConstant rw={})
  {
    constexpr int cache_line=64;
    const char    *p0=reinterpret_cast<const char*>(pe),
                  *p1=p0+sizeof(element_type)*N/2;
    for(auto p=p0;p<p1;p+=cache_line)prefetch(p,rw);
  }

  std::size_t position_for(std::size_t hash)const
  {
    return size_policy::position(hash,group_size_index);
  }

  template<typename Key>
//#if defined(BOOST_MSVC)
  BOOST_FORCEINLINE 
//#endif
  iterator find_impl(
    const Key& x,std::size_t pos0,std::size_t short_hash)const
  {    
    for(prober pb(pos0);;){
      auto pos=pb.get();
      auto pg=groups.data()+pos;
      auto mask=pg->match(short_hash);
      if(mask){
        auto pe=elements.data()+pos*N;
#if BOOST_ARCH_ARM
        prefetch_elements(pe);
#else
        prefetch(pe,std::false_type{});
#endif
        do{
          auto n=unchecked_countr_zero((unsigned int)mask);
          if(BOOST_LIKELY(pred(x,pe[n].value()))){
            return {pg,(std::size_t)(n),pe+n};
          }
          mask&=mask-1;
        }while(mask);
      }
      if(BOOST_LIKELY(
        pg->is_not_overflowed(short_hash)||!pb.next(groups.size()))){
        return end();
      }
    }
  }

  template<typename Value>
  BOOST_FORCEINLINE std::pair<iterator,bool> insert_impl(Value&& x)
  {
    auto hash=h(x);
    auto long_hash=hash_split_policy::long_hash(hash);
    auto pos0=position_for(long_hash);
    auto short_hash=hash_split_policy::short_hash(hash);
    auto it=find_impl(extract_key(x),pos0,short_hash);

    if(it!=end()){
      return {it,false};
    }
    else if(BOOST_UNLIKELY(size_>=ml)){
      unchecked_reserve(size_+1);
      pos0=position_for(long_hash);
    }
    return {
      unchecked_insert(std::forward<Value>(x),pos0,short_hash),
      true
    };
  }

  BOOST_NOINLINE void unchecked_reserve(size_type new_size)
  {
    std::size_t nc =(std::numeric_limits<std::size_t>::max)();
    float       fnc=1.0f+static_cast<float>(new_size)/mlf;
    if(nc>fnc)nc=static_cast<std::size_t>(fnc);

    foa_unordered_rc_set new_container{nc,al};
    std::size_t          num_tx=0;
    try{
      for(std::size_t pos=0,last=groups.size();pos!=last;++pos){
        auto pg=groups.data()+pos;
        auto pe=elements.data()+pos*N;
        auto mask=pg->match_really_occupied();
        while(mask){
          auto n=unchecked_countr_zero((unsigned int)mask);
          auto& x=pe[(std::size_t)n];
          new_container.unchecked_insert(std::move(x.value()));
          destroy_element(x.data());
          ++num_tx;
          mask&=mask-1;
        }
      }
    }
    catch(...){
      size_-=num_tx;
      if(num_tx){
        for(auto pg=groups.data();;++pg){
          auto mask=pg->match_really_occupied();
          while(mask){
            auto n=unchecked_countr_zero((unsigned int)mask);
            pg->reset(n);
            if(!(--num_tx))goto end;
          }
        }
      }
    end:;
      throw;
    }
    group_size_index=new_container.group_size_index;
    clean_without_destruction(groups);
    groups=std::move(new_container.groups);
    clean_without_destruction(elements);
    elements=std::move(new_container.elements);
    ml=max_load();
  }

  template<typename Value>
  iterator unchecked_insert(Value&& x)
  {
    auto hash=h(x);
    return unchecked_insert(
      std::forward<Value>(x),
      position_for(hash_split_policy::long_hash(hash)),
      hash_split_policy::short_hash(hash));
  }

  template<typename Value>
  iterator unchecked_insert(
    Value&& x,std::size_t pos0,std::size_t short_hash)
  {
    auto [pos,n]=unchecked_insert_position(pos0,short_hash);
    auto pg=groups.data()+pos;
    auto pe=elements.data()+pos*N+n;
    construct_element(std::forward<Value>(x),pe->data());
    pg->set(n,short_hash);
    ++size_;
    return {pg,std::size_t(n),pe};
  }

  std::pair<std::size_t,std::size_t>
  unchecked_insert_position(std::size_t pos0,std::size_t short_hash)
  {
    for(prober pb(pos0);;pb.next(groups.size())){
      auto pos=pb.get();
      auto pg=groups.data()+pos;
      auto mask=pg->match_available();
      if(BOOST_LIKELY(mask)){
        return {pos,unchecked_countr_zero((unsigned int)mask)};
      }
      else pg->mark_overflow(short_hash);
    }
  }

  size_type max_load()const
  {
    float fml=mlf*static_cast<float>(size_policy::size(group_size_index)*N-1);
    auto res=(std::numeric_limits<size_type>::max)();
    if(res>fml)res=static_cast<size_type>(fml);
    return res;
  }  

  Hash                                     h;
  Pred                                     pred;
  Allocator                                al;
  float                                    mlf=0.875;
  std::size_t                              size_=0;
  std::size_t                              group_size_index=size_policy::size_index(size_/N+1);
  std::vector<
    group_type,
    typename alloc_traits::
      template rebind_alloc<group_type>>   groups{size_policy::size(group_size_index),al};
  std::vector<
    element_type,
    typename alloc_traits::
      template rebind_alloc<element_type>> elements{groups.size()*N,al};
  size_type                                ml=max_load();
};

template<
  typename Key,typename Value,
  typename Hash=boost::hash<Key>,typename Pred=std::equal_to<Key>,
  typename Allocator=std::allocator<map_value_adaptor<Key,Value>>,
  typename Group=group15,
  typename SizePolicy=pow2_size,
  typename Prober=pow2_prober,
  typename HashSplitPolicy=shift_hash<0>
>
using foa_unordered_rc_map=foa_unordered_rc_set<
  map_value_adaptor<Key,Value>,
  map_hash_adaptor<Hash>,map_pred_adaptor<Pred>,
  Allocator,Group,SizePolicy,Prober,HashSplitPolicy
>;

} // namespace rc

} // namespace fxa_unordered

using fxa_unordered::rc::foa_unordered_rc_set;
using fxa_unordered::rc::foa_unordered_rc_map;

#endif
