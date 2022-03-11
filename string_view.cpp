// Copyright 2021 Peter Dimov.
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt
//
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
#include <boost/config.hpp>
#include "fca_simple_unordered.hpp"
#include "fca_unordered.hpp"
#ifdef HAVE_ABSEIL
# include "absl/container/node_hash_map.h"
# include "absl/container/flat_hash_map.h"
#endif
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string_view>
#include <unordered_map>
#include <vector>

using namespace std::chrono_literals;

static void print_time( std::chrono::steady_clock::time_point & t1, char const* label, std::uint32_t s, std::size_t size )
{
    auto t2 = std::chrono::steady_clock::now();

    std::cout << label << ": " << ( t2 - t1 ) / 1ms << " ms (s=" << s << ", size=" << size << ")\n";

    t1 = t2;
}

constexpr unsigned N = 2'000'000;
constexpr int K = 10;

static std::vector<std::string> indices1, indices2;

static std::string make_index( unsigned x )
{
    char buffer[ 64 ];
    std::snprintf( buffer, sizeof(buffer), "pfx_%u_sfx", x );

    return buffer;
}

static std::string make_random_index( unsigned x )
{
    char buffer[ 64 ];
    std::snprintf( buffer, sizeof(buffer), "pfx_%0*d_%u_sfx", x % 8 + 1, 0, x );

    return buffer;
}

static void init_indices()
{
    indices1.reserve( N*2+1 );
    indices1.push_back( make_index( 0 ) );

    for( unsigned i = 1; i <= N*2; ++i )
    {
        indices1.push_back( make_index( i ) );
    }

    indices2.reserve( N*2+1 );
    indices2.push_back( make_index( 0 ) );

    {
        boost::detail::splitmix64 rng;

        for( unsigned i = 1; i <= N*2; ++i )
        {
            indices2.push_back( make_random_index( static_cast<std::uint32_t>( rng() ) ) );
        }
    }
}

template<class Map> void BOOST_NOINLINE test_insert( Map& map, std::chrono::steady_clock::time_point & t1 )
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

    std::cout << std::endl;
}

template<class Map> void BOOST_NOINLINE test_lookup( Map& map, std::chrono::steady_clock::time_point & t1 )
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

    std::cout << std::endl;
}

template<class Map> void BOOST_NOINLINE test_iteration( Map& map, std::chrono::steady_clock::time_point & t1 )
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

template<class Map> void BOOST_NOINLINE test_erase( Map& map, std::chrono::steady_clock::time_point & t1 )
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

template<template<class...> class Map> void BOOST_NOINLINE test( char const* label )
{
    std::cout << label << ":\n\n";

    s_alloc_bytes = 0;
    s_alloc_count = 0;
 
    Map<std::string_view, std::uint32_t> map;

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

// alternative size policies / bucket arrays for fca_unordered

template<class K, class V, class H=boost::hash<K>>
using fca_switch_unordered_map =
  fca_unordered_map<
    K, V, H, std::equal_to<K>,
    ::allocator<fca_unordered_impl::map_value_adaptor<K, V>>,
    fca_unordered_impl::prime_switch_size>;

template<class K, class V, class H=boost::hash<K>>
using fca_fmod_unordered_map =
  fca_unordered_map<
    K, V, H, std::equal_to<K>,
    ::allocator<fca_unordered_impl::map_value_adaptor<K, V>>,
    fca_unordered_impl::prime_fmod_size>;

template<class K, class V, class H=boost::hash<K>>
using fca_frng_unordered_map =
  fca_unordered_map<
    K, V, H, std::equal_to<K>,
    ::allocator<fca_unordered_impl::map_value_adaptor<K, V>>,
    fca_unordered_impl::prime_frng_size>;

template<class K, class V, class H=boost::hash<K>>
using fca_frng_fib_unordered_map =
  fca_unordered_map<
    K, V, H, std::equal_to<K>,
    ::allocator<fca_unordered_impl::map_value_adaptor<K, V>>,
    fca_unordered_impl::prime_frng_fib_size>;

template<class K, class V, class H=boost::hash<K>>
using fca_pow2_unordered_map =
  fca_unordered_map<
    K, V, H, std::equal_to<K>,
    ::allocator<fca_unordered_impl::map_value_adaptor<K, V>>,
    fca_unordered_impl::pow2_size>;

template<class K, class V, class H=boost::hash<K>>
using fca_pow2_fib_unordered_map =
  fca_unordered_map<
    K, V, H, std::equal_to<K>,
    ::allocator<fca_unordered_impl::map_value_adaptor<K, V>>,
    fca_unordered_impl::pow2_fib_size>;

template<class K, class V, class H=boost::hash<K>>
using fca_fmod_unordered_bucket_map =
  fca_unordered_map<
    K, V, H, std::equal_to<K>,
    ::allocator<fca_unordered_impl::map_value_adaptor<K, V>>,
    fca_unordered_impl::prime_fmod_size,
    fca_unordered_impl::simple_buckets>;

template<class K, class V, class H=boost::hash<K>>
using fca_fmod_bcached_unordered_bucket_map =
  fca_unordered_map<
    K, V, H, std::equal_to<K>,
    ::allocator<fca_unordered_impl::map_value_adaptor<K, V>>,
    fca_unordered_impl::prime_fmod_size,
    fca_unordered_impl::bcached_simple_buckets>;

template<class K, class V, class H=boost::hash<K>>
using fca_fmod_unordered_hybrid_map =
  fca_unordered_map<
    K, V, H, std::equal_to<K>,
    ::allocator<fca_unordered_impl::map_value_adaptor<K, V>>,
    fca_unordered_impl::prime_fmod_size,
    fca_unordered_impl::grouped_buckets,
    fca_unordered_impl::hybrid_node_allocation>;

template<class K, class V, class H=boost::hash<K>>
using fca_fmod_unordered_hybrid_bucket_map =
  fca_unordered_map<
    K, V, H, std::equal_to<K>,
    ::allocator<fca_unordered_impl::map_value_adaptor<K, V>>,
    fca_unordered_impl::prime_fmod_size,
    fca_unordered_impl::simple_buckets,
    fca_unordered_impl::hybrid_node_allocation>;

template<class K, class V, class H=boost::hash<K>>
using fca_fmod_bcached_unordered_hybrid_bucket_map =
  fca_unordered_map<
    K, V, H, std::equal_to<K>,
    ::allocator<fca_unordered_impl::map_value_adaptor<K, V>>,
    fca_unordered_impl::prime_fmod_size,
    fca_unordered_impl::bcached_simple_buckets,
    fca_unordered_impl::hybrid_node_allocation>;

template<class K, class V, class H=boost::hash<K>>
using fca_fmod_unordered_linear_map =
  fca_unordered_map<
    K, V, H, std::equal_to<K>,
    ::allocator<fca_unordered_impl::map_value_adaptor<K, V>>,
    fca_unordered_impl::prime_fmod_size,
    fca_unordered_impl::grouped_buckets,
    fca_unordered_impl::linear_node_allocation>;

template<class K, class V, class H=boost::hash<K>>
using fca_fmod_unordered_linear_bucket_map =
  fca_unordered_map<
    K, V, H, std::equal_to<K>,
    ::allocator<fca_unordered_impl::map_value_adaptor<K, V>>,
    fca_unordered_impl::prime_fmod_size,
    fca_unordered_impl::simple_buckets,
    fca_unordered_impl::linear_node_allocation>;

template<class K, class V, class H=boost::hash<K>>
using fca_fmod_bcached_unordered_linear_bucket_map =
  fca_unordered_map<
    K, V, H, std::equal_to<K>,
    ::allocator<fca_unordered_impl::map_value_adaptor<K, V>>,
    fca_unordered_impl::prime_fmod_size,
    fca_unordered_impl::bcached_simple_buckets,
    fca_unordered_impl::linear_node_allocation>;

// fnv1a_hash

template<int Bits> struct fnv1a_hash_impl;

template<> struct fnv1a_hash_impl<32>
{
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
    ::allocator<fca_unordered_impl::map_value_adaptor<K, V>>>;

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

template<class K, class V> using fca_fmod_bcached_unordered_lineard_bucket_map_fnv1a =
  fca_fmod_bcached_unordered_linear_bucket_map<K, V, fnv1a_hash>;

#ifdef HAVE_ABSEIL

template<class K, class V> using absl_node_hash_map_fnv1a =
    absl::node_hash_map<K, V, fnv1a_hash, std::equal_to<K>, allocator_for<K, V>>;

template<class K, class V> using absl_flat_hash_map_fnv1a =
    absl::flat_hash_map<K, V, fnv1a_hash, std::equal_to<K>, allocator_for<K, V>>;

#endif
//

int main()
{
    init_indices();
    
    test<std_unordered_map_fnv1a>( "std::unordered_map, FNV-1a" );
    test<boost_unordered_map_fnv1a>( "boost::unordered_map, FNV-1a" );
    test<multi_index_map_fnv1a>( "multi_index_map, FNV-1a" );

#ifdef BENCHMARK_EVERYTHING
    test<fca_simple_unordered_map_fnv1a>( "fca_simple_unordered_map, FNV-1a" );
    test<fca_unordered_map_fnv1a>( "fca_unordered_map, FNV-1a" );
    test<fca_switch_unordered_map_fnv1a>( "fca_switch_unordered_map, FNV-1a" );
#endif

    test<fca_fmod_unordered_map_fnv1a>( "fca_fmod_unordered_map, FNV-1a" );

#ifdef BENCHMARK_EVERYTHING
    test<fca_frng_unordered_map_fnv1a>( "fca_frng_unordered_map, FNV-1a" );
    test<fca_frng_fib_unordered_map_fnv1a>( "fca_frng_fib_unordered_map, FNV-1a" );
    test<fca_pow2_unordered_map_fnv1a>( "fca_pow2_unordered_map, FNV-1a" );
#endif
    
    test<fca_pow2_fib_unordered_map_fnv1a>( "fca_pow2_fib_unordered_map, FNV-1a" );
    test<fca_fmod_unordered_bucket_map_fnv1a>( "fca_fmod_unordered_bucket_map, FNV-1a" );

#ifdef BENCHMARK_EVERYTHING
    test<fca_fmod_bcached_unordered_bucket_map_fnv1a>( "fca_fmod_bcached_unordered_bucket_map, FNV-1a" );
    test<fca_fmod_unordered_hybrid_map_fnv1a>( "fca_fmod_unordered_hybrid_map, FNV-1a" );
#endif

    test<fca_fmod_unordered_hybrid_bucket_map_fnv1a>( "fca_fmod_unordered_hybrid_bucket_map, FNV-1a" );

#ifdef BENCHMARK_EVERYTHING
    test<fca_fmod_bcached_unordered_hybrid_bucket_map_fnv1a>( "fca_fmod_bcached_unordered_hybrid_bucket_map_fnv1a, FNV-1a" );
#endif

    test<fca_fmod_unordered_linear_map_fnv1a>( "fca_fmod_unordered_linear_map, FNV-1a" );
    test<fca_fmod_unordered_linear_bucket_map_fnv1a>( "fca_fmod_unordered_lineard_bucket_map, FNV-1a" );

#ifdef BENCHMARK_EVERYTHING
    test<fca_fmod_bcached_unordered_linear_bucket_map_fnv1a>( "fca_fmod_bcached_unordered_linear_bucket_map_fnv1a, FNV-1a" );
#endif
    
#ifdef HAVE_ABSEIL
    test<absl_node_hash_map_fnv1a>( "absl::node_hash_map, FNV-1a" );
    test<absl_flat_hash_map_fnv1a>( "absl::flat_hash_map, FNV-1a" );
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
