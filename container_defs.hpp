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
#ifdef HAVE_ABSEIL
# include "absl/container/node_hash_map.h"
# include "absl/container/flat_hash_map.h"
#endif
#include <memory>
#include <string>
#include <unordered_map>

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

// alternative size policies / bucket arrays for fca_unordered

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

template<class K, class V, class H=absl::container_internal::hash_default_hash<K>>
using foa_absl_unordered_soa_nwayplus_map =
  foa_unordered_nwayplus_map<
    K, V, H,std::equal_to<K>,
    ::allocator<fxa_unordered::map_value_adaptor<K, V>>,
    fxa_unordered::pow2_size,
    fxa_unordered::shift_hash<7>,
    fxa_unordered::nwayplus::soa_allocation>;

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

template<class K, class V> using foa_fmod_hcached_unordered_coalesced_map_fnv1a =
  foa_fmod_hcached_unordered_coalesced_map<K, V, fnv1a_hash>;

template<class K, class V> using foa_pow2_fib_unordered_nway_map_fnv1a =
  foa_pow2_fib_unordered_nway_map<K, V, fnv1a_hash>;

template<class K, class V> using foa_fmod_unordered_nwayplus_map_fnv1a =
  foa_fmod_unordered_nwayplus_map<K, V, fnv1a_hash>;

template<class K, class V> using foa_pow2_fib_unordered_nwayplus_map_fnv1a =
  foa_pow2_fib_unordered_nwayplus_map<K, V, fnv1a_hash>;

template<class K, class V> using foa_fmod_unordered_soa_nwayplus_map_fnv1a =
  foa_fmod_unordered_soa_nwayplus_map<K, V, fnv1a_hash>;

template<class K, class V> using foa_pow2_fib_unordered_soa_nwayplus_map_fnv1a =
  foa_pow2_fib_unordered_soa_nwayplus_map<K, V, fnv1a_hash>;

template<class K, class V> using foa_frng_fib_unordered_soa_nwayplus_map_fnv1a =
  foa_frng_fib_unordered_soa_nwayplus_map<K, V, fnv1a_hash>;

template<class K, class V> using foa_absl_unordered_soa_nwayplus_map_fnv1a =
  foa_absl_unordered_soa_nwayplus_map<K, V, fnv1a_hash>;

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
