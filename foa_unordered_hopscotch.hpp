/* Proof of concept of closed- and open-addressing
 * unordered associative containers.
 *
 * Copyright 2022 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 */
 
#ifndef FOA_UNORDERED_HOPSCOTCH_HPP
#define FOA_UNORDERED_HOPSCOTCH_HPP

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

namespace hopscotch{

struct bucket
{
  static constexpr std::size_t N=16;

  auto hopmask()const{return hopmask_;}
  auto& hopmask(){return hopmask_;}
  void set(std::size_t n){hopmask_|=set_bit(n);}
  void reset(std::size_t n){hopmask_&=reset_bit(n);}
  bool is_set(std::size_t n)const{return hopmask_&set_bit(n);}

private:
  uint16_t hopmask_=0;
};

struct control
{
  auto value()const{return control_;}
  void set(std::size_t pos,std::size_t hash){control_=set_value(pos,hash);}
  void resit(std::size_t pos,control x){control_=set_value(pos,x.hash_value());}
  void reset(){control_=0;}
  bool occupied()const{return control_;}
  bool empty()const{return !occupied();}
  bool match(std::size_t pos,std::size_t hash)const
    {return control_==set_value(pos,hash);}

private:
  std::size_t hash_value()const
  {
    return control_-1;
  }

  static unsigned char set_value(std::size_t /*pos*/,std::size_t hash)
  {
    return (hash%255)+1;
  }
  unsigned char control_=0;
};

template<typename T>
struct element
{
  T* data(){return reinterpret_cast<T*>(&storage);}
  const T* data()const{return reinterpret_cast<const T*>(&storage);}
  T& value(){return *data();}
  const T& value()const{return *data();}

private:
  std::aligned_storage_t<sizeof(T),alignof(T)> storage;
};

template<
  typename T,typename Hash=boost::hash<T>,typename Pred=std::equal_to<T>,
  typename Allocator=std::allocator<T>,
  typename SizePolicy=prime_size
>
class foa_unordered_hopscotch_set 
{
  using size_policy=SizePolicy;
  using alloc_traits=std::allocator_traits<Allocator>;
  using bucket_array=std::vector<
    bucket,
    typename alloc_traits::template rebind_alloc<bucket>>;
  using control_array=std::vector<
    control,
    typename alloc_traits::template rebind_alloc<control>>;
  using element_array=std::vector<
    element<T>,
    typename alloc_traits::template rebind_alloc<element<T>>>;
  static constexpr auto N=bucket::N;

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
    friend class foa_unordered_hopscotch_set;
    friend class boost::iterator_core_access;

    const_iterator(control *pc,element<T> *pe):pc{pc},pe{pe}{}

    const value_type& dereference()const noexcept{return pe->value();}
    bool equal(const const_iterator& x)const noexcept{return pe==x.pe;}
    void increment()noexcept{do{++pc;++pe;}while(pc->empty());}

    control    *pc=nullptr;
    element<T> *pe=nullptr;
  };
  using iterator=const_iterator;

  foa_unordered_hopscotch_set()
  {
    controls.back().set(0,0);
  }

  foa_unordered_hopscotch_set(foa_unordered_hopscotch_set&&)=default;
  foa_unordered_hopscotch_set& operator=(foa_unordered_hopscotch_set&&)=default;

  ~foa_unordered_hopscotch_set()
  {
    if(!elements.empty()){
      for(auto first=begin(),last=end();first!=last;)erase(first++);
    }
  }
  
  const_iterator begin()const noexcept
  {
    auto it=at(0);
    if(it.pc->empty())++it;
    return it;
  }
  
  const_iterator end()const noexcept{return at(capacity_);}

  const_iterator at(std::size_t n)const noexcept
  {
    return {
      const_cast<control*>(controls.data()+n),
      const_cast<element<T>*>(elements.data()+n)
    };
  }

  size_type size()const noexcept{return size_;};

  auto insert(const T& x){return insert_impl(x);}
  auto insert(T&& x){return insert_impl(std::move(x));}

  void erase(const_iterator posit)
  {
    auto        [pc,pe]=posit;
    std::size_t pos=pe-elements.data();
    for(std::size_t i=0;;++i){
      auto base=minus_wrap(pos,i);
      if(buckets[base].is_set(i)){
        destroy_element(pe->data());
        pc->reset();
        buckets[base].reset(i);  
        --size_;

        // we could try moving further elements down here 
        return;
      }
    }
  }

  template<typename Key>
  size_type erase(const Key& x)
  {
    auto hash=h(x);
    auto pos=position_for(hash);
    auto it=find(x,pos,hash);
    if(it!=end()){
      auto [pc,pe]=it;
      destroy_element(pe->data());
      pc->reset();
      buckets[pos].reset(minus_wrap(pe-elements.data(),pos));
      --size_;
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

#ifdef FXA_UNORDERED_HOPSCOTCH_STATUS
  void status()const
  {
    long long int occupancy=0;
    int           nonempty_buckets=0;
    int           full_buckets=0;
    for(const auto& b:buckets){
      auto n=boost::core::popcount(b.hopmask());
      if(n==N)++full_buckets;
      if(n!=0){
        occupancy+=n;
        ++nonempty_buckets;
      }
    }
    std::cout<<"\n"
      <<"size: "<<size_<<"\n"
      <<"capacity: "<<capacity_<<"\n"
      <<"load factor: "<<(float)size_/capacity_<<"\n"
      <<"no of hops: "<<num_hops<<"\n"
      <<"no of hopscotch blocks: "<<num_hopscotch_blocks<<"\n"
      <<"no of full buckets: "<<full_buckets<<"\n"
      <<"non-empty bucket occupancy: "<<(float)occupancy/nonempty_buckets<<"/"<<N<<"\n"
      ;
  }
#endif

private:
  // used only on rehash
  foa_unordered_hopscotch_set(std::size_t n,Allocator al):
    al{al},size_index{size_policy::size_index(n)}
  {
    controls.back().set(0,0);
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
    return size_policy::position(hash,size_index);
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
      if(it==end())throw std::runtime_error("hopscotching failed");
    }

    return {it,true};
  }

  void rehash(size_type new_size)
  {
    std::size_t nc =(std::numeric_limits<std::size_t>::max)();
    float       fnc=1.0f+static_cast<float>(new_size)/mlf;
    if(nc>fnc)nc=static_cast<std::size_t>(fnc);

    foa_unordered_hopscotch_set new_container{nc,al};
    std::size_t                 num_tx=0;
    try{
      for(std::size_t pos=0;pos<capacity_;++pos){
        for(auto& mask=buckets[pos].hopmask();mask;mask&=mask-1){
          auto n=boost::core::countr_zero(mask);
          auto pos_n=plus_wrap(pos,n);
          new_container.unchecked_insert(std::move(elements[pos_n].value()));
          destroy_element(elements[pos_n].data());
          controls[pos_n].reset();
          ++num_tx;
        }
      }
    }
    catch(...){
      size_-=num_tx;
      throw;
    }
#ifdef FXA_UNORDERED_HOPSCOTCH_STATUS
    new_container.num_hops+=num_hops;
    new_container.num_hopscotch_blocks+=num_hopscotch_blocks;
#endif      
    *this=std::move(new_container);
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
    auto        &bucket=buckets[pos];
    auto        dst=find_empty_slot(pos);
    std::size_t n=0;
    
    while((n=minus_wrap(dst,pos))>=N){
      for(auto i=N-1;i;--i){
        auto hop=minus_wrap(dst,i);
        auto j=(std::size_t)boost::core::countr_zero(buckets[hop].hopmask());
        if(j<i){ // hop
          auto hop_j=plus_wrap(hop,j);
          construct_element(
            std::move(elements[hop_j].value()),elements[dst].data());
          destroy_element(elements[hop_j].data());
          controls[dst].resit(hop,controls[hop_j]);
          controls[hop_j].reset();
          buckets[hop].set(i);
          buckets[hop].reset(j);
          dst=hop_j;
#ifdef FXA_UNORDERED_HOPSCOTCH_STATUS
          ++num_hops;
#endif
          goto continue_;
        }
      }
#ifdef FXA_UNORDERED_HOPSCOTCH_STATUS
      ++num_hopscotch_blocks;
#endif
      return end();
    continue_:;
    }

    construct_element(std::forward<Value>(x),elements[dst].data());  
    controls[dst].set(pos,hash);
    bucket.set(n);
    ++size_;
    return at(dst);
  }

  std::size_t find_empty_slot(std::size_t pos)const
  {
    for(auto dst=pos;dst<capacity_;++dst){
      if(controls[dst].empty())return dst;
    } 
    for(std::size_t dst=0;;++dst){
      if(controls[dst].empty())return dst;        
    }
  }

  template<typename Key>
  iterator find(const Key& x,std::size_t pos,std::size_t hash)const
  {
    if(BOOST_LIKELY(pos+N<=capacity_)){
#ifdef FXA_UNORDERED_SSE2
      control ctrl;
      ctrl.set(pos,hash);
      auto        a=_mm_set1_epi8(ctrl.value()),
                  b=_mm_loadu_si128(
                    reinterpret_cast<const __m128i*>(&controls[pos]));
#if 0
      std::size_t mask=_mm_movemask_epi8(_mm_cmpeq_epi8(a,b))&
                    buckets[pos].hopmask();
#else /* not loading buckets seems to outperform hopmask filtering */
      std::size_t mask=_mm_movemask_epi8(_mm_cmpeq_epi8(a,b));
#endif
      for(;mask;mask&=mask-1){
        auto n=boost::core::countr_zero(mask);
        auto pos_n=pos+n;
        if(BOOST_LIKELY(pred(x,elements[pos_n].value()))){
          return at(pos_n); 
        }
      }
#else
      for(auto mask=buckets[pos].hopmask();mask;mask&=mask-1){
        auto n=boost::core::countr_zero(mask);
        auto pos_n=pos+n;
        if(controls[pos_n].match(pos,hash)&&
           BOOST_LIKELY(pred(x,elements[pos_n].value()))){
          return at(pos_n); 
        }
      }
#endif /* FXA_UNORDERED_SSE2 */
    }
    else{
      for(auto mask=buckets[pos].hopmask();mask;mask&=mask-1){
        auto n=boost::core::countr_zero(mask);
        auto pos_n=plus_wrap(pos,n);
        if(controls[pos_n].match(pos,hash)&&
           BOOST_LIKELY(pred(x,elements[pos_n].value()))){
          return at(pos_n); 
        }
      }
    }
    return end();
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
  bucket_array  buckets{capacity_,al};
  control_array controls{capacity_+1,al};
  element_array elements{capacity_,al};

#ifdef FXA_UNORDERED_HOPSCOTCH_STATUS
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
using foa_unordered_hopscotch_map=foa_unordered_hopscotch_set<
  map_value_adaptor<Key,Value>,
  map_hash_adaptor<Hash>,map_pred_adaptor<Pred>,
  Allocator,SizePolicy
>;

} // namespace hopscotch

} // namespace fxa_unordered

using fxa_unordered::hopscotch::foa_unordered_hopscotch_set;
using fxa_unordered::hopscotch::foa_unordered_hopscotch_map;

#endif
