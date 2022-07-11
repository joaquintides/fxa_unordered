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

using duration = std::chrono::steady_clock::duration;
using size_type = std::size_t;

struct insert_benchmark {
    duration consecutive;
    duration random;
    duration shifted;
    size_type sizes[3];
};

struct lookup_benchmark {
    duration consecutive;
    duration random;
    duration shifted;
    size_type sizes[3];
};

struct iteration_benchmark {
    duration iterate_erase_odd;
    size_type size;
};

struct erase_benchmark {
    duration consecutive;
    duration random;
    duration shifted;
    size_type sizes[3];
};

template<class Map> BOOST_NOINLINE auto test_insert( Map& map ) -> insert_benchmark
{
    auto timings = insert_benchmark{};

    auto t2 = std::chrono::steady_clock::now();
    auto t1 = std::chrono::steady_clock::now();

    for( unsigned i = 1; i <= N; ++i )
    {
        map.insert( { indices1[ i ], i } );
    }

    t2 = std::chrono::steady_clock::now();
    timings.consecutive = (t2 - t1);
    timings.sizes[0] = map.size();
    t1 = t2;

    for( unsigned i = 1; i <= N; ++i )
    {
        map.insert( { indices2[ i ], i } );
    }

    t2 = std::chrono::steady_clock::now();
    timings.random = (t2 - t1);
    timings.sizes[1] = map.size();
    t1 = t2;

    t2 = std::chrono::steady_clock::now();
    timings.shifted = (t2 - t1);
    timings.sizes[2] = map.size();
    t1 = t2;

    return timings;
}

template<class Map> lookup_benchmark BOOST_NOINLINE test_lookup( Map& map )
{
    auto timings = lookup_benchmark{};

    auto t2 = std::chrono::steady_clock::now();
    auto t1 = std::chrono::steady_clock::now();

    volatile std::uint64_t s;

    s = 0;
    for( int j = 0; j < K; ++j )
    {
        for( unsigned i = 1; i <= N * 2; ++i )
        {
            auto it = map.find( indices1[ i ] );
            if( it != map.end() ) s = it->second;
        }
    }

    t2 = std::chrono::steady_clock::now();
    timings.consecutive = (t2 - t1);
    timings.sizes[0] = map.size();
    t1 = t2;

    s = 0;
    for( int j = 0; j < K; ++j )
    {
        for( unsigned i = 1; i <= N * 2; ++i )
        {
            auto it = map.find( indices2[ i ] );
            if( it != map.end() ) s = it->second;
        }
    }

    t2 = std::chrono::steady_clock::now();
    timings.random = (t2 - t1);
    timings.sizes[1] = map.size();
    t1 = t2;

    t2 = std::chrono::steady_clock::now();
    timings.shifted = (t2 - t1);
    timings.sizes[2] = map.size();
    t1 = t2;

    return timings;
}

template<class Map> BOOST_NOINLINE auto test_iteration( Map& map ) -> iteration_benchmark
{
    auto timings = iteration_benchmark{};

    auto t2 = std::chrono::steady_clock::now();
    auto t1 = std::chrono::steady_clock::now();

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

    t2 = std::chrono::steady_clock::now();
    timings.iterate_erase_odd = (t2 - t1);
    timings.size = map.size();

    return timings;
}

template<class Map> BOOST_NOINLINE auto test_erase( Map& map ) -> erase_benchmark
{
    auto timings = erase_benchmark{};

    auto t2 = std::chrono::steady_clock::now();
    auto t1 = std::chrono::steady_clock::now();

    for( unsigned i = 1; i <= N; ++i )
    {
        map.erase( indices1[ i ] );
    }

    t2 = std::chrono::steady_clock::now();
    timings.consecutive = (t2 - t1);
    timings.sizes[0] = map.size();
    t1 = t2;

    {
        boost::detail::splitmix64 rng;

        for( unsigned i = 1; i <= N; ++i )
        {
            map.erase( indices2[ i ] );
        }
    }

    t2 = std::chrono::steady_clock::now();
    timings.random = (t2 - t1);
    timings.sizes[1] = map.size();
    t1 = t2;

    t2 = std::chrono::steady_clock::now();
    timings.shifted = (t2 - t1);
    timings.sizes[2] = map.size();
    t1 = t2;

    return timings;
}

struct record
{
    std::string label_;
    long long time_;
    std::size_t bytes_;
    std::size_t count_;
};

static std::vector<record> times;

template <class Timing>
auto get_total(Timing const& timings) -> duration {
    return timings.consecutive + timings.random + timings.shifted;
}

template <class Timing>
auto get_average(std::vector<Timing> timings) -> Timing {
    std::sort(timings.begin(), timings.end(), [](auto& b1, auto& b2) {
        return get_total(b1) < get_total(b2);
    });

    timings.pop_back();
    timings.pop_back();

    timings.erase(timings.begin());
    timings.erase(timings.begin());

    Timing out;
    out.consecutive = duration::zero();
    out.random = duration::zero();
    out.shifted = duration::zero();

    for (auto& timing : timings) {
        out.consecutive += timing.consecutive;
        out.random += timing.random;
        out.shifted += timing.shifted;
    }

    out.consecutive /= timings.size();
    out.random /= timings.size();
    out.shifted /= timings.size();

    out.sizes[0] = timings.front().sizes[0];
    out.sizes[1] = timings.front().sizes[1];
    out.sizes[2] = timings.front().sizes[2];

    return out;
}

static iteration_benchmark get_average_iterator(std::vector<iteration_benchmark> timings) {
    std::sort(timings.begin(), timings.end(), [](auto& b1, auto& b2) {
        return b1.iterate_erase_odd < b2.iterate_erase_odd;
    });

    timings.pop_back();
    timings.pop_back();

    timings.erase(timings.begin());
    timings.erase(timings.begin());

    iteration_benchmark out;
    out.iterate_erase_odd = duration::zero();

    for (auto& timing : timings) {
        out.iterate_erase_odd += timing.iterate_erase_odd;
    }

    out.iterate_erase_odd /= timings.size();
    out.size = timings.front().size;
    return out;
}

template <class ...Ts>
auto get_total_time(iteration_benchmark b, Ts... benches) -> duration {
    return b.iterate_erase_odd + ((benches.consecutive + benches.random + benches.shifted) + ...);
}

template<template<class...> class Map> void BOOST_NOINLINE test( char const* label )
{
    std::cout << label << ":\n\n";

    auto insert_timings = std::vector<insert_benchmark>();
    auto lookup_timings = std::vector<lookup_benchmark>();
    auto lookup_timings2 = std::vector<lookup_benchmark>();
    auto iteration_timings = std::vector<iteration_benchmark>();
    auto erase_timings = std::vector<erase_benchmark>();

    record rec;
    rec.label_ = label;

    for (int i = 0; i < K; ++i) {
        s_alloc_bytes = 0;
        s_alloc_count = 0;
        
        Map<std::string_view, std::uint32_t> map;

        insert_timings.push_back( test_insert( map ) );

        if (i == (K - 1)) {
            rec.bytes_ = s_alloc_bytes;
            rec.count_ = s_alloc_count;
        }

        lookup_timings.push_back( test_lookup( map ) );
        iteration_timings.push_back( test_iteration( map ) );
        lookup_timings2.push_back( test_lookup( map ) );
        erase_timings.push_back( test_erase( map ) );
    }

    auto insert_timing = get_average(std::move(insert_timings));
    auto lookup_timing = get_average(std::move(lookup_timings));
    auto lookup_timing2 = get_average(std::move(lookup_timings2));
    auto erase_timing = get_average(std::move(erase_timings));
    auto iteration_timing = get_average_iterator(std::move(iteration_timings));
    auto total_time = get_total_time(iteration_timing, insert_timing, lookup_timing, lookup_timing2, erase_timing);

    rec.time_ = total_time / 1ms;
    times.push_back(rec);

    std::cout << "Consecutive insert: " << insert_timing.consecutive / 1ms << "ms (size=" << insert_timing.sizes[0] << ")\n";
    std::cout << "Random insert: " << insert_timing.random / 1ms << "ms (size=" << insert_timing.sizes[1] << ")\n";

    std::cout << "\n";

    std::cout << "Memory: " << rec.bytes_ << " bytes in " << rec.count_ << " allocations\n\n";

    std::cout << "Consecutive lookup: " << lookup_timing.consecutive / 1ms << "ms (size=" << lookup_timing.sizes[0] << ")\n";
    std::cout << "Random lookup: " << lookup_timing.random / 1ms << "ms (size=" << lookup_timing.sizes[1] << ")\n";

    std::cout << "\n";

    std::cout << "Iterate and erase odd elements: " << iteration_timing.iterate_erase_odd / 1ms << "ms (size=" << iteration_timing.size << ")\n";

    std::cout << "\n";

    std::cout << "Consecutive lookup: " << lookup_timing2.consecutive / 1ms << "ms (size=" << lookup_timing2.sizes[0] << ")\n";
    std::cout << "Random lookup: " << lookup_timing2.random / 1ms << "ms (size=" << lookup_timing2.sizes[1] << ")\n";

    std::cout << "\n";

    std::cout << "Consecutive erase: " << erase_timing.consecutive / 1ms << "ms (size=" << erase_timing.sizes[0] << ")\n";
    std::cout << "Random erase: " << erase_timing.random / 1ms << "ms (size=" << erase_timing.sizes[1] << ")\n";

    std::cout << "\n";
    std::cout << "Total: " << total_time / 1ms << "ms\n";

    std::cout << std::endl;
}

#if ((SIZE_MAX>>16)>>16)==0 
#define IN_32BIT_ARCHITECTURE
#endif

int main()
{
    init_indices();
    
    test<std_unordered_map_fnv1a>( "std::unordered_map, FNV-1a" );
    test<boost_unordered_map_fnv1a>( "boost::unordered_map, FNV-1a" );
    // test<multi_index_map_fnv1a>( "multi_index_map, FNV-1a" );

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
    test<foa_absl_unordered_coalesced_map_fnv1a>( "foa_absl_unordered_coalesced_map, FNV-1a" );
    test<foa_fmod_hcached_unordered_coalesced_map_fnv1a>( "foa_fmod_hcached_unordered_coalesced_map, FNV-1a" );

#ifdef BENCHMARK_EVERYTHING    
    test<foa_pow2_fib_unordered_nway_map_fnv1a>( "foa_pow2_fib_unordered_nway_map, FNV-1a" );
#endif

    test<foa_fmod_unordered_nwayplus_map_fnv1a>( "foa_fmod_unordered_nwayplus_map, FNV-1a" );
    test<foa_pow2_fib_unordered_nwayplus_map_fnv1a>( "foa_pow2_fib_unordered_nwayplus_map, FNV-1a" );
    test<foa_absl_unordered_nwayplus_map_fnv1a>( "foa_absl_unordered_nwayplus_map, FNV-1a" );
    test<foa_fmod_unordered_soa_nwayplus_map_fnv1a>( "foa_fmod_unordered_soa_nwayplus_map, FNV-1a" );
    test<foa_pow2_fib_unordered_soa_nwayplus_map_fnv1a>( "foa_pow2_fib_unordered_soa_nwayplus_map, FNV-1a" );
    test<foa_frng_fib_unordered_soa_nwayplus_map_fnv1a>( "foa_frng_fib_unordered_soa_nwayplus_map, FNV-1a" );
    test<foa_frng_fib_unordered_soa15_nwayplus_map_fnv1a>( "foa_frng_fib_unordered_soa15_nwayplus_map, FNV-1a" );
    test<foa_absl_unordered_soa_nwayplus_map_fnv1a>( "foa_absl_unordered_soa_nwayplus_map, FNV-1a" );
    test<foa_absl_unordered_intersoa_nwayplus_map_fnv1a>( "foa_absl_unordered_intersoa_nwayplus_map, FNV-1a" );
    test<foa_absl_unordered_soa15_nwayplus_map_fnv1a>( "foa_absl_unordered_soa15_nwayplus_map, FNV-1a" );
    test<foa_absl_unordered_intersoa15_nwayplus_map_fnv1a>( "foa_absl_unordered_intersoa15_nwayplus_map, FNV-1a" );

#ifdef BENCHMARK_EVERYTHING    
    test<foa_pow2_fib_unordered_coalesced_nwayplus_map_fnv1a>( "foa_pow2_fib_unordered_coalesced_nwayplus_map, FNV-1a" );
    test<foa_pow2_fib_unordered_soa_coalesced_nwayplus_map_fnv1a>( "foa_pow2_fib_unordered_soa_coalesced_nwayplus_map, FNV-1a" );
    test<foa_frng_fib_unordered_hopscotch_map_fnv1a>( "foa_frng_fib_unordered_hopscotch_map, FNV-1a" );
    test<foa_absl_unordered_hopscotch_map_fnv1a>( "foa_absl_unordered_hopscotch_map, FNV-1a" );
    test<foa_frng_fib_unordered_longhop_map_fnv1a>( "foa_frng_fib_unordered_longhop_map, FNV-1a" );
    test<foa_absl_unordered_longhop_map_fnv1a>( "foa_absl_unordered_longhop_map, FNV-1a" );
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
