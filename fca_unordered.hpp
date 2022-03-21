/* Proof of concept of closed-addressing unordered associative containers.
 *
 * Copyright 2022 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 */
 
#ifndef FCA_UNORDERED_HPP
#define FCA_UNORDERED_HPP

#include <algorithm>
#include <boost/config.hpp>
#include <boost/container_hash/hash.hpp>
#include <boost/core/bit.hpp>
#include <boost/iterator/iterator_facade.hpp>
#include <climits>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <type_traits>
#include <vector>
#include "fastrange.h"

namespace fca_unordered_impl{
    
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

#if !defined(BOOST_NO_INT64_T)&&\
    (defined(BOOST_HAS_INT128) || (defined(BOOST_MSVC)&&defined(_WIN64)))
#define FCA_FASTMOD_SUPPORT
#endif

struct prime_fmod_size
{
  constexpr static std::size_t sizes[]={
    13ul,29ul,53ul,97ul,193ul,389ul,769ul,
    1543ul,3079ul,6151ul,12289ul,24593ul,
    49157ul,98317ul,196613ul,393241ul,786433ul,
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
    46909513691883ull,23456218233098ull,11728086747027ull,
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

#  if defined(_MSC_VER)
  static inline uint64_t mul128_u32(uint64_t lowbits, uint32_t d)
  {
    return __umulh(lowbits, d);
  }
#  else
  static inline uint64_t mul128_u32(uint64_t lowbits, uint32_t d)
  {
    return ((unsigned __int128)lowbits * d) >> 64;
  }
#  endif /* defined(_MSC_VER) */

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

struct bucket
{
  bucket *next=nullptr;
};

template<typename Bucket>
struct bucket_group
{
  static constexpr std::size_t N=sizeof(std::size_t)*8;

  Bucket       *buckets;
  std::size_t  bitmask=0;
  bucket_group *next=nullptr,*prev=nullptr;
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

template<typename Bucket>
struct grouped_bucket_iterator:public boost::iterator_facade<
  grouped_bucket_iterator<Bucket>,Bucket,boost::forward_traversal_tag>
{
public:
  grouped_bucket_iterator()=default;
      
private:
  friend class boost::iterator_core_access;
  template <typename,typename,typename> friend class grouped_bucket_array;
  
  static constexpr auto N=bucket_group<Bucket>::N;

  grouped_bucket_iterator(Bucket* p,bucket_group<Bucket>* pbg):p{p},pbg{pbg}{}

  auto& dereference()const noexcept{return *p;}
  bool equal(const grouped_bucket_iterator& x)const noexcept{return p==x.p;}

  void increment()noexcept
  {
    auto n=std::size_t(boost::core::countr_zero(
      pbg->bitmask&reset_first_bits((p-pbg->buckets)+1)));
    if(n<N){
      p=pbg->buckets+n;
    }
    else{
      pbg=pbg->next;
      p=pbg->buckets+boost::core::countr_zero(pbg->bitmask);
    }
  }

  Bucket               *p=nullptr;
  bucket_group<Bucket> *pbg=nullptr; 
};
  
template<typename T>
struct span
{
  auto begin()const noexcept{return data;} 
  auto end()const noexcept{return data+size;} 
  
  T           *data;
  std::size_t size;
};

template<typename Bucket,typename Allocator,typename SizePolicy>
class grouped_bucket_array
{
  using size_policy=SizePolicy;
  using node_type=bucket; // as node is required to derive from bucket

public:
  using value_type=Bucket;
  using size_type=std::size_t;
  using allocator_type=Allocator;
  using iterator=grouped_bucket_iterator<value_type>;
  
  grouped_bucket_array(size_type n,const Allocator& al):
    size_index_(size_policy::size_index(n)),
    size_(size_policy::size(size_index_)),
    buckets(size_+1,al),
    groups(size_/N+1,al)
  {
    auto pbg=&groups.back();
    pbg->buckets=&buckets[N*(size_/N)];
    pbg->bitmask=set_bit(size_%N);
    pbg->next=pbg->prev=pbg;
  }
  
  grouped_bucket_array(grouped_bucket_array&&)=default;
  grouped_bucket_array& operator=(grouped_bucket_array&&)=default;

  allocator_type get_allocator(){return buckets.get_allocator();}

  iterator begin()const{return ++at(size_);}

  iterator end()const 
  {
    // micro optimization: no need to return the bucket group
    // as end() is not incrementable
    return {const_cast<value_type*>(&buckets.back()),nullptr};
  }

  size_type capacity()const{return size_;}

  iterator at(size_type n)const
  {
    return {
      const_cast<value_type*>(&buckets[n]),
      const_cast<group*>(&groups[n/N])
    };
  }

  auto raw(){return span<value_type>{buckets.data(),size_};}
  
  size_type position(std::size_t hash)const
  {
    return size_policy::position(hash,size_index_);
  }

  void insert_node(iterator itb,node_type* p)noexcept
  {
    if(!itb->next){ // empty bucket
      auto [p,pbg]=itb;
      auto n=p-&buckets[0];
      if(!pbg->bitmask){ // empty group
        pbg->buckets=&buckets[N*(n/N)];
        pbg->next=groups.back().next;
        pbg->next->prev=pbg;
        pbg->prev=&groups.back();
        pbg->prev->next=pbg;
      }
      pbg->bitmask|=set_bit(n%N);
    }
    p->next=itb->next;
    itb->next=p;
  }
  
  void extract_node(iterator itb,node_type* p)noexcept
  {
    node_type** pp=&itb->next;
    while((*pp)!=p)pp=&(*pp)->next;
    *pp=p->next;
    if(!itb->next)unlink_bucket(itb);
  }

  void extract_node_after(iterator itb,node_type** pp)noexcept
  {
    *pp=(*pp)->next;
    if(!itb->next)unlink_bucket(itb);
  }
    
  void unlink_empty_buckets()noexcept
  {
    auto pbg=&groups.front(),last=&groups.back();
    for(;pbg!=last;++pbg){
      for(std::size_t n=0;n<N;++n){
        if(!pbg->buckets[n].next)pbg->bitmask&=reset_bit(n);
      }
      if(!pbg->bitmask&&pbg->next)unlink_group(pbg);
    }
    for(std::size_t n=0;n<size_%N;++n){ // do not check end bucket
      if(!pbg->buckets[n].next)pbg->bitmask&=reset_bit(n);
    }
  }

private:
  using bucket_allocator_type=
    typename std::allocator_traits<Allocator>::
      template rebind_alloc<value_type>;      
  using group=bucket_group<value_type>;
  using group_allocator_type=
    typename std::allocator_traits<Allocator>::
      template rebind_alloc<group>;
  static constexpr auto N=group::N;
  
  void unlink_bucket(iterator itb)
  {
    auto [p,pbg]=itb;
    if(!(pbg->bitmask&=reset_bit(p-pbg->buckets)))unlink_group(pbg);
  }

  void unlink_group(group* pbg){
    pbg->next->prev=pbg->prev;
    pbg->prev->next=pbg->next;
    pbg->prev=pbg->next=nullptr;
  }

  std::size_t                                   size_index_,size_;
  std::vector<value_type,bucket_allocator_type> buckets;
  std::vector<group,group_allocator_type>       groups;
};

struct grouped_buckets
{
  static constexpr bool has_constant_iterator_increment=true;

  template<typename Bucket,typename Allocator,typename SizePolicy>
  using array_type=grouped_bucket_array<Bucket,Allocator,SizePolicy>;
};

template<typename Bucket>
struct simple_bucket_iterator:public boost::iterator_facade<
  simple_bucket_iterator<Bucket>,Bucket,boost::forward_traversal_tag>
{
public:
  simple_bucket_iterator()=default;
      
private:
  friend class boost::iterator_core_access;
  template <typename,typename,typename> friend class simple_bucket_array;
  
  simple_bucket_iterator(Bucket* p):p{p}{}

  auto& dereference()const noexcept{return *p;}
  bool equal(const simple_bucket_iterator& x)const noexcept{return p==x.p;}
  void increment()noexcept{while(!(++p)->next);}

  Bucket *p=nullptr;
};

template<typename Bucket,typename Allocator,typename SizePolicy>
class simple_bucket_array
{
protected:
  using size_policy=SizePolicy;
  using node_type=bucket; // as node is required to derive from bucket

public:
  using value_type=Bucket;
  using size_type=std::size_t;
  using allocator_type=Allocator;
  using iterator=simple_bucket_iterator<value_type>;
   
  simple_bucket_array(size_type n,const Allocator& al):
    size_index_(size_policy::size_index(n)),
    size_(size_policy::size(size_index_)),
    buckets(size_+1,al)
  {
    buckets.back().next=&buckets.back();
  }
  
  simple_bucket_array(simple_bucket_array&&)=default;
  simple_bucket_array& operator=(simple_bucket_array&&)=default;

  allocator_type get_allocator(){return buckets.get_allocator();}

  iterator begin()const
  {
    auto it=at(0);
    if(!it->next)++it;
    return it;
  }
  
  iterator end()const{return at(size_);} 
  size_type capacity()const{return size_;}
  iterator at(size_type n)const{return const_cast<value_type*>(&buckets[n]);}

  auto raw(){return span<value_type>{buckets.data(),size_};}
  
  size_type position(std::size_t hash)const
  {
    return size_policy::position(hash,size_index_);
  }

  void insert_node(iterator itb,node_type* p)noexcept
  {
    p->next=itb->next;
    itb->next=p;
  }
  
  void extract_node(iterator itb,node_type* p)noexcept
  {
    node_type** pp=&itb->next;
    while((*pp)!=p)pp=&(*pp)->next;
    *pp=p->next;
  }

  void extract_node_after(iterator /*itb*/,node_type** pp)noexcept
  {
    *pp=(*pp)->next;
  }
  
  void unlink_empty_buckets()noexcept{}

private:    
  using bucket_allocator_type=
    typename std::allocator_traits<Allocator>::
      template rebind_alloc<value_type>;      

  std::size_t                                   size_index_,size_;
  std::vector<value_type,bucket_allocator_type> buckets;
};

struct simple_buckets
{
  static constexpr bool has_constant_iterator_increment=false;

  template<typename Bucket,typename Allocator,typename SizePolicy>
  using array_type=simple_bucket_array<Bucket,Allocator,SizePolicy>;
};

template<typename Bucket,typename Allocator,typename SizePolicy>
class bcached_simple_bucket_array:
  public simple_bucket_array<Bucket,Allocator,SizePolicy>
{
  using super=simple_bucket_array<Bucket,Allocator,SizePolicy>;
  using node_type=typename super::node_type;
  
public:
  using iterator=typename super::iterator;
   
  using super::super;
   
  bcached_simple_bucket_array(bcached_simple_bucket_array&&)=default;
  bcached_simple_bucket_array& operator=(bcached_simple_bucket_array&&)
    =default;

  iterator begin()const{return begin_;}
  
  void insert_node(iterator itb,node_type* p)noexcept
  {
    super::insert_node(itb,p);
    if(&*begin_>&*itb)begin_=itb;
  }
  
  void extract_node(iterator itb,node_type* p)noexcept
  {
    super::extract_node(itb,p);
    adjust_begin(itb);
  }

  void extract_node_after(iterator itb,node_type** pp)noexcept
  {
    super::extract_node_after(itb,pp);
    adjust_begin(itb);
  }
  
  void unlink_empty_buckets()noexcept
  {
    super::unlink_empty_buckets();
    adjust_begin(begin_);
  }

private:    
  void adjust_begin(iterator itb)
  {
    if(begin_==itb&&!begin_->next)++begin_;
  }

  iterator begin_=super::end(); 
};

struct bcached_simple_buckets:simple_buckets
{
  template<typename Bucket,typename Allocator,typename SizePolicy>
  using array_type=bcached_simple_bucket_array<Bucket,Allocator,SizePolicy>;
};

template<typename T>
struct node:bucket
{
  T value;
};

template<typename Node,typename Allocator>
class dynamic_node_allocator
{
public:
  using node_type=Node;
  
  dynamic_node_allocator(std::size_t /*n*/,const Allocator& al):al{al}{}

  auto& get_allocator(){return al;}

  template<typename Value,typename RawBucketArray,typename Bucket>
  node_type* new_node(Value&& x,RawBucketArray,Bucket&)
  {  
    node_type* p=&*alloc_traits::allocate(al,1);
    try{
      alloc_traits::construct(al,&p->value,std::forward<Value>(x));
      return p;
    }
    catch(...){
      alloc_traits::deallocate(al,p,1);
      throw;
    }
  }
  
  template<typename RawBucketArray,typename Bucket>
  void delete_node(node_type* p,RawBucketArray,Bucket&)
  {
    alloc_traits::destroy(al,&p->value);
    alloc_traits::deallocate(al,p,1);
  }
  
  template<typename RawBucketArray,typename Bucket>
  node_type* relocate_node(
    node_type* p,RawBucketArray,Bucket&,
    dynamic_node_allocator&,RawBucketArray,Bucket&)
  {return p;}    

protected:
  using alloc_traits=std::allocator_traits<Allocator>;

  Allocator al; 
};

struct dynamic_node_allocation
{
  template<typename Node>
  using bucket_type=bucket;
  
  template<typename Node,typename Allocator>
  using allocator_type=dynamic_node_allocator<Node,Allocator>;
};

template<typename Node>
struct hybrid_node_allocator_bucket:bucket
{
  hybrid_node_allocator_bucket(){reset_payload();}

  Node* data(){return reinterpret_cast<Node*>(&storage);}
  const Node* data()const{return reinterpret_cast<const Node*>(&storage);}
  Node& node(){return *data();}
  const Node& value()const{return *data();}
  
  bool has_payload()const{return data()->next!=data();}
  void reset_payload(){data()->next=data();}
  
  std::aligned_storage_t<sizeof(Node),alignof(Node)> storage;
};

template<typename Node,typename Allocator>
class hybrid_node_allocator:public dynamic_node_allocator<Node,Allocator>
{
  using super=dynamic_node_allocator<Node,Allocator>;
 
public:
  static constexpr std::ptrdiff_t LINEAR_PROBE_N=4;
  using node_type=Node;
  
  using super::super;

  template<typename Value,typename RawBucketArray,typename Bucket>
  node_type* new_node(Value&& x,RawBucketArray buckets,Bucket& b)
  {  
    if(auto pb=find_available_bucket(buckets,b)){
      auto p=pb->data();
      alloc_traits::construct(
        this->get_allocator(),&p->value,std::forward<Value>(x));
      return p;
    }
    else return super::new_node(x,buckets,b);
  }
  
  template<typename RawBucketArray,typename Bucket>
  void delete_node(node_type* p,RawBucketArray buckets,Bucket& b)
  {
    if(auto pb=find_hosting_bucket(p,buckets)){
      alloc_traits::destroy(this->get_allocator(),&pb->data()->value);
      pb->reset_payload();
    }
    else super::delete_node(p,buckets,b);
  }
  
  template<typename RawBucketArray,typename Bucket>
  node_type* relocate_node(
    node_type* p,RawBucketArray buckets,Bucket&,
    hybrid_node_allocator& new_node_allocator,
    RawBucketArray new_buckets,Bucket& newb)
  {
    if(auto pb=find_hosting_bucket(p,buckets)){
      auto newp=new_node_allocator.new_node(
        std::move(pb->data()->value),new_buckets,newb);
      alloc_traits::destroy(al,&pb->data()->value);
      pb->reset_payload();
      return newp;
    }
    else return p;
  }    

private:
  using alloc_traits=std::allocator_traits<Allocator>;
  
  template<std::ptrdiff_t N,typename Bucket>
  Bucket* find_available_bucket(Bucket* pb,Bucket* pbend)
  {
    if constexpr(N==0)return nullptr;
    else return
      !pb->has_payload()?
        pb:
        pb+1==pbend?
          nullptr:
          find_available_bucket<N-1>(pb+1,pbend);
  }

  template<typename RawBucketArray,typename Bucket>
  Bucket* find_available_bucket(RawBucketArray buckets,Bucket& b)
  {
    return find_available_bucket<LINEAR_PROBE_N>(&b,buckets.end());
  }
  
  template<typename RawBucketArray>
  auto find_hosting_bucket(node_type* p,RawBucketArray buckets)
  {
    using bucket=std::remove_reference_t<decltype(*buckets.begin())>;
    
    std::uintptr_t u=(std::uintptr_t)p,
                   ubegin=(std::uintptr_t)buckets.begin(),
                   uend=(std::uintptr_t)buckets.end();
    if(ubegin<=u&&u<uend){
      return (bucket*)(ubegin+(u-ubegin)/sizeof(bucket)*sizeof(bucket));
    }
    else return (bucket*)nullptr;
  }

  Allocator al; 
};

struct hybrid_node_allocation
{
  template<typename Node>
  using bucket_type=hybrid_node_allocator_bucket<Node>;
  
  template<typename Node,typename Allocator>
  using allocator_type=hybrid_node_allocator<Node,Allocator>;
};

enum struct quadratic_prober_variant
{
  // ways to handle the group for the first location tried
  standard, // every slot in the group considered
  forward,  // only the slots from the location onwards
  exact,    // only the exact slot for the location
};

template<typename Allocator,quadratic_prober_variant Variant>
class quadratic_prober
{
public:
  static constexpr std::size_t N=sizeof(std::size_t)*8;
  using allocator_type=Allocator;

  quadratic_prober(std::size_t n,const Allocator& al):
    pow2mask(boost::core::bit_ceil((n+N-1)/N)-1),
    bitmask((n+N-1)/N,al)
  {
    std::size_t nmod=n%N;
    bitmask.back()=~std::size_t(0)&reset_first_bits(nmod);  
  }

  std::size_t allocate(std::size_t n)
  {
    std::size_t ndiv=n/N,
                nmod=n%N;

    if constexpr(Variant==quadratic_prober_variant::standard){
      nmod=std::size_t(boost::core::countr_one(bitmask[ndiv]));      
    }
    else if constexpr(Variant==quadratic_prober_variant::forward){
      nmod=std::size_t(boost::core::countr_one(
        nmod==0?
          bitmask[ndiv]:
          bitmask[ndiv]|set_first_bits(nmod)));
    }
    else{
      static_assert(Variant==quadratic_prober_variant::exact);
      if(bitmask[ndiv]&set_bit(nmod))nmod=N;
    }
    if(nmod>=N){ // first probe failed
      std::size_t i=1;
      for(;;){
        ndiv=(ndiv+i)&pow2mask;
        i+=1;
        if(ndiv<bitmask.size()&&
           (nmod=std::size_t(boost::core::countr_one(bitmask[ndiv])))<N)break;
      }
    }

    bitmask[ndiv]|=set_bit(nmod);
    return ndiv*N+nmod;
  }

  void deallocate(std::size_t n)
  {
    std::size_t ndiv=n/N,
                nmod=n%N;
    bitmask[ndiv]&=reset_bit(nmod);      
  }

private:
  std::size_t                        pow2mask;
  std::vector<std::size_t,Allocator> bitmask;
};

template<typename T>
struct uninitialized
{
  T* data(){return reinterpret_cast<T*>(&storage);}

  std::aligned_storage_t<sizeof(T),alignof(T)> storage;
};

template<typename Node,typename Allocator>
class linear_node_allocator
{
public:
  using allocator_type=Allocator;
  using node_type=Node;
  
  linear_node_allocator(std::size_t n,const Allocator& al):
    al(al),
    nodes(n,al),
    prober(n,al)
  {}

  allocator_type get_allocator(){return al;}

  template<typename Value,typename RawBucketArray,typename Bucket>
  node_type* new_node(Value&& x,RawBucketArray buckets,Bucket& b)
  {  
    auto p=nodes[prober.allocate(&b-buckets.begin())].data();
    try{
      alloc_traits::construct(al,&p->value,std::forward<Value>(x));
      return p;
    }
    catch(...){
      prober.deallocate(p-reinterpret_cast<node_type*>(nodes.data()));
      throw;
    }
  }
  
  template<typename RawBucketArray,typename Bucket>
  void delete_node(node_type* p,RawBucketArray,Bucket&)
  {
    alloc_traits::destroy(al,&p->value);
    prober.deallocate(p-reinterpret_cast<node_type*>(nodes.data()));
  }
  
  template<typename RawBucketArray,typename Bucket>
  node_type* relocate_node(
    node_type* p,
    RawBucketArray buckets,Bucket& b,
    linear_node_allocator& new_node_allocator,
    RawBucketArray new_buckets,Bucket& newb)
  {
    auto newp=new_node_allocator.new_node(
      std::move(p->value),new_buckets,newb);
    delete_node(p,buckets,b);
    return newp;
  }    

private:
  using alloc_traits=std::allocator_traits<Allocator>;
  using uninitialized_node=uninitialized<Node>;
  using uninitialized_node_allocator_type=typename alloc_traits::
    template rebind_alloc<uninitialized_node>;
  using size_t_allocator_type=typename alloc_traits::
    template rebind_alloc<std::size_t>;

  allocator_type                                                    al;
  std::vector<uninitialized_node,uninitialized_node_allocator_type> nodes;
  quadratic_prober<
    size_t_allocator_type,quadratic_prober_variant::standard>       prober;
};

struct linear_node_allocation
{
  template<typename Node>
  using bucket_type=bucket;
  
  template<typename Node,typename Allocator>
  using allocator_type=linear_node_allocator<Node,Allocator>;
};

template<typename Node,typename Allocator>
class pool_node_allocator
{
public:
  using allocator_type=Allocator;
  using node_type=Node;
  
  pool_node_allocator(std::size_t n,const Allocator& al):
    al(al),
    nodes(n,al)
  {}

  allocator_type get_allocator(){return al;}

  template<typename Value,typename RawBucketArray,typename Bucket>
  node_type* new_node(Value&& x,RawBucketArray,Bucket&)
  {  
    auto p=allocate_node();
    try{
      alloc_traits::construct(al,&p->value,std::forward<Value>(x));
      return p;
    }
    catch(...){
      deallocate_node(p);
      throw;
    }
  }
  
  template<typename RawBucketArray,typename Bucket>
  void delete_node(node_type* p,RawBucketArray,Bucket&)
  {
    alloc_traits::destroy(al,&p->value);
    deallocate_node(p);
  }
  
  template<typename RawBucketArray,typename Bucket>
  node_type* relocate_node(
    node_type* p,
    RawBucketArray buckets,Bucket& b,
    pool_node_allocator& new_node_allocator,
    RawBucketArray new_buckets,Bucket& newb)
  {
    auto newp=new_node_allocator.new_node(
      std::move(p->value),new_buckets,newb);
    delete_node(p,buckets,b);
    return newp;
  }    

private:
  using alloc_traits=std::allocator_traits<Allocator>;
  using uninitialized_node=uninitialized<Node>;
  using uninitialized_node_allocator_type=typename alloc_traits::
    template rebind_alloc<uninitialized_node>;
  using size_t_allocator_type=typename alloc_traits::
    template rebind_alloc<std::size_t>;

  node_type* allocate_node()
  {
    if(free){
      auto p=free;
      free=static_cast<node_type*>(free->next);
      return p;
    }
    else{
      return reinterpret_cast<node_type*>(&nodes[top++]);
    }
  }

  void deallocate_node(node_type* p)
  {
    p->next=free;
    free=p;
  }

  allocator_type                                          al;
  std::size_t                                             top=0;
  node_type                                               *free=nullptr;
  std::vector<
    uninitialized_node,uninitialized_node_allocator_type> nodes;
};

struct pool_node_allocation
{
  template<typename Node>
  using bucket_type=bucket;
  
  template<typename Node,typename Allocator>
  using allocator_type=pool_node_allocator<Node,Allocator>;
};

template<typename Node>
struct embedded_node_allocator_bucket:bucket
{
  Node* data(){return reinterpret_cast<Node*>(&storage);}
  const Node* data()const{return reinterpret_cast<const Node*>(&storage);}
  Node& node(){return *data();}
  const Node& value()const{return *data();}
  
  std::aligned_storage_t<sizeof(Node),alignof(Node)> storage;
};

template<typename Node,typename Allocator>
class embedded_node_allocator
{
public:
  using allocator_type=Allocator;
  using node_type=Node;
  
  embedded_node_allocator(std::size_t n,const Allocator& al):
    al(al),
    prober(n,al)
  {}

  allocator_type get_allocator(){return al;}

  template<typename Value,typename RawBucketArray,typename Bucket>
  node_type* new_node(Value&& x,RawBucketArray buckets,Bucket& b)
  {  
    auto p=(buckets.begin()+prober.allocate(&b-buckets.begin()))->data();
    try{
      alloc_traits::construct(al,&p->value,std::forward<Value>(x));
      return p;
    }
    catch(...){
      deallocate_node(p,buckets);
      throw;
    }
  }
  
  template<typename RawBucketArray,typename Bucket>
  void delete_node(node_type* p,RawBucketArray buckets,Bucket&)
  {
    alloc_traits::destroy(al,&p->value);
    deallocate_node(p,buckets);
  }
  
  template<typename RawBucketArray,typename Bucket>
  node_type* relocate_node(
    node_type* p,
    RawBucketArray buckets,Bucket& b,
    embedded_node_allocator& new_node_allocator,
    RawBucketArray new_buckets,Bucket& newb)
  {
    auto newp=new_node_allocator.new_node(
      std::move(p->value),new_buckets,newb);
    delete_node(p,buckets,b);
    return newp;
  }    

private:
  using alloc_traits=std::allocator_traits<Allocator>;
  using size_t_allocator_type=typename alloc_traits::
    template rebind_alloc<std::size_t>;

  template<typename RawBucketArray>
  void deallocate_node(node_type* p,RawBucketArray buckets)
  {
    using bucket=std::remove_reference_t<decltype(*buckets.begin())>;
      
    std::uintptr_t u=(std::uintptr_t)p,
                   ubegin=(std::uintptr_t)buckets.begin();
    prober.deallocate((u-ubegin)/sizeof(bucket));      
  }

  allocator_type                                           al;
  quadratic_prober<
    size_t_allocator_type,quadratic_prober_variant::exact> prober;
};

struct embedded_node_allocation
{
  template<typename Node>
  using bucket_type=embedded_node_allocator_bucket<Node>;
  
  template<typename Node,typename Allocator>
  using allocator_type=embedded_node_allocator<Node,Allocator>;
};

template<
  typename T,typename Hash=boost::hash<T>,typename Pred=std::equal_to<T>,
  typename Allocator=std::allocator<T>,
  typename SizePolicy=prime_size,typename BucketArrayPolicy=grouped_buckets,
  typename NodeAllocationPolicy=dynamic_node_allocation
>
class fca_unordered_set
{
  using node_type=node<T>;
  using bucket=typename NodeAllocationPolicy::
    template bucket_type<node_type>;
  using node_allocator_type=typename NodeAllocationPolicy::
    template allocator_type<
      node_type,
      typename std::allocator_traits<Allocator>::
        template rebind_alloc<node_type>>;
  using bucket_array_type=typename BucketArrayPolicy::
    template array_type<
      bucket,
      typename std::allocator_traits<Allocator>::template rebind_alloc<bucket>,
      SizePolicy>;
  using bucket_iterator=typename bucket_array_type::iterator;
    
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
    friend class fca_unordered_set;
    friend class boost::iterator_core_access;
    
    const_iterator(node_type* p,bucket_iterator itb):p{p},itb{itb}{}

    const value_type& dereference()const noexcept{return p->value;}
    bool equal(const const_iterator& x)const noexcept{return p==x.p;}
    
    void increment()noexcept
    {
      if(!(p=static_cast<node_type*>(p->next))){
        p=static_cast<node_type*>((++itb)->next);
      }
    }
  
    node_type       *p=nullptr; 
    bucket_iterator itb={}; 
  };
  using iterator=const_iterator;
  
  ~fca_unordered_set()
  {
    for(auto first=begin(),last=end();first!=last;)erase(first++);
  }
  
  const_iterator begin()const noexcept
  {
    auto itb=buckets.begin();
    return {static_cast<node_type *>(itb->next),itb};
  }
    
  const_iterator end()const noexcept
  {
    auto itb=buckets.end();
    return {static_cast<node_type *>(itb->next),itb};
  }
  
  size_type size()const noexcept{return size_;}

  auto insert(const T& x){return insert_impl(x);}
  auto insert(T&& x){return insert_impl(std::move(x));}
  
  auto erase(const_iterator pos)
  {
    if constexpr(BucketArrayPolicy::has_constant_iterator_increment){
      auto [p,itb]=pos;
      ++pos;
      buckets.extract_node(itb,p);
      node_allocator.delete_node(p,buckets.raw(),*itb);
      --size_;
      return pos;
    }
    else{
      auto [p,itb]=pos;
      buckets.extract_node(itb,p);
      delete_node(p,*itb);
      --size_;
    }
  }
  
  template<typename Key>
  size_type erase(const Key& x)
  {
    auto [pp,itb]=find_prev(x);
    if(!pp){
      return 0;
    }
    else{
      auto p=*pp;
      buckets.extract_node_after(itb,pp);
      delete_node(static_cast<node_type*>(p),*itb);
      --size_;
      return 1;
    }
  }

  template<typename Key>
  iterator find(const Key& x)const
  {
    return find(x,buckets.at(buckets.position(h(x))));
  }

private:
  template<typename Value>
  node_type* new_node(Value&& x,bucket& b)
  {
    return node_allocator.new_node(std::forward<Value>(x),buckets.raw(),b);
  }
  
  void delete_node(node_type* p,bucket& b)
  {
     node_allocator.delete_node(p,buckets.raw(),b);
  }

  void transfer_node(
    node_type* p,bucket& b,
    node_allocator_type& new_node_allocator,bucket_array_type& new_buckets)
  {
    auto itnewb=new_buckets.at(new_buckets.position(h(p->value)));
    p=node_allocator.relocate_node(
      p,buckets.raw(),b,
      new_node_allocator,new_buckets.raw(),*itnewb);
    new_buckets.insert_node(itnewb,p);
  }

  template<typename Value>
  std::pair<iterator,bool> insert_impl(Value&& x)
  {
    auto hash=h(x);
    auto itb=buckets.at(buckets.position(hash));
    auto it=find(x,itb);
    if(it!=end())return {it,false};
        
    if(BOOST_UNLIKELY(size_+1>ml)){
      rehash(size_+1);
      itb=buckets.at(buckets.position(hash));
    }
    
    auto p=new_node(std::forward<Value>(x),*itb);
    buckets.insert_node(itb,p);
    ++size_;
    return {{p,itb},true};
  }

  void rehash(size_type n)
  {
    std::size_t bc =(std::numeric_limits<std::size_t>::max)();
    float       fbc=1.0f+static_cast<float>(n)/mlf;
    if(bc>fbc)bc=static_cast<std::size_t>(fbc);

    bucket_array_type   new_buckets(bc,buckets.get_allocator());
    node_allocator_type new_node_allocator(
                          new_buckets.capacity(),new_buckets.get_allocator());
    try{
      for(auto& b:buckets.raw()){            
        for(auto p=b.next;p;){
          auto  next_p=p->next;
          transfer_node(static_cast<node_type*>(p),b,new_node_allocator,new_buckets);
          b.next=p=next_p;
        }
      }
    }
    catch(...){
      for(auto& b:new_buckets){
        for(auto p=b.next;p;){
          auto next_p=p->next;
          delete_node(static_cast<node_type*>(p),b);
          --size_;
          p=next_p;
        }
      }
      buckets.unlink_empty_buckets();
      throw;
    }
    buckets=std::move(new_buckets);
    node_allocator=std::move(new_node_allocator);
    ml=max_load();   
  }
  
  template<typename Key>
  iterator find(const Key& x,bucket_iterator itb)const
  {
    for(auto p=itb->next;p;p=p->next){
      if(BOOST_LIKELY(pred(x,static_cast<node_type*>(p)->value))){
        return {static_cast<node_type*>(p),itb};
      }
    }
    return end();
  }
  
  template<typename Key>
  std::pair<fca_unordered_impl::bucket**,bucket_iterator>
  find_prev(const Key& x)const
  {
    auto itb=buckets.at(buckets.position(h(x)));
    for(auto pp=&itb->next;*pp;pp=&(*pp)->next){
      if(BOOST_LIKELY(pred(x,static_cast<node_type*>(*pp)->value))){
        return {pp,itb};
      }
    }
    return {nullptr,{}};
  }
     
  size_type max_load()const
  {
    float fml=mlf*static_cast<float>(buckets.capacity());
    auto res=(std::numeric_limits<size_type>::max)();
    if(res>fml)res=static_cast<size_type>(fml);
    return res;
  }  

  Hash                h;
  Pred                pred;
  float               mlf=1.0f;
  size_type           size_=0;
  bucket_array_type   buckets{0,Allocator()};
  node_allocator_type node_allocator{
                        buckets.capacity(),buckets.get_allocator()};
  size_type           ml=max_load();  
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

template<
  typename Key,typename Value,
  typename Hash=boost::hash<Key>,typename Pred=std::equal_to<Key>,
  typename Allocator=std::allocator<map_value_adaptor<Key,Value>>,
  typename SizePolicy=prime_size,typename BucketArrayPolicy=grouped_buckets,
  typename NodeAllocationPolicy=dynamic_node_allocation
>
using fca_unordered_map=fca_unordered_set<
  map_value_adaptor<Key,Value>,
  map_hash_adaptor<Hash>,map_pred_adaptor<Pred>,
  Allocator,SizePolicy,BucketArrayPolicy,NodeAllocationPolicy
>;

template<typename T>
struct coalesced_set_node
{
  bool is_occupied()const{return next_&occupied;}
  bool is_head()const{return next_&head;}
  bool is_free()const{return !(next_&(occupied|head));}
  void mark_occupied(){next_|=occupied;} 
  void mark_deleted(){next_&=~occupied;} 
  void mark_head(){next_|=head;} 
  void reset(){next_=0;} 

  coalesced_set_node* next()
  {
    return reinterpret_cast<coalesced_set_node*>(next_&~(occupied|head));
  }

  void set_next(coalesced_set_node* p)
  {
    next_=reinterpret_cast<std::uintptr_t>(p)|(next_&(occupied|head));
  }

  T* data(){return reinterpret_cast<T*>(&storage);}
  T& value(){return *data();}
  
private:
  static constexpr std::uintptr_t occupied=1,
                                  head=2;

  std::uintptr_t next_=0;
  std::aligned_storage_t<sizeof(T),alignof(T)> storage;
};

template<typename Node,typename Allocator>
struct coalesced_set_node_array
{
  static constexpr auto address_factor=0.86f;

  coalesced_set_node_array(std::size_t n,const Allocator& al):
    address_size_{n},v{std::size_t(n/address_factor)+1,al},top{&v.back()}
  {
    v.back().mark_occupied();
  }

  coalesced_set_node_array(coalesced_set_node_array&&)=default;
  coalesced_set_node_array& operator=(coalesced_set_node_array&&)=default;

  auto begin()const{return const_cast<Node*>(&v.front());}
  auto end()const{return const_cast<Node*>(&v.back());}
  auto at(std::size_t n)const{return const_cast<Node*>(&v[n]);}
  
  auto address_size()const{return address_size_;}
  auto count()const{return count_;}

  bool in_cellar(Node* p)const
  {
    return std::size_t(p-&v.front())>=address_size_;
  }

  Node* new_node()
  {
    ++count_;
    while(free){
      auto p=free;
      free=free->next();
      if(p->is_free()){
        p->mark_occupied();
        return p;
      }
    };
    while(top>v.data()){
      if((--top)->is_free()){
        top->mark_occupied();
        return top;
      }
    }

    // address nodes released past decreasing top
    top=&v[address_size_];
    while(!(--top)->is_free());
    top->mark_occupied();
    return top;
  }

  void acquire_node(Node* p)
  {
    assert(!in_cellar(p));
    p->mark_occupied();
    ++count_;
  }

  void release_node(Node* p)
  {
    p->reset();
    if(in_cellar(p)){
      p->set_next(free);
      free=p;
    }
    --count_;
  }
  
private:
  std::size_t                 address_size_;
  std::size_t                 count_=0;
  std::vector<Node,Allocator> v;
  Node                        *top;
  Node                        *free=nullptr; 
};

template<
  typename T,typename Hash=boost::hash<T>,typename Pred=std::equal_to<T>,
  typename Allocator=std::allocator<T>,
  typename SizePolicy=prime_size
>
class fca_unordered_coalesced_set 
{
  using size_policy=SizePolicy;
  using node_type=coalesced_set_node<T>;

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
    friend class fca_unordered_coalesced_set;
    friend class boost::iterator_core_access;
    
    const_iterator(node_type* p):p{p}{}

    const value_type& dereference()const noexcept{return p->value();}
    bool equal(const const_iterator& x)const noexcept{return p==x.p;}
    void increment()noexcept{while(!(++p)->is_occupied());}

    node_type *p=nullptr; 
  };
  using iterator=const_iterator;

  ~fca_unordered_coalesced_set()
  {
    for(auto& n:nodes)if(n.is_occupied())delete_element(&n);
  }
  
  const_iterator begin()const noexcept
  {
    auto p=nodes.begin();
    const_iterator it=p;
    if(!p->is_occupied())++it;
    return it;
  }
  
  const_iterator end()const noexcept{return nodes.end();}
  size_type size()const noexcept{return size_;};

  auto insert(const T& x){return insert_impl(x);}
  auto insert(T&& x){return insert_impl(std::move(x));}

  void erase(const_iterator pos)
  {
    // Erasing by key may throw, but allows for node unlinking.
    // Caveat: erasing by pointer with delete_element(p) would leave
    // linked non-head nodes as deleted+non-head, and they could be
    // wronlgy picked up by the node_array new_node function.
    erase(*pos);
  }

  template<typename Key>
  size_type erase(const Key& x)
  {
    auto [prev,p]=find_match_and_prev(x);
    if(!p){
      return 0;
    }
    else{
      if(!p->is_head()||(prev&&!p->next())){
        prev->set_next(p->next());
        delete_element(p);
        nodes.release_node(p);
      }
      else{
        delete_element(p);
      }
      --size_;
      return 1;
    }
  }
  
  template<typename Key>
  iterator find(const Key& x)const
  {
    auto p=nodes.at(size_policy::position(h(x),size_index));
    do{
      if(p->is_occupied()&&pred(x,p->value()))return p;
      p=p->next();
    }while(p);
    return end();
  }

private:
  using alloc_traits=std::allocator_traits<Allocator>;
  using node_array_type=coalesced_set_node_array<
    node_type,
    typename alloc_traits::template rebind_alloc<node_type>>;

  template<typename Value>
  node_type* new_element(Value&& x,node_type* pi,node_type* p)
  // pi: point before insertion (if p is null)
  // p:  available node (if found during lookup)
  {
    return new_element(nodes,std::forward<Value>(x),pi,p);
  }

  template<typename Value>
  node_type* new_element(
    node_array_type& nodes,Value&& x,node_type* pi,node_type* p)
  {
    if(p){
        alloc_traits::construct(al,p->data(),std::forward<Value>(x));
        if(p->is_free()){
          nodes.acquire_node(p);
          p->set_next(nullptr);
        }
        else p->mark_occupied();
    }
    else{
      p=nodes.new_node();
      try{
        alloc_traits::construct(al,p->data(),std::forward<Value>(x));
      }
      catch(...){
        nodes.release_node(p); 
      }
      p->set_next(pi->next());
      pi->set_next(p);
    }
    return p;
  }
  
  void delete_element(node_type* p)
  {
    alloc_traits::destroy(al,p->data());
    p->mark_deleted();
  }

  template<typename Value>
  std::pair<iterator,bool> insert_impl(Value&& x)
  {
    auto hash=h(x);
    auto ph=nodes.at(size_policy::position(hash,size_index));
    auto [pi,p]=find_match_or_insertion_point_or_available(x,ph);
    if(p&&p->is_occupied())return {p,false};

    if(BOOST_UNLIKELY(!p&&nodes.count()+1>ml)){
      rehash(nodes.count()+1);
      ph=nodes.at(size_policy::position(hash,size_index));
      pi=ph; // not VICH insertion point, but we won't look up again
      p=!pi->is_occupied()?pi:nullptr;
    }

    p=new_element(std::forward<Value>(x),pi,p);
    ph->mark_head();
    ++size_;
    return {p,true};  
  }

  void rehash(size_type n)
  {
    std::size_t nc =(std::numeric_limits<std::size_t>::max)();
    float       fnc=1.0f+static_cast<float>(n)/mlf;
    if(nc>fnc)nc=static_cast<std::size_t>(fnc);

    std::size_t       new_size_index=size_policy::size_index(nc);
    node_array_type   new_nodes{size_policy::size(new_size_index),al};
    try{
      for(auto& n:nodes){
        if(n.is_occupied()){
          auto ph=new_nodes.at(
                 size_policy::position(h(n.value()),new_size_index)),
               p=!ph->is_occupied()?ph:nullptr;
          // not VICH insertion point, but we don't want to look up
          new_element(new_nodes,std::move(n.value()),ph,p);
          ph->mark_head();
          delete_element(&n);
        }
      }
    }
    catch(...){
      for(auto& n:new_nodes){
        if(n.is_occupied()){
          delete_element(&n);
          --size_;
        }
      }
      throw;
    }
    size_index=new_size_index;
    nodes=std::move(new_nodes);
    ml=max_load();   
  }
  
  template<typename Key>
  std::pair<node_type*,node_type*>
  find_match_or_insertion_point_or_available(const Key& x,node_type* p)const
  {
    node_type *pi=p,*pa=nullptr;
    do{
      // VICH algorithm: insertion after last cellar node
      if(nodes.in_cellar(p))pi=p;
        
      if(!p->is_occupied())pa=p;
      else if(pred(x,p->value()))return {nullptr,p};
      p=p->next();
    }while(p);
    return {pi,pa};
  }

  template<typename Key>
  std::pair<node_type*,node_type*>
  find_match_and_prev(const Key& x)const
  {
    node_type *prev=nullptr,
              *p=nodes.at(size_policy::position(h(x),size_index));
    do{
      if(p->is_occupied()&&pred(x,p->value()))return {prev,p};
      prev=p;
      p=p->next();
    }while(p);
    return {nullptr,nullptr};
  }

  size_type max_load()const
  {
    float fml=mlf*static_cast<float>(nodes.address_size());
    auto res=(std::numeric_limits<size_type>::max)();
    if(res>fml)res=static_cast<size_type>(fml);
    return res;
  }  

  Hash            h;
  Pred            pred;
  Allocator       al;
  float           mlf=1.0f;
  std::size_t     size_=0;
  std::size_t     size_index=size_policy::size_index(size_);
  node_array_type nodes{size_policy::size(size_index),al};
  size_type       ml=max_load();
};

template<
  typename Key,typename Value,
  typename Hash=boost::hash<Key>,typename Pred=std::equal_to<Key>,
  typename Allocator=std::allocator<map_value_adaptor<Key,Value>>,
  typename SizePolicy=prime_size
>
using fca_unordered_coalesced_map=fca_unordered_coalesced_set<
  map_value_adaptor<Key,Value>,
  map_hash_adaptor<Hash>,map_pred_adaptor<Pred>,
  Allocator,SizePolicy
>;

} // namespace fca_unordered

using fca_unordered_impl::fca_unordered_set;
using fca_unordered_impl::fca_unordered_map;
using fca_unordered_impl::fca_unordered_coalesced_set;
using fca_unordered_impl::fca_unordered_coalesced_map;

#endif
