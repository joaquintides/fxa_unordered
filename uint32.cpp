// Copyright 2021 Peter Dimov.
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt
// Modified by Joaquin M Lopez Munoz: added fca[_*]_unordered[_*]_map
// Copyright 2022 Joaquin M Lopez Munoz.
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#define _SILENCE_CXX17_OLD_ALLOCATOR_MEMBERS_DEPRECATION_WARNING

#include <boost/unordered_map.hpp>
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/core/detail/splitmix64.hpp>
#include "fca_simple_unordered.hpp"
#include "fca_unordered.hpp"
#ifdef HAVE_ABSEIL
# include "absl/container/node_hash_map.h"
# include "absl/container/flat_hash_map.h"
#endif
#include <algorithm>
#include <unordered_map>
#include <map>
#include <cstdint>
#include <iostream>
#include <iomanip>
#include <chrono>

using namespace std::chrono_literals;

static void print_time( std::chrono::steady_clock::time_point & t1, char const* label, std::uint32_t s, std::size_t size )
{
    auto t2 = std::chrono::steady_clock::now();

    std::cout << label << ": " << ( t2 - t1 ) / 1ms << " ms (s=" << s << ", size=" << size << ")\n";

    t1 = t2;
}

constexpr unsigned N = 2'000'000;
constexpr int K = 10;

static std::vector< std::uint32_t > indices1, indices2, indices3;

static void init_indices()
{
    indices1.push_back( 0 );

    for( unsigned i = 1; i <= N*2; ++i )
    {
        indices1.push_back( i );
    }

    indices2.push_back( 0 );

    {
        boost::detail::splitmix64 rng;

        for( unsigned i = 1; i <= N*2; ++i )
        {
            indices2.push_back( static_cast<std::uint32_t>( rng() ) );
        }
    }

    indices3.push_back( 0 );

    for( unsigned i = 1; i <= N*2; ++i )
    {
        indices3.push_back( (std::uint32_t)i << 11 );
    }
}

template<class Map> void test_insert( Map& map, std::chrono::steady_clock::time_point & t1 )
{
    for( unsigned i = 1; i <= N; ++i )
    {
        map.insert( { indices1[ i ], i } );
    }

    print_time( t1, "Consecutive insert",  0, map.size() );

    for( unsigned i = 1; i <= N; ++i )
    {
        map.insert( { indices2[ i ], i } );
    }

    print_time( t1, "Random insert",  0, map.size() );

    for( unsigned i = 1; i <= N; ++i )
    {
        map.insert( { indices3[ i ], i } );
    }

    print_time( t1, "Consecutive shifted insert",  0, map.size() );

    std::cout << std::endl;
}

template<class Map> void test_lookup( Map& map, std::chrono::steady_clock::time_point & t1 )
{
    std::uint32_t s;
    
    s = 0;

    for( int j = 0; j < K; ++j )
    {
        for( unsigned i = 1; i <= N * 2; ++i )
        {
            auto it = map.find( indices1[ i ] );
            if( it != map.end() ) s += it->second;
        }
    }

    print_time( t1, "Consecutive lookup",  s, map.size() );

    s = 0;

    for( int j = 0; j < K; ++j )
    {
        for( unsigned i = 1; i <= N * 2; ++i )
        {
            auto it = map.find( indices2[ i ] );
            if( it != map.end() ) s += it->second;
        }
    }

    print_time( t1, "Random lookup",  s, map.size() );

    s = 0;

    for( int j = 0; j < K; ++j )
    {
        for( unsigned i = 1; i <= N * 2; ++i )
        {
            auto it = map.find( indices3[ i ] );
            if( it != map.end() ) s += it->second;
        }
    }

    print_time( t1, "Consecutive shifted lookup",  s, map.size() );

    std::cout << std::endl;
}

template<class Map> void test_iteration( Map& map, std::chrono::steady_clock::time_point & t1 )
{
    auto it = map.begin();

    while( it != map.end() )
    {
        if( it->second & 1 )
        {
            map.erase( it++ );
        }
        else
        {
            ++it;
        }
    }

    print_time( t1, "Iterate and erase odd elements",  0, map.size() );

    std::cout << std::endl;
}

template<class Map> void test_erase( Map& map, std::chrono::steady_clock::time_point & t1 )
{
    for( unsigned i = 1; i <= N; ++i )
    {
        map.erase( indices1[ i ] );
    }

    print_time( t1, "Consecutive erase",  0, map.size() );

    {
        boost::detail::splitmix64 rng;

        for( unsigned i = 1; i <= N; ++i )
        {
            map.erase( indices2[ i ] );
        }
    }

    print_time( t1, "Random erase",  0, map.size() );

    for( unsigned i = 1; i <= N; ++i )
    {
        map.erase( indices3[ i ] );
    }

    print_time( t1, "Consecutive shifted erase",  0, map.size() );

    std::cout << std::endl;
}

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

//

struct record
{
    std::string label_;
    long long time_;
    std::size_t bytes_;
    std::size_t count_;
};

static std::vector<record> times;

template<template<class...> class Map> void test( char const* label )
{
    std::cout << label << ":\n\n";

    s_alloc_bytes = 0;
    s_alloc_count = 0;

    Map<std::uint32_t, std::uint32_t> map;

    auto t0 = std::chrono::steady_clock::now();
    auto t1 = t0;

    test_insert( map, t1 );

    std::cout << "Memory: " << s_alloc_bytes << " bytes in " << s_alloc_count << " allocations\n\n";
    record rec = { label, 0, s_alloc_bytes, s_alloc_count };
    
    test_lookup( map, t1 );
    test_iteration( map, t1 );
    test_lookup( map, t1 );
    test_erase( map, t1 );

    auto tN = std::chrono::steady_clock::now();
    std::cout << "Total: " << ( tN - t0 ) / 1ms << " ms\n\n";

    rec.time_ = ( tN - t0 ) / 1ms;
    times.push_back( rec );
}

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

// aliases using the counting allocator

template<class K, class V> using allocator_for = ::allocator< std::pair<K const, V> >;

template<class K, class V> using std_unordered_map =
    std::unordered_map<K, V, std::hash<K>, std::equal_to<K>, allocator_for<K, V>>;

template<class K, class V> using boost_unordered_map =
    boost::unordered_map<K, V, boost::hash<K>, std::equal_to<K>, allocator_for<K, V>>;

#ifdef HAVE_ABSEIL

template<class K, class V> using absl_node_hash_map =
    absl::node_hash_map<K, V, absl::container_internal::hash_default_hash<K>, absl::container_internal::hash_default_eq<K>, allocator_for<K, V>>;

template<class K, class V> using absl_flat_hash_map =
    absl::flat_hash_map<K, V, absl::container_internal::hash_default_hash<K>, absl::container_internal::hash_default_eq<K>, allocator_for<K, V>>;

#endif

// alternative size policies for fca_unordered

template<class K, class V, class H=boost::hash<K>>
using fca_switch_unordered_map =
  fca_unordered_map<
    K, V, H,std::equal_to<K>,
    ::allocator<fca_unordered_impl::map_value_adaptor<K, V>>,
    fca_unordered_impl::prime_switch_size>;

template<class K, class V, class H=boost::hash<K>>
using fca_fmod_unordered_map =
  fca_unordered_map<
    K, V, H,std::equal_to<K>,
    ::allocator<fca_unordered_impl::map_value_adaptor<K, V>>,
    fca_unordered_impl::prime_fmod_size>;

template<class K, class V, class H=boost::hash<K>>
using fca_frng_unordered_map =
  fca_unordered_map<
    K, V, H,std::equal_to<K>,
    ::allocator<fca_unordered_impl::map_value_adaptor<K, V>>,
    fca_unordered_impl::prime_frng_size>;

template<class K, class V, class H=boost::hash<K>>
using fca_frng_fib_unordered_map =
  fca_unordered_map<
    K, V, H,std::equal_to<K>,
    ::allocator<fca_unordered_impl::map_value_adaptor<K, V>>,
    fca_unordered_impl::prime_frng_fib_size>;

template<class K, class V, class H=boost::hash<K>>
using fca_pow2_unordered_map =
  fca_unordered_map<
    K, V, H,std::equal_to<K>,
    ::allocator<fca_unordered_impl::map_value_adaptor<K, V>>,
    fca_unordered_impl::pow2_size>;

template<class K, class V, class H=boost::hash<K>>
using fca_pow2_fib_unordered_map =
  fca_unordered_map<
    K, V, H,std::equal_to<K>,
    ::allocator<fca_unordered_impl::map_value_adaptor<K, V>>,
    fca_unordered_impl::pow2_fib_size>;

template<class K, class V, class H=boost::hash<K>>
using fca_fmod_unordered_bucket_map =
  fca_unordered_map<
    K, V, H,std::equal_to<K>,
    ::allocator<fca_unordered_impl::map_value_adaptor<K, V>>,
    fca_unordered_impl::prime_fmod_size,
    fca_unordered_impl::simple_buckets>;

template<class K, class V, class H=boost::hash<K>>
using fca_fmod_bcached_unordered_bucket_map =
  fca_unordered_map<
    K, V, H,std::equal_to<K>,
    ::allocator<fca_unordered_impl::map_value_adaptor<K, V>>,
    fca_unordered_impl::prime_fmod_size,
    fca_unordered_impl::bcached_simple_buckets>;

template<class K, class V, class H=boost::hash<K>>
using fca_fmod_unordered_hybrid_map =
  fca_unordered_map<
    K, V, H,std::equal_to<K>,
    ::allocator<fca_unordered_impl::map_value_adaptor<K, V>>,
    fca_unordered_impl::prime_fmod_size,
    fca_unordered_impl::grouped_buckets,
    fca_unordered_impl::hybrid_node_allocation>;

template<class K, class V, class H=boost::hash<K>>
using fca_fmod_unordered_hybrid_bucket_map =
  fca_unordered_map<
    K, V, H,std::equal_to<K>,
    ::allocator<fca_unordered_impl::map_value_adaptor<K, V>>,
    fca_unordered_impl::prime_fmod_size,
    fca_unordered_impl::simple_buckets,
    fca_unordered_impl::hybrid_node_allocation>;

template<class K, class V, class H=boost::hash<K>>
using fca_fmod_bcached_unordered_hybrid_bucket_map =
  fca_unordered_map<
    K, V, H,std::equal_to<K>,
    ::allocator<fca_unordered_impl::map_value_adaptor<K, V>>,
    fca_unordered_impl::prime_fmod_size,
    fca_unordered_impl::bcached_simple_buckets,
    fca_unordered_impl::hybrid_node_allocation>;

int main()
{
    init_indices();

    test<std::unordered_map>( "std::unordered_map" );
    test<boost::unordered_map>( "boost::unordered_map" );
    test<multi_index_map>( "multi_index_map" );

#ifdef BENCHMARK_EVERYTHING
    test<fca_simple_unordered_map>( "fca_simple_unordered_map" );
    test<fca_unordered_map>( "fca_unordered_map" );
    test<fca_switch_unordered_map>( "fca_switch_unordered_map" );
#endif

    test<fca_fmod_unordered_map>( "fca_fmod_unordered_map" );

#ifdef BENCHMARK_EVERYTHING
    // frng is spectacularly slow for consecutive uint64 insertion
    // (as expected, boost::hash is the identity and position ignores low bits)
    // test<fca_frng_unordered_map>( "fca_frng_unordered_map" );
    
    test<fca_frng_fib_unordered_map>( "fca_frng_fib_unordered_map" );

    // same as frng
    // test<fca_pow2_unordered_map>( "fca_pow2_unordered_map" );
#endif    
    
    test<fca_pow2_fib_unordered_map>( "fca_pow2_fib_unordered_map" );
    test<fca_fmod_unordered_bucket_map>( "fca_fmod_unordered_bucket_map" );

#ifdef BENCHMARK_EVERYTHING
    test<fca_fmod_bcached_unordered_bucket_map>( "fca_fmod_bcached_unordered_bucket_map" );
    test<fca_fmod_unordered_hybrid_map>( "fca_fmod_unordered_hybrid_map" );
#endif
    
    test<fca_fmod_unordered_hybrid_bucket_map>( "fca_fmod_unordered_hybrid_bucket_map" );

#ifdef BENCHMARK_EVERYTHING    
    test<fca_fmod_bcached_unordered_hybrid_bucket_map>( "fca_fmod_bcached_unordered_hybrid_bucket_map" );
#endif
    
#ifdef HAVE_ABSEIL
    test<absl::node_hash_map>( "absl::node_hash_map" );
    test<absl::flat_hash_map>( "absl::flat_hash_map" );
#endif

    std::cout << "---\n\n";

    int label_witdh = 0;
    for( auto const& x: times ) label_witdh = (std::max)((int)( x.label_ + ": " ).size(), label_witdh);
    
    for( auto const& x: times )
    {
        std::cout << std::setw( label_witdh ) << ( x.label_ + ": " ) << std::setw( 5 ) << x.time_ << " ms, " << std::setw( 9 ) << x.bytes_ << " bytes in " << x.count_ << " allocations\n";
    }
}

#ifdef HAVE_ABSEIL
# include "absl/container/internal/raw_hash_set.cc"
# include "absl/hash/internal/hash.cc"
# include "absl/hash/internal/low_level_hash.cc"
# include "absl/hash/internal/city.cc"
#endif
