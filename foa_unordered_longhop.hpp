/* Proof of concept of closed- and open-addressing
 * unordered associative containers.
 *
 * Copyright 2022 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 */
 
#ifndef FOA_UNORDERED_LONGHOP_HPP
#define FOA_UNORDERED_LONGHOP_HPP

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
#include <stdexcept>
#include <type_traits>
#include <vector>
#include "fxa_common.hpp"

namespace fxa_unordered{

namespace longhop{

template<typename T>
struct element
{
  static constexpr std::size_t N=16;

  T*       data(){return reinterpret_cast<T*>(&storage);}
  const T* data()const{return reinterpret_cast<const T*>(&storage);}
  T&       value(){return *data();}
  const T& value()const{return *data();}

  std::size_t hash()const{return get(width_hash,shift_hash);}
  std::size_t first()const{return get(width_first,shift_first);}
  std::size_t next()const{return get(width_next,shift_next);}

  void set_hash(std::size_t hash)
  {
    set(hash|set_bit(width_hash-1),width_hash,shift_hash);
  }

  void set_first(std::size_t n)
  {
    assert(n<N);
    set(n,width_first,shift_first);
  }

  void set_next(std::size_t n)
  {
    assert(n<N);
    set(n,width_next,shift_next);
  }

  bool occupied()const{return metadata&set_bit(width-1);}
  bool empty()const{return !occupied();}
  void reset(){set(0,width_hash,shift_hash);}

  bool match(std::size_t hash_)const
  {
    return
      ((hash_|set_bit(width_hash-1))&set_first_bits(width_hash))==hash();
  }

private:
  std::aligned_storage_t<sizeof(T),alignof(T)> storage;
  uint16_t                                     metadata=0;

  static constexpr std::size_t floorlog2(std::size_t n)
  {
#ifdef _MSC_VER
    // https://github.com/boostorg/core/issues/109
    return n==1?0:1+floorlog2(n>>1);
#else
    return boost::core::bit_width(n)-1;
#endif
  }

  static constexpr std::size_t width=sizeof(metadata)*CHAR_BIT,
                               width_next=floorlog2(N),
                               shift_next=0u,
                               width_first=width_next,
                               shift_first=width_next,
                               width_hash=width-width_next-width_first,
                               shift_hash=width_next+width_first;

  void set(std::size_t value,std::size_t width,std::size_t shift)
  {
    metadata&=~(set_first_bits(width)<<shift);
    metadata|=(value&set_first_bits(width))<<shift;
  }

  std::size_t get(std::size_t width,std::size_t shift)const
  {
    return (metadata>>shift)&set_first_bits(width);  
  }
};

template<
  typename T,typename Hash=boost::hash<T>,typename Pred=std::equal_to<T>,
  typename Allocator=std::allocator<T>,
  typename SizePolicy=prime_size
>
class foa_unordered_longhop_set 
{
  using size_policy=SizePolicy;
  using alloc_traits=std::allocator_traits<Allocator>;
  using element_array=std::vector<
    element<T>,
    typename alloc_traits::template rebind_alloc<element<T>>>;
  static constexpr auto N=element<T>::N;

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
    friend class foa_unordered_longhop_set;
    friend class boost::iterator_core_access;

    const_iterator(element<T> *p):p{p}{}

    const value_type& dereference()const noexcept{return p->value();}
    bool equal(const const_iterator& x)const noexcept{return p==x.p;}
    void increment()noexcept{do{++p;}while(p->empty());}

    element<T> *p=nullptr;
  };
  using iterator=const_iterator;

  foa_unordered_longhop_set()
  {
    elements.back().set_hash(0);
  }

  foa_unordered_longhop_set(foa_unordered_longhop_set&&)=default;
  foa_unordered_longhop_set& operator=(foa_unordered_longhop_set&&)=default;

  ~foa_unordered_longhop_set()
  {
    if(!elements.empty()){
      for(auto first=begin(),last=end();first!=last;)first=erase(first);
    }
  }
  
  const_iterator begin()const noexcept
  {
    auto it=at(0);
    if(it.p->empty())++it;
    return it;
  }
  
  const_iterator end()const noexcept{return at(capacity_);}

  const_iterator at(std::size_t n)const noexcept
  {
    return const_cast<element<T>*>(elements.data()+n);
  }

  size_type size()const noexcept{return size_;};

  auto insert(const T& x){return insert_impl(x);}
  auto insert(T&& x){return insert_impl(std::move(x));}

  iterator erase(const_iterator it)
  {    
    std::size_t pos=it.p-elements.data();
    auto pos1=move_to_end_of_bucket_and_erase(capacity_,pos);
    if(pos1!=capacity_){
      elements[pos1].set_next(0);
      assert(it.p->occupied());
      return it;
    }
    else{
      for(std::size_t i=0;;++i){
        auto prev=minus_wrap(pos,i);
        auto n=elements[prev].first();
        if(n&&pos==plus_wrap(prev,n-1)){
          elements[prev].set_first(0);
          break;
        }
        n=elements[prev].next();
        if(n&&pos==plus_wrap(prev,n)){
          elements[prev].set_next(0);
          break;
        }
      }
      assert(it.p->empty());
      return ++it;
    }
  }

  template<typename Key>
  size_type erase(const Key& x)
  {
    auto hash=h(x);
    auto pos=position_for(hash);
    auto prev=find_prev(x,pos,hash);
    if(prev<capacity_){
      if(prev==minus_wrap(pos,1))erase_first(pos);
      else                       erase_next(prev);
      return 1;
    }
    else return 0;
  }
  
  template<typename Key>
  iterator find(const Key& x)const
  {
    auto hash=h(x);
    auto pos=position_for(hash);
    return find(x,pos,hash);
  }

#ifdef FXA_UNORDERED_LONGHOP_STATUS
  void status()const
  {
     std::cout<<"\n"
      <<"size: "<<size_<<"\n"
      <<"capacity: "<<capacity_<<"\n"
      <<"load factor: "<<(float)size_/capacity_<<"\n"
      <<"no of hops: "<<num_hops<<"\n"
      <<"no of hopscotch blocks: "<<num_hopscotch_blocks<<"\n"
      ;
  }
#endif

private:
  struct hopscotch_failure:std::runtime_error
  {
    hopscotch_failure():std::runtime_error{"hopscotching failed"}{}
  };

  // used only on rehash
  foa_unordered_longhop_set(std::size_t n,Allocator al):
    al{al},size_index{size_policy::size_index(n)}
  {
    elements.back().set_hash(0);
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

  std::size_t position_for(std::size_t hash)const
  {
    return size_policy::position(boost::core::rotl(hash,4),size_index);
    //return size_policy::position(hash,size_index);
  }

  std::size_t plus_wrap(std::size_t n,std::size_t m)const
  {
    auto res=n+m;
    return res-capacity_*(res>=capacity_);
  }

  std::size_t minus_wrap(std::size_t n,std::size_t m)const
  {
    auto res=n-m;
    return res+capacity_*(m>n);
  }

  template<typename Value>
  std::pair<iterator,bool> insert_impl(Value&& x)
  {
    auto hash=h(x);
    auto pos=position_for(hash);
    auto it=find(x,pos,hash);
    if(it!=end())return {it,false};

    if(BOOST_UNLIKELY(
      size_+1>ml||
      (it=unchecked_insert(std::forward<Value>(x),pos,hash))==end())){
      rehash(ml+1);
      pos=position_for(hash);
      it=unchecked_insert(std::forward<Value>(x),pos,hash);
      if(it==end())throw hopscotch_failure();
    }

    return {it,true};
  }

  void rehash(size_type new_size)
  {
    std::size_t nc =(std::numeric_limits<std::size_t>::max)();
    float       fnc=1.0f+static_cast<float>(new_size)/mlf;
    if(nc>fnc)nc=static_cast<std::size_t>(fnc);

    foa_unordered_longhop_set new_container{nc,al};
    std::size_t               num_tx=0;
    try{
      for(std::size_t pos=0;pos<capacity_;++pos){
        transfer_bucket(pos,new_container,num_tx);
      }
    }
    catch(...){
      size_-=num_tx;
      throw;
    }
#ifdef FXA_UNORDERED_LONGHOP_STATUS
    new_container.num_hops+=num_hops;
    new_container.num_hopscotch_blocks+=num_hopscotch_blocks;
#endif      
    *this=std::move(new_container);
  }

  void transfer_bucket(
    std::size_t pos,
    foa_unordered_longhop_set& new_container,std::size_t& num_tx)
  {
    auto n=elements[pos].first();
    if(n){
      transfer_bucket_next(plus_wrap(pos,n-1),new_container,num_tx);
      elements[pos].set_first(0);
    }
  }

  void transfer_bucket_next(
    std::size_t pos,
    foa_unordered_longhop_set& new_container,std::size_t& num_tx)
  {
    auto n=elements[pos].next();
    if(n){
      transfer_bucket_next(plus_wrap(pos,n),new_container,num_tx);
      elements[pos].set_next(0);
    }
    if(new_container.unchecked_insert(
      std::move(elements[pos].value()))==new_container.end()){
      throw hopscotch_failure();
    }
    destroy_element(elements[pos].data());
    elements[pos].reset();
    ++num_tx;
  }

  template<typename Value>
  iterator unchecked_insert(Value&& x)
  {
    auto hash=h(x);
    auto pos=position_for(hash);
    return unchecked_insert(std::forward<Value>(x),pos,hash);
  }

  template<typename Value>
  iterator unchecked_insert(Value&& x,std::size_t pos,std::size_t hash)
  {
#if 0
    auto        dst=find_empty_slot(pos);
    std::size_t n=0;
    auto        prev=last_in_bucket_before(pos,dst);
#else
    std::size_t n=0;
    auto        prev=last_in_bucket(pos);
    auto        dst=find_empty_slot(plus_wrap(prev,1));
#endif
    while((n=minus_wrap(dst,prev))>=N){
      for(auto i=N-1;i;--i){
        auto mid=minus_wrap(dst,i);
        auto j=elements[mid].first();
        if(i<N-1 && j && j-1<i){
          auto hop=plus_wrap(mid,j-1);
          auto k=elements[hop].next();
          if(!k || j-1+k>i){  
            construct_element(
              std::move(elements[hop].value()),elements[dst].data());
            elements[dst].set_hash(elements[hop].hash());
            elements[dst].set_next(k? j-1+k-i : 0);
            elements[hop].reset();
            elements[mid].set_first(i+1);
            dst=hop;
            goto continue_;
          }
        }
        j=elements[mid].next();
        if(j && j<i){
          auto hop=plus_wrap(mid,j);
          auto k=elements[hop].next();
          if(!k || j+k>i){  
            construct_element(
              std::move(elements[hop].value()),elements[dst].data());
            elements[dst].set_hash(elements[hop].hash());
            elements[dst].set_next(k? j+k-i: 0);
            elements[hop].reset();
            elements[mid].set_next(i);
            dst=hop;
            goto continue_;
          }
        }
      }
#ifdef FXA_UNORDERED_LONGHOP_STATUS
      ++num_hopscotch_blocks;
#endif
      return end();
    continue_:;
#ifdef FXA_UNORDERED_LONGHOP_STATUS
      ++num_hops;
#endif
    }

    assert(elements[dst].empty());
    construct_element(std::forward<Value>(x),elements[dst].data());  
    elements[dst].set_hash(hash);
    if(prev==minus_wrap(pos,1)){ // first element of bucket
      elements[dst].set_next(elements[pos].first()?elements[pos].first()-n:0);
      elements[pos].set_first(n);
    }
    else{
      elements[dst].set_next(elements[prev].next()?elements[prev].next()-n:0);
      elements[prev].set_next(n);
    }
    ++size_;
    return at(dst);
  }

  std::size_t find_empty_slot(std::size_t pos)const
  {
    while(elements[pos].occupied())++pos;
    if(pos<capacity_)return pos;
    pos=0;
    while(elements[pos].occupied())++pos;
    return pos;
  }

  std::size_t last_in_bucket_before(std::size_t pos,std::size_t dst)
  // returns pos-1 if result is first in bucket
  {
    auto d=minus_wrap(dst,pos);
    auto n=elements[pos].first(),m=n-1;
    pos=minus_wrap(pos,1);
    while(n&&m<d){
      pos=plus_wrap(pos,n);
      n=elements[pos].next();
      m+=n;        
    }
    return pos;
  }

  std::size_t last_in_bucket(std::size_t pos)
  // returns pos-1 if bucket empty
  {
    auto n=elements[pos].first();
    pos=minus_wrap(pos,1);
    while(n){
      pos=plus_wrap(pos,n);
      n=elements[pos].next();
    }
    return pos;
  }

  void erase_first(std::size_t pos)
  {
    auto pos0=pos,
         prev0=minus_wrap(pos,1);
    pos=plus_wrap(pos,elements[pos].first()-1);
    auto prev=move_to_end_of_bucket_and_erase(prev0,pos);
    if(prev==prev0)elements[pos0].set_first(0);
    else           elements[prev].set_next(0); 
  }

  void erase_next(std::size_t prev)
  {
    auto pos=plus_wrap(prev,elements[prev].next());
    prev=move_to_end_of_bucket_and_erase(prev,pos);
    elements[prev].set_next(0); 
  }

  std::size_t move_to_end_of_bucket_and_erase(std::size_t prev,std::size_t pos)
  {
    while(elements[pos].next()){
      auto next=plus_wrap(pos,elements[pos].next());
      std::swap(elements[pos].value(),elements[next].value());
      auto hash_pos=elements[pos].hash();
      elements[pos].set_hash(elements[next].hash());
      elements[next].set_hash(hash_pos);
      prev=pos;
      pos=next;
    }      
    destroy_element(elements[pos].data());
    elements[pos].reset();
    --size_;
    return prev;
  }

  template<typename Key>
  iterator find(const Key& x,std::size_t pos,std::size_t hash)const
  {
    auto n=elements[pos].first();
    pos=minus_wrap(pos,1);
    while(n){
      pos=plus_wrap(pos,n);
      if(elements[pos].match(hash)&&
        BOOST_LIKELY(pred(x,elements[pos].value()))){
        return at(pos); 
      }
      n=elements[pos].next();
    }
    return end();
  }

  template<typename Key>
  std::size_t find_prev(const Key& x,std::size_t pos,std::size_t hash)const
  // returns pos-1 if result is first in bucket
  {
    auto n=elements[pos].first();
    pos=minus_wrap(pos,1);
    auto prev=pos;
    while(n){
      pos=plus_wrap(pos,n);
      if(elements[pos].match(hash)&&
        BOOST_LIKELY(pred(x,elements[pos].value()))){
        return prev;
      }
      n=elements[pos].next();
      prev=pos;
    }
    return capacity_;
  }

  size_type max_load()const
  {
    float fml=mlf*static_cast<float>(capacity_);
    auto res=(std::numeric_limits<size_type>::max)();
    if(res>fml)res=static_cast<size_type>(fml);
    return res;
  }  

  Hash          h;
  Pred          pred;
  Allocator     al;
  float         mlf=0.875;
  std::size_t   size_=0;
  std::size_t   size_index=size_policy::size_index(size_);
  std::size_t   capacity_=size_policy::size(size_index);
  size_type     ml=max_load();
  element_array elements{capacity_+1,al};

#ifdef FXA_UNORDERED_LONGHOP_STATUS
  int           num_hops=0;
  int           num_hopscotch_blocks=0;
#endif
};

template<
  typename Key,typename Value,
  typename Hash=boost::hash<Key>,typename Pred=std::equal_to<Key>,
  typename Allocator=std::allocator<map_value_adaptor<Key,Value>>,
  typename SizePolicy=prime_size
>
using foa_unordered_longhop_map=foa_unordered_longhop_set<
  map_value_adaptor<Key,Value>,
  map_hash_adaptor<Hash>,map_pred_adaptor<Pred>,
  Allocator,SizePolicy
>;

} // namespace longhop

} // namespace fxa_unordered

using fxa_unordered::longhop::foa_unordered_longhop_set;
using fxa_unordered::longhop::foa_unordered_longhop_map;

#endif
