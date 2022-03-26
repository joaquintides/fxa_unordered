/* Proof of concept of closed- and open-addressing
 * unordered associative containers.
 *
 * Copyright 2022 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 */
 
#ifndef FOA_UNORDERED_NWAY_HPP
#define FOA_UNORDERED_NWAY_HPP

#include <boost/config.hpp>
#include <boost/container_hash/hash.hpp>
#include <boost/core/bit.hpp>
#include <boost/iterator/iterator_facade.hpp>
#include <cassert>
#include <climits>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <type_traits>
#include <vector>
#include "fxa_common.hpp"

namespace fxa_unordered{

namespace uint64_ops{

constexpr inline uint64_t smask(uint64_t n)
{
  return
    (n&1)<< 0|
    (n&2)<<15|
    (n&4)<<30|
    (n&8)<<45;
}
    
constexpr inline uint64_t simask(uint64_t n)
{
  return smask(~n);
}

static constexpr uint64_t smasks[]=
{
  smask(0),smask(1),smask(2),smask(3),smask(4),smask(5),
  smask(6),smask(7),smask(8),smask(9),smask(10),smask(11),
  smask(12),smask(13),smask(14),smask(15),
};

static constexpr uint64_t simasks[]=
{
  simask(0),simask(1),simask(2),simask(3),simask(4),simask(5),
  simask(6),simask(7),simask(8),simask(9),simask(10),simask(11),
  simask(12),simask(13),simask(14),simask(15),
};

constexpr void set(uint64_t& x,unsigned pos,unsigned n)
{
  assert(n<16&&pos<16);
    
  x|=   smasks[n]<<pos;
  x&=~(simasks[n]<<pos);
}

constexpr inline uint64_t mmask(uint64_t n)
{
  uint64_t m=0;
  for(unsigned pos=0;pos<16;++pos)set(m,pos,n);
  return m;
}

static constexpr uint64_t mmasks[]=
{
  mmask(0),mmask(1),mmask(2),mmask(3),mmask(4),mmask(5),
  mmask(6),mmask(7),mmask(8),mmask(9),mmask(10),mmask(11),
  mmask(12),mmask(13),mmask(14),mmask(15),
};

unsigned match(uint64_t x,unsigned n)
{
  assert(n<16);
  
  x=~(x^mmasks[n]);
  return
    (x & uint64_t(0x000000000000FFFFull))>> 0&
    (x & uint64_t(0x00000000FFFF0000ull))>>16&
    (x & uint64_t(0x0000FFFF00000000ull))>>32&
    (x & uint64_t(0xFFFF000000000000ull))>>48;
}

} // namespace uint64_ops

template<typename T>
struct element_bunch
{
  static constexpr std::size_t N=16;
  struct element
  {
    T* data(){return reinterpret_cast<T*>(&storage);}
    T& value(){return *data();}

  private:
    std::aligned_storage_t<sizeof(T),alignof(T)> storage;
  };
  struct node:element
  {
    node *next=nullptr;
  };

  void set(std::size_t pos,std::size_t n,std::size_t hash)
  {
    assert(pos<N&&n<N);
    uint64_ops::set(lowmask,pos,n);
    uint64_ops::set(himask,pos,0x8u | (hash&0x7u));
  }

  void reset(std::size_t pos)
  {
    assert(pos<N);
    uint64_ops::set(himask,pos,0);
  }

  std::size_t match(std::size_t n,std::size_t hash)const
  {
    assert(n<N);
    return uint64_ops::match(lowmask,n)&
           uint64_ops::match(himask,0x8u | (hash&0x7u));
  }

  std::size_t match_non_empty()const
  {
    return himask>>48;  
  }

  std::size_t match_empty()const{return ~match_non_empty();}

  element& at(std::size_t n){return storage[n];}
  node*& extra(){return extra_;}

private:
  uint64_t  lowmask=0,himask=0;
  element   storage[N];
  node      *extra_=nullptr;
};

template<typename T,typename Allocator>
class bunch_array
{
  using bunch=element_bunch<T>;
  static constexpr auto N=bunch::N;

public:
  bunch_array(std::size_t n,const Allocator& al):
    v{(n+N-1)/N+1,al}
  {
    static typename bunch::node node_;
    // we're wasting a whole extra bunch for end signalling :-/
    v.back().extra()=&node_;
  }

  bunch_array(bunch_array&&)=default;
  bunch_array& operator=(bunch_array&&)=default;

  auto begin()const{return at(0);}
  auto end()const{return at(v.size()-1);}
  auto at(std::size_t n)const{return const_cast<bunch*>(&v[n]);}

  // only in moved-from state when rehashing
  bool empty()const{return v.empty();}
   
private:
  std::vector<bunch,Allocator> v;
};

template<
  typename T,typename Hash=boost::hash<T>,typename Pred=std::equal_to<T>,
  typename Allocator=std::allocator<T>,
  typename SizePolicy=prime_size
>
class foa_unordered_nway_set 
{
  using size_policy=SizePolicy;
  using bunch=element_bunch<T>;
  using node=typename bunch::node;
  static constexpr auto N=bunch::N;

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
    friend class foa_unordered_nway_set;
    friend class boost::iterator_core_access;

    const_iterator(bunch* pb,std::size_t n=0,node* px=nullptr):
      pb{pb},n{n},px{px}{}

    const value_type& dereference()const noexcept
    {
      if(px)return px->value();
      else  return pb->at(n).value();
    }

    bool equal(const const_iterator& x)const noexcept
    {
      return pb==x.pb&&n==x.n&&px==x.px;
    }

    void increment()noexcept
    {
      if(px)px=px->next;
      else goto in_bunch;
        
      for(;;){
        if(px)return;
        else{
          ++pb;
          n=0;
          px=nullptr;
        }

      in_bunch:
        n=std::size_t(boost::core::countr_zero(
            pb->match_non_empty()&reset_first_bits(n+1)));
        if(n<N)return;
        px=pb->extra();
      }
    }

    bunch       *pb=nullptr;
    std::size_t n=0;
    node        *px=nullptr;
  };
  using iterator=const_iterator;

  foa_unordered_nway_set()=default;

  ~foa_unordered_nway_set()
  {
    if(!bunches.empty()){
     for(auto first=begin(),last=end();first!=last;)erase(first++);
    }
  }
  
  const_iterator begin()const noexcept
  {
    auto pb=bunches.begin();
    const_iterator it=pb;
    if(!(pb->match_non_empty()&0x1u))++it;
    return it;
  }
  
  const_iterator end()const noexcept
  {
    const_iterator it=bunches.end();
    return ++it;
  }

  size_type size()const noexcept{return size_;};

  auto insert(const T& x){return insert_impl(x);}
  auto insert(T&& x){return insert_impl(std::move(x));}

  void erase(const_iterator pos)
  {
    auto [pb,n,px]=pos;
    if(px){
      auto** ppx=&pb->extra();
      while(*ppx!=px)ppx=&((*ppx)->next);
      *ppx=px->next;
      delete_node(px);
    }
    else{
      destroy_element(pb->at(n).data());
      pb->reset(n);
    }
    --size_;
  }

  template<typename Key>
  size_type erase(const Key& x)
  {
    // TODO: optimize
    auto it=find(x);
    if(it!=end()){
      erase(it);
      return 1;
    }
    else return 0;
  }
  
  template<typename Key>
  iterator find(const Key& x)const
  {
    auto hash=h(x);
    auto pos=size_policy::position(hash,size_index);
    auto pb=bunches.at(pos/N);
    return find(x,pb,pos,hash);
  }

private:
  using alloc_traits=std::allocator_traits<Allocator>;
  using node_allocator_type=typename alloc_traits::
    template rebind_alloc<node>;
  using node_alloc_traits=std::allocator_traits<node_allocator_type>;
  using bunch_array_type=bunch_array<
    T,
    typename alloc_traits::template rebind_alloc<bunch>>;

  // used only on rehash
  foa_unordered_nway_set(std::size_t n,node_allocator_type al):
    al{al},size_{n}{}

  template<typename Value>
  node* new_node(Value&& x)
  {
    node* p=&*node_alloc_traits::allocate(al,1);
    node_alloc_traits::construct(al,p);
    try{
      construct_element(std::forward<Value>(x),p->data());
      return p;
    }
    catch(...){
      node_alloc_traits::deallocate(al,p,1);
      throw;
    }
  }

  template<typename Value>
  void construct_element(Value&& x,value_type* p)
  {
    node_alloc_traits::construct(al,p,std::forward<Value>(x));
  }

  void delete_node(node* p)
  {
    destroy_element(p->data());
    node_alloc_traits::deallocate(al,p,1);
  }

  void destroy_element(value_type* p)
  {
    node_alloc_traits::destroy(al,p);
  }

  template<typename Value>
  std::pair<iterator,bool> insert_impl(Value&& x)
  {
    auto hash=h(x);
    auto pos=size_policy::position(hash,size_index);
    auto pb=bunches.at(pos/N);
    auto it=find(x,pb,pos,hash);
    if(it!=end())return {it,false};
        
    if(BOOST_UNLIKELY(size_+1>ml)){
      rehash(size_+1);
      pos=size_policy::position(hash,size_index);
      pb=bunches.at(pos/N);
    }

    return {unchecked_insert(std::forward<Value>(x),pb,pos,hash),true};
  }

  void rehash(size_type new_size)
  {
    std::size_t nc =(std::numeric_limits<std::size_t>::max)();
    float       fnc=1.0f+static_cast<float>(new_size)/mlf;
    if(nc>fnc)nc=static_cast<std::size_t>(fnc);

    foa_unordered_nway_set new_container{nc,al};
    std::size_t            num_tx=0;
    try{
      for(auto& b:bunches){
        auto mask=b.match_non_empty();
        auto n=std::size_t(boost::core::countr_zero(mask));
        while(n<N){
          auto& x=b.at(n).value();
          new_container.unchecked_insert(std::move(x));
          destroy_element(&x);
          b.reset(n);
          ++num_tx;
          n=std::size_t(boost::core::countr_zero(mask&reset_first_bits(n+1)));
        }
        auto px=b.extra();
        while(px){
          auto& x=px->value();
          new_container.unchecked_insert(std::move(x));
          b.extra()=px->next;
          delete_node(px);
          px=b.extra();
          ++num_tx;
        }
      }
    }
    catch(...){
      size_-=num_tx;
      throw;
    }
    size_index=new_container.size_index;
    bunches=std::move(new_container.bunches);
    ml=max_load();   
  }

  template<typename Value>
  iterator unchecked_insert(Value&& x)
  {
    auto  hash=h(x);
    auto  pos=size_policy::position(hash,size_index);
    auto  pb=bunches.at(pos/N);
    return unchecked_insert(std::move(x),pb,pos,hash);
  }

  template<typename Value>
  iterator unchecked_insert(
    Value&& x,bunch* pb,std::size_t pos,std::size_t hash)
  {
    auto n=std::size_t(boost::core::countr_zero(pb->match_empty()));
    if(n<N){
      construct_element(std::forward<Value>(x),pb->at(n).data());
      pb->set(n,pos%N,hash);
      ++size_;
      return {pb,n};
    }
    else{
      auto px=new_node(std::forward<Value>(x));
      px->next=pb->extra();
      pb->extra()=px;
      ++size_;
      return {pb,n,px};
    }
  }

  template<typename Key>
  iterator find(const Key& x,bunch* pb,std::size_t pos,std::size_t hash)const
  {
    auto mask=pb->match(pos%N,hash),
         n=std::size_t(boost::core::countr_zero(mask));

    while(n<N){
      if(BOOST_LIKELY(pred(x,pb->at(n).value()))){
        return {pb,n};
      }
      n=std::size_t(boost::core::countr_zero(mask&reset_first_bits(n+1)));
    }
    for(auto px=pb->extra();px;px=px->next){
      if(BOOST_LIKELY(pred(x,px->value()))){
        return {pb,n,px};
      }        
    }
    return end();
  }

  size_type max_load()const
  {
    float fml=mlf*static_cast<float>(size_policy::size(size_index));
    auto res=(std::numeric_limits<size_type>::max)();
    if(res>fml)res=static_cast<size_type>(fml);
    return res;
  }  

  Hash                h;
  Pred                pred;
  node_allocator_type al;
  float               mlf=1.0f;
  std::size_t         size_=0;
  std::size_t         size_index=size_policy::size_index(size_);
  bunch_array_type    bunches{size_policy::size(size_index),al};
  size_type           ml=max_load();
};

template<
  typename Key,typename Value,
  typename Hash=boost::hash<Key>,typename Pred=std::equal_to<Key>,
  typename Allocator=std::allocator<map_value_adaptor<Key,Value>>,
  typename SizePolicy=prime_size
>
using foa_unordered_nway_map=foa_unordered_nway_set<
  map_value_adaptor<Key,Value>,
  map_hash_adaptor<Hash>,map_pred_adaptor<Pred>,
  Allocator,SizePolicy
>;

} // namespace fxa_unordered

using fxa_unordered::foa_unordered_nway_set;
using fxa_unordered::foa_unordered_nway_map;

#endif
