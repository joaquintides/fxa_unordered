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
#include <boost/iterator/iterator_facade.hpp>
#include <functional>
#include <limits>
#include <memory>
#include <vector>

namespace fca_unordered_impl{
    
struct bucket_array_base
{
  constexpr static std::size_t sizes[]={
    53ul,97ul,193ul,389ul,769ul,
    1543ul,3079ul,6151ul,12289ul,24593ul,
    49157ul,98317ul,196613ul,393241ul,786433ul,
    1572869ul,3145739ul,6291469ul,12582917ul,25165843ul,
    50331653ul,100663319ul,201326611ul,402653189ul,805306457ul,};
    
  static std::size_t size_index(std::size_t n)
  {
    const std::size_t *bound=std::lower_bound(
      std::begin(sizes),std::end(sizes),n);
    if(bound==std::end(sizes))--bound;
    return bound-sizes;
  }
  
  template<std::size_t SizeIndex,std::size_t Size=sizes[SizeIndex]>
  static std::size_t position(std::size_t hash){return hash%Size;}
    
  static std::size_t position(std::size_t hash,std::size_t size_index)
  {
    switch(size_index){
      default:
      case  0: return position<0>(hash);
      case  1: return position<1>(hash);
      case  2: return position<2>(hash);
      case  3: return position<3>(hash);
      case  4: return position<4>(hash);
      case  5: return position<5>(hash);
      case  6: return position<6>(hash);
      case  7: return position<7>(hash);
      case  8: return position<8>(hash);
      case  9: return position<9>(hash);
      case 10: return position<10>(hash);
      case 11: return position<11>(hash);
      case 12: return position<12>(hash);
      case 13: return position<13>(hash);
      case 14: return position<14>(hash);
      case 15: return position<15>(hash);
      case 16: return position<16>(hash);
      case 17: return position<17>(hash);
      case 18: return position<18>(hash);
      case 19: return position<19>(hash);
      case 20: return position<20>(hash);
      case 21: return position<21>(hash);
      case 22: return position<22>(hash);
      case 23: return position<23>(hash);
      case 24: return position<24>(hash);
    }
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

  bucket<Node> buckets[N];
  std::size_t  bitmask=0;
  bucket_group *next=nullptr,*prev=nullptr;
};

inline std::size_t set_bit(std::size_t n){return std::size_t(1)<<n;}
inline std::size_t reset_bit(std::size_t n){return ~(std::size_t(1)<<n);}
inline std::size_t reset_first_bits(std::size_t n) // n >0
{
  return ~(~(std::size_t(0))>>(sizeof(std::size_t)*8-n));
}

template<typename Node>
struct bucket_iterator:public boost::iterator_facade<
  bucket_iterator<Node>,bucket<Node>,boost::forward_traversal_tag>
{
public:
  bucket_iterator()=default;
  
  void advance_to_next_nonempty()
  {
    if((n=boost::core::countr_zero(pbg->bitmask&reset_first_bits(n+1)))>=N){
      pbg=pbg->next;
      n=boost::core::countr_zero(pbg->bitmask);
    }
  }
    
private:
  friend class boost::iterator_core_access;
  template <typename,typename> friend class bucket_array;
  
  static constexpr auto N=bucket_group<Node>::N;

  bucket_iterator(bucket_group<Node>* pbg,std::size_t n):pbg{pbg},n{n}{}

  auto& dereference()const noexcept{return pbg->buckets[n];}
  bool equal(const bucket_iterator& x)const noexcept{return pbg==x.pbg&&n==x.n;}

  void increment()noexcept
  {
   if(++n>=N){
     ++pbg;
     n=0;
   }
  }

  bucket_group<Node> *pbg=nullptr; 
  std::size_t        n=0; 
};
  
template<typename Node,typename Allocator>
class bucket_array:bucket_array_base
{
  using super=bucket_array_base;
  using node_type=Node;

public:
  using value_type=bucket<Node>;
  using size_type=std::size_t;
  using iterator=bucket_iterator<Node>;
  
  bucket_array(size_type n,const Allocator& al):
    size_index_(super::size_index(n)),
    v(super::sizes[size_index_]/N+1,al)
  {
    auto [pbg,m]=end();
    pbg->bitmask|=set_bit(m);
    pbg->next=pbg->prev=pbg;
  }
  
  bucket_array(bucket_array&&)=default;
  bucket_array& operator=(bucket_array&&)=default;
  
  iterator begin()const{return at(0);}
  iterator end()const{return at(size());}
  size_type size()const{return super::sizes[size_index_];}
  iterator at(size_type n)const{return {const_cast<group*>(&v[n/N]),n%N};}
  
  size_type position(std::size_t hash)const
  {
    return super::position(hash,size_index_);
  }

  void insert_node(iterator itb,node_type* p)noexcept
  {
    if(!itb->node){ // empty bucket
      auto [pbg,n]=itb;
      if(!pbg->bitmask){ // empty group
          pbg->next=v.back().next;
          pbg->next->prev=pbg;
          pbg->prev=&v.back();
          pbg->prev->next=pbg;
      }
      pbg->bitmask|=set_bit(n);
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
    auto [pbg,n]=itb;
    if(!(pbg->bitmask&=reset_bit(n)))unlink_group(pbg);
  }
  
  void unlink_empty_buckets()noexcept
  {
    for(auto& bg:v){
      for(std::size_t n=0;n<N;++n){
        if(!bg.buckets[n].node)bg.bitmask&=reset_bit(n);
      }
      if(!bg.bitmask&&bg.next)unlink_group(&bg);
    }
  }

private:
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

  std::size_t                             size_index_;
  std::vector<group,group_allocator_type> v;
};

template<typename T>
struct node
{
  node *next=nullptr;
  T    value;
};

template<
  typename T,typename Hash=std::hash<T>,typename Pred=std::equal_to<T>,
  typename Allocator=std::allocator<T>
>
class fca_unordered_set
{
  using node_type=node<T>;
  using node_allocator_type=    
    typename std::allocator_traits<Allocator>::
      template rebind_alloc<node_type>;
  using node_alloc_traits=std::allocator_traits<node_allocator_type>;
  using bucket_array_type=bucket_array<node_type,node_allocator_type>;
  using bucket=typename bucket_array_type::value_type;
    
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
    
    const_iterator(node_type* p,bucket_iterator<node_type> itb):p{p},itb{itb}{}

    const value_type& dereference()const noexcept{return p->value;}
    bool equal(const const_iterator& x)const noexcept{return p==x.p;}
    
    void increment()noexcept
    {
      if(!(p=p->next)){
        itb.advance_to_next_nonempty();
        p=itb->node;
      }
    }
  
    node_type                  *p=nullptr; 
    bucket_iterator<node_type> itb={}; 
  };
  using iterator=const_iterator;
  
  ~fca_unordered_set()
  {
    for(auto first=begin(),last=end();first!=last;)first=erase(first);
  }
  
  const_iterator begin()const noexcept
  {
    auto itb=buckets.end();
    itb.advance_to_next_nonempty();
    return {itb->node,itb};
  }
    
  const_iterator end()const noexcept
  {
    auto itb=buckets.end();
    return {itb->node,itb};
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
    return find(x,buckets.position(h(x)));
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
    auto pos=buckets.position(hash);
    auto it=find(x,pos);
    if(it!=end())return {it,false};
        
    if(size_+1>ml){
      std::size_t bc =(std::numeric_limits<std::size_t>::max)();
      float       fbc=1.0f+static_cast<float>(size_+1)/mlf;
      if(bc>fbc)bc=static_cast<std::size_t>(fbc);

      bucket_array_type new_buckets(bc,al);
      try{
        for(auto& b:buckets){            
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
      pos=buckets.position(hash);
    }
    
    auto itb=buckets.at(pos);
    auto p=new_node(std::forward<Value>(x));
    buckets.insert_node(itb,p);
    ++size_;
    return {{p,itb},true};
  }
  
  template<typename Key>
  iterator find(const Key& x,size_type pos)const
  {
    auto itb=buckets.at(pos);
    for(auto p=itb->node;p;p=p->next){
      if(pred(x,p->value))return {p,itb};
    }
    return end();
  }
  
  template<typename Key>
  std::pair<node_type**,bucket_iterator<node_type>>
  find_prev(const Key& x)const
  {
    auto itb=buckets.at(buckets.position(h(x)));
    for(auto pp=&itb->node;*pp;pp=&(*pp)->next){
      if(pred(x,(*pp)->value))return {pp,itb};
    }
    return {nullptr,{}};
  }
     
  size_type max_load()
  {
    float fml=mlf*static_cast<float>(buckets.size());
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
  typename Hash=std::hash<Key>,typename Pred=std::equal_to<Key>,
  typename Allocator=std::allocator<map_value_adaptor<Key,Value>>
>
using fca_unordered_map=fca_unordered_set<
  map_value_adaptor<Key,Value>,
  map_hash_adaptor<Hash>,map_pred_adaptor<Pred>,
  Allocator
>;

} // namespace fca_unordered

using fca_unordered_impl::fca_unordered_set;
using fca_unordered_impl::fca_unordered_map;

#endif
