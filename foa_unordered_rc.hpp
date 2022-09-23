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

constexpr unsigned char overflow_table[]={1u,2u,4u,8u,16u,32u,64u,128u};

#ifdef FXA_UNORDERED_SSE2

constexpr uint32_t match_table[]=
{
  0x02020202u,0x03030303u,0x02020202u,0x03030303u,0x04040404u,0x05050505u,0x06060606u,0x07070707u,
  0x08080808u,0x09090909u,0x0A0A0A0Au,0x0B0B0B0Bu,0x0C0C0C0Cu,0x0D0D0D0Du,0x0E0E0E0Eu,0x0F0F0F0Fu,
  0x10101010u,0x11111111u,0x12121212u,0x13131313u,0x14141414u,0x15151515u,0x16161616u,0x17171717u,
  0x18181818u,0x19191919u,0x1A1A1A1Au,0x1B1B1B1Bu,0x1C1C1C1Cu,0x1D1D1D1Du,0x1E1E1E1Eu,0x1F1F1F1Fu,
  0x20202020u,0x21212121u,0x22222222u,0x23232323u,0x24242424u,0x25252525u,0x26262626u,0x27272727u,
  0x28282828u,0x29292929u,0x2A2A2A2Au,0x2B2B2B2Bu,0x2C2C2C2Cu,0x2D2D2D2Du,0x2E2E2E2Eu,0x2F2F2F2Fu,
  0x30303030u,0x31313131u,0x32323232u,0x33333333u,0x34343434u,0x35353535u,0x36363636u,0x37373737u,
  0x38383838u,0x39393939u,0x3A3A3A3Au,0x3B3B3B3Bu,0x3C3C3C3Cu,0x3D3D3D3Du,0x3E3E3E3Eu,0x3F3F3F3Fu,
  0x40404040u,0x41414141u,0x42424242u,0x43434343u,0x44444444u,0x45454545u,0x46464646u,0x47474747u,
  0x48484848u,0x49494949u,0x4A4A4A4Au,0x4B4B4B4Bu,0x4C4C4C4Cu,0x4D4D4D4Du,0x4E4E4E4Eu,0x4F4F4F4Fu,
  0x50505050u,0x51515151u,0x52525252u,0x53535353u,0x54545454u,0x55555555u,0x56565656u,0x57575757u,
  0x58585858u,0x59595959u,0x5A5A5A5Au,0x5B5B5B5Bu,0x5C5C5C5Cu,0x5D5D5D5Du,0x5E5E5E5Eu,0x5F5F5F5Fu,
  0x60606060u,0x61616161u,0x62626262u,0x63636363u,0x64646464u,0x65656565u,0x66666666u,0x67676767u,
  0x68686868u,0x69696969u,0x6A6A6A6Au,0x6B6B6B6Bu,0x6C6C6C6Cu,0x6D6D6D6Du,0x6E6E6E6Eu,0x6F6F6F6Fu,
  0x70707070u,0x71717171u,0x72727272u,0x73737373u,0x74747474u,0x75757575u,0x76767676u,0x77777777u,
  0x78787878u,0x79797979u,0x7A7A7A7Au,0x7B7B7B7Bu,0x7C7C7C7Cu,0x7D7D7D7Du,0x7E7E7E7Eu,0x7F7F7F7Fu,
  0x80808080u,0x81818181u,0x82828282u,0x83838383u,0x84848484u,0x85858585u,0x86868686u,0x87878787u,
  0x88888888u,0x89898989u,0x8A8A8A8Au,0x8B8B8B8Bu,0x8C8C8C8Cu,0x8D8D8D8Du,0x8E8E8E8Eu,0x8F8F8F8Fu,
  0x90909090u,0x91919191u,0x92929292u,0x93939393u,0x94949494u,0x95959595u,0x96969696u,0x97979797u,
  0x98989898u,0x99999999u,0x9A9A9A9Au,0x9B9B9B9Bu,0x9C9C9C9Cu,0x9D9D9D9Du,0x9E9E9E9Eu,0x9F9F9F9Fu,
  0xA0A0A0A0u,0xA1A1A1A1u,0xA2A2A2A2u,0xA3A3A3A3u,0xA4A4A4A4u,0xA5A5A5A5u,0xA6A6A6A6u,0xA7A7A7A7u,
  0xA8A8A8A8u,0xA9A9A9A9u,0xAAAAAAAAu,0xABABABABu,0xACACACACu,0xADADADADu,0xAEAEAEAEu,0xAFAFAFAFu,
  0xB0B0B0B0u,0xB1B1B1B1u,0xB2B2B2B2u,0xB3B3B3B3u,0xB4B4B4B4u,0xB5B5B5B5u,0xB6B6B6B6u,0xB7B7B7B7u,
  0xB8B8B8B8u,0xB9B9B9B9u,0xBABABABAu,0xBBBBBBBBu,0xBCBCBCBCu,0xBDBDBDBDu,0xBEBEBEBEu,0xBFBFBFBFu,
  0xC0C0C0C0u,0xC1C1C1C1u,0xC2C2C2C2u,0xC3C3C3C3u,0xC4C4C4C4u,0xC5C5C5C5u,0xC6C6C6C6u,0xC7C7C7C7u,
  0xC8C8C8C8u,0xC9C9C9C9u,0xCACACACAu,0xCBCBCBCBu,0xCCCCCCCCu,0xCDCDCDCDu,0xCECECECEu,0xCFCFCFCFu,
  0xD0D0D0D0u,0xD1D1D1D1u,0xD2D2D2D2u,0xD3D3D3D3u,0xD4D4D4D4u,0xD5D5D5D5u,0xD6D6D6D6u,0xD7D7D7D7u,
  0xD8D8D8D8u,0xD9D9D9D9u,0xDADADADAu,0xDBDBDBDBu,0xDCDCDCDCu,0xDDDDDDDDu,0xDEDEDEDEu,0xDFDFDFDFu,
  0xE0E0E0E0u,0xE1E1E1E1u,0xE2E2E2E2u,0xE3E3E3E3u,0xE4E4E4E4u,0xE5E5E5E5u,0xE6E6E6E6u,0xE7E7E7E7u,
  0xE8E8E8E8u,0xE9E9E9E9u,0xEAEAEAEAu,0xEBEBEBEBu,0xECECECECu,0xEDEDEDEDu,0xEEEEEEEEu,0xEFEFEFEFu,
  0xF0F0F0F0u,0xF1F1F1F1u,0xF2F2F2F2u,0xF3F3F3F3u,0xF4F4F4F4u,0xF5F5F5F5u,0xF6F6F6F6u,0xF7F7F7F7u,
  0xF8F8F8F8u,0xF9F9F9F9u,0xFAFAFAFAu,0xFBFBFBFBu,0xFCFCFCFCu,0xFDFDFDFDu,0xFEFEFEFEu,0xFFFFFFFFu,
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
    auto m=_mm_set1_epi32((int)match_table[(unsigned char)hash]);
    return _mm_movemask_epi8(_mm_cmpeq_epi8(mask,m))&0x7FFF;
  }

  inline auto is_not_overflowed(std::size_t hash)const
  {
    return !(overflow()&overflow_table[hash%8]);
  }

  inline void mark_overflow(std::size_t hash)
  {
    overflow()|=1<<(hash%8);
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
    return (unsigned char)match_table[hash];
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
    return !(overflow()&overflow_table[hash%8]);
  }

  inline void mark_overflow(std::size_t hash)
  {
    overflow()|=1<<(hash%8);
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

      auto mask=(reinterpret_cast<group_type*>(pc)->match_occupied()>>(n0+1))<<(n0+1);
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

  foa_unordered_rc_set(std::size_t n):foa_unordered_rc_set()
  {
    this->rehash(n);
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
     prober pb(pos0);
    do{
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
      if(BOOST_LIKELY(pg->is_not_overflowed(short_hash))){
        return end();
      }
    }
    while(BOOST_LIKELY(pb.next(groups.size())));
    return end();
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
