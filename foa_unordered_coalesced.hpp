/* Proof of concept of closed- and open-addressing
 * unordered associative containers.
 *
 * Copyright 2022 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 */
 
#ifndef FOA_UNORDERED_COALESCED_HPP
#define FOA_UNORDERED_COALESCED_HPP

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
#include "fxa_common.hpp"

namespace fxa_unordered{    

template<typename T>
struct simple_coalesced_set_node
{
  bool is_occupied()const{return next_&occupied;}
  bool is_head()const{return next_&head;}
  bool is_free()const{return !(next_&(occupied|head));}
  void mark_occupied(){next_|=occupied;} 
  void mark_deleted(){next_&=~occupied;} 
  void mark_head(){next_|=head;} 
  void reset(){next_=0;} 

  simple_coalesced_set_node* next()
  {
    return reinterpret_cast<simple_coalesced_set_node*>(next_&~(occupied|head));
  }

  void set_next(simple_coalesced_set_node* p)
  {
    next_=reinterpret_cast<std::uintptr_t>(p)|(next_&(occupied|head));
  }

  T* data(){return reinterpret_cast<T*>(&storage);}
  T& value(){return *data();}
  
private:
  static constexpr std::uintptr_t occupied=1,
                                  head=2;

  std::uintptr_t                               next_=0;
  std::aligned_storage_t<sizeof(T),alignof(T)> storage;
};

struct simple_coalesced_set_nodes
{
 template<typename T>
 using node_type=simple_coalesced_set_node<T>;

 template<typename Node>
 static std::size_t hash(Node*){return 0;}
 template<typename Node>
 static void set_hash(Node*,std::size_t){}

 template<typename T,typename Node,typename Pred>
 static bool eq(const T& x,Node* p,std::size_t /*hash*/,Pred pred)
 {
    return pred(x,p->value());
 }

 template<typename T,typename Node,typename Pred>
 static bool occupied_and_eq(const T& x,Node* p,std::size_t /*hash*/,Pred pred)
 {
    return p->is_occupied()&&pred(x,p->value());
 }
};

template<typename T>
struct hcached_coalesced_set_node
{
  bool is_occupied()const{return hash_&occupied;}
  bool is_head()const{return hash_&head;}
  bool is_free()const{return !(hash_&(occupied|head));}
  void mark_occupied(){hash_|=occupied;} 
  void mark_deleted(){hash_&=~occupied;} 
  void mark_head(){hash_|=head;} 
  void reset(){hash_=0;} 

  hcached_coalesced_set_node* next(){return next_;}
  void set_next(hcached_coalesced_set_node* p){next_=p;}

  std::size_t hash()const{return hash_&~(occupied|head);}

  void set_hash(std::size_t hash)
  {
    hash_=(hash&~(occupied|head))|(hash_&(occupied|head));
  }

  bool eq_hash(std::size_t hash)const
  {
    return (hash|occupied|head)==(hash_|occupied|head);
  }

  bool occupied_and_eq_hash(std::size_t hash)const
  {
    return (hash|occupied|head)==(hash_|head);
  }


  T* data(){return reinterpret_cast<T*>(&storage);}
  T& value(){return *data();}
  
private:
  static constexpr std::size_t occupied=1,
                               head=2;

  hcached_coalesced_set_node                   *next_;
  std::size_t                                  hash_=0;
  std::aligned_storage_t<sizeof(T),alignof(T)> storage;
};

struct hcached_coalesced_set_nodes
{
 template<typename T>
 using node_type=hcached_coalesced_set_node<T>;

 template<typename Node>
 static std::size_t hash(Node* p){return p->hash();}
 template<typename Node>
 static void set_hash(Node* p,std::size_t hash){p->set_hash(hash);}

template<typename T,typename Node,typename Pred>
 static bool eq(const T& x,Node* p,std::size_t hash,Pred pred)
 {
    return p->eq_hash(hash)&&pred(x,p->value());
 }

 template<typename T,typename Node,typename Pred>
 static bool occupied_and_eq(const T& x,Node* p,std::size_t hash,Pred pred)
 {
    return p->occupied_and_eq_hash(hash)&&pred(x,p->value());
 }
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
  typename SizePolicy=prime_size,typename NodePolicy=simple_coalesced_set_nodes
>
class foa_unordered_coalesced_set 
{
  using size_policy=SizePolicy;
  using node_policy=NodePolicy;
  using node_type=typename node_policy::template node_type<T>;

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
    friend class foa_unordered_coalesced_set;
    friend class boost::iterator_core_access;
    
    const_iterator(node_type* p):p{p}{}

    const value_type& dereference()const noexcept{return p->value();}
    bool equal(const const_iterator& x)const noexcept{return p==x.p;}
    void increment()noexcept{while(!(++p)->is_occupied());}

    node_type *p=nullptr; 
  };
  using iterator=const_iterator;

  ~foa_unordered_coalesced_set()
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
    auto hash=h(x);
    auto p=nodes.at(size_policy::position(hash,size_index));
    do{
      if(node_policy::occupied_and_eq(x,p,hash,pred))return p;
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
  node_type* new_element(Value&& x,std::size_t hash,node_type* pi,node_type* p)
  // pi: point before insertion (if p is null)
  // p:  available node (if found during lookup)
  {
    return new_element(nodes,std::forward<Value>(x),hash,pi,p);
  }

  template<typename Value>
  node_type* new_element(
    node_array_type& nodes,Value&& x,std::size_t hash,
    node_type* pi,node_type* p)
  {
    if(p){
        alloc_traits::construct(al,p->data(),std::forward<Value>(x));
        node_policy::set_hash(p,hash);
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
        node_policy::set_hash(p,hash);
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
    auto [pi,p]=find_match_or_insertion_point_or_available(x,ph,hash);
    if(p&&p->is_occupied())return {p,false};

    if(BOOST_UNLIKELY(!p&&nodes.count()+1>ml)){
      rehash(nodes.count()+1);
      ph=nodes.at(size_policy::position(hash,size_index));
      pi=ph; // not VICH insertion point, but we won't look up again
      p=!pi->is_occupied()?pi:nullptr;
    }

    p=new_element(std::forward<Value>(x),hash,pi,p);
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
          new_element(
            new_nodes,std::move(n.value()),node_policy::hash(&n),ph,p);
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
  find_match_or_insertion_point_or_available(
    const Key& x,node_type* p,std::size_t hash)const
  {
    node_type *pi=p,*pa=nullptr;
    do{
      // VICH algorithm: insertion after last cellar node
      if(nodes.in_cellar(p))pi=p;
        
      if(!p->is_occupied())pa=p;
      else if(node_policy::eq(x,p,hash,pred))return {nullptr,p};
      p=p->next();
    }while(p);
    return {pi,pa};
  }

  template<typename Key>
  std::pair<node_type*,node_type*>
  find_match_and_prev(const Key& x)const
  {
    auto      hash=h(x);
    node_type *prev=nullptr,
              *p=nodes.at(size_policy::position(hash,size_index));
    do{
      if(node_policy::occupied_and_eq(x,p,hash,pred))return {prev,p};
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
  typename SizePolicy=prime_size,typename NodePolicy=simple_coalesced_set_nodes
>
using foa_unordered_coalesced_map=foa_unordered_coalesced_set<
  map_value_adaptor<Key,Value>,
  map_hash_adaptor<Hash>,map_pred_adaptor<Pred>,
  Allocator,SizePolicy,NodePolicy
>;

} // namespace fxa_unordered

using fxa_unordered::foa_unordered_coalesced_set;
using fxa_unordered::foa_unordered_coalesced_map;

#endif
