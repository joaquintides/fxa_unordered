// Copyright 2021 Peter Dimov.
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt
//
// Modified by Joaquin M Lopez Munoz: extended for fxa_unordered
// Copyright 2022 Joaquin M Lopez Munoz.
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt


#ifndef CONTAINER_DEFS_HPP
#define CONTAINER_DEFS_HPP

#define _SILENCE_CXX17_OLD_ALLOCATOR_MEMBERS_DEPRECATION_WARNING

#include <boost/unordered_map.hpp>
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/core/detail/splitmix64.hpp>
#include "fca_simple_unordered.hpp"
#include "fca_unordered.hpp"
#include "foa_unordered_coalesced.hpp"
#include "foa_unordered_nway.hpp"
#include "foa_unordered_hopscotch.hpp"
#include "foa_unordered_longhop.hpp"
#include "foa_unordered_rc.hpp"
#ifdef HAVE_ABSEIL
# include "absl/container/node_hash_map.h"
# include "absl/container/flat_hash_map.h"
#endif
#include <memory>
#include <string>
#include <unordered_map>
#include <cstdint>

// counting allocator

static std::size_t s_alloc_bytes = 0;
static std::size_t s_alloc_count = 0;

template<class T> struct allocator
{
    using value_type = T;

    allocator() = default;

    template<class U> allocator( allocator<U> const & ) noexcept
    {
    }

    template<class U> bool operator==( allocator<U> const & ) const noexcept
    {
        return true;
    }

    template<class U> bool operator!=( allocator<U> const& ) const noexcept
    {
        return false;
    }

    T* allocate( std::size_t n ) const
    {
        s_alloc_bytes += n * sizeof(T);
        s_alloc_count++;

        return std::allocator<T>().allocate( n );
    }

    void deallocate( T* p, std::size_t n ) const noexcept
    {
        s_alloc_bytes -= n * sizeof(T);
        s_alloc_count--;

        std::allocator<T>().deallocate( p, n );
    }
};

// multi_index emulation of unordered_map

template<class K, class V> struct pair
{
    K first;
    mutable V second;
};

using namespace boost::multi_index;

template<class K, class V> using multi_index_map = multi_index_container<
  pair<K, V>,
  indexed_by<
    hashed_unique< member<pair<K, V>, K, &pair<K, V>::first> >
  >,
  ::allocator< pair<K, V> >
>;

// mulx

#if ((SIZE_MAX>>16)>>16)==0

inline std::uint32_t mulx( std::uint32_t x, std::uint32_t y )
{
	std::uint64_t r = (std::uint64_t)x * y;
	return (std::uint32_t)r ^ (std::uint32_t)( r >> 32 );
}

#elif defined(_MSC_VER) && !defined(__clang__)

#include <intrin.h>

__forceinline std::uint64_t mulx( std::uint64_t x, std::uint64_t y )
{
	std::uint64_t r2;
	std::uint64_t r = _umul128( x, y, &r2 );
	return r ^ r2;
}

#else

inline std::uint64_t mulx( std::uint64_t x, std::uint64_t y )
{
	__uint128_t r = (__uint128_t)x * y;
	return (std::uint64_t)r ^ (std::uint64_t)( r >> 64 );
}

#endif

template<class T, class H = boost::hash<T> >
struct mulx_hash
{
  std::size_t operator()(const T& x) const
  {
    boost::uint64_t z = H()(x);

	z = mulx( z, 0x9DDFEA08EB382D69ull ); // should be 0xCC9E2D51 for 32 bit

    return (std::size_t)z; // good results only in 64 bits
  }
};

// xmxmx

template<class T>
struct xmxmx_hash
{
  std::size_t operator()(const T& x) const
  {
    boost::uint64_t z = boost::hash<T>()(x);

	z ^= z >> 32;
	z *= 0xe9846af9b1a615dull;
	z ^= z >> 32;
	z *= 0xe9846af9b1a615dull;
	z ^= z >> 28;

    return (std::size_t)z; // good results only in 64 bits
  }
};

// mxm

template<class T>
struct mxm_hash
{
  std::size_t operator()(const T& x) const
  {
    boost::uint64_t z = boost::hash<T>()(x);

    z *= 0xbf58476d1ce4e5b9ull;
    z ^= z >> 56;
    z *= 0x94d049bb133111ebull;

    return (std::size_t)z; // good results only in 64 bits
  }
};

// mxm2

template<class T>
struct mxm2_hash
{
  std::size_t operator()(const T& x) const
  {
    boost::uint64_t z = boost::hash<T>()(x);

    z *= 0x94d049bb133111ebull;
    z ^= z >> 57;
    z *= 0x94d049bb133111ebull;

    return (std::size_t)z; // good results only in 64 bits
  }
};

// xmx

template<class T, class H = boost::hash<T> >
struct xmx_hash
{
  std::size_t operator()(const T& x) const
  {
    boost::uint64_t z = H()(x);

    z ^= z >> 23;
    z *= 0xff51afd7ed558ccdull;
    z ^= z >> 23;

    return (std::size_t)z; // good results only in 64 bits
  }
};

// xmx2

template<class T>
struct xmx2_hash
{
  std::size_t operator()(const T& x) const
  {
    boost::uint64_t z = boost::hash<T>()(x);

    z ^= z >> 30;
    z *= 0xc4ceb9fe1a85ec53ull;
    z ^= z >> 27;

    return (std::size_t)z; // good results only in 64 bits
  }
};

// xm

template<class T>
struct xm_hash
{
  std::size_t operator()(const T& x) const
  {
    boost::uint64_t z = boost::hash<T>()(x);

    z ^= z >> 23;
    z *= 0xff51afd7ed558ccdull;

    return (std::size_t)z; // good results only in 64 bits
  }
};

// xm2

template<class T, class H = boost::hash<T> >
struct xm2_hash
{
  std::size_t operator()(const T& x) const
  {
    boost::uint64_t z = H()(x);

    z ^= z >> 31;
    z *= 0xbf58476d1ce4e5b9ull;

    return (std::size_t)z; // good results only in 64 bits
  }
};

// xmxmx32

template<class T>
struct xmxmx32_hash
{
  std::size_t operator()(const T& x) const
  {
    std::size_t z = boost::hash<T>()(x);

    // 0.10704308166917044

    z ^= z >> 16;
    z *= 0x21f0aaadU;
    z ^= z >> 15;
    z *= 0x735a2d97U;
    z ^= z >> 15;

    return z;
  }
};

// mxm32

template<class T>
struct mxm32_hash
{
  std::size_t operator()(const T& x) const
  {
    std::size_t z = boost::hash<T>()(x);

    // score = 193.45195822264921
    z *= 0x6acd36d3U;
    z ^= z >> 28;
    z *= 0x0acdb2adU;

    return z;
  }
};

// mxm33

template<class T>
struct mxm33_hash
{
  std::size_t operator()(const T& t) const
  {
    std::size_t x = boost::hash<T>()(t);

    // score = 183.29936089234624
    x *= 0x0aa49955U;
    x ^= x >> 28;
    x *= 0xea69945bU;

    return x;
  }
};

// xmx32

template<class T>
struct xmx32_hash
{
  std::size_t operator()(const T& x) const
  {
    std::size_t z = boost::hash<T>()(x);

    // score = 347.64244373273169
    z ^= z >> 13;
    z *= 0x64aad355U;
    z ^= x >> 17;

    return z;
  }
};

// xmx33

template<class T, class H = boost::hash<T> >
struct xmx33_hash
{
  std::size_t operator()(const T& x) const
  {
    std::size_t z = H()(x);

    // score = 333.7934929677524
    z ^= z >> 18;
    z *= 0x56b5aaadU;
    z ^= z >> 16;

    return z;
  }
};

// xmx34

template<class T>
struct xmx34_hash
{
  std::size_t operator()(const T& t) const
  {
    std::size_t x = boost::hash<T>()(t);

    // score = 323.48134076841461
    x ^= x >> 14;
    x *= 0x72b55aabU;
    x ^= x >> 15;

    return x;
  }
};

// rmr32

template<class T>
struct rmr32_hash
{
  std::size_t operator()(const T& t) const
  {
    std::size_t x = boost::hash<T>()(t);

    // score = 95.87173427168284
    x ^= ((x << 12) | (x >> 20)) ^ ((x << 24) | (x >> 8));
    x *= 0xa8ee8555U;
    x ^= ((x << 11) | (x >> 21)) ^ ((x << 20) | (x >> 12));

    return x;
  }
};

// rmr33

template<class T>
struct rmr33_hash
{
  std::size_t operator()(const T& t) const
  {
    std::size_t x = boost::hash<T>()(t);

    // score = 88.031149345621088
    x ^= ((x << 21) | (x >> 11)) ^ ((x << 11) | (x >> 21));
    x *= 0x6d4e2953U;
    x ^= ((x << 20) | (x >> 12)) ^ ((x << 10) | (x >> 22));

    return x;
  }
};

// xm32

template<class T>
struct xm32_hash
{
  std::size_t operator()(const T& t) const
  {
    std::size_t x = boost::hash<T>()(t);

    // score = 603.51995712559471
    x ^= x >> 15;
    x *= 0xc92adaabU;

    return x;
  }
};

// xm33

template<class T, class H = boost::hash<T> >
struct xm33_hash
{
  std::size_t operator()(const T& t) const
  {
    std::size_t x = H()(t);

    // score = 597.07647040752659
    x ^= x >> 14;
    x *= 0xa535aaabU;

    return x;
  }
};

// fxa_unordered variations

template<class K, class V, class H=boost::hash<K>>
using fca_switch_unordered_map =
  fca_unordered_map<
    K, V, H, std::equal_to<K>,
    ::allocator<fxa_unordered::map_value_adaptor<K, V>>,
    fxa_unordered::prime_switch_size>;

template<class K, class V, class H=boost::hash<K>>
using fca_fmod_unordered_map =
  fca_unordered_map<
    K, V, H, std::equal_to<K>,
    ::allocator<fxa_unordered::map_value_adaptor<K, V>>,
    fxa_unordered::prime_fmod_size>;

template<class K, class V, class H=boost::hash<K>>
using fca_frng_unordered_map =
  fca_unordered_map<
    K, V, H, std::equal_to<K>,
    ::allocator<fxa_unordered::map_value_adaptor<K, V>>,
    fxa_unordered::prime_frng_size>;

template<class K, class V, class H=boost::hash<K>>
using fca_frng_fib_unordered_map =
  fca_unordered_map<
    K, V, H, std::equal_to<K>,
    ::allocator<fxa_unordered::map_value_adaptor<K, V>>,
    fxa_unordered::prime_frng_fib_size>;

template<class K, class V, class H=boost::hash<K>>
using fca_pow2_unordered_map =
  fca_unordered_map<
    K, V, H, std::equal_to<K>,
    ::allocator<fxa_unordered::map_value_adaptor<K, V>>,
    fxa_unordered::pow2_size>;

template<class K, class V, class H=boost::hash<K>>
using fca_pow2_fib_unordered_map =
  fca_unordered_map<
    K, V, H, std::equal_to<K>,
    ::allocator<fxa_unordered::map_value_adaptor<K, V>>,
    fxa_unordered::pow2_fib_size>;

template<class K, class V, class H=boost::hash<K>>
using fca_fmod_unordered_bucket_map =
  fca_unordered_map<
    K, V, H, std::equal_to<K>,
    ::allocator<fxa_unordered::map_value_adaptor<K, V>>,
    fxa_unordered::prime_fmod_size,
    fxa_unordered::simple_buckets>;

template<class K, class V, class H=boost::hash<K>>
using fca_fmod_bcached_unordered_bucket_map =
  fca_unordered_map<
    K, V, H, std::equal_to<K>,
    ::allocator<fxa_unordered::map_value_adaptor<K, V>>,
    fxa_unordered::prime_fmod_size,
    fxa_unordered::bcached_simple_buckets>;

template<class K, class V, class H=boost::hash<K>>
using fca_fmod_unordered_hybrid_map =
  fca_unordered_map<
    K, V, H, std::equal_to<K>,
    ::allocator<fxa_unordered::map_value_adaptor<K, V>>,
    fxa_unordered::prime_fmod_size,
    fxa_unordered::grouped_buckets,
    fxa_unordered::hybrid_node_allocation>;

template<class K, class V, class H=boost::hash<K>>
using fca_fmod_unordered_hybrid_bucket_map =
  fca_unordered_map<
    K, V, H, std::equal_to<K>,
    ::allocator<fxa_unordered::map_value_adaptor<K, V>>,
    fxa_unordered::prime_fmod_size,
    fxa_unordered::simple_buckets,
    fxa_unordered::hybrid_node_allocation>;

template<class K, class V, class H=boost::hash<K>>
using fca_fmod_bcached_unordered_hybrid_bucket_map =
  fca_unordered_map<
    K, V, H, std::equal_to<K>,
    ::allocator<fxa_unordered::map_value_adaptor<K, V>>,
    fxa_unordered::prime_fmod_size,
    fxa_unordered::bcached_simple_buckets,
    fxa_unordered::hybrid_node_allocation>;

template<class K, class V, class H=boost::hash<K>>
using fca_fmod_unordered_linear_map =
  fca_unordered_map<
    K, V, H, std::equal_to<K>,
    ::allocator<fxa_unordered::map_value_adaptor<K, V>>,
    fxa_unordered::prime_fmod_size,
    fxa_unordered::grouped_buckets,
    fxa_unordered::linear_node_allocation>;

template<class K, class V, class H=boost::hash<K>>
using fca_fmod_unordered_linear_bucket_map =
  fca_unordered_map<
    K, V, H, std::equal_to<K>,
    ::allocator<fxa_unordered::map_value_adaptor<K, V>>,
    fxa_unordered::prime_fmod_size,
    fxa_unordered::simple_buckets,
    fxa_unordered::linear_node_allocation>;

template<class K, class V, class H=boost::hash<K>>
using fca_fmod_bcached_unordered_linear_bucket_map =
  fca_unordered_map<
    K, V, H, std::equal_to<K>,
    ::allocator<fxa_unordered::map_value_adaptor<K, V>>,
    fxa_unordered::prime_fmod_size,
    fxa_unordered::bcached_simple_buckets,
    fxa_unordered::linear_node_allocation>;

template<class K, class V, class H=boost::hash<K>>
using fca_fmod_unordered_pool_map =
  fca_unordered_map<
    K, V, H, std::equal_to<K>,
    ::allocator<fxa_unordered::map_value_adaptor<K, V>>,
    fxa_unordered::prime_fmod_size,
    fxa_unordered::grouped_buckets,
    fxa_unordered::pool_node_allocation>;

template<class K, class V, class H=boost::hash<K>>
using fca_fmod_unordered_pool_bucket_map =
  fca_unordered_map<
    K, V, H, std::equal_to<K>,
    ::allocator<fxa_unordered::map_value_adaptor<K, V>>,
    fxa_unordered::prime_fmod_size,
    fxa_unordered::simple_buckets,
    fxa_unordered::pool_node_allocation>;

template<class K, class V, class H=boost::hash<K>>
using fca_fmod_bcached_unordered_pool_bucket_map =
  fca_unordered_map<
    K, V, H, std::equal_to<K>,
    ::allocator<fxa_unordered::map_value_adaptor<K, V>>,
    fxa_unordered::prime_fmod_size,
    fxa_unordered::bcached_simple_buckets,
    fxa_unordered::pool_node_allocation>;

template<class K, class V, class H=boost::hash<K>>
using fca_fmod_unordered_embedded_map =
  fca_unordered_map<
    K, V, H, std::equal_to<K>,
    ::allocator<fxa_unordered::map_value_adaptor<K, V>>,
    fxa_unordered::prime_fmod_size,
    fxa_unordered::grouped_buckets,
    fxa_unordered::embedded_node_allocation>;

template<class K, class V, class H=boost::hash<K>>
using fca_fmod_unordered_embedded_bucket_map =
  fca_unordered_map<
    K, V, H, std::equal_to<K>,
    ::allocator<fxa_unordered::map_value_adaptor<K, V>>,
    fxa_unordered::prime_fmod_size,
    fxa_unordered::simple_buckets,
    fxa_unordered::embedded_node_allocation>;

template<class K, class V, class H=boost::hash<K>>
using fca_fmod_bcached_unordered_embedded_bucket_map =
  fca_unordered_map<
    K, V, H, std::equal_to<K>,
    ::allocator<fxa_unordered::map_value_adaptor<K, V>>,
    fxa_unordered::prime_fmod_size,
    fxa_unordered::bcached_simple_buckets,
    fxa_unordered::embedded_node_allocation>;

template<class K, class V, class H=boost::hash<K>>
using foa_fmod_unordered_coalesced_map =
  foa_unordered_coalesced_map<
    K, V, H,std::equal_to<K>,
    ::allocator<fxa_unordered::map_value_adaptor<K, V>>,
    fxa_unordered::prime_fmod_size>;

template<class K, class V, class H=absl::container_internal::hash_default_hash<K>>
using foa_absl_unordered_coalesced_map =
  foa_unordered_coalesced_map<
    K, V, H,std::equal_to<K>,
    ::allocator<fxa_unordered::map_value_adaptor<K, V>>,
    fxa_unordered::pow2_size>;

template<class K, class V, class H=boost::hash<K>>
using foa_fmod_hcached_unordered_coalesced_map =
  foa_unordered_coalesced_map<
    K, V, H,std::equal_to<K>,
    ::allocator<fxa_unordered::map_value_adaptor<K, V>>,
    fxa_unordered::prime_fmod_size,
    fxa_unordered::hcached_coalesced_set_nodes>;

template<class K, class V, class H=boost::hash<K>>
using foa_pow2_fib_unordered_nway_map =
  foa_unordered_nway_map<
    K, V, H,std::equal_to<K>,
    ::allocator<fxa_unordered::map_value_adaptor<K, V>>,
    fxa_unordered::pow2_fib_size>;

template<class K, class V, class H=boost::hash<K>>
using foa_fmod_unordered_nwayplus_map =
  foa_unordered_nwayplus_map<
    K, V, H,std::equal_to<K>,
    ::allocator<fxa_unordered::map_value_adaptor<K, V>>,
    fxa_unordered::prime_fmod_size>;
    
template<class K, class V, class H=boost::hash<K>>
using foa_pow2_fib_unordered_nwayplus_map =
  foa_unordered_nwayplus_map<
    K, V, H,std::equal_to<K>,
    ::allocator<fxa_unordered::map_value_adaptor<K, V>>,
    fxa_unordered::pow2_fib_size>;
    
template<class K, class V, class H=absl::container_internal::hash_default_hash<K>>
using foa_absl_unordered_nwayplus_map =
  foa_unordered_nwayplus_map<
    K, V, H,std::equal_to<K>,
    ::allocator<fxa_unordered::map_value_adaptor<K, V>>,
    fxa_unordered::pow2_size,
    fxa_unordered::shift_hash<0>>;

template<class K, class V, class H=boost::hash<K>>
using foa_fmod_unordered_soa_nwayplus_map =
  foa_unordered_nwayplus_map<
    K, V, H,std::equal_to<K>,
    ::allocator<fxa_unordered::map_value_adaptor<K, V>>,
    fxa_unordered::prime_fmod_size,
    fxa_unordered::shift_mod_hash<0>,
    fxa_unordered::nwayplus::soa_allocation>;

template<class K, class V, class H=boost::hash<K>>
using foa_pow2_fib_unordered_soa_nwayplus_map =
  foa_unordered_nwayplus_map<
    K, V, H,std::equal_to<K>,
    ::allocator<fxa_unordered::map_value_adaptor<K, V>>,
    fxa_unordered::pow2_fib_size,
    fxa_unordered::shift_mod_hash<0>,
    fxa_unordered::nwayplus::soa_allocation>;

template<class K, class V, class H=boost::hash<K>>
using foa_frng_fib_unordered_soa_nwayplus_map =
  foa_unordered_nwayplus_map<
    K, V, H,std::equal_to<K>,
    ::allocator<fxa_unordered::map_value_adaptor<K, V>>,
    fxa_unordered::prime_frng_fib_size,
    fxa_unordered::shift_mod_hash<0>,
    fxa_unordered::nwayplus::soa_allocation>;

template<class K, class V, class H=boost::hash<K>>
using foa_frng_fib_unordered_soa15_nwayplus_map =
  foa_unordered_nwayplus_map<
    K, V, H,std::equal_to<K>,
    ::allocator<fxa_unordered::map_value_adaptor<K, V>>,
    fxa_unordered::prime_frng_fib_size,
    fxa_unordered::shift_mod_hash<0>,
    fxa_unordered::nwayplus::soa15_allocation>;

template<class K, class V, class H=absl::container_internal::hash_default_hash<K>>
using foa_absl_unordered_soa_nwayplus_map =
  foa_unordered_nwayplus_map<
    K, V, H,std::equal_to<K>,
    ::allocator<fxa_unordered::map_value_adaptor<K, V>>,
    fxa_unordered::pow2_size,
    fxa_unordered::shift_hash<0>,
    fxa_unordered::nwayplus::soa_allocation>;

template<class K, class V, class H=absl::container_internal::hash_default_hash<K>>
using foa_absl_unordered_intersoa_nwayplus_map =
  foa_unordered_nwayplus_map<
    K, V, H,std::equal_to<K>,
    ::allocator<fxa_unordered::map_value_adaptor<K, V>>,
    fxa_unordered::pow2_size,
    fxa_unordered::shift_hash<0>,
    fxa_unordered::nwayplus::intersoa_allocation>;

template<class K, class V, class H=absl::container_internal::hash_default_hash<K>>
using foa_absl_unordered_soa15_nwayplus_map =
  foa_unordered_nwayplus_map<
    K, V, H,std::equal_to<K>,
    ::allocator<fxa_unordered::map_value_adaptor<K, V>>,
    fxa_unordered::pow2_size,
    fxa_unordered::shift_hash<0>,
    fxa_unordered::nwayplus::soa15_allocation>;

template<class K, class V, class H=absl::container_internal::hash_default_hash<K>>
using foa_absl_unordered_intersoa15_nwayplus_map =
  foa_unordered_nwayplus_map<
    K, V, H,std::equal_to<K>,
    ::allocator<fxa_unordered::map_value_adaptor<K, V>>,
    fxa_unordered::pow2_size,
    fxa_unordered::shift_hash<0>,
    fxa_unordered::nwayplus::intersoa15_allocation>;

template<class K, class V, class H=boost::hash<K>>
using foa_pow2_fib_unordered_coalesced_nwayplus_map =
  foa_unordered_nwayplus_map<
    K, V, H,std::equal_to<K>,
    ::allocator<fxa_unordered::map_value_adaptor<K, V>>,
    fxa_unordered::pow2_fib_size,
    fxa_unordered::shift_mod_hash<0>,
    fxa_unordered::nwayplus::coalesced_allocation>;

template<class K, class V, class H=boost::hash<K>>
using foa_pow2_fib_unordered_soa_coalesced_nwayplus_map =
  foa_unordered_nwayplus_map<
    K, V, H,std::equal_to<K>,
    ::allocator<fxa_unordered::map_value_adaptor<K, V>>,
    fxa_unordered::pow2_fib_size,
    fxa_unordered::shift_mod_hash<0>,
    fxa_unordered::nwayplus::soa_coalesced_allocation>;
    
template<class K, class V, class H=boost::hash<K>>
using foa_frng_fib_unordered_hopscotch_map =
  foa_unordered_hopscotch_map<
    K, V, H,std::equal_to<K>,
    ::allocator<fxa_unordered::map_value_adaptor<K, V>>,
    fxa_unordered::prime_frng_fib_size>;
    
template<class K, class V, class H=absl::container_internal::hash_default_hash<K>>
using foa_absl_unordered_hopscotch_map =
  foa_unordered_hopscotch_map<
    K, V, H,std::equal_to<K>,
    ::allocator<fxa_unordered::map_value_adaptor<K, V>>,
    fxa_unordered::pow2_size>;

template<class K, class V, class H=boost::hash<K>>
using foa_frng_fib_unordered_longhop_map =
  foa_unordered_longhop_map<
    K, V, H,std::equal_to<K>,
    ::allocator<fxa_unordered::map_value_adaptor<K, V>>,
    fxa_unordered::prime_frng_fib_size>;
    
template<class K, class V, class H=absl::container_internal::hash_default_hash<K>>
using foa_absl_unordered_longhop_map =
  foa_unordered_longhop_map<
    K, V, H,std::equal_to<K>,
    ::allocator<fxa_unordered::map_value_adaptor<K, V>>,
    fxa_unordered::pow2_size>;

template<class K, class V, class H=boost::hash<K>>
using foa_fmod_unordered_rc16_map =
  foa_unordered_rc_map<
    K, V, H,std::equal_to<K>,
    ::allocator<fxa_unordered::map_value_adaptor<K, V>>,
    fxa_unordered::rc::group16,
    fxa_unordered::prime_fmod_size,
    fxa_unordered::rc::nonpow2_prober,
    fxa_unordered::shift_mod_hash<0>>;

template<class K, class V, class H=boost::hash<K>>
using foa_fmod_unordered_rc15_map =
  foa_unordered_rc_map<
    K, V, H,std::equal_to<K>,
    ::allocator<fxa_unordered::map_value_adaptor<K, V>>,
    fxa_unordered::rc::group15,
    fxa_unordered::prime_fmod_size,
    fxa_unordered::rc::nonpow2_prober,
    fxa_unordered::shift_mod_hash<0,257>>;

template<class K, class V, class H=boost::hash<K>>
using foa_fmodxm_unordered_rc16_map =
  foa_unordered_rc_map<
    K, V, H,std::equal_to<K>,
    ::allocator<fxa_unordered::map_value_adaptor<K, V>>,
    fxa_unordered::rc::group16,
    fxa_unordered::prime_fmod_size,
    fxa_unordered::rc::nonpow2_prober,
    fxa_unordered::xm_hash>;

template<class K, class V, class H=boost::hash<K>>
using foa_fmodxm_unordered_rc15_map =
  foa_unordered_rc_map<
    K, V, H,std::equal_to<K>,
    ::allocator<fxa_unordered::map_value_adaptor<K, V>>,
    fxa_unordered::rc::group15,
    fxa_unordered::prime_fmod_size,
    fxa_unordered::rc::nonpow2_prober,
    fxa_unordered::xm_hash>;

template<class K, class V, class H=absl::container_internal::hash_default_hash<K>>
using foa_absl_unordered_rc16_map =
  foa_unordered_rc_map<
    K, V, H,std::equal_to<K>,
    ::allocator<fxa_unordered::map_value_adaptor<K, V>>,
    fxa_unordered::rc::group16>;

template<class K, class V, class H=absl::container_internal::hash_default_hash<K>>
using foa_absl_unordered_rc15_map =
  foa_unordered_rc_map<
    K, V, H,std::equal_to<K>,
    ::allocator<fxa_unordered::map_value_adaptor<K, V>>,
    fxa_unordered::rc::group15>;

template<class K, class V, class H=mulx_hash<K>>
using foa_mulx_unordered_rc16_map =
  foa_unordered_rc_map<
    K, V, H,std::equal_to<K>,
    ::allocator<fxa_unordered::map_value_adaptor<K, V>>,
    fxa_unordered::rc::group16>;

template<class K, class V, class H=mulx_hash<K>>
using foa_mulx_unordered_rc15_map =
  foa_unordered_rc_map<
    K, V, H,std::equal_to<K>,
    ::allocator<fxa_unordered::map_value_adaptor<K, V>>,
    fxa_unordered::rc::group15>;

template<class K, class V, class H=xmxmx_hash<K>>
using foa_xmxmx_unordered_rc16_map =
  foa_unordered_rc_map<
    K, V, H,std::equal_to<K>,
    ::allocator<fxa_unordered::map_value_adaptor<K, V>>,
    fxa_unordered::rc::group16>;

template<class K, class V, class H=xmxmx_hash<K>>
using foa_xmxmx_unordered_rc15_map =
  foa_unordered_rc_map<
    K, V, H,std::equal_to<K>,
    ::allocator<fxa_unordered::map_value_adaptor<K, V>>,
    fxa_unordered::rc::group15>;

template<class K, class V, class H=mxm_hash<K>>
using foa_mxm_unordered_rc16_map =
  foa_unordered_rc_map<
    K, V, H,std::equal_to<K>,
    ::allocator<fxa_unordered::map_value_adaptor<K, V>>,
    fxa_unordered::rc::group16>;

template<class K, class V, class H=mxm_hash<K>>
using foa_mxm_unordered_rc15_map =
  foa_unordered_rc_map<
    K, V, H,std::equal_to<K>,
    ::allocator<fxa_unordered::map_value_adaptor<K, V>>,
    fxa_unordered::rc::group15>;

template<class K, class V, class H=mxm2_hash<K>>
using foa_mxm2_unordered_rc16_map =
  foa_unordered_rc_map<
    K, V, H,std::equal_to<K>,
    ::allocator<fxa_unordered::map_value_adaptor<K, V>>,
    fxa_unordered::rc::group16>;

template<class K, class V, class H=mxm2_hash<K>>
using foa_mxm2_unordered_rc15_map =
  foa_unordered_rc_map<
    K, V, H,std::equal_to<K>,
    ::allocator<fxa_unordered::map_value_adaptor<K, V>>,
    fxa_unordered::rc::group15>;

template<class K, class V, class H=xmx_hash<K>>
using foa_xmx_unordered_rc16_map =
  foa_unordered_rc_map<
    K, V, H,std::equal_to<K>,
    ::allocator<fxa_unordered::map_value_adaptor<K, V>>,
    fxa_unordered::rc::group16>;

template<class K, class V, class H=xmx_hash<K>>
using foa_xmx_unordered_rc15_map =
  foa_unordered_rc_map<
    K, V, H,std::equal_to<K>,
    ::allocator<fxa_unordered::map_value_adaptor<K, V>>,
    fxa_unordered::rc::group15>;

template<class K, class V, class H=xmx2_hash<K>>
using foa_xmx2_unordered_rc16_map =
  foa_unordered_rc_map<
    K, V, H,std::equal_to<K>,
    ::allocator<fxa_unordered::map_value_adaptor<K, V>>,
    fxa_unordered::rc::group16>;

template<class K, class V, class H=xmx2_hash<K>>
using foa_xmx2_unordered_rc15_map =
  foa_unordered_rc_map<
    K, V, H,std::equal_to<K>,
    ::allocator<fxa_unordered::map_value_adaptor<K, V>>,
    fxa_unordered::rc::group15>;

template<class K, class V, class H=xm_hash<K>>
using foa_xm_unordered_rc16_map =
  foa_unordered_rc_map<
    K, V, H,std::equal_to<K>,
    ::allocator<fxa_unordered::map_value_adaptor<K, V>>,
    fxa_unordered::rc::group16>;

template<class K, class V, class H=xm_hash<K>>
using foa_xm_unordered_rc15_map =
  foa_unordered_rc_map<
    K, V, H,std::equal_to<K>,
    ::allocator<fxa_unordered::map_value_adaptor<K, V>>,
    fxa_unordered::rc::group15>;

template<class K, class V, class H=xm_hash<K>>
using foa_hxm_unordered_rc16_map =
  foa_unordered_rc_map<
    K, V, H,std::equal_to<K>,
    ::allocator<fxa_unordered::map_value_adaptor<K, V>>,
    fxa_unordered::rc::group16,
    fxa_unordered::pow2_size,
    fxa_unordered::rc::pow2_prober,
    fxa_unordered::rshift_hash<7>>;

template<class K, class V, class H=xm_hash<K>>
using foa_hxm_unordered_rc15_map =
  foa_unordered_rc_map<
    K, V, H,std::equal_to<K>,
    ::allocator<fxa_unordered::map_value_adaptor<K, V>>,
    fxa_unordered::rc::group15,
    fxa_unordered::pow2_size,
    fxa_unordered::rc::pow2_prober,
    fxa_unordered::rshift_hash<8>>;

template<class K, class V, class H=xm2_hash<K>>
using foa_xm2_unordered_rc16_map =
  foa_unordered_rc_map<
    K, V, H,std::equal_to<K>,
    ::allocator<fxa_unordered::map_value_adaptor<K, V>>,
    fxa_unordered::rc::group16>;

template<class K, class V, class H=xm2_hash<K>>
using foa_xm2_unordered_rc15_map =
  foa_unordered_rc_map<
    K, V, H,std::equal_to<K>,
    ::allocator<fxa_unordered::map_value_adaptor<K, V>>,
    fxa_unordered::rc::group15>;

template<class K, class V, class H=xm2_hash<K>>
using foa_hxm2_unordered_rc16_map =
  foa_unordered_rc_map<
    K, V, H,std::equal_to<K>,
    ::allocator<fxa_unordered::map_value_adaptor<K, V>>,
    fxa_unordered::rc::group16,
    fxa_unordered::pow2_size,
    fxa_unordered::rc::pow2_prober,
    fxa_unordered::rshift_hash<7>>;

template<class K, class V, class H=xm2_hash<K>>
using foa_hxm2_unordered_rc15_map =
  foa_unordered_rc_map<
    K, V, H,std::equal_to<K>,
    ::allocator<fxa_unordered::map_value_adaptor<K, V>>,
    fxa_unordered::rc::group15,
    fxa_unordered::pow2_size,
    fxa_unordered::rc::pow2_prober,
    fxa_unordered::rshift_hash<8>>;

template<class K, class V, class H=xmxmx32_hash<K>>
using foa_xmxmx32_unordered_rc16_map =
  foa_unordered_rc_map<
    K, V, H,std::equal_to<K>,
    ::allocator<fxa_unordered::map_value_adaptor<K, V>>,
    fxa_unordered::rc::group16>;

template<class K, class V, class H=xmxmx32_hash<K>>
using foa_xmxmx32_unordered_rc15_map =
  foa_unordered_rc_map<
    K, V, H,std::equal_to<K>,
    ::allocator<fxa_unordered::map_value_adaptor<K, V>>,
    fxa_unordered::rc::group15>;

template<class K, class V, class H=mxm32_hash<K>>
using foa_mxm32_unordered_rc16_map =
  foa_unordered_rc_map<
    K, V, H,std::equal_to<K>,
    ::allocator<fxa_unordered::map_value_adaptor<K, V>>,
    fxa_unordered::rc::group16>;

template<class K, class V, class H=mxm32_hash<K>>
using foa_mxm32_unordered_rc15_map =
  foa_unordered_rc_map<
    K, V, H,std::equal_to<K>,
    ::allocator<fxa_unordered::map_value_adaptor<K, V>>,
    fxa_unordered::rc::group15>;

template<class K, class V, class H=mxm33_hash<K>>
using foa_mxm33_unordered_rc16_map =
  foa_unordered_rc_map<
    K, V, H,std::equal_to<K>,
    ::allocator<fxa_unordered::map_value_adaptor<K, V>>,
    fxa_unordered::rc::group16>;

template<class K, class V, class H=mxm33_hash<K>>
using foa_mxm33_unordered_rc15_map =
  foa_unordered_rc_map<
    K, V, H,std::equal_to<K>,
    ::allocator<fxa_unordered::map_value_adaptor<K, V>>,
    fxa_unordered::rc::group15>;

template<class K, class V, class H=xmx32_hash<K>>
using foa_xmx32_unordered_rc16_map =
  foa_unordered_rc_map<
    K, V, H,std::equal_to<K>,
    ::allocator<fxa_unordered::map_value_adaptor<K, V>>,
    fxa_unordered::rc::group16>;

template<class K, class V, class H=xmx32_hash<K>>
using foa_xmx32_unordered_rc15_map =
  foa_unordered_rc_map<
    K, V, H,std::equal_to<K>,
    ::allocator<fxa_unordered::map_value_adaptor<K, V>>,
    fxa_unordered::rc::group15>;

template<class K, class V, class H=xmx33_hash<K>>
using foa_xmx33_unordered_rc16_map =
  foa_unordered_rc_map<
    K, V, H,std::equal_to<K>,
    ::allocator<fxa_unordered::map_value_adaptor<K, V>>,
    fxa_unordered::rc::group16>;

template<class K, class V, class H=xmx33_hash<K>>
using foa_xmx33_unordered_rc15_map =
  foa_unordered_rc_map<
    K, V, H,std::equal_to<K>,
    ::allocator<fxa_unordered::map_value_adaptor<K, V>>,
    fxa_unordered::rc::group15>;

template<class K, class V, class H=xmx34_hash<K>>
using foa_xmx34_unordered_rc16_map =
  foa_unordered_rc_map<
    K, V, H,std::equal_to<K>,
    ::allocator<fxa_unordered::map_value_adaptor<K, V>>,
    fxa_unordered::rc::group16>;

template<class K, class V, class H=xmx34_hash<K>>
using foa_xmx34_unordered_rc15_map =
  foa_unordered_rc_map<
    K, V, H,std::equal_to<K>,
    ::allocator<fxa_unordered::map_value_adaptor<K, V>>,
    fxa_unordered::rc::group15>;

template<class K, class V, class H=rmr32_hash<K>>
using foa_rmr32_unordered_rc16_map =
  foa_unordered_rc_map<
    K, V, H,std::equal_to<K>,
    ::allocator<fxa_unordered::map_value_adaptor<K, V>>,
    fxa_unordered::rc::group16>;

template<class K, class V, class H=rmr32_hash<K>>
using foa_rmr32_unordered_rc15_map =
  foa_unordered_rc_map<
    K, V, H,std::equal_to<K>,
    ::allocator<fxa_unordered::map_value_adaptor<K, V>>,
    fxa_unordered::rc::group15>;

template<class K, class V, class H=rmr33_hash<K>>
using foa_rmr33_unordered_rc16_map =
  foa_unordered_rc_map<
    K, V, H,std::equal_to<K>,
    ::allocator<fxa_unordered::map_value_adaptor<K, V>>,
    fxa_unordered::rc::group16>;

template<class K, class V, class H=rmr33_hash<K>>
using foa_rmr33_unordered_rc15_map =
  foa_unordered_rc_map<
    K, V, H,std::equal_to<K>,
    ::allocator<fxa_unordered::map_value_adaptor<K, V>>,
    fxa_unordered::rc::group15>;

template<class K, class V, class H=xm32_hash<K>>
using foa_xm32_unordered_rc16_map =
  foa_unordered_rc_map<
    K, V, H,std::equal_to<K>,
    ::allocator<fxa_unordered::map_value_adaptor<K, V>>,
    fxa_unordered::rc::group16>;

template<class K, class V, class H=xm32_hash<K>>
using foa_xm32_unordered_rc15_map =
  foa_unordered_rc_map<
    K, V, H,std::equal_to<K>,
    ::allocator<fxa_unordered::map_value_adaptor<K, V>>,
    fxa_unordered::rc::group15>;

template<class K, class V, class H=xm33_hash<K>>
using foa_xm33_unordered_rc16_map =
  foa_unordered_rc_map<
    K, V, H,std::equal_to<K>,
    ::allocator<fxa_unordered::map_value_adaptor<K, V>>,
    fxa_unordered::rc::group16>;

template<class K, class V, class H=xm33_hash<K>>
using foa_xm33_unordered_rc15_map =
  foa_unordered_rc_map<
    K, V, H,std::equal_to<K>,
    ::allocator<fxa_unordered::map_value_adaptor<K, V>>,
    fxa_unordered::rc::group15>;

template<class K, class V, class H=xmx32_hash<K>>
using foa_hxmx32_unordered_rc16_map =
  foa_unordered_rc_map<
    K, V, H,std::equal_to<K>,
    ::allocator<fxa_unordered::map_value_adaptor<K, V>>,
    fxa_unordered::rc::group16,
    fxa_unordered::pow2_size,
    fxa_unordered::rc::pow2_prober,
    fxa_unordered::rshift_hash<7>>;

template<class K, class V, class H=xmx32_hash<K>>
using foa_hxmx32_unordered_rc15_map =
  foa_unordered_rc_map<
    K, V, H,std::equal_to<K>,
    ::allocator<fxa_unordered::map_value_adaptor<K, V>>,
    fxa_unordered::rc::group15,
    fxa_unordered::pow2_size,
    fxa_unordered::rc::pow2_prober,
    fxa_unordered::rshift_hash<8>>;

template<class K, class V, class H=xmx33_hash<K>>
using foa_hxmx33_unordered_rc16_map =
  foa_unordered_rc_map<
    K, V, H,std::equal_to<K>,
    ::allocator<fxa_unordered::map_value_adaptor<K, V>>,
    fxa_unordered::rc::group16,
    fxa_unordered::pow2_size,
    fxa_unordered::rc::pow2_prober,
    fxa_unordered::rshift_hash<7>>;

template<class K, class V, class H=xmx33_hash<K>>
using foa_hxmx33_unordered_rc15_map =
  foa_unordered_rc_map<
    K, V, H,std::equal_to<K>,
    ::allocator<fxa_unordered::map_value_adaptor<K, V>>,
    fxa_unordered::rc::group15,
    fxa_unordered::pow2_size,
    fxa_unordered::rc::pow2_prober,
    fxa_unordered::rshift_hash<8>>;

template<class K, class V, class H=xm32_hash<K>>
using foa_hxm32_unordered_rc16_map =
  foa_unordered_rc_map<
    K, V, H,std::equal_to<K>,
    ::allocator<fxa_unordered::map_value_adaptor<K, V>>,
    fxa_unordered::rc::group16,
    fxa_unordered::pow2_size,
    fxa_unordered::rc::pow2_prober,
    fxa_unordered::rshift_hash<7>>;

template<class K, class V, class H=xm32_hash<K>>
using foa_hxm32_unordered_rc15_map =
  foa_unordered_rc_map<
    K, V, H,std::equal_to<K>,
    ::allocator<fxa_unordered::map_value_adaptor<K, V>>,
    fxa_unordered::rc::group15,
    fxa_unordered::pow2_size,
    fxa_unordered::rc::pow2_prober,
    fxa_unordered::rshift_hash<8>>;

template<class K, class V, class H=xm33_hash<K>>
using foa_hxm33_unordered_rc16_map =
  foa_unordered_rc_map<
    K, V, H,std::equal_to<K>,
    ::allocator<fxa_unordered::map_value_adaptor<K, V>>,
    fxa_unordered::rc::group16,
    fxa_unordered::pow2_size,
    fxa_unordered::rc::pow2_prober,
    fxa_unordered::rshift_hash<7>>;

template<class K, class V, class H=xm33_hash<K>>
using foa_hxm33_unordered_rc15_map =
  foa_unordered_rc_map<
    K, V, H,std::equal_to<K>,
    ::allocator<fxa_unordered::map_value_adaptor<K, V>>,
    fxa_unordered::rc::group15,
    fxa_unordered::pow2_size,
    fxa_unordered::rc::pow2_prober,
    fxa_unordered::rshift_hash<8>>;

// fnv1a_hash

template<int Bits> struct fnv1a_hash_impl;

template<> struct fnv1a_hash_impl<32>
{
    std::size_t operator()( std::string const& s ) const
    {
        std::size_t h = 0x811C9DC5u;

        char const * first = s.data();
        char const * last = first + s.size();

        for( ; first != last; ++first )
        {
            h ^= static_cast<unsigned char>( *first );
            h *= 0x01000193ul;
        }

        return h;
    }

    std::size_t operator()( std::string_view const& s ) const
    {
        std::size_t h = 0x811C9DC5u;

        char const * first = s.data();
        char const * last = first + s.size();

        for( ; first != last; ++first )
        {
            h ^= static_cast<unsigned char>( *first );
            h *= 0x01000193ul;
        }

        return h;
    }
};

template<> struct fnv1a_hash_impl<64>
{
    std::size_t operator()( std::string const& s ) const
    {
        std::size_t h = 0xCBF29CE484222325ull;

        char const * first = s.data();
        char const * last = first + s.size();

        for( ; first != last; ++first )
        {
            h ^= static_cast<unsigned char>( *first );
            h *= 0x00000100000001B3ull;
        }

        return h;
    }

    std::size_t operator()( std::string_view const& s ) const
    {
        std::size_t h = 0xCBF29CE484222325ull;

        char const * first = s.data();
        char const * last = first + s.size();

        for( ; first != last; ++first )
        {
            h ^= static_cast<unsigned char>( *first );
            h *= 0x00000100000001B3ull;
        }

        return h;
    }
};

struct fnv1a_hash: fnv1a_hash_impl< std::numeric_limits<std::size_t>::digits > {};

// aliases using the counting allocator

template<class K, class V> using allocator_for = ::allocator< std::pair<K const, V> >;

template<class K, class V> using std_unordered_map =
    std::unordered_map<K, V, std::hash<K>, std::equal_to<K>, allocator_for<K, V>>;

template<class K, class V> using boost_unordered_map =
    boost::unordered_map<K, V, boost::hash<K>, std::equal_to<K>, allocator_for<K, V>>;

template<class K, class V> using fca_simple_unordered_map_ =
  fca_simple_unordered_map<
    K, V, boost::hash<K>, std::equal_to<K>,
    ::allocator<fca_simple_unordered_impl::map_value_adaptor<K, V>>>;

template<class K, class V> using fca_unordered_map_ =
  fca_unordered_map<
    K, V, boost::hash<K>, std::equal_to<K>,
    ::allocator<fxa_unordered::map_value_adaptor<K, V>>>;

#ifdef HAVE_ABSEIL

template<class K, class V> using absl_node_hash_map =
    absl::node_hash_map<K, V, absl::container_internal::hash_default_hash<K>, absl::container_internal::hash_default_eq<K>, allocator_for<K, V>>;

template<class K, class V> using absl_flat_hash_map =
    absl::flat_hash_map<K, V, absl::container_internal::hash_default_hash<K>, absl::container_internal::hash_default_eq<K>, allocator_for<K, V>>;

#endif

template<class K, class V> using std_unordered_map_fnv1a =
    std::unordered_map<K, V, fnv1a_hash, std::equal_to<K>, allocator_for<K, V>>;

template<class K, class V> using boost_unordered_map_fnv1a =
    boost::unordered_map<K, V, fnv1a_hash, std::equal_to<K>, allocator_for<K, V>>;

template<class K, class V> using multi_index_map_fnv1a = multi_index_container<
  pair<K, V>,
  indexed_by<
    hashed_unique< member<pair<K, V>, K, &pair<K, V>::first>, fnv1a_hash >
  >,
  ::allocator< pair<K, V> >
>;

template<class K, class V> using fca_simple_unordered_map_fnv1a =
  fca_simple_unordered_map<
    K, V, fnv1a_hash, std::equal_to<K>,
    ::allocator<fca_simple_unordered_impl::map_value_adaptor<K, V>>>;

template<class K, class V> using fca_unordered_map_fnv1a =
  fca_unordered_map<
    K, V, fnv1a_hash, std::equal_to<K>,
    ::allocator<fxa_unordered::map_value_adaptor<K, V>>>;

template<class K, class V> using fca_switch_unordered_map_fnv1a =
  fca_switch_unordered_map<K, V, fnv1a_hash>;

template<class K, class V> using fca_fmod_unordered_map_fnv1a =
  fca_fmod_unordered_map<K, V, fnv1a_hash>;

template<class K, class V> using fca_frng_unordered_map_fnv1a =
  fca_frng_unordered_map<K, V, fnv1a_hash>;

template<class K, class V> using fca_frng_fib_unordered_map_fnv1a =
  fca_frng_fib_unordered_map<K, V, fnv1a_hash>;

template<class K, class V> using fca_pow2_unordered_map_fnv1a =
  fca_pow2_unordered_map<K, V, fnv1a_hash>;

template<class K, class V> using fca_pow2_fib_unordered_map_fnv1a =
  fca_pow2_fib_unordered_map<K, V, fnv1a_hash>;

template<class K, class V> using fca_fmod_unordered_bucket_map_fnv1a =
  fca_fmod_unordered_bucket_map<K, V, fnv1a_hash>;

template<class K, class V> using fca_fmod_bcached_unordered_bucket_map_fnv1a =
  fca_fmod_bcached_unordered_bucket_map<K, V, fnv1a_hash>;

template<class K, class V> using fca_fmod_unordered_hybrid_map_fnv1a =                                 
  fca_fmod_unordered_hybrid_map<K, V, fnv1a_hash>;

template<class K, class V> using fca_fmod_unordered_hybrid_bucket_map_fnv1a =
  fca_fmod_unordered_hybrid_bucket_map<K, V, fnv1a_hash>;

template<class K, class V> using fca_fmod_bcached_unordered_hybrid_bucket_map_fnv1a =
  fca_fmod_bcached_unordered_hybrid_bucket_map<K, V, fnv1a_hash>;

template<class K, class V> using fca_fmod_unordered_linear_map_fnv1a =                                 
  fca_fmod_unordered_linear_map<K, V, fnv1a_hash>;

template<class K, class V> using fca_fmod_unordered_linear_bucket_map_fnv1a =
  fca_fmod_unordered_linear_bucket_map<K, V, fnv1a_hash>;

template<class K, class V> using fca_fmod_bcached_unordered_linear_bucket_map_fnv1a =
  fca_fmod_bcached_unordered_linear_bucket_map<K, V, fnv1a_hash>;

template<class K, class V> using fca_fmod_unordered_pool_map_fnv1a =                                 
  fca_fmod_unordered_pool_map<K, V, fnv1a_hash>;

template<class K, class V> using fca_fmod_unordered_pool_bucket_map_fnv1a =
  fca_fmod_unordered_pool_bucket_map<K, V, fnv1a_hash>;

template<class K, class V> using fca_fmod_bcached_unordered_pool_bucket_map_fnv1a =
  fca_fmod_bcached_unordered_pool_bucket_map<K, V, fnv1a_hash>;

template<class K, class V> using fca_fmod_unordered_embedded_map_fnv1a =                                 
  fca_fmod_unordered_embedded_map<K, V, fnv1a_hash>;

template<class K, class V> using fca_fmod_unordered_embedded_bucket_map_fnv1a =
  fca_fmod_unordered_embedded_bucket_map<K, V, fnv1a_hash>;

template<class K, class V> using fca_fmod_bcached_unordered_embedded_bucket_map_fnv1a =
  fca_fmod_bcached_unordered_embedded_bucket_map<K, V, fnv1a_hash>;

template<class K, class V> using foa_fmod_unordered_coalesced_map_fnv1a =
  foa_fmod_unordered_coalesced_map<K, V, fnv1a_hash>;

template<class K, class V> using foa_absl_unordered_coalesced_map_fnv1a =
  foa_absl_unordered_coalesced_map<K, V, fnv1a_hash>;

template<class K, class V> using foa_fmod_hcached_unordered_coalesced_map_fnv1a =
  foa_fmod_hcached_unordered_coalesced_map<K, V, fnv1a_hash>;

template<class K, class V> using foa_pow2_fib_unordered_nway_map_fnv1a =
  foa_pow2_fib_unordered_nway_map<K, V, fnv1a_hash>;

template<class K, class V> using foa_fmod_unordered_nwayplus_map_fnv1a =
  foa_fmod_unordered_nwayplus_map<K, V, fnv1a_hash>;

template<class K, class V> using foa_pow2_fib_unordered_nwayplus_map_fnv1a =
  foa_pow2_fib_unordered_nwayplus_map<K, V, fnv1a_hash>;

template<class K, class V> using foa_absl_unordered_nwayplus_map_fnv1a =
  foa_absl_unordered_nwayplus_map<K, V, fnv1a_hash>;

template<class K, class V> using foa_fmod_unordered_soa_nwayplus_map_fnv1a =
  foa_fmod_unordered_soa_nwayplus_map<K, V, fnv1a_hash>;

template<class K, class V> using foa_pow2_fib_unordered_soa_nwayplus_map_fnv1a =
  foa_pow2_fib_unordered_soa_nwayplus_map<K, V, fnv1a_hash>;

template<class K, class V> using foa_frng_fib_unordered_soa_nwayplus_map_fnv1a =
  foa_frng_fib_unordered_soa_nwayplus_map<K, V, fnv1a_hash>;

template<class K, class V> using foa_frng_fib_unordered_soa15_nwayplus_map_fnv1a =
  foa_frng_fib_unordered_soa15_nwayplus_map<K, V, fnv1a_hash>;

template<class K, class V> using foa_absl_unordered_soa_nwayplus_map_fnv1a =
  foa_absl_unordered_soa_nwayplus_map<K, V, fnv1a_hash>;

template<class K, class V> using foa_absl_unordered_intersoa_nwayplus_map_fnv1a =
  foa_absl_unordered_intersoa_nwayplus_map<K, V, fnv1a_hash>;

template<class K, class V> using foa_absl_unordered_soa15_nwayplus_map_fnv1a =
  foa_absl_unordered_soa15_nwayplus_map<K, V, fnv1a_hash>;

template<class K, class V> using foa_absl_unordered_intersoa15_nwayplus_map_fnv1a =
  foa_absl_unordered_intersoa15_nwayplus_map<K, V, fnv1a_hash>;

template<class K, class V> using foa_pow2_fib_unordered_coalesced_nwayplus_map_fnv1a =
  foa_pow2_fib_unordered_coalesced_nwayplus_map<K, V, fnv1a_hash>;
  
template<class K, class V> using foa_pow2_fib_unordered_soa_coalesced_nwayplus_map_fnv1a =
  foa_pow2_fib_unordered_soa_coalesced_nwayplus_map<K, V, fnv1a_hash>;
  
template<class K, class V> using foa_frng_fib_unordered_hopscotch_map_fnv1a =
  foa_frng_fib_unordered_hopscotch_map<K, V, fnv1a_hash>;

template<class K, class V> using foa_absl_unordered_hopscotch_map_fnv1a =
  foa_absl_unordered_hopscotch_map<K, V, fnv1a_hash>;

template<class K, class V> using foa_frng_fib_unordered_longhop_map_fnv1a =
  foa_frng_fib_unordered_longhop_map<K, V, fnv1a_hash>;

template<class K, class V> using foa_absl_unordered_longhop_map_fnv1a =
  foa_absl_unordered_longhop_map<K, V, fnv1a_hash>;

template<class K, class V> using foa_fmod_unordered_rc16_map_fnv1a =
  foa_fmod_unordered_rc16_map<K, V, fnv1a_hash>;

template<class K, class V> using foa_fmod_unordered_rc15_map_fnv1a =
  foa_fmod_unordered_rc15_map<K, V, fnv1a_hash>;

template<class K, class V> using foa_fmodxm_unordered_rc16_map_fnv1a =
  foa_fmodxm_unordered_rc16_map<K, V, fnv1a_hash>;

template<class K, class V> using foa_fmodxm_unordered_rc15_map_fnv1a =
  foa_fmodxm_unordered_rc15_map<K, V, fnv1a_hash>;

template<class K, class V> using foa_absl_unordered_rc16_map_fnv1a =
  foa_absl_unordered_rc16_map<K, V, fnv1a_hash>;

template<class K, class V> using foa_absl_unordered_rc15_map_fnv1a =
  foa_absl_unordered_rc15_map<K, V, fnv1a_hash>;

template<class K, class V>
using foa_mulx_unordered_rc15_map_fnv1a =
  foa_unordered_rc_map<
    K, V, mulx_hash<K, fnv1a_hash>, std::equal_to<K>,
    ::allocator<fxa_unordered::map_value_adaptor<K, V>>,
    fxa_unordered::rc::group15>;

template<class K, class V>
using foa_xmx_unordered_rc15_map_fnv1a =
  foa_unordered_rc_map<
    K, V, xmx_hash<K, fnv1a_hash>, std::equal_to<K>,
    ::allocator<fxa_unordered::map_value_adaptor<K, V>>,
    fxa_unordered::rc::group15>;

template<class K, class V>
using foa_hxm2_unordered_rc15_map_fnv1a =
  foa_unordered_rc_map<
    K, V, xm2_hash<K, fnv1a_hash>, std::equal_to<K>,
    ::allocator<fxa_unordered::map_value_adaptor<K, V>>,
    fxa_unordered::rc::group15,
    fxa_unordered::pow2_size,
    fxa_unordered::rc::pow2_prober,
    fxa_unordered::rshift_hash<8>>;

template<class K, class V>
using foa_xmx33_unordered_rc15_map_fnv1a =
  foa_unordered_rc_map<
    K, V, xmx33_hash<K, fnv1a_hash>, std::equal_to<K>,
    ::allocator<fxa_unordered::map_value_adaptor<K, V>>,
    fxa_unordered::rc::group15>;

template<class K, class V>
using foa_hxm33_unordered_rc15_map_fnv1a =
  foa_unordered_rc_map<
    K, V, xm33_hash<K, fnv1a_hash>, std::equal_to<K>,
    ::allocator<fxa_unordered::map_value_adaptor<K, V>>,
    fxa_unordered::rc::group15,
    fxa_unordered::pow2_size,
    fxa_unordered::rc::pow2_prober,
    fxa_unordered::rshift_hash<8>>;

#ifdef HAVE_ABSEIL

template<class K, class V> using absl_node_hash_map_fnv1a =
    absl::node_hash_map<K, V, fnv1a_hash, std::equal_to<K>, allocator_for<K, V>>;

template<class K, class V> using absl_flat_hash_map_fnv1a =
    absl::flat_hash_map<K, V, fnv1a_hash, std::equal_to<K>, allocator_for<K, V>>;

#endif

#ifdef HAVE_ABSEIL
# include "absl/container/internal/raw_hash_set.cc"
# include "absl/hash/internal/hash.cc"
# include "absl/hash/internal/low_level_hash.cc"
# include "absl/hash/internal/city.cc"
#endif

#endif
