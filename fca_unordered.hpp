/* Proof of concept of closed-addressing, O(1)-erase, standards-compliant
 * unordered associative containers.
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
#include <limits>
#include <memory>
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

template<typename Node>
struct bucket
{
  Node *node=nullptr;
};

template<typename Node>
struct bucket_group
{
  static constexpr std::size_t N=sizeof(std::size_t)*8;

  bucket<Node> *buckets;
  std::size_t  bitmask=0;
  bucket_group *next=nullptr,*prev=nullptr;
};

inline std::size_t set_bit(std::size_t n){return std::size_t(1)<<n;}
inline std::size_t reset_bit(std::size_t n){return ~(std::size_t(1)<<n);}
inline std::size_t reset_first_bits(std::size_t n) // n>0
{
  return ~(~(std::size_t(0))>>(sizeof(std::size_t)*8-n));
}

template<typename Node>
struct bucket_iterator:public boost::iterator_facade<
  bucket_iterator<Node>,bucket<Node>,boost::forward_traversal_tag>
{
public:
  bucket_iterator()=default;
      
private:
  friend class boost::iterator_core_access;
  template <typename,typename,typename> friend class bucket_array;
  
  static constexpr auto N=bucket_group<Node>::N;

  bucket_iterator(bucket<Node>* p,bucket_group<Node>* pbg):p{p},pbg{pbg}{}

  auto& dereference()const noexcept{return *p;}
  bool equal(const bucket_iterator& x)const noexcept{return p==x.p;}

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

  bucket<Node>       *p=nullptr;
  bucket_group<Node> *pbg=nullptr; 
};
  
template<
  typename Node,typename Allocator,typename SizePolicy
>
class bucket_array
{
  using size_policy=SizePolicy;
  using node_type=Node;

public:
  using value_type=bucket<Node>;
  using size_type=std::size_t;
  using iterator=bucket_iterator<Node>;
  
  bucket_array(size_type n,const Allocator& al):
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
  
  bucket_array(bucket_array&&)=default;
  bucket_array& operator=(bucket_array&&)=default;
  
  iterator begin()const{return ++end();}
  iterator end()const{return at(size_);}
  size_type capacity()const{return size_;}
  iterator at(size_type n)const
  {
    return {
      const_cast<value_type*>(&buckets[n]),
      const_cast<group*>(&groups[n/N])
    };
  }

  auto& raw(){return buckets;}
  
  size_type position(std::size_t hash)const
  {
    return size_policy::position(hash,size_index_);
  }

  void insert_node(iterator itb,node_type* p)noexcept
  {
    if(!itb->node){ // empty bucket
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
    p->next=itb->node;
    itb->node=p;
  }
  
  void extract_node(iterator itb,node_type* p)noexcept
  {
    node_type** pp=&itb->node;
    while((*pp)!=p)pp=&(*pp)->next;
    *pp=p->next;
    if(!itb->node)unlink_bucket(itb);
  }

  void extract_node_after(iterator itb,node_type** pp)noexcept
  {
    *pp=(*pp)->next;
    if(!itb->node)unlink_bucket(itb);
  }
  
  void unlink_bucket(iterator itb)noexcept
  {
    auto [p,pbg]=itb;
    if(!(pbg->bitmask&=reset_bit(p-pbg->buckets)))unlink_group(pbg);
  }
  
  void unlink_empty_buckets()noexcept
  {
    auto pbg=&groups.front(),last=&groups.back();
    for(;pbg!=last;++pbg){
      for(std::size_t n=0;n<N;++n){
        if(!pbg->buckets[n].node)pbg->bitmask&=reset_bit(n);
      }
      if(!pbg->bitmask&&pbg->next)unlink_group(pbg);
    }
    for(std::size_t n=0;n<size_%N;++n){ // do not check end bucket
      if(!pbg->buckets[n].node)pbg->bitmask&=reset_bit(n);
    }
  }

private:
  using bucket_allocator_type=
    typename std::allocator_traits<Allocator>::
      template rebind_alloc<value_type>;
  using group=bucket_group<Node>;
  using group_allocator_type=
    typename std::allocator_traits<Allocator>::
      template rebind_alloc<group>;
  static constexpr auto N=group::N;
  
  void unlink_group(group* pbg){
    pbg->next->prev=pbg->prev;
    pbg->prev->next=pbg->next;
    pbg->prev=pbg->next=nullptr;
  }

  std::size_t                                   size_index_,size_;
  std::vector<value_type,bucket_allocator_type> buckets;
  std::vector<group,group_allocator_type>       groups;
};

template<typename T>
struct node
{
  node *next;
  T    value;
};

template<
  typename T,typename Hash=boost::hash<T>,typename Pred=std::equal_to<T>,
  typename Allocator=std::allocator<T>,typename SizePolicy=prime_size
>
class fca_unordered_set
{
  using node_type=node<T>;
  using node_allocator_type=    
    typename std::allocator_traits<Allocator>::
      template rebind_alloc<node_type>;
  using node_alloc_traits=std::allocator_traits<node_allocator_type>;
  using bucket_array_type=
    bucket_array<node_type,node_allocator_type,SizePolicy>;
  using bucket=typename bucket_array_type::value_type;
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
      if(!(p=p->next))p=(++itb)->node;
    }
  
    node_type       *p=nullptr; 
    bucket_iterator itb={}; 
  };
  using iterator=const_iterator;
  
  ~fca_unordered_set()
  {
    for(auto first=begin(),last=end();first!=last;)first=erase(first);
  }
  
  const_iterator begin()const noexcept
  {
    auto itb=buckets.begin();
    return {itb->node,itb};
  }
    
  const_iterator end()const noexcept
  {
    return {};
  }
  
  size_type size()const noexcept{return size_;}

  auto insert(const T& x){return insert_impl(x);}
  auto insert(T&& x){return insert_impl(std::move(x));}
  
  iterator erase(const_iterator pos)
  {
    auto [p,itb]=pos;
    ++pos;
    buckets.extract_node(itb,p);
    delete_node(p);
    --size_;
    return pos;
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
      delete_node(p);
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
  node_type* new_node(Value&& x)
  {
    node_type* p=&*node_alloc_traits::allocate(al,1);
    try{
      node_alloc_traits::construct(al,&p->value,std::forward<Value>(x));
      return p;
    }
    catch(...){
      node_alloc_traits::deallocate(al,p,1);
      throw;
    }
  }
  
  void delete_node(node_type* p)
  {
    node_alloc_traits::destroy(al,&p->value);
    node_alloc_traits::deallocate(al,p,1);
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
    
    auto p=new_node(std::forward<Value>(x));
    buckets.insert_node(itb,p);
    ++size_;
    return {{p,itb},true};
  }

  void rehash(size_type n)
  {
    std::size_t bc =(std::numeric_limits<std::size_t>::max)();
    float       fbc=1.0f+static_cast<float>(n)/mlf;
    if(bc>fbc)bc=static_cast<std::size_t>(fbc);

    bucket_array_type new_buckets(bc,al);
    try{
      for(auto& b:buckets.raw()){            
        for(auto p=b.node;p;){
          auto next_p=p->next;
          new_buckets.insert_node(
            new_buckets.at(new_buckets.position(h(p->value))),p);
          b.node=p=next_p;
        }
      }
    }
    catch(...){
      for(auto& b:new_buckets){
        for(auto p=b.node;p;){
          auto next_p=p->next;
          delete_node(p);
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
    for(auto p=itb->node;p;p=p->next){
      if(pred(x,p->value))return {p,itb};
    }
    return end();
  }
  
  template<typename Key>
  std::pair<node_type**,bucket_iterator> find_prev(const Key& x)const
  {
    auto itb=buckets.at(buckets.position(h(x)));
    for(auto pp=&itb->node;*pp;pp=&(*pp)->next){
      if(pred(x,(*pp)->value))return {pp,itb};
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
  node_allocator_type al;
  float               mlf=1.0f;
  size_type           size_=0;
  bucket_array_type   buckets{0,al};
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
  typename SizePolicy=prime_size
>
using fca_unordered_map=fca_unordered_set<
  map_value_adaptor<Key,Value>,
  map_hash_adaptor<Hash>,map_pred_adaptor<Pred>,
  Allocator,SizePolicy
>;

} // namespace fca_unordered

using fca_unordered_impl::fca_unordered_set;
using fca_unordered_impl::fca_unordered_map;

#endif
