/* Proof of concept of closed- and open-addressing
 * unordered associative containers.
 *
 * Copyright 2022 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 */
 
#ifndef FXA_COMMON_HPP
#define FXA_COMMON_HPP

#include <algorithm>
#include <boost/config.hpp>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include "fastrange.h"

#if defined(_MSC_VER)
# include <intrin.h>
#endif

#if defined(__SSE2__) || \
    defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2)
# define FXA_UNORDERED_SSE2
# include <emmintrin.h>
#endif

// ripped from
// https://github.com/abseil/abseil-cpp/blob/master/absl/base/optimization.h
#ifdef __has_builtin
#define FXA_HAVE_BUILTIN(x) __has_builtin(x)
#else
#define FXA_HAVE_BUILTIN(x) 0
#endif

#if !defined(NDEBUG)
#define FXA_ASSUME(cond) assert(cond)
#elif FXA_HAVE_BUILTIN(__builtin_assume)
#define FXA_ASSUME(cond) __builtin_assume(cond)
#elif defined(__GNUC__) || FXA_HAVE_BUILTIN(__builtin_unreachable)
#define FXA_ASSUME(cond)                 \
  do {                                    \
    if (!(cond)) __builtin_unreachable(); \
  } while (0)
#elif defined(_MSC_VER)
#define FXA_ASSUME(cond) __assume(cond)
#else
#define FXA_ASSUME(cond)               \
  do {                                  \
    static_cast<void>(false && (cond)); \
  } while (0)
#endif

namespace fxa_unordered{
    
struct prime_size
{
  constexpr static std::size_t sizes[]={
    13ul,29ul,53ul,97ul,193ul,389ul,769ul,
    1543ul,3079ul,6151ul,12289ul,24593ul,
    49157ul,98317ul,196613ul,393241ul,786433ul,
    1572869ul,3145739ul,6291469ul,12582917ul,25165843ul,
    50331653ul,100663319ul,201326611ul,402653189ul,805306457ul,};
    
  static inline std::size_t size_index(std::size_t n)
  {
    const std::size_t *bound=std::lower_bound(
      std::begin(sizes),std::end(sizes),n);
    if(bound==std::end(sizes))--bound;
    return bound-sizes;
  }

  static inline std::size_t size(std::size_t size_index)
  {
    return sizes[size_index];
  }
  
  template<std::size_t SizeIndex,std::size_t Size=sizes[SizeIndex]>
  static std::size_t position(std::size_t hash){return hash%Size;}

  constexpr static std::size_t (*positions[])(std::size_t)={
    position<0>,position<1>,position<2>,position<3>,position<4>,
    position<5>,position<6>,position<7>,position<8>,position<9>,
    position<10>,position<11>,position<12>,position<13>,position<14>,
    position<15>,position<16>,position<17>,position<18>,position<19>,
    position<20>,position<21>,position<22>,position<23>,position<24>,
    position<25>,position<26>,
  };

  static inline std::size_t position(std::size_t hash,std::size_t size_index)
  {
    return positions[size_index](hash);
  }
};

struct prime_switch_size:prime_size
{
  static std::size_t position(std::size_t hash,std::size_t size_index)
  {
    switch(size_index){
      default:
      case  0: return prime_size::position<0>(hash);
      case  1: return prime_size::position<1>(hash);
      case  2: return prime_size::position<2>(hash);
      case  3: return prime_size::position<3>(hash);
      case  5: return prime_size::position<5>(hash);
      case  6: return prime_size::position<6>(hash);
      case  7: return prime_size::position<7>(hash);
      case  8: return prime_size::position<8>(hash);
      case  9: return prime_size::position<9>(hash);
      case 10: return prime_size::position<10>(hash);
      case 11: return prime_size::position<11>(hash);
      case 12: return prime_size::position<12>(hash);
      case 13: return prime_size::position<13>(hash);
      case 14: return prime_size::position<14>(hash);
      case 15: return prime_size::position<15>(hash);
      case 16: return prime_size::position<16>(hash);
      case 17: return prime_size::position<17>(hash);
      case 18: return prime_size::position<18>(hash);
      case 19: return prime_size::position<19>(hash);
      case 20: return prime_size::position<20>(hash);
      case 21: return prime_size::position<21>(hash);
      case 22: return prime_size::position<22>(hash);
      case 23: return prime_size::position<23>(hash);
      case 24: return prime_size::position<24>(hash);
      case 25: return prime_size::position<25>(hash);
      case 26: return prime_size::position<26>(hash);
    }
  }
};

#if defined(SIZE_MAX)
#  if ((((SIZE_MAX>>16)>>16)>>16)>>15)!=0
#  define FCA_HAS_64B_SIZE_T
#  endif
#elif defined(UINTPTR_MAX) /* used as proxy for std::size_t */
#  if ((((UINTPTR_MAX>>16)>>16)>>16)>>15)!=0
#  define FCA_HAS_64B_SIZE_T
#  endif
#endif

#if defined(__SIZEOF_INT128__) || (defined(_MSC_VER) && defined(_WIN64))
#define FCA_FASTMOD_SUPPORT
#endif

struct prime_fmod_size
{
  constexpr static std::size_t sizes[]={
    13ul,29ul,53ul,97ul,193ul,389ul,769ul,
    1543ul,3079ul,6151ul,12289ul,24593ul,
    49157ul,98317ul,196613ul,393241ul,786449ul /* 786433ul */,
    1572869ul,3145739ul,6291469ul,12582917ul,25165843ul,
    50331653ul,100663319ul,201326611ul,402653189ul,805306457ul,
    1610612741ul,3221225473ul,
#if !defined(FCA_HAS_64B_SIZE_T)
    4294967291ul};
#else
    // more than 32 bits
    6442450939ull,12884901893ull,25769803751ull,51539607551ull,
    103079215111ull,206158430209ull,412316860441ull,824633720831ull,
    1649267441651ull};
#endif

#if defined(FCA_FASTMOD_SUPPORT)
  constexpr static uint64_t inv_sizes32[]={
    1418980313362273202ull,636094623231363849ull,348051774975651918ull,
    190172619316593316ull,95578984837873325ull,47420935922132524ull,
    23987963684927896ull,11955116055547344ull,5991147799191151ull,
    2998982941588287ull,1501077717772769ull,750081082979285ull,
    375261795343686ull,187625172388393ull,93822606204624ull,
    46909513691883ull,23455741025432ull /* 23456218233098ull */,11728086747027ull,
    5864041509391ull,2932024948977ull,1466014921160ull,
    733007198436ull,366503839517ull,183251896093ull,
    91625960335ull,45812983922ull,22906489714ull,
    11453246088ull,5726623060ull,
#  if !defined(FCA_HAS_64B_SIZE_T)
  };
#  else
    4294967302ull};
#  endif /* !defined(FCA_HAS_64B_SIZE_T) */
#endif /* defined(FCA_FASTMOD_SUPPORT) */

  static inline std::size_t size_index(std::size_t n)
  {
    const std::size_t *bound=std::lower_bound(
      std::begin(sizes),std::end(sizes),n);
    if(bound==std::end(sizes))--bound;
    return bound-sizes;
  }

  static inline std::size_t size(std::size_t size_index)
  {
    return sizes[size_index];
  }

  template<std::size_t SizeIndex,std::size_t Size=sizes[SizeIndex]>
  static std::size_t position(std::size_t hash){return hash%Size;}

  constexpr static std::size_t (*positions[])(std::size_t)={
#if !defined(FCA_FASTMOD_SUPPORT)
    position<0>,position<1>,position<2>,position<3>,position<4>,
    position<5>,position<6>,position<7>,position<8>,position<9>,
    position<10>,position<11>,position<12>,position<13>,position<14>,
    position<15>,position<16>,position<17>,position<18>,position<19>,
    position<20>,position<21>,position<22>,position<23>,position<24>,
    position<25>,position<26>,position<27>,position<28>,
# if !defined(FCA_HAS_64B_SIZE_T)
    position<29>,
# endif
#endif

#if defined(FCA_HAS_64B_SIZE_T)
    position<29>,position<30>,position<31>,position<32>,position<33>,
    position<34>,position<35>,position<36>,position<37>,
#endif
  };

#if defined(FCA_FASTMOD_SUPPORT)
  // https://github.com/lemire/fastmod

#if defined(__SIZEOF_INT128__)

  static inline uint64_t mul128_u32(uint64_t lowbits, uint32_t d)
  {
    return ((unsigned __int128)lowbits * d) >> 64;
  }

#else

  static inline uint64_t mul128_u32(uint64_t lowbits, uint32_t d)
  {
    return __umulh(lowbits, d);
  }

#endif

  static inline uint32_t fastmod_u32(uint32_t a, uint64_t M, uint32_t d)
  {
    uint64_t lowbits = M * a;
    return (uint32_t)(mul128_u32(lowbits, d));
  }

#endif /* defined(FCA_FASTMOD_SUPPORT) */

  static inline std::size_t position(std::size_t hash,std::size_t size_index)
  {
#if defined(FCA_FASTMOD_SUPPORT)
# if defined(FCA_HAS_64B_SIZE_T)
    constexpr std::size_t sizes_under_32bit=
      sizeof(inv_sizes32)/sizeof(inv_sizes32[0]);
    if(BOOST_LIKELY(size_index<sizes_under_32bit)){
      return fastmod_u32(
        uint32_t(hash)+uint32_t(hash>>32),
        inv_sizes32[size_index],uint32_t(sizes[size_index]));
    }
    else{
      return positions[size_index-sizes_under_32bit](hash);
    }
# else
    return fastmod_u32(
      hash,inv_sizes32[size_index],uint32_t(sizes[size_index]));
# endif /* defined(FCA_HAS_64B_SIZE_T) */
#else
    return positions[size_index](hash);
#endif /* defined(FCA_FASTMOD_SUPPORT) */
  }
};

#ifdef FCA_FASTMOD_SUPPORT
#undef FCA_FASTMOD_SUPPORT
#endif
#ifdef FCA_HAS_64B_SIZE_T
#undef FCA_HAS_64B_SIZE_T
#endif

struct prime_frng_size:prime_size
{      
  static inline std::size_t position(std::size_t hash,std::size_t size_index)
  {
    return fastrangesize(hash,prime_size::sizes[size_index]);
  }
};

template<int> struct fibonacci_constant_impl;
template<> struct fibonacci_constant_impl<32>
{static constexpr auto value=2654435769u;};
template<> struct fibonacci_constant_impl<64>
{static constexpr auto value=11400714819323198485ull;};

static constexpr std::size_t fibonacci_constant=
  fibonacci_constant_impl<sizeof(std::size_t)*8>::value;

struct prime_frng_fib_size:prime_frng_size
{      
  static inline std::size_t mix_hash(std::size_t hash)
  {
    return hash*fibonacci_constant;
  } 

  static inline std::size_t position(std::size_t hash,std::size_t size_index)
  {
    return prime_frng_size::position(mix_hash(hash),size_index);
  }
};

struct pow2_size
{
  static inline std::size_t size_index(std::size_t n)
  {
    return n<=32?
      5:
      static_cast<std::size_t>(boost::core::bit_width(n-1));
  }

  static inline std::size_t size(std::size_t size_index)
  {
     return std::size_t(1)<<size_index;  
  }
    
  static inline std::size_t position(std::size_t hash,std::size_t size_index)
  {
    return hash>>(sizeof(std::size_t)*8-size_index);
  }
};

struct low_pow2_size
{
  static inline std::size_t size_index(std::size_t n)
  {
    return n<=32?
      5:
      static_cast<std::size_t>(boost::core::bit_width(n-1));
  }

  static inline std::size_t size(std::size_t size_index)
  {
     return std::size_t(1)<<size_index;  
  }
    
  static inline std::size_t position(std::size_t hash,std::size_t size_index)
  {
    return hash&(size(size_index)-1);
  }
};

struct pow2_fib_size:pow2_size
{
  static inline std::size_t mix_hash(std::size_t hash)
  {
    return hash*fibonacci_constant;
  } 

  static inline std::size_t position(std::size_t hash,std::size_t size_index)
  {
    return pow2_size::position(mix_hash(hash),size_index);
  }
};

template<unsigned N>
struct shift_hash
{
  static inline std::size_t long_hash(std::size_t hash){return hash>>N;}
  static inline std::size_t short_hash(std::size_t hash){return hash;}
};

template<unsigned N>
struct rshift_hash
{
  static inline std::size_t long_hash(std::size_t hash){return hash<<N;}

  static inline std::size_t short_hash(std::size_t hash)
  {
    return hash>>(sizeof(std::size_t)*8-N);
  }
};

template<unsigned N,unsigned Mod=127>
struct shift_mod_hash
{
  static inline std::size_t long_hash(std::size_t hash){return hash>>N;}
  static inline std::size_t short_hash(std::size_t hash){return hash%Mod;}
};

struct xm_hash
{
  static inline std::size_t long_hash(std::size_t hash){return hash;}

  static inline std::size_t short_hash(std::size_t hash)
  {
    boost::uint64_t z = hash;

    z ^= z >> 23;
    z *= 0xff51afd7ed558ccdull;

    return (std::size_t)(z>>(sizeof(boost::uint64_t)*8-8));
  }
};

template<class Key,class Value>
struct map_value_adaptor
{
  Key           first;
  mutable Value second;
};

template<typename Hash>
struct map_hash_adaptor
{
  template<typename T>
  auto operator()(const T& x)const
  {
    return h(x);
  }
    
  template<class Key,class Value>
  auto operator()(const map_value_adaptor<Key,Value>& x)const
  {
    return h(x.first);
  }
  
  Hash h;
};

template<typename Pred>
struct map_pred_adaptor
{
  template<typename T,class Key,class Value>
  auto operator()(
    const T& x,const map_value_adaptor<Key,Value>& y)const
  {
    return pred(x,y.first);
  }

  template<class Key,class Value,typename T>
  auto operator()(
    const map_value_adaptor<Key,Value>& x,const T& y)const
  {
    return pred(x.first,y);
  }

  template<class Key,class Value>
  auto operator()(
    const map_value_adaptor<Key,Value>& x,
    const map_value_adaptor<Key,Value>& y)const
  {
    return pred(x.first,y.first);
  }

  Pred pred;
};

inline std::size_t set_bit(std::size_t n){return std::size_t(1)<<n;}
inline std::size_t reset_bit(std::size_t n){return ~set_bit(n);}
inline std::size_t set_first_bits(std::size_t n) // n>0
{
  return ~(std::size_t(0))>>(sizeof(std::size_t)*8-n);
}
inline std::size_t reset_first_bits(std::size_t n) // n>0
{
  return ~set_first_bits(n);
}

} // namespace fxa_unordered

#endif
