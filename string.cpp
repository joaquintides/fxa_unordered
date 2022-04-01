// Copyright 2021 Peter Dimov.
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt
//
// Modified by Joaquin M Lopez Munoz: extended for fxa_unordered
// Copyright 2022 Joaquin M Lopez Munoz.
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/algorithm/minmax_element.hpp>
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <vector>
#include "container_defs.hpp"

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
 
    Map<std::string, std::uint32_t> map;

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

#if ((SIZE_MAX>>16)>>16)==0 
#define IN_32BIT_ARCHITECTURE
#endif

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
#endif

#if defined(IN_32BIT_ARCHITECTURE)
    test<fca_frng_fib_unordered_map_fnv1a>( "fca_frng_fib_unordered_map, FNV-1a" );
#endif

#ifdef BENCHMARK_EVERYTHING
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
    test<fca_fmod_unordered_linear_map_fnv1a>( "fca_fmod_unordered_linear_map, FNV-1a" );
#endif

    test<fca_fmod_unordered_linear_bucket_map_fnv1a>( "fca_fmod_unordered_linear_bucket_map, FNV-1a" );

#ifdef BENCHMARK_EVERYTHING
    test<fca_fmod_bcached_unordered_linear_bucket_map_fnv1a>( "fca_fmod_bcached_unordered_linear_bucket_map_fnv1a, FNV-1a" );
    test<fca_fmod_unordered_pool_map_fnv1a>( "fca_fmod_unordered_pool_map, FNV-1a" );
#endif

    test<fca_fmod_unordered_pool_bucket_map_fnv1a>( "fca_fmod_unordered_pool_bucket_map, FNV-1a" );

#ifdef BENCHMARK_EVERYTHING
    test<fca_fmod_bcached_unordered_pool_bucket_map_fnv1a>( "fca_fmod_bcached_unordered_pool_bucket_map_fnv1a, FNV-1a" );  
    test<fca_fmod_unordered_embedded_map_fnv1a>( "fca_fmod_unordered_embedded_map, FNV-1a" );
#endif

    test<fca_fmod_unordered_embedded_bucket_map_fnv1a>( "fca_fmod_unordered_embedded_bucket_map, FNV-1a" );

#ifdef BENCHMARK_EVERYTHING
    test<fca_fmod_bcached_unordered_embedded_bucket_map_fnv1a>( "fca_fmod_bcached_unordered_embedded_bucket_map_fnv1a, FNV-1a" );
#endif

    test<foa_fmod_unordered_coalesced_map_fnv1a>( "foa_fmod_unordered_coalesced_map, FNV-1a" );
    test<foa_fmod_hcached_unordered_coalesced_map_fnv1a>( "foa_fmod_hcached_unordered_coalesced_map, FNV-1a" );
    test<foa_pow2_fib_unordered_nway_map_fnv1a>( "foa_pow2_fib_unordered_nway_map, FNV-1a" );
    test<foa_pow2_fib_unordered_nwayplus_map_fnv1a>( "foa_pow2_fib_unordered_nwayplus_map, FNV-1a" );

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

    auto precision = std::cout.precision();
    std::cout << std::fixed <<std::setprecision(2);

    auto [pmint, pmaxt] = boost::minmax_element(
        times.begin(), times.end(), [](const record& x, const record& y){ return x.time_< y.time_; });
    auto [pminb, pmaxb] = boost::minmax_element(
        times.begin(), times.end(), [](const record& x, const record& y){ return x.bytes_< y.bytes_; });

    std::cout << "\n" << std::setw( 28 ) << "Time(worst)/time(best): " << (float)(pmaxt->time_) / pmint->time_ << "\n"; 
    std::cout << "Memory(worst)/memory(best): " << (float)(pmaxb->bytes_) / pminb->bytes_ << "\n"; 

    std::cout << std::setprecision(precision) << std::defaultfloat<< "\n";
}
