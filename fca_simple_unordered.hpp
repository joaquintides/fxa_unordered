/* Proof of concept of closed-addressing, O(1)-erase, standards-compliant
 * unordered associative containers.
 * Simple version with bucket double linking.
 *
 * Copyright 2022 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 */
 
#ifndef FCA_SIMPLE_UNORDERED_HPP
#define FCA_SIMPLE_UNORDERED_HPP
 
#include <algorithm>
#include <boost/container_hash/hash.hpp>
#include <boost/iterator/iterator_facade.hpp>
#include <functional>
#include <limits>
#include <memory>
#include <vector>

namespace fca_simple_unordered_impl{
    
struct bucket_array_base
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
  
  static inline std::size_t position(std::size_t hash,std::size_t size_index)
  {
    switch(size_index){
      default:
      case   0: return hash%53ul;
      case   1: return hash%97ul;
      case   2: return hash%193ul;
      case   3: return hash%389ul;
      case   4: return hash%769ul;
      case   5: return hash%1543ul;
      case   6: return hash%3079ul;
      case   7: return hash%6151ul;
      case   8: return hash%12289ul;
      case   9: return hash%24593ul;
      case  10: return hash%49157ul;
      case  11: return hash%98317ul;
      case  12: return hash%196613ul;
      case  13: return hash%393241ul;
      case  14: return hash%786433ul;
      case  15: return hash%1572869ul;
      case  16: return hash%3145739ul;
      case  17: return hash%6291469ul;
      case  18: return hash%12582917ul;
      case  19: return hash%25165843ul;
      case  20: return hash%50331653ul;
      case  21: return hash%100663319ul;
      case  22: return hash%201326611ul;
      case  23: return hash%402653189ul;
      case  24: return hash%805306457ul;
    }
  }
};

template<typename Node,typename Allocator>
struct bucket_array_element
{
  Node                 *node=nullptr;
  bucket_array_element *next=nullptr,*prev=nullptr;
};

template<typename Node,typename Allocator>
class bucket_array:bucket_array_base
{
  using super=bucket_array_base;

public:
  using node_type=Node;
  using value_type=bucket_array_element<Node,Allocator>;
  using size_type=std::size_t;
  using pointer=value_type*;
  using allocator_type=
    typename std::allocator_traits<Allocator>::
      template rebind_alloc<value_type>;
  
  bucket_array(size_type n,const Allocator& al):
    size_index_(super::size_index(n)),
    v(super::sizes[size_index_]+1,al)
  {
    end()->next=end()->prev=end();
  }
  
  bucket_array(bucket_array&&)=default;
  bucket_array& operator=(bucket_array&&)=default;
  
  pointer begin()const{return const_cast<pointer>(&v.front());}
  pointer end()const{return const_cast<pointer>(&v.back());}
  size_type size()const{return v.size()-1;}
  pointer at(size_type n)const{return const_cast<pointer>(&v[n]);}
  
  size_type position(std::size_t hash)const
  {
    return super::position(hash,size_index_);
  }

  void insert_node(pointer pb,node_type* p)noexcept
  {
    if(!pb->node){ // empty bucket
      pb->next=end()->next;
      pb->next->prev=pb;
      pb->prev=end();
      pb->prev->next=pb;
    }
    p->next=pb->node;
    pb->node=p;
  }
  
  void extract_node(pointer pb,node_type* p)noexcept
  {
    node_type** pp=&pb->node;
    while((*pp)!=p)pp=&(*pp)->next;
    *pp=p->next;
    if(!pb->node)unlink_bucket(pb);
  }

  void extract_node_after(pointer pb,node_type** pp)noexcept
  {
    *pp=(*pp)->next;
    if(!pb->node)unlink_bucket(pb);
  }
  
  void unlink_bucket(pointer pb)noexcept
  {
    pb->next->prev=pb->prev;
    pb->prev->next=pb->next;
    pb->prev=pb->next=nullptr;
  }

private:
  std::size_t                            size_index_;
  std::vector<value_type,allocator_type> v;
};

template<typename T>
struct node
{
  node *next=nullptr;
  T    value;
};

template<
  typename T,typename Hash=boost::hash<T>,typename Pred=std::equal_to<T>,
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
    const_iterator,const value_type&,boost::forward_traversal_tag>
  {
  public:
    const_iterator()=default;
    const_iterator(node_type* p,bucket* pb):p{p},pb{pb}{}
        
  private:
    friend class fca_unordered_set;
    friend class boost::iterator_core_access;
    
    const value_type& dereference()const noexcept{return p->value;}
    bool equal(const const_iterator& x)const noexcept{return p==x.p;}
    
    void increment()noexcept
    {
      if(!(p=p->next)){
        pb=pb->next;
        p=pb->node;
      }
    }
  
    node_type *p=nullptr; 
    bucket    *pb=nullptr; 
  };
  using iterator=const_iterator;
  
  ~fca_unordered_set()
  {
    for(auto first=begin(),last=end();first!=last;)first=erase(first);
  }
  
  const_iterator begin()const noexcept
  {
    auto pb=buckets.end()->next;
    return {pb->node,pb};
  }
    
  const_iterator end()const noexcept
  {
    auto pb=buckets.end();
    return {pb->node,pb};
  }
  
  size_type size()const noexcept{return size_;}

  auto insert(const T& x){return insert_impl(x);}
  auto insert(T&& x){return insert_impl(std::move(x));}
  
  iterator erase(const_iterator pos)
  {
    auto [p,pb]=pos;
    ++pos;
    buckets.extract_node(pb,p);
    delete_node(p);
    --size_;
    return pos;
  }
  
  template<typename Key>
  size_type erase(const Key& x)
  {
    auto [pp,pb]=find_prev(x);
    if(!pp){
      return 0;
    }
    else{
      auto p=*pp;
      buckets.extract_node_after(pb,pp);
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
        for(auto& b:buckets){
          if(!b.node&&b.next)buckets.unlink_bucket(&b);
        }
        throw;
      }
      buckets=std::move(new_buckets);
      ml=max_load();
      pos=buckets.position(hash);
    }
    
    auto pb=buckets.at(pos);
    auto p=new_node(std::forward<Value>(x));
    buckets.insert_node(pb,p);
    ++size_;
    return {{p,pb},true};
  }
  
  template<typename Key>
  iterator find(const Key& x,size_type pos)const
  {
    auto pb=buckets.at(pos);
    for(auto p=pb->node;p;p=p->next){
      if(pred(x,p->value))return {p,pb};
    }
    return end();
  }
  
  template<typename Key>
  std::pair<node_type**,bucket*> find_prev(const Key& x)const
  {
    auto pb=buckets.at(buckets.position(h(x)));
    for(auto pp=&pb->node;*pp;pp=&(*pp)->next){
      if(pred(x,(*pp)->value))return {pp,pb};
    }
    return {nullptr,nullptr};
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
  typename Hash=boost::hash<Key>,typename Pred=std::equal_to<Key>,
  typename Allocator=std::allocator<map_value_adaptor<Key,Value>>
>
using fca_unordered_map=fca_unordered_set<
  map_value_adaptor<Key,Value>,
  map_hash_adaptor<Hash>,map_pred_adaptor<Pred>,
  Allocator
>;

} // namespace fca_simple_unordered

template<typename... Args>
using fca_simple_unordered_set=
  fca_simple_unordered_impl::fca_unordered_set<Args...>;

// not using Args... because of
// https://gcc.gnu.org/bugzilla/show_bug.cgi?id=59498
template<
  typename Key,typename Value,
  typename Hash=boost::hash<Key>,typename Pred=std::equal_to<Key>,
  typename Allocator=
    std::allocator<fca_simple_unordered_impl::map_value_adaptor<Key,Value>>
>
using fca_simple_unordered_map=
  fca_simple_unordered_impl::fca_unordered_map<
    Key,Value,Hash,Pred,Allocator>;

#endif
