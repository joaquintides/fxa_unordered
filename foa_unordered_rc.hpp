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
  {0x02020202ULL,0x02020202ULL},{0x03030303ULL,0x03030303ULL},{0x02020202ULL,0x02020202ULL},{0x03030303ULL,0x03030303ULL},
  {0x04040404ULL,0x04040404ULL},{0x05050505ULL,0x05050505ULL},{0x06060606ULL,0x06060606ULL},{0x07070707ULL,0x07070707ULL},
  {0x08080808ULL,0x08080808ULL},{0x09090909ULL,0x09090909ULL},{0x0A0A0A0AULL,0x0A0A0A0AULL},{0x0B0B0B0BULL,0x0B0B0B0BULL},
  {0x0C0C0C0CULL,0x0C0C0C0CULL},{0x0D0D0D0DULL,0x0D0D0D0DULL},{0x0E0E0E0EULL,0x0E0E0E0EULL},{0x0F0F0F0FULL,0x0F0F0F0FULL},
  {0x10101010ULL,0x10101010ULL},{0x11111111ULL,0x11111111ULL},{0x12121212ULL,0x12121212ULL},{0x13131313ULL,0x13131313ULL},
  {0x14141414ULL,0x14141414ULL},{0x15151515ULL,0x15151515ULL},{0x16161616ULL,0x16161616ULL},{0x17171717ULL,0x17171717ULL},
  {0x18181818ULL,0x18181818ULL},{0x19191919ULL,0x19191919ULL},{0x1A1A1A1AULL,0x1A1A1A1AULL},{0x1B1B1B1BULL,0x1B1B1B1BULL},
  {0x1C1C1C1CULL,0x1C1C1C1CULL},{0x1D1D1D1DULL,0x1D1D1D1DULL},{0x1E1E1E1EULL,0x1E1E1E1EULL},{0x1F1F1F1FULL,0x1F1F1F1FULL},
  {0x20202020ULL,0x20202020ULL},{0x21212121ULL,0x21212121ULL},{0x22222222ULL,0x22222222ULL},{0x23232323ULL,0x23232323ULL},
  {0x24242424ULL,0x24242424ULL},{0x25252525ULL,0x25252525ULL},{0x26262626ULL,0x26262626ULL},{0x27272727ULL,0x27272727ULL},
  {0x28282828ULL,0x28282828ULL},{0x29292929ULL,0x29292929ULL},{0x2A2A2A2AULL,0x2A2A2A2AULL},{0x2B2B2B2BULL,0x2B2B2B2BULL},
  {0x2C2C2C2CULL,0x2C2C2C2CULL},{0x2D2D2D2DULL,0x2D2D2D2DULL},{0x2E2E2E2EULL,0x2E2E2E2EULL},{0x2F2F2F2FULL,0x2F2F2F2FULL},
  {0x30303030ULL,0x30303030ULL},{0x31313131ULL,0x31313131ULL},{0x32323232ULL,0x32323232ULL},{0x33333333ULL,0x33333333ULL},
  {0x34343434ULL,0x34343434ULL},{0x35353535ULL,0x35353535ULL},{0x36363636ULL,0x36363636ULL},{0x37373737ULL,0x37373737ULL},
  {0x38383838ULL,0x38383838ULL},{0x39393939ULL,0x39393939ULL},{0x3A3A3A3AULL,0x3A3A3A3AULL},{0x3B3B3B3BULL,0x3B3B3B3BULL},
  {0x3C3C3C3CULL,0x3C3C3C3CULL},{0x3D3D3D3DULL,0x3D3D3D3DULL},{0x3E3E3E3EULL,0x3E3E3E3EULL},{0x3F3F3F3FULL,0x3F3F3F3FULL},
  {0x40404040ULL,0x40404040ULL},{0x41414141ULL,0x41414141ULL},{0x42424242ULL,0x42424242ULL},{0x43434343ULL,0x43434343ULL},
  {0x44444444ULL,0x44444444ULL},{0x45454545ULL,0x45454545ULL},{0x46464646ULL,0x46464646ULL},{0x47474747ULL,0x47474747ULL},
  {0x48484848ULL,0x48484848ULL},{0x49494949ULL,0x49494949ULL},{0x4A4A4A4AULL,0x4A4A4A4AULL},{0x4B4B4B4BULL,0x4B4B4B4BULL},
  {0x4C4C4C4CULL,0x4C4C4C4CULL},{0x4D4D4D4DULL,0x4D4D4D4DULL},{0x4E4E4E4EULL,0x4E4E4E4EULL},{0x4F4F4F4FULL,0x4F4F4F4FULL},
  {0x50505050ULL,0x50505050ULL},{0x51515151ULL,0x51515151ULL},{0x52525252ULL,0x52525252ULL},{0x53535353ULL,0x53535353ULL},
  {0x54545454ULL,0x54545454ULL},{0x55555555ULL,0x55555555ULL},{0x56565656ULL,0x56565656ULL},{0x57575757ULL,0x57575757ULL},
  {0x58585858ULL,0x58585858ULL},{0x59595959ULL,0x59595959ULL},{0x5A5A5A5AULL,0x5A5A5A5AULL},{0x5B5B5B5BULL,0x5B5B5B5BULL},
  {0x5C5C5C5CULL,0x5C5C5C5CULL},{0x5D5D5D5DULL,0x5D5D5D5DULL},{0x5E5E5E5EULL,0x5E5E5E5EULL},{0x5F5F5F5FULL,0x5F5F5F5FULL},
  {0x60606060ULL,0x60606060ULL},{0x61616161ULL,0x61616161ULL},{0x62626262ULL,0x62626262ULL},{0x63636363ULL,0x63636363ULL},
  {0x64646464ULL,0x64646464ULL},{0x65656565ULL,0x65656565ULL},{0x66666666ULL,0x66666666ULL},{0x67676767ULL,0x67676767ULL},
  {0x68686868ULL,0x68686868ULL},{0x69696969ULL,0x69696969ULL},{0x6A6A6A6AULL,0x6A6A6A6AULL},{0x6B6B6B6BULL,0x6B6B6B6BULL},
  {0x6C6C6C6CULL,0x6C6C6C6CULL},{0x6D6D6D6DULL,0x6D6D6D6DULL},{0x6E6E6E6EULL,0x6E6E6E6EULL},{0x6F6F6F6FULL,0x6F6F6F6FULL},
  {0x70707070ULL,0x70707070ULL},{0x71717171ULL,0x71717171ULL},{0x72727272ULL,0x72727272ULL},{0x73737373ULL,0x73737373ULL},
  {0x74747474ULL,0x74747474ULL},{0x75757575ULL,0x75757575ULL},{0x76767676ULL,0x76767676ULL},{0x77777777ULL,0x77777777ULL},
  {0x78787878ULL,0x78787878ULL},{0x79797979ULL,0x79797979ULL},{0x7A7A7A7AULL,0x7A7A7A7AULL},{0x7B7B7B7BULL,0x7B7B7B7BULL},
  {0x7C7C7C7CULL,0x7C7C7C7CULL},{0x7D7D7D7DULL,0x7D7D7D7DULL},{0x7E7E7E7EULL,0x7E7E7E7EULL},{0x7F7F7F7FULL,0x7F7F7F7FULL},
  {0x80808080ULL,0x80808080ULL},{0x81818181ULL,0x81818181ULL},{0x82828282ULL,0x82828282ULL},{0x83838383ULL,0x83838383ULL},
  {0x84848484ULL,0x84848484ULL},{0x85858585ULL,0x85858585ULL},{0x86868686ULL,0x86868686ULL},{0x87878787ULL,0x87878787ULL},
  {0x88888888ULL,0x88888888ULL},{0x89898989ULL,0x89898989ULL},{0x8A8A8A8AULL,0x8A8A8A8AULL},{0x8B8B8B8BULL,0x8B8B8B8BULL},
  {0x8C8C8C8CULL,0x8C8C8C8CULL},{0x8D8D8D8DULL,0x8D8D8D8DULL},{0x8E8E8E8EULL,0x8E8E8E8EULL},{0x8F8F8F8FULL,0x8F8F8F8FULL},
  {0x90909090ULL,0x90909090ULL},{0x91919191ULL,0x91919191ULL},{0x92929292ULL,0x92929292ULL},{0x93939393ULL,0x93939393ULL},
  {0x94949494ULL,0x94949494ULL},{0x95959595ULL,0x95959595ULL},{0x96969696ULL,0x96969696ULL},{0x97979797ULL,0x97979797ULL},
  {0x98989898ULL,0x98989898ULL},{0x99999999ULL,0x99999999ULL},{0x9A9A9A9AULL,0x9A9A9A9AULL},{0x9B9B9B9BULL,0x9B9B9B9BULL},
  {0x9C9C9C9CULL,0x9C9C9C9CULL},{0x9D9D9D9DULL,0x9D9D9D9DULL},{0x9E9E9E9EULL,0x9E9E9E9EULL},{0x9F9F9F9FULL,0x9F9F9F9FULL},
  {0xA0A0A0A0ULL,0xA0A0A0A0ULL},{0xA1A1A1A1ULL,0xA1A1A1A1ULL},{0xA2A2A2A2ULL,0xA2A2A2A2ULL},{0xA3A3A3A3ULL,0xA3A3A3A3ULL},
  {0xA4A4A4A4ULL,0xA4A4A4A4ULL},{0xA5A5A5A5ULL,0xA5A5A5A5ULL},{0xA6A6A6A6ULL,0xA6A6A6A6ULL},{0xA7A7A7A7ULL,0xA7A7A7A7ULL},
  {0xA8A8A8A8ULL,0xA8A8A8A8ULL},{0xA9A9A9A9ULL,0xA9A9A9A9ULL},{0xAAAAAAAAULL,0xAAAAAAAAULL},{0xABABABABULL,0xABABABABULL},
  {0xACACACACULL,0xACACACACULL},{0xADADADADULL,0xADADADADULL},{0xAEAEAEAEULL,0xAEAEAEAEULL},{0xAFAFAFAFULL,0xAFAFAFAFULL},
  {0xB0B0B0B0ULL,0xB0B0B0B0ULL},{0xB1B1B1B1ULL,0xB1B1B1B1ULL},{0xB2B2B2B2ULL,0xB2B2B2B2ULL},{0xB3B3B3B3ULL,0xB3B3B3B3ULL},
  {0xB4B4B4B4ULL,0xB4B4B4B4ULL},{0xB5B5B5B5ULL,0xB5B5B5B5ULL},{0xB6B6B6B6ULL,0xB6B6B6B6ULL},{0xB7B7B7B7ULL,0xB7B7B7B7ULL},
  {0xB8B8B8B8ULL,0xB8B8B8B8ULL},{0xB9B9B9B9ULL,0xB9B9B9B9ULL},{0xBABABABAULL,0xBABABABAULL},{0xBBBBBBBBULL,0xBBBBBBBBULL},
  {0xBCBCBCBCULL,0xBCBCBCBCULL},{0xBDBDBDBDULL,0xBDBDBDBDULL},{0xBEBEBEBEULL,0xBEBEBEBEULL},{0xBFBFBFBFULL,0xBFBFBFBFULL},
  {0xC0C0C0C0ULL,0xC0C0C0C0ULL},{0xC1C1C1C1ULL,0xC1C1C1C1ULL},{0xC2C2C2C2ULL,0xC2C2C2C2ULL},{0xC3C3C3C3ULL,0xC3C3C3C3ULL},
  {0xC4C4C4C4ULL,0xC4C4C4C4ULL},{0xC5C5C5C5ULL,0xC5C5C5C5ULL},{0xC6C6C6C6ULL,0xC6C6C6C6ULL},{0xC7C7C7C7ULL,0xC7C7C7C7ULL},
  {0xC8C8C8C8ULL,0xC8C8C8C8ULL},{0xC9C9C9C9ULL,0xC9C9C9C9ULL},{0xCACACACAULL,0xCACACACAULL},{0xCBCBCBCBULL,0xCBCBCBCBULL},
  {0xCCCCCCCCULL,0xCCCCCCCCULL},{0xCDCDCDCDULL,0xCDCDCDCDULL},{0xCECECECEULL,0xCECECECEULL},{0xCFCFCFCFULL,0xCFCFCFCFULL},
  {0xD0D0D0D0ULL,0xD0D0D0D0ULL},{0xD1D1D1D1ULL,0xD1D1D1D1ULL},{0xD2D2D2D2ULL,0xD2D2D2D2ULL},{0xD3D3D3D3ULL,0xD3D3D3D3ULL},
  {0xD4D4D4D4ULL,0xD4D4D4D4ULL},{0xD5D5D5D5ULL,0xD5D5D5D5ULL},{0xD6D6D6D6ULL,0xD6D6D6D6ULL},{0xD7D7D7D7ULL,0xD7D7D7D7ULL},
  {0xD8D8D8D8ULL,0xD8D8D8D8ULL},{0xD9D9D9D9ULL,0xD9D9D9D9ULL},{0xDADADADAULL,0xDADADADAULL},{0xDBDBDBDBULL,0xDBDBDBDBULL},
  {0xDCDCDCDCULL,0xDCDCDCDCULL},{0xDDDDDDDDULL,0xDDDDDDDDULL},{0xDEDEDEDEULL,0xDEDEDEDEULL},{0xDFDFDFDFULL,0xDFDFDFDFULL},
  {0xE0E0E0E0ULL,0xE0E0E0E0ULL},{0xE1E1E1E1ULL,0xE1E1E1E1ULL},{0xE2E2E2E2ULL,0xE2E2E2E2ULL},{0xE3E3E3E3ULL,0xE3E3E3E3ULL},
  {0xE4E4E4E4ULL,0xE4E4E4E4ULL},{0xE5E5E5E5ULL,0xE5E5E5E5ULL},{0xE6E6E6E6ULL,0xE6E6E6E6ULL},{0xE7E7E7E7ULL,0xE7E7E7E7ULL},
  {0xE8E8E8E8ULL,0xE8E8E8E8ULL},{0xE9E9E9E9ULL,0xE9E9E9E9ULL},{0xEAEAEAEAULL,0xEAEAEAEAULL},{0xEBEBEBEBULL,0xEBEBEBEBULL},
  {0xECECECECULL,0xECECECECULL},{0xEDEDEDEDULL,0xEDEDEDEDULL},{0xEEEEEEEEULL,0xEEEEEEEEULL},{0xEFEFEFEFULL,0xEFEFEFEFULL},
  {0xF0F0F0F0ULL,0xF0F0F0F0ULL},{0xF1F1F1F1ULL,0xF1F1F1F1ULL},{0xF2F2F2F2ULL,0xF2F2F2F2ULL},{0xF3F3F3F3ULL,0xF3F3F3F3ULL},
  {0xF4F4F4F4ULL,0xF4F4F4F4ULL},{0xF5F5F5F5ULL,0xF5F5F5F5ULL},{0xF6F6F6F6ULL,0xF6F6F6F6ULL},{0xF7F7F7F7ULL,0xF7F7F7F7ULL},
  {0xF8F8F8F8ULL,0xF8F8F8F8ULL},{0xF9F9F9F9ULL,0xF9F9F9F9ULL},{0xFAFAFAFAULL,0xFAFAFAFAULL},{0xFBFBFBFBULL,0xFBFBFBFBULL},
  {0xFCFCFCFCULL,0xFCFCFCFCULL},{0xFDFDFDFDULL,0xFDFDFDFDULL},{0xFEFEFEFEULL,0xFEFEFEFEULL},{0xFFFFFFFFULL,0xFFFFFFFFULL},
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
