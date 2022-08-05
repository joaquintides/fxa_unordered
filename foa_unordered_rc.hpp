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
    return (~match_available())&0xFFFFul;
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
  int simde_mm_movemask_epi8(uint8x16_t a)
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
    return
      (himask & uint64_t(0x0000FFFF00000000ull))>>32&
      (himask & uint64_t(0xFFFF000000000000ull))>>48;
  }

  inline int match_available()const
  {
    return
      (himask & uint64_t(0x00000000FFFF0000ull))>>16&
      (himask & uint64_t(0xFFFF000000000000ull))>>48;
  }

  inline int match_occupied()const
  {
    return // ~match_available()
      (~himask | uint64_t(0xFFFFFFFF0000FFFFull))>>16|
      (~himask | uint64_t(0x0000FFFFFFFFFFFFull))>>48;
  }

  inline int match_really_occupied()const // excluding sentinel
  {
    return
      (~himask & uint64_t(0xFFFF000000000000ull))>>48;
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

  inline int match(std::size_t hash)const
  {
    auto m=_mm_set1_epi8(adjust_hash(hash));
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
  inline static unsigned char adjust_hash(unsigned char hash)
  {
    return hash|(2*(hash<2));
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
    return (~match_available())&0x7FFF;
  }

  inline int match_really_occupied()const // excluding sentinel
  {
    return reinterpret_cast<const unsigned char*>(&mask)[N-1]==0x01?
      match_occupied()&0x3FFF:match_occupied();
  }

private:
  // https://github.com/simd-everywhere/simde/blob/e1bc968696e6533d6b0bf8dddb0614737c983479/simde/x86/sse2.h#L3755
  // (adapted)
  int simde_mm_movemask_epi8(uint8x16_t a)
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
    return hash|(2*(hash<2));
  }

  inline unsigned char& overflow()
  {
    return reinterpret_cast<unsigned char*>(&mask)[N];
  }

  inline unsigned char overflow()const
  {
    return reinterpret_cast<const unsigned char*>(&mask)[N];
  }

  int8x16_t mask=vdupq_n_s8(0)  ;
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
    return match_impl(0);
  }

  inline int match_occupied()const
  {
    return ~match_available()&0x7FFF;
  }

  inline int match_really_occupied()const // excluding sentinel
  {
    return ~(match_impl(0)|match_impl(1))&0x7FFF;
  }

protected:
  inline static unsigned char adjust_hash(unsigned char hash)
  {
    return hash|(2*(hash<2));
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
      group_type  *pg=group();
      std::size_t n0=offset(),
                  n;

      if((n=boost::core::countr_zero((unsigned int)(
          pg->match_occupied()&reset_first_bits(n0+1))))>=N){
        do{
          ++pg;
          pe+=N;
        }
        while((n=boost::core::countr_zero(
          (unsigned int)((pg)->match_occupied())))>=N);
      }

      if(BOOST_UNLIKELY(pg->is_sentinel(n))){
        pe=nullptr;
      }
      else{
        pc=reinterpret_cast<unsigned char*>(pg)+n;
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

  auto insert(const T& x){return insert_impl(x);}
  auto insert(T&& x){return insert_impl(std::move(x));}

  void erase(const_iterator pos)
  {
    destroy_element(pos.pe->data());
    pos.group()->reset(pos.offset());
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
#if defined(BOOST_MSVC)
  BOOST_FORCEINLINE 
#endif
  iterator find(const Key& x)const
  {
    auto   hash=h(x);
    return find_impl(
      x,
      position_for(hash_split_policy::long_hash(hash)),
      hash_split_policy::short_hash(hash));
  }

private:
  // used only on rehash
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

  static void prefetch(const void* p)
  {
#if defined(BOOST_GCC)||defined(BOOST_CLANG)
    __builtin_prefetch((const char*)p,0);
#elif defined(FXA_UNORDERED_SSE2)
    _mm_prefetch((const char*)p,_MM_HINT_T0);
#endif    
  }

  static void prefetch_elements(const element_type* pe)
  {
    constexpr int cache_line=64;
    const char    *p0=reinterpret_cast<const char*>(pe),
                  *p1=p0+sizeof(element_type)*N/2;
    for(auto p=p0;p<p1;p+=cache_line)prefetch(p);
  }

  std::size_t position_for(std::size_t hash)const
  {
    return size_policy::position(hash,group_size_index);
  }

  template<typename Key>
#if defined(BOOST_MSVC)
  BOOST_FORCEINLINE 
#endif
  iterator find_impl(
    const Key& x,std::size_t pos0,std::size_t short_hash)const
  {    
    for(prober pb(pos0);;){
      auto pos=pb.get();
      auto pg=groups.data()+pos;
      auto mask=pg->match(short_hash);
      if(mask){
        auto pe=elements.data()+pos*N;
        prefetch_elements(pe);
        do{
          auto n=countr_zero((unsigned int)mask);
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
  std::pair<iterator,bool> insert_impl(Value&& x)
  {
    auto hash=h(x);
    auto long_hash=hash_split_policy::long_hash(hash);
    auto pos0=position_for(long_hash);
    auto short_hash=hash_split_policy::short_hash(hash);
    auto it=find_impl(x,pos0,short_hash);

    if(it!=end()){
      return {it,false};
    }
    else if(BOOST_LIKELY(size_+1<=ml)){
      return {
        unchecked_insert(std::forward<Value>(x),pos0,short_hash),
        true
      };
    }
    else{
      rehash(size_+1);
      return {
        unchecked_insert(
          std::forward<Value>(x),position_for(long_hash),short_hash),
        true
      };
    }
  }

  void rehash(size_type new_size)
  {
    std::size_t nc =(std::numeric_limits<std::size_t>::max)();
    float       fnc=1.0f+static_cast<float>(new_size)/mlf;
    if(nc>fnc)nc=static_cast<std::size_t>(fnc);

    foa_unordered_rc_set new_container{nc,al};
    std::size_t          num_tx=0;
    try{
      for(std::size_t pos=0,last=groups.size();pos!=last;++pos){
        auto pg=groups.data()+pos;
        auto mask=pg->match_really_occupied();
        while(mask){
          auto n=countr_zero((unsigned int)mask);
          auto& x=elements[pos*N+(std::size_t)n];
          new_container.unchecked_insert(std::move(x.value()));
          destroy_element(x.data());
          pg->reset(n);
          ++num_tx;
          mask&=mask-1;
        }
      }
    }
    catch(...){
      size_-=num_tx;
      throw;
    }
    group_size_index=new_container.group_size_index;
    groups=std::move(new_container.groups);
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
    for(prober pb(pos0);;pb.next(groups.size())){
      auto pos=pb.get();
      auto pg=groups.data()+pos;
      auto mask=pg->match_available();
      if(BOOST_LIKELY(mask)){
        int n=countr_zero((unsigned int)mask);
        auto pe=elements.data()+pos*N+n;
        construct_element(std::forward<Value>(x),pe->data());
        pg->set(n,short_hash);
        ++size_;
        return {pg,std::size_t(n),pe};
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

  static inline int countr_zero(unsigned int x)
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
