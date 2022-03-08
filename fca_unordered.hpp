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
#include <boost/core/bit.hpp>
#include <boost/container_hash/hash.hpp>
#include <boost/iterator/iterator_facade.hpp>
#include <functional>
#include <iterator>
#include <limits>
#include <memory>
#include <type_traits>
#include <vector>
#include "fastrange.h"

namespace fca_unordered_impl{
    
struct prime_size
{
  constexpr static std::size_t sizes[]={
    53ul,97ul,193ul,389ul,769ul,
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
    }
  }
};

struct prime_fmod_size
{
  constexpr static std::size_t sizes[]={
    53ul,97ul,193ul,389ul,769ul,
    1543ul,3079ul,6151ul,12289ul,24593ul,
    49157ul,98317ul,196613ul,393241ul,786433ul,
    1572869ul,3145739ul,6291469ul,12582917ul,25165843ul,
    50331653ul,100663319ul,201326611ul,402653189ul,805306457ul,};

  constexpr static uint64_t inv_sizes[]={
    348051774975651918ull,190172619316593316ull,95578984837873325ull,
    47420935922132524ull,23987963684927896ull,11955116055547344ull,
    5991147799191151ull,2998982941588287ull,1501077717772769ull,
    750081082979285ull,375261795343686ull,187625172388393ull,
    93822606204624ull,46909513691883ull,23456218233098ull,
    11728086747027ull,5864041509391ull,2932024948977ull,
    1466014921160ull,733007198436ull,366503839517ull,
    183251896093ull,91625960335ull,45812983922ull,22906489714ull,
  };
    
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

  // https://github.com/lemire/fastmod

#ifdef _MSC_VER
  static inline uint64_t mul128_u32(uint64_t lowbits, uint32_t d)
  {
    return __umulh(lowbits, d);
  }
#else // _MSC_VER
  static inline uint64_t mul128_u32(uint64_t lowbits, uint32_t d)
  {
    return ((__uint128_t)lowbits * d) >> 64;
  }
#endif

  static inline uint32_t fastmod_u32(uint32_t a, uint64_t M, uint32_t d)
  {
    uint64_t lowbits = M * a;
    return (uint32_t)(mul128_u32(lowbits, d));
  }

  static inline std::size_t position(std::size_t hash,std::size_t size_index)
  {
    return fastmod_u32(
      uint32_t(hash)+uint32_t(hash>>32),
      inv_sizes[size_index],uint32_t(sizes[size_index]));
  }
};

struct prime_frng_size:prime_size
{      
  static inline std::size_t position(std::size_t hash,std::size_t size_index)
  {
    return fastrangesize(hash,prime_size::sizes[size_index]);
  }
};

struct prime_frng_fib_size:prime_frng_size
{      
  static inline std::size_t mix_hash(std::size_t hash)
  {
    // https://en.wikipedia.org/wiki/Hash_function#Fibonacci_hashing
    const std::size_t m=11400714819323198485ull;
    return hash*m;
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
    // https://en.wikipedia.org/wiki/Hash_function#Fibonacci_hashing
    const std::size_t m=11400714819323198485ull;
    return hash*m;
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
inline std::size_t reset_bit(std::size_t n){return ~(std::size_t(1)<<n);}
inline std::size_t reset_first_bits(std::size_t n) // n>0
{
  return ~(~(std::size_t(0))>>(sizeof(std::size_t)*8-n));
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
  
  dynamic_node_allocator(const Allocator& al):al{al}{}

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
  node_type* transfer_node
    (node_type* p,RawBucketArray,Bucket&,RawBucketArray,Bucket&){return p;}    

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
struct extended_bucket:bucket
{
  extended_bucket(){reset_payload();}

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
    if(auto pb=find_hosting_bucket(p,buckets,b)){
      alloc_traits::destroy(this->get_allocator(),&pb->data()->value);
      pb->reset_payload();
    }
    else super::delete_node(p,buckets,b);
  }
  
  template<typename RawBucketArray,typename Bucket>
  node_type* transfer_node(
    node_type* p,RawBucketArray buckets,Bucket& b,
    RawBucketArray new_buckets,Bucket& newb)
  {
    if(auto pb=find_hosting_bucket(p,buckets,b)){
      auto newp=new_node(std::move(pb->data()->value),new_buckets,newb);
      alloc_traits::destroy(al,&pb->data()->value);
      pb->reset_payload();
      return newp;
    }
    else return p;
  }    

private:
  using alloc_traits=std::allocator_traits<Allocator>;
  
  template<typename RawBucketArray,typename Bucket>
  auto look_ahead(RawBucketArray buckets,Bucket& b)
  {
    return (std::min)(LINEAR_PROBE_N,std::distance(&b,buckets.end()));
  }

  template<typename RawBucketArray,typename Bucket>
  Bucket* find_available_bucket(RawBucketArray buckets,Bucket& b)
  {
    auto pb=&b;
    for(auto n=look_ahead(buckets,b);n;++pb,--n){
      if(pb->has_payload())return pb;
    }
        
    return nullptr;
  }
  
  template<typename RawBucketArray,typename Bucket>
  Bucket* find_hosting_bucket(node_type* p,RawBucketArray buckets,Bucket& b)
  {
    auto pb=&b;
    for(auto n=look_ahead(buckets,b);n;++pb,--n){
      if(p==pb->data())return pb;
    }
    return nullptr;
  }

  Allocator al; 
};

struct hybrid_node_allocation
{
  template<typename Node>
  using bucket_type=extended_bucket<Node>;
  
  template<typename Node,typename Allocator>
  using allocator_type=hybrid_node_allocator<Node,Allocator>;
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
    node_type* p,bucket& b,bucket_array_type& new_buckets)
  {
    auto itnewb=new_buckets.at(new_buckets.position(h(p->value)));
    p=node_allocator.transfer_node(p,buckets.raw(),b,new_buckets.raw(),*itnewb);
    new_buckets.insert_node(itnewb,p);
  }

  template<typename Value>
  std::pair<iterator,bool> insert_impl(Value&& x)
  {
    auto hash=h(x);
    auto itb=buckets.at(buckets.position(hash));
    auto it=find(x,itb);
    if(it!=end())return {it,false};
        
    if(size_+1>ml){
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

    bucket_array_type new_buckets(bc,node_allocator.get_allocator());
    try{
      for(auto& b:buckets.raw()){            
        for(auto p=b.next;p;){
          auto  next_p=p->next;
          transfer_node(static_cast<node_type*>(p),b,new_buckets);
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
    ml=max_load();   
  }
  
  template<typename Key>
  iterator find(const Key& x,bucket_iterator itb)const
  {
    for(auto p=itb->next;p;p=p->next){
      if(pred(x,static_cast<node_type*>(p)->value)){
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
      if(pred(x,static_cast<node_type*>(*pp)->value))return {pp,itb};
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
  node_allocator_type node_allocator{Allocator()};
  float               mlf=1.0f;
  size_type           size_=0;
  bucket_array_type   buckets{0,node_allocator.get_allocator()};
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

} // namespace fca_unordered

using fca_unordered_impl::fca_unordered_set;
using fca_unordered_impl::fca_unordered_map;

#endif
