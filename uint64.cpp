// Copyright 2021 Peter Dimov.
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt
// Modified by Joaquin M Lopez Munoz: extended for fxa_unordered
// Copyright 2022 Joaquin M Lopez Munoz.
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/algorithm/minmax_element.hpp>
#include <boost/endian/conversion.hpp>
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <vector>
#include "container_defs.hpp"

using namespace std::chrono_literals;

static void print_time( std::chrono::steady_clock::time_point & t1, char const* label, std::uint64_t s, std::size_t size )
{
    auto t2 = std::chrono::steady_clock::now();

    std::cout << label << ": " << ( t2 - t1 ) / 1ms << " ms (s=" << s << ", size=" << size << ")\n";

    t1 = t2;
}

constexpr unsigned N = 2'000'000;
constexpr int K = 10;

static std::vector< std::uint64_t > indices1, indices2, indices3;

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
            indices2.push_back( rng() );
        }
    }

    indices3.push_back( 0 );

    for( unsigned i = 1; i <= N*2; ++i )
    {
        indices3.push_back( boost::endian::endian_reverse( static_cast<std::uint64_t>( i ) ) );
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

    for( unsigned i = 1; i <= N; ++i )
    {
        map.insert( { indices3[ i ], i } );
    }

    print_time( t1, "Consecutive reversed insert",  0, map.size() );

    std::cout << "Fingerprint: ";
    std::size_t seed = 0;
    for( const auto& x: map )
    {
      boost::hash_combine( seed, x.first );
    }
    std::cout << seed << std::endl;

    std::cout << std::endl;
}

template<class Map> void BOOST_NOINLINE test_lookup( Map& map, std::chrono::steady_clock::time_point & t1 )
{
    std::uint64_t s;
    
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

    print_time( t1, "Consecutive reversed lookup",  s, map.size() );

    std::cout << std::endl;
}

template<class Map> void BOOST_NOINLINE test_iteration( Map& map, std::chrono::steady_clock::time_point & t1 )
{
    auto it = map.begin();

    while( it != map.end() )
    {
        if( it->second & 1 )
        {
          if constexpr( std::is_void_v< decltype( map.erase( it ) ) > )
          {
            map.erase( it++ );
          }
          else
          {
            it = map.erase( it );    
          }
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

    for( unsigned i = 1; i <= N; ++i )
    {
        map.erase( indices2[ i ] );
    }

    print_time( t1, "Random erase",  0, map.size() );

    for( unsigned i = 1; i <= N; ++i )
    {
        map.erase( indices3[ i ] );
    }

    print_time( t1, "Consecutive reversed erase",  0, map.size() );

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
    
    Map<std::uint64_t, std::uint64_t> map;

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

#if defined(BOOST_LIBSTDCXX_VERSION) && __SIZE_WIDTH__ == 32
    // Pathological behavior:
    // https://gcc.gnu.org/bugzilla/show_bug.cgi?id=104945
#else
    test<std_unordered_map>( "std::unordered_map" );
#endif

    test<boost_unordered_map>( "boost::unordered_map" );
    test<multi_index_map>( "multi_index_map" );

#ifdef BENCHMARK_EVERYTHING
    test<fca_simple_unordered_map_>( "fca_simple_unordered_map" );
    test<fca_unordered_map_>( "fca_unordered_map" );
    test<fca_switch_unordered_map>( "fca_switch_unordered_map" );
    test<fca_fmod_unordered_map>( "fca_fmod_unordered_map" );

    // frng is spectacularly slow for consecutive uint64 insertion
    // (as expected, boost::hash is the identity and position ignores low bits)
    // test<fca_frng_unordered_map>( "fca_frng_unordered_map" );
    
#if defined(IN_32BIT_ARCHITECTURE)
    test<fca_frng_fib_unordered_map>( "fca_frng_fib_unordered_map" );
#endif
    // same as frng
    // test<fca_pow2_unordered_map>( "fca_pow2_unordered_map" );
    
    test<fca_pow2_fib_unordered_map>( "fca_pow2_fib_unordered_map" );
    test<fca_fmod_unordered_bucket_map>( "fca_fmod_unordered_bucket_map" );
    test<fca_fmod_bcached_unordered_bucket_map>( "fca_fmod_bcached_unordered_bucket_map" );
    test<fca_fmod_unordered_hybrid_map>( "fca_fmod_unordered_hybrid_map" );
    test<fca_fmod_unordered_hybrid_bucket_map>( "fca_fmod_unordered_hybrid_bucket_map" );
    test<fca_fmod_bcached_unordered_hybrid_bucket_map>( "fca_fmod_bcached_unordered_hybrid_bucket_map" );
    test<fca_fmod_unordered_linear_map>( "fca_fmod_unordered_linear_map" );
    test<fca_fmod_unordered_linear_bucket_map>( "fca_fmod_unordered_linear_bucket_map" );
    test<fca_fmod_bcached_unordered_linear_bucket_map>( "fca_fmod_bcached_unordered_linear_bucket_map" );
    test<fca_fmod_unordered_pool_map>( "fca_fmod_unordered_pool_map" );
    test<fca_fmod_unordered_pool_bucket_map>( "fca_fmod_unordered_pool_bucket_map" );
    test<fca_fmod_bcached_unordered_pool_bucket_map>( "fca_fmod_bcached_unordered_pool_bucket_map" );
    test<fca_fmod_unordered_embedded_map>( "fca_fmod_unordered_embedded_map" );
    test<fca_fmod_unordered_embedded_bucket_map>( "fca_fmod_unordered_embedded_bucket_map" );
    test<fca_fmod_bcached_unordered_embedded_bucket_map>( "fca_fmod_bcached_unordered_embedded_bucket_map" );
    test<foa_fmod_unordered_coalesced_map>( "foa_fmod_unordered_coalesced_map" );
    test<foa_absl_unordered_coalesced_map>( "foa_absl_unordered_coalesced_map" );
    test<foa_fmod_hcached_unordered_coalesced_map>( "foa_fmod_hcached_unordered_coalesced_map" );
    test<foa_pow2_fib_unordered_nway_map>( "foa_pow2_fib_unordered_nway_map" );
    test<foa_fmod_unordered_nwayplus_map>( "foa_fmod_unordered_nwayplus_map" );
    test<foa_pow2_fib_unordered_nwayplus_map>( "foa_pow2_fib_unordered_nwayplus_map" );
    test<foa_absl_unordered_nwayplus_map>( "foa_absl_unordered_nwayplus_map" );
    test<foa_fmod_unordered_soa_nwayplus_map>( "foa_fmod_unordered_soa_nwayplus_map" );
    test<foa_pow2_fib_unordered_soa_nwayplus_map>( "foa_pow2_fib_unordered_soa_nwayplus_map" );
    test<foa_frng_fib_unordered_soa_nwayplus_map>( "foa_frng_fib_unordered_soa_nwayplus_map" );
    test<foa_frng_fib_unordered_soa15_nwayplus_map>( "foa_frng_fib_unordered_soa15_nwayplus_map" );
    test<foa_absl_unordered_soa_nwayplus_map>( "foa_absl_unordered_soa_nwayplus_map" );
    test<foa_absl_unordered_intersoa_nwayplus_map>( "foa_absl_unordered_intersoa_nwayplus_map" );
    test<foa_absl_unordered_soa15_nwayplus_map>( "foa_absl_unordered_soa15_nwayplus_map" );
    test<foa_absl_unordered_intersoa15_nwayplus_map>( "foa_absl_unordered_intersoa15_nwayplus_map" );
    test<foa_pow2_fib_unordered_coalesced_nwayplus_map>( "foa_pow2_fib_unordered_coalesced_nwayplus_map" );
    test<foa_pow2_fib_unordered_soa_coalesced_nwayplus_map>( "foa_pow2_fib_unordered_soa_coalesced_nwayplus_map" );
    test<foa_frng_fib_unordered_hopscotch_map>( "foa_frng_fib_unordered_hopscotch_map" );
    test<foa_absl_unordered_hopscotch_map>( "foa_absl_unordered_hopscotch_map" );
    test<foa_frng_fib_unordered_longhop_map>( "foa_frng_fib_unordered_longhop_map" );
    test<foa_absl_unordered_longhop_map>( "foa_absl_unordered_longhop_map" );
#endif

    // test<foa_fmod_unordered_rc16_map>( "foa_fmod_unordered_rc16_map" );
    // test<foa_fmod_unordered_rc15_map>( "foa_fmod_unordered_rc15_map" );
    // test<foa_fmodxm_unordered_rc16_map>( "foa_fmodxm_unordered_rc16_map" );
    // test<foa_fmodxm_unordered_rc15_map>( "foa_fmodxm_unordered_rc15_map" );
    // test<foa_absl_unordered_rc16_map>( "foa_absl_unordered_rc16_map" );
    // test<foa_absl_unordered_rc15_map>( "foa_absl_unordered_rc15_map" );
    test<foa_mulx_unordered_rc16_map>( "foa_mulx_unordered_rc16_map" );
    test<foa_mulx_unordered_rc15_map>( "foa_mulx_unordered_rc15_map" );

#if !defined(IN_32BIT_ARCHITECTURE)
    // test<foa_xmxmx_unordered_rc16_map>( "foa_xmxmx_unordered_rc16_map" );
    // test<foa_xmxmx_unordered_rc15_map>( "foa_xmxmx_unordered_rc15_map" );
    // test<foa_mxm_unordered_rc16_map>( "foa_mxm_unordered_rc16_map" );
    // test<foa_mxm_unordered_rc15_map>( "foa_mxm_unordered_rc15_map" );
    // test<foa_mxm2_unordered_rc16_map>( "foa_mxm2_unordered_rc16_map" );
    // test<foa_mxm2_unordered_rc15_map>( "foa_mxm2_unordered_rc15_map" );
    // test<foa_xmx_unordered_rc16_map>( "foa_xmx_unordered_rc16_map" );
    test<foa_xmx_unordered_rc15_map>( "foa_xmx_unordered_rc15_map" );
    // test<foa_xmx2_unordered_rc16_map>( "foa_xmx2_unordered_rc16_map" );
    // test<foa_xmx2_unordered_rc15_map>( "foa_xmx2_unordered_rc15_map" );
    // test<foa_xm_unordered_rc16_map>( "foa_xm_unordered_rc16_map" );
    // test<foa_xm_unordered_rc15_map>( "foa_xm_unordered_rc15_map" );
    // test<foa_hxm_unordered_rc16_map>( "foa_hxm_unordered_rc16_map" );
    // test<foa_hxm_unordered_rc15_map>( "foa_hxm_unordered_rc15_map" );
    // test<foa_xm2_unordered_rc16_map>( "foa_xm2_unordered_rc16_map" );
    // test<foa_xm2_unordered_rc15_map>( "foa_xm2_unordered_rc15_map" );
    // test<foa_hxm2_unordered_rc16_map>( "foa_hxm2_unordered_rc16_map" );
    test<foa_hxm2_unordered_rc15_map>( "foa_hxm2_unordered_rc15_map" );
#else
    // test<foa_xmxmx32_unordered_rc16_map>( "foa_xmxmx32_unordered_rc16_map" );
    // test<foa_xmxmx32_unordered_rc15_map>( "foa_xmxmx32_unordered_rc15_map" );
    // test<foa_mxm32_unordered_rc16_map>( "foa_mxm32_unordered_rc16_map" );
    // test<foa_mxm32_unordered_rc15_map>( "foa_mxm32_unordered_rc15_map" );
    // test<foa_mxm33_unordered_rc16_map>( "foa_mxm33_unordered_rc16_map" );
    // test<foa_mxm33_unordered_rc15_map>( "foa_mxm33_unordered_rc15_map" );
    // test<foa_xmx32_unordered_rc16_map>( "foa_xmx32_unordered_rc16_map" );
    // test<foa_xmx32_unordered_rc15_map>( "foa_xmx32_unordered_rc15_map" );
    // test<foa_hxmx32_unordered_rc16_map>( "foa_hxmx32_unordered_rc16_map" );
    // test<foa_hxmx32_unordered_rc15_map>( "foa_hxmx32_unordered_rc15_map" );
    // test<foa_xmx33_unordered_rc16_map>( "foa_xmx33_unordered_rc16_map" );
    test<foa_xmx33_unordered_rc15_map>( "foa_xmx33_unordered_rc15_map" );
    // test<foa_hxmx33_unordered_rc16_map>( "foa_hxmx33_unordered_rc16_map" );
    // test<foa_hxmx33_unordered_rc15_map>( "foa_hxmx33_unordered_rc15_map" );
    // test<foa_xmx34_unordered_rc16_map>( "foa_xmx34_unordered_rc16_map" );
    // test<foa_xmx34_unordered_rc15_map>( "foa_xmx34_unordered_rc15_map" );
    // test<foa_rmr32_unordered_rc16_map>( "foa_rmr32_unordered_rc16_map" );
    // test<foa_rmr32_unordered_rc15_map>( "foa_rmr32_unordered_rc15_map" );
    // test<foa_rmr33_unordered_rc16_map>( "foa_rmr33_unordered_rc16_map" );
    // test<foa_rmr33_unordered_rc15_map>( "foa_rmr33_unordered_rc15_map" );
    // test<foa_xm32_unordered_rc16_map>( "foa_xm32_unordered_rc16_map" );
    // test<foa_xm32_unordered_rc15_map>( "foa_xm32_unordered_rc15_map" );
    // test<foa_hxm32_unordered_rc16_map>( "foa_hxm32_unordered_rc16_map" );
    // test<foa_hxm32_unordered_rc15_map>( "foa_hxm32_unordered_rc15_map" );
    // test<foa_xm33_unordered_rc16_map>( "foa_xm33_unordered_rc16_map" );
    // test<foa_xm33_unordered_rc15_map>( "foa_xm33_unordered_rc15_map" );
    // test<foa_hxm33_unordered_rc16_map>( "foa_hxm33_unordered_rc16_map" );
    test<foa_hxm33_unordered_rc15_map>( "foa_hxm33_unordered_rc15_map" );
#endif

#ifdef HAVE_ANKERL_UNORDERED_DENSE
   test<ankerl_unordered_dense_map>( "ankerl::unordered_dense::map" );
#endif

#ifdef HAVE_ABSEIL
    test<absl_node_hash_map>( "absl::node_hash_map" );
    test<absl_flat_hash_map>( "absl::flat_hash_map" );
#endif

    std::cout << "---\n\n";

    int label_witdh = 0;
    for( auto const& x: times ) label_witdh = (std::max)((int)( x.label_ + ": " ).size(), label_witdh);
    
    auto precision = std::cout.precision();
    std::cout << std::fixed << std::setprecision(2);

    for (auto const& x : times)
    {
        std::cout << std::setw(label_witdh) << (x.label_ + ": ") <<
        std::setw( 5 ) << x.time_ << " ms, " <<
        std::setw( 7 ) << (double)x.time_ * x.bytes_ / 1'048'576 / 1'000 << " us*MB, " <<
        std::setw( 9 ) << x.bytes_ << " bytes in " << x.count_ << " allocations\n";
    }

    auto [pmint, pmaxt] = boost::minmax_element(
        times.begin(), times.end(), [](const record& x, const record& y){ return x.time_< y.time_; });
    auto [pminb, pmaxb] = boost::minmax_element(
        times.begin(), times.end(), [](const record& x, const record& y){ return x.bytes_< y.bytes_; });

    std::cout << "\n" << std::setw( 28 ) << "Time(worst)/time(best): " << (float)(pmaxt->time_) / pmint->time_ << "\n"; 
    std::cout << "Memory(worst)/memory(best): " << (float)(pmaxb->bytes_) / pminb->bytes_ << "\n"; 

    std::cout << std::setprecision(precision) << std::defaultfloat<< "\n";
}
