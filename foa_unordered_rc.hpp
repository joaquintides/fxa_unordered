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

#ifdef FXA_UNORDERED_SSE2

constexpr unsigned char adjust_hash(unsigned char hash)
{
  int x=int(hash)+2;
  return x^(x>>1);
}

struct group15
{
  static constexpr int N=15;
  
  inline void set(std::size_t pos,std::size_t hash)
  {
    assert(pos<N);
    reinterpret_cast<unsigned char*>(&mask)[pos]=adjust_hash_table[(unsigned char)hash];
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
    auto m=_mm_set1_epi8(adjust_hash_table[(unsigned char)hash]);
    return _mm_movemask_epi8(_mm_cmpeq_epi8(mask,m))&0x7FFF;
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
  static constexpr unsigned char adjust_hash_table[]={
    adjust_hash(0),adjust_hash(1),adjust_hash(2),adjust_hash(3),adjust_hash(4),adjust_hash(5),adjust_hash(6),adjust_hash(7),
    adjust_hash(8),adjust_hash(9),adjust_hash(10),adjust_hash(11),adjust_hash(12),adjust_hash(13),adjust_hash(14),adjust_hash(15),
    adjust_hash(16),adjust_hash(17),adjust_hash(18),adjust_hash(19),adjust_hash(20),adjust_hash(21),adjust_hash(22),adjust_hash(23),
    adjust_hash(24),adjust_hash(25),adjust_hash(26),adjust_hash(27),adjust_hash(28),adjust_hash(29),adjust_hash(30),adjust_hash(31),
    adjust_hash(32),adjust_hash(33),adjust_hash(34),adjust_hash(35),adjust_hash(36),adjust_hash(37),adjust_hash(38),adjust_hash(39),
    adjust_hash(40),adjust_hash(41),adjust_hash(42),adjust_hash(43),adjust_hash(44),adjust_hash(45),adjust_hash(46),adjust_hash(47),
    adjust_hash(48),adjust_hash(49),adjust_hash(50),adjust_hash(51),adjust_hash(52),adjust_hash(53),adjust_hash(54),adjust_hash(55),
    adjust_hash(56),adjust_hash(57),adjust_hash(58),adjust_hash(59),adjust_hash(60),adjust_hash(61),adjust_hash(62),adjust_hash(63),
    adjust_hash(64),adjust_hash(65),adjust_hash(66),adjust_hash(67),adjust_hash(68),adjust_hash(69),adjust_hash(70),adjust_hash(71),
    adjust_hash(72),adjust_hash(73),adjust_hash(74),adjust_hash(75),adjust_hash(76),adjust_hash(77),adjust_hash(78),adjust_hash(79),
    adjust_hash(80),adjust_hash(81),adjust_hash(82),adjust_hash(83),adjust_hash(84),adjust_hash(85),adjust_hash(86),adjust_hash(87),
    adjust_hash(88),adjust_hash(89),adjust_hash(90),adjust_hash(91),adjust_hash(92),adjust_hash(93),adjust_hash(94),adjust_hash(95),
    adjust_hash(96),adjust_hash(97),adjust_hash(98),adjust_hash(99),adjust_hash(100),adjust_hash(101),adjust_hash(102),adjust_hash(103),
    adjust_hash(104),adjust_hash(105),adjust_hash(106),adjust_hash(107),adjust_hash(108),adjust_hash(109),adjust_hash(110),adjust_hash(111),
    adjust_hash(112),adjust_hash(113),adjust_hash(114),adjust_hash(115),adjust_hash(116),adjust_hash(117),adjust_hash(118),adjust_hash(119),
    adjust_hash(120),adjust_hash(121),adjust_hash(122),adjust_hash(123),adjust_hash(124),adjust_hash(125),adjust_hash(126),adjust_hash(127),
    adjust_hash(128),adjust_hash(129),adjust_hash(130),adjust_hash(131),adjust_hash(132),adjust_hash(133),adjust_hash(134),adjust_hash(135),
    adjust_hash(136),adjust_hash(137),adjust_hash(138),adjust_hash(139),adjust_hash(140),adjust_hash(141),adjust_hash(142),adjust_hash(143),
    adjust_hash(144),adjust_hash(145),adjust_hash(146),adjust_hash(147),adjust_hash(148),adjust_hash(149),adjust_hash(150),adjust_hash(151),
    adjust_hash(152),adjust_hash(153),adjust_hash(154),adjust_hash(155),adjust_hash(156),adjust_hash(157),adjust_hash(158),adjust_hash(159),
    adjust_hash(160),adjust_hash(161),adjust_hash(162),adjust_hash(163),adjust_hash(164),adjust_hash(165),adjust_hash(166),adjust_hash(167),
    adjust_hash(168),adjust_hash(169),adjust_hash(170),adjust_hash(171),adjust_hash(172),adjust_hash(173),adjust_hash(174),adjust_hash(175),
    adjust_hash(176),adjust_hash(177),adjust_hash(178),adjust_hash(179),adjust_hash(180),adjust_hash(181),adjust_hash(182),adjust_hash(183),
    adjust_hash(184),adjust_hash(185),adjust_hash(186),adjust_hash(187),adjust_hash(188),adjust_hash(189),adjust_hash(190),adjust_hash(191),
    adjust_hash(192),adjust_hash(193),adjust_hash(194),adjust_hash(195),adjust_hash(196),adjust_hash(197),adjust_hash(198),adjust_hash(199),
    adjust_hash(200),adjust_hash(201),adjust_hash(202),adjust_hash(203),adjust_hash(204),adjust_hash(205),adjust_hash(206),adjust_hash(207),
    adjust_hash(208),adjust_hash(209),adjust_hash(210),adjust_hash(211),adjust_hash(212),adjust_hash(213),adjust_hash(214),adjust_hash(215),
    adjust_hash(216),adjust_hash(217),adjust_hash(218),adjust_hash(219),adjust_hash(220),adjust_hash(221),adjust_hash(222),adjust_hash(223),
    adjust_hash(224),adjust_hash(225),adjust_hash(226),adjust_hash(227),adjust_hash(228),adjust_hash(229),adjust_hash(230),adjust_hash(231),
    adjust_hash(232),adjust_hash(233),adjust_hash(234),adjust_hash(235),adjust_hash(236),adjust_hash(237),adjust_hash(238),adjust_hash(239),
    adjust_hash(240),adjust_hash(241),adjust_hash(242),adjust_hash(243),adjust_hash(244),adjust_hash(245),adjust_hash(246),adjust_hash(247),
    adjust_hash(248),adjust_hash(249),adjust_hash(250),adjust_hash(251),adjust_hash(252),adjust_hash(253),adjust_hash(254),adjust_hash(255),
  };

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
    int x=int(hash)+2;
    return x^(x>>1);
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
    int x=int(hash)+2;
    return x^(x>>1);
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
#if 1
      std::size_t n0=reinterpret_cast<uintptr_t>(pc)%sizeof(group_type);

      auto mask=reinterpret_cast<group_type*>(pc-n0)->match_occupied()&
                reset_first_bits(n0+1);
      if(!mask){
        do{
          pc+=sizeof(group_type);
          pe+=N;
        }
        while(!(mask=reinterpret_cast<group_type*>(pc-n0)->match_occupied()));
      }

      auto n=unchecked_countr_zero((unsigned int)mask);
      if(BOOST_UNLIKELY(reinterpret_cast<group_type*>(pc-n0)->is_sentinel(n))){
        pe=nullptr;
      }
      else{
        pc+=n-n0;
        pe+=n-n0;
      }
#else
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
#endif
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
