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
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <tuple>
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

int match(uint64_t x,int n)
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
struct nway_group
{
  static constexpr int N=16;
  struct element
  {
    T* data(){return reinterpret_cast<T*>(&storage);}
    T& value(){return *data();}

  private:
    std::aligned_storage_t<sizeof(T),alignof(T)> storage;
  };
  struct node_type:element
  {
    node_type *next=nullptr;
  };

#ifdef FXA_UNORDERED_SSE2

  void set(std::size_t pos,std::size_t hash)
  {
    reinterpret_cast<unsigned char*>(&mask)[pos]=0x80u|(hash&0x7Fu);
  }

  void reset(std::size_t pos)
  {
    assert(pos<N);
    reinterpret_cast<unsigned char*>(&mask)[pos]=0;
  }

  int match(std::size_t hash)const
  {
    auto m=_mm_set1_epi8(0x80u|(hash&0x7Fu));
    return _mm_movemask_epi8(_mm_cmpeq_epi8(mask,m));
  }

  int match_non_empty()const
  {
    return (~match_empty())&0xFFFFul;
  }

  int match_empty()const
  {
    auto m=_mm_set1_epi8(0);
    return _mm_movemask_epi8(_mm_cmpeq_epi8(mask,m));      
  }

#else

  void set(std::size_t pos,std::size_t hash)
  {
    assert(pos<N&&n<N);
    uint64_ops::set(lowmask,pos,hash&0xFu);
    uint64_ops::set(himask,pos,0x8u | ((hash&0x70u)>>8));
  }

  void reset(std::size_t pos)
  {
    assert(pos<N);
    uint64_ops::set(himask,pos,0);
  }

  int match(std::size_t hash)const
  {
    assert(n<N);
    return uint64_ops::match(lowmask,hash&0xFu)&
           uint64_ops::match(himask,0x8u | ((hash&0x70u)>>8));
  }

  int match_non_empty()const
  {
    return himask>>48;  
  }

  std::size_t match_empty()const{return (~match_non_empty())&0xFFFFul;}
  
#endif /* FXA_UNORDERED_SSE2 */

  element& at(std::size_t n){return storage[n];}
  node_type*& extra(){return extra_;}

private:
#ifdef FXA_UNORDERED_SSE2
  __m128i   mask=_mm_set1_epi8(0);
#else
  uint64_t  lowmask=0,himask=0;
#endif

  element   storage[N];
  node_type *extra_=nullptr;
};

template<typename T,typename Allocator>
class nway_group_array
{
  using group=nway_group<T>;
  static constexpr auto N=group::N;

public:
  nway_group_array(std::size_t n,const Allocator& al):
    v{(n+N-1)/N+1,al}
  {
    // we're wasting a whole extra group for end signalling :-/
    v.back().set(0,0);
  }

  nway_group_array(nway_group_array&&)=default;
  nway_group_array& operator=(nway_group_array&&)=default;

  auto begin()const{return at(0);}
  auto end()const{return at(v.size()-1);}
  auto at(std::size_t n)const{return const_cast<group*>(&v[n]);}

  // only in moved-from state when rehashing
  bool empty()const{return v.empty();}
   
private:
  std::vector<group,Allocator> v;
};

template<
  typename T,typename Hash=boost::hash<T>,typename Pred=std::equal_to<T>,
  typename Allocator=std::allocator<T>,
  typename SizePolicy=prime_size
>
class foa_unordered_nway_set 
{
  using size_policy=SizePolicy;
  using group=nway_group<T>;
  using node_type=typename group::node_type;
  static constexpr auto N=group::N;

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

    const_iterator(group* pb,int n=0,node_type* px=nullptr):
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
      if(!px){
        if((n=boost::core::countr_zero((unsigned int)(
          pb->match_non_empty()&reset_first_bits(n+1))))<N)return;
      }
      else{
        if((px=px->next))return;
        else goto check_group;
      }
        
      for(;;){
        if((px=pb->extra()))return;
      check_group:
        if((n=boost::core::countr_zero((unsigned int)(
          (++pb)->match_non_empty())))<N)return;
      }
    }

    group     *pb=nullptr;
    int       n=0;
    node_type *px=nullptr;
  };
  using iterator=const_iterator;

  foa_unordered_nway_set()=default;

  ~foa_unordered_nway_set()
  {
    if(!groups.empty()){
     for(auto first=begin(),last=end();first!=last;)erase(first++);
    }
  }
  
  const_iterator begin()const noexcept
  {
    auto pb=groups.begin();
    const_iterator it=pb;
    if(!(pb->match_non_empty()&0x1u))++it;
    return it;
  }
  
  const_iterator end()const noexcept{return groups.end();}
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
    auto pb=groups.at(size_policy::position(hash,size_index)/N);
    return find(x,pb,hash);
  }

private:
  using alloc_traits=std::allocator_traits<Allocator>;
  using node_allocator_type=typename alloc_traits::
    template rebind_alloc<node_type>;
  using node_alloc_traits=std::allocator_traits<node_allocator_type>;
  using group_array_type=nway_group_array<
    T,
    typename alloc_traits::template rebind_alloc<group>>;

  // used only on rehash
  foa_unordered_nway_set(std::size_t n,node_allocator_type al):
    al{al},size_{n}{}

  template<typename Value>
  node_type* new_node(Value&& x)
  {
    node_type* p=&*node_alloc_traits::allocate(al,1);
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

  void delete_node(node_type* p)
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
    auto pb=groups.at(size_policy::position(hash,size_index)/N);
    auto it=find(x,pb,hash);
    if(it!=end())return {it,false};
        
    if(BOOST_UNLIKELY(size_+1>ml)){
      rehash(size_+1);
      pb=groups.at(size_policy::position(hash,size_index)/N);
    }

    return {unchecked_insert(std::forward<Value>(x),pb,hash),true};
  }

  void rehash(size_type new_size)
  {
    std::size_t nc =(std::numeric_limits<std::size_t>::max)();
    float       fnc=1.0f+static_cast<float>(new_size)/mlf;
    if(nc>fnc)nc=static_cast<std::size_t>(fnc);

    foa_unordered_nway_set new_container{nc,al};
    std::size_t            num_tx=0;
    try{
      for(auto& b:groups){
        auto mask=b.match_non_empty();
        auto n=std::size_t(boost::core::countr_zero((unsigned int)mask));
        while(n<N){
          auto& x=b.at(n).value();
          new_container.unchecked_insert(std::move(x));
          destroy_element(&x);
          b.reset(n);
          ++num_tx;
          mask&=mask-1;
          n=boost::core::countr_zero((unsigned int)mask);
        }
        while(auto px=b.extra()){
          auto& x=px->value();
          new_container.unchecked_insert(std::move(x));
          b.extra()=px->next;
          delete_node(px);
          ++num_tx;
        }
      }
    }
    catch(...){
      size_-=num_tx;
      throw;
    }
    size_index=new_container.size_index;
    groups=std::move(new_container.groups);
    ml=max_load();   
  }

  template<typename Value>
  iterator unchecked_insert(Value&& x)
  {
    auto  hash=h(x);
    auto  pb=groups.at(size_policy::position(hash,size_index)/N);
    return unchecked_insert(std::move(x),pb,hash);
  }

  template<typename Value>
  iterator unchecked_insert(Value&& x,group* pb,std::size_t hash)
  {
    auto n=boost::core::countr_zero((unsigned int)(pb->match_empty()));
    if(n<N){
      construct_element(std::forward<Value>(x),pb->at(n).data());
      pb->set(n,hash);
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
  iterator find(const Key& x,group* pb,std::size_t hash)const
  {
    auto mask=pb->match(hash),
         n=boost::core::countr_zero((unsigned int)mask);

    while(n<N){
      if(BOOST_LIKELY(pred(x,pb->at(n).value()))){
        return {pb,n};
      }
      mask&=mask-1;
      n=boost::core::countr_zero((unsigned int)mask);
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
  float               mlf=0.875f;
  std::size_t         size_=0;
  std::size_t         size_index=size_policy::size_index(size_);
  group_array_type    groups{size_policy::size(size_index),al};
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

namespace nwayplus{

struct group_base
{
  static constexpr int N=16;

#ifdef FXA_UNORDERED_SSE2

  inline void set(std::size_t pos,unsigned char hash)
  {
    reinterpret_cast<unsigned char*>(&mask)[pos]=hash&0x7Fu;
  }

  inline void set_sentinel()
  {
    reinterpret_cast<unsigned char*>(&mask)[N-1]=sentinel_;
  }

  inline void reset(std::size_t pos)
  {
    assert(pos<N);
    reinterpret_cast<unsigned char*>(&mask)[pos]=deleted_;
  }

  inline int match(unsigned char hash)const
  {
    auto m=_mm_set1_epi8(hash&0x7Fu);
    return _mm_movemask_epi8(_mm_cmpeq_epi8(mask,m));
  }

  inline int match_empty()const
  {
    auto m=_mm_set1_epi8(empty_);
    return _mm_movemask_epi8(_mm_cmpeq_epi8(mask,m));
  }

  inline int match_empty_or_deleted()const
  {
    auto m=_mm_set1_epi8(sentinel_);
    return _mm_movemask_epi8(_mm_cmpgt_epi8_fixed(m,mask));    
  }

  inline int match_occupied()const
  {
    return (~match_empty_or_deleted())&0xFFFFul;
  }

  inline int match_really_occupied()const // excluding sentinel
  {
    return (~_mm_movemask_epi8(mask))&0xFFFFul;
  }

private:
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

#else

  inline void set(std::size_t pos,unsigned char hash)
  {
    set_impl(pos,hash&0x7Fu);
  }

  inline void set_sentinel()
  {
    uint64_ops::set(himask,N-1,sentinel_);
  }

  inline void reset(std::size_t pos)
  {
    uint64_ops::set(himask,pos,deleted_);
  }

  inline int match(unsigned char hash)const
  {
    return match_impl(hash&0x7Fu);
  }

  inline int match_empty()const
  {
    return
      (himask & uint64_t(0x0000FFFF00000000ull))>>32&
      (himask & uint64_t(0xFFFF000000000000ull))>>48;
  }

  inline int match_empty_or_deleted()const
  {
    return
      (himask & uint64_t(0x00000000FFFF0000ull))>>16&
      (himask & uint64_t(0xFFFF000000000000ull))>>48;
  }

  inline int match_occupied()const
  {
    return // ~match_empty_or_deleted()
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

  inline void set_impl(std::size_t pos,unsigned char m)
  {
    uint64_ops::set(lowmask,pos,m&0xFu);
    uint64_ops::set(himask,pos,m>>4);
  }
  
  inline int match_impl(unsigned char m)const
  {
    return uint64_ops::match(lowmask,m&0xFu)&
           uint64_ops::match(himask,m>>4);
  }

  uint64_t lowmask=0,himask=0xFFFFFFFFFFFF0000ull;
#endif /* FXA_UNORDERED_SSE2 */
};

template<typename T>
struct element
{
  T* data(){return reinterpret_cast<T*>(&storage);}
  T& value(){return *data();}

private:
  std::aligned_storage_t<sizeof(T),alignof(T)> storage;
};

template<typename T>
struct soa_group:group_base
{
  using element_type=element<T>;
};

template<typename T>
struct regular_group:soa_group<T>
{
  auto& at(std::size_t n){return storage[n];}

private:
  using super=soa_group<T>;
  typename super::element_type storage[super::N];
};

template<typename T>
struct soa_coalesced_group:soa_group<T>
{
  soa_coalesced_group*& next(){return next_;}

private:
  soa_coalesced_group *next_=nullptr;
};

template<typename T>
struct coalesced_group:regular_group<T>
{
  coalesced_group*& next(){return next_;}

private:
  coalesced_group *next_=nullptr;
};

template<typename Group,typename Allocator>
class group_array
{
public:
  static constexpr auto N=Group::N;
  using iterator=Group*;

  group_array(std::size_t n,const Allocator& al):
    v{n,al}{}
  group_array(group_array&&)=default;
  group_array& operator=(group_array&&)=default;

  static auto& control(iterator it){return *it;}
  static auto& elements(iterator it){return *it;}
  
  iterator begin()const{return const_cast<Group*>(v.data());}
  iterator end()const{return const_cast<Group*>(v.data()+v.size());}
  iterator at(std::size_t n)const{return const_cast<Group*>(&v[n]);}
  std::size_t size()const{return v.size();}

private:
  std::vector<Group,Allocator> v;
};

template<typename Group,typename Allocator>
class soa_group_array
{
public:
  static constexpr auto N=Group::N;

private:
  struct elements_type
  {
    auto& at(std::size_t n){return storage[n];}

  private:
    typename Group::element_type storage[N];
  };

public:
  class iterator:public boost::iterator_facade<
    iterator,const Group,boost::random_access_traversal_tag>
  {
    using super=boost::iterator_facade<
      iterator,const Group,boost::random_access_traversal_tag>;

  public:
    iterator()=default;

    using super::operator=;
    iterator& operator=(Group* pg)noexcept
    {
      auto diff=pg-this->pg;
      this->pg=pg;
      this->pe+=diff;
      return *this;
    }

    operator Group*()const noexcept{return pg;}

    explicit operator bool()const noexcept{return pg;}
    bool operator!()const noexcept{return !pg;}

  private:
    friend class soa_group_array;
    friend class boost::iterator_core_access;

    iterator(Group *pg,elements_type* pe):pg{pg},pe{pe}{}

    bool equal(Group* pg)const noexcept{return this->pg==pg;}
    bool equal(const iterator& x)const noexcept{return pg==x.pg;}

    friend bool operator==(const iterator& x,Group* pg)noexcept
      {return x.equal(pg);}
    friend bool operator==(Group* pg,const iterator& x)noexcept
      {return x.equal(pg);}
    friend bool operator!=(const iterator& x,Group* pg)noexcept
      {return !x.equal(pg);}
    friend bool operator!=(Group* pg,const iterator& x)noexcept
      {return !x.equal(pg);}
    friend bool operator==(const iterator& x,const iterator& y)noexcept
      {return x.equal(y);}
    friend bool operator!=(const iterator& x,const iterator& y)noexcept
      {return !x.equal(y);}

    const auto& dereference()const noexcept{return *pg;} // not really used
    void increment()noexcept{++pg;++pe;}
    void decrement()noexcept{--pg;--pe;}
    void advance(std::ptrdiff_t i)noexcept{pg+=i;pe+=i;}
    std::ptrdiff_t distance_to(const iterator& x)const{return x.pg-pg;}

    Group         *pg=nullptr;
    elements_type *pe;
  };
  friend class iterator;

  soa_group_array(std::size_t n,const Allocator& al):
    v{n,al},e{n,al}{}
  soa_group_array(soa_group_array&&)=default;
  soa_group_array& operator=(soa_group_array&&)=default;

  static auto& control(iterator it){return *it.pg;}
  static auto& elements(iterator it){return *it.pe;}
  
  iterator begin()const{return at(0);}
  iterator end()const{return at(size());}

  iterator at(std::size_t n)const
  {
    return {
      const_cast<Group*>(v.data()+n),
      const_cast<elements_type*>(e.data()+n)
    };
  }

  std::size_t size()const{return v.size();}

private:
  using elements_allocator_type=typename std::allocator_traits<Allocator>::
    template rebind_alloc<elements_type>;

  std::vector<Group,Allocator>                       v;
  std::vector<elements_type,elements_allocator_type> e;
};

template<typename GroupArray>
class group_allocator:public GroupArray
{
public:
  using GroupArray::N;
  using iterator=typename GroupArray::iterator;

  template<typename Allocator>
  group_allocator(std::size_t n,const Allocator& al):
    GroupArray{n,al}
  {
    control(this->end()-1).set_sentinel();
  }

  group_allocator(group_allocator&&)=default;
  group_allocator& operator=(group_allocator&& x)=default;

  using GroupArray::control;

  // only in moved-from state when rehashing
  bool empty()const{return !this->size();}

  std::pair<iterator,int> new_group_after(iterator first,iterator /*it*/)
  {
    for(auto pr=make_prober(first);;){
      pr.next();
      if(auto mask=control(pr.get()).match_empty_or_deleted()){
        FXA_ASSUME(mask!=0);
        return {pr.get(),boost::core::countr_zero((unsigned int)mask)};
      }
    }
  }

  struct prober
  {        
    iterator get()const noexcept{return self->begin()+n;}

    void next()noexcept
    {
      for(;;){
        n=(n+i)&self->pow2mask;
        i+=1;
        if(n<self->size())break;
      }
    }

  private:
    friend class group_allocator;

    prober(const group_allocator* self,iterator it):
      self{self},n{(std::size_t)(it-self->begin())}{}

    const group_allocator* self;
    std::size_t            n,
                           i=1;
  };
  friend class prober;

  prober make_prober(iterator it)const
  {
    return {this,it};
  }

#ifdef FOA_UNORDERED_NWAYPLUS_STATUS
  void status(){}
#endif

  std::size_t pow2mask=boost::core::bit_ceil(this->size())-1;
};


template<typename GroupArray>
class pyramid_allocator:public GroupArray
{
public:
  using GroupArray::N;
  using iterator=typename GroupArray::iterator;

  template<typename Allocator>
  pyramid_allocator(std::size_t n,const Allocator& al):
    GroupArray{std::size_t(std::ceil(n/0.875)),al},
    address_size{n}
  {
    control(this->end()-1).set_sentinel();
  }

  pyramid_allocator(pyramid_allocator&&)=default;
  pyramid_allocator& operator=(pyramid_allocator&& x)=default;

  using GroupArray::control;

  // only in moved-from state when rehashing
  bool empty()const{return !this->size();}

  std::pair<iterator,int> new_group_after(iterator first,iterator /*it*/)
  {
    for(auto pr=make_prober(first);;){
      pr.next();
      if(auto mask=control(pr.get()).match_empty_or_deleted()){
        FXA_ASSUME(mask!=0);
        return {pr.get(),boost::core::countr_zero((unsigned int)mask)};
      }
    }
  }

  struct prober
  {        
    iterator get()const noexcept{return begin+n;}

    void next()noexcept
    {
      if(delta){
        n=base+delta+(n-base)/8;
        base+=delta;
        delta/=8;
        return;
      }
      for(;;){
        n0=(n0+i)&pow2mask;
        i+=1;
        if(n0<size){
          n=n0;
          return;
        }
      }
    }

  private:
    friend class pyramid_allocator;

    prober(iterator begin,std::size_t size,iterator it):
      begin{begin},size{size},
      n0{(std::size_t)(it-begin)}
    {
      assert(n0<size);
      next();
    }

    iterator    begin;
    std::size_t size,
                n0,
                n=n0,
                delta=size,
                base=0,
                i=1,
                pow2mask=boost::core::bit_ceil(size)-1;
  };

  prober make_prober(iterator it)const
  {
    return {this->begin(),address_size,it};
  }

#ifdef FOA_UNORDERED_NWAYPLUS_STATUS
  void status()
  {
    int occupied_groups=0,
        occupied_address_groups=0,
        occupied_cellar_groups=0,
        free_groups=0;
    long long int occupancy_used_address=0,
                  occupancy_used_cellar=0;
    for(auto it=this->begin(),last=this->end();it!=last;++it){
      if(control(it).match_occupied()){
        ++occupied_groups;
        if(it<this->begin()+address_size){
          ++occupied_address_groups;
          occupancy_used_address+=boost::core::popcount((unsigned int)
            control(it).match_occupied());
        }
        else{            
          ++occupied_cellar_groups;
          occupancy_used_cellar+=boost::core::popcount((unsigned int)
            control(it).match_occupied());
        }
      }
      else ++free_groups;
    }
    std::cout<<"\n"
      <<"size: "<<this->size()<<"\n"
      <<"address size: "<<address_size<<"\n"
      <<"occupied groups: "<<occupied_groups<<"\n"
      <<"occupied address groups: "<<occupied_address_groups<<"\n"
      <<"occupied cellar groups: "<<occupied_cellar_groups<<"\n"     
      <<"free groups: "<<free_groups<<"\n"
      <<"occup. used address: "<<(double)occupancy_used_address/occupied_address_groups<<"\n"
      <<"occup. used cellar: "<<(double)occupancy_used_cellar/occupied_cellar_groups<<"\n"
      ;
  }
#endif

private:
  std::size_t address_size;
};

template<typename GroupArray>
class coalesced_group_allocator:public group_allocator<GroupArray>
{
  using super=group_allocator<GroupArray>;
public:
  static constexpr auto address_factor=0.86f;
  static constexpr int  max_saturation=4;

  using GroupArray::N;
  using iterator=typename GroupArray::iterator;

  template<typename Allocator>
  coalesced_group_allocator(std::size_t n,const Allocator& al):
    super{std::size_t(n/address_factor),al},address_size_{n}{}

  coalesced_group_allocator(coalesced_group_allocator&&)=default;
  coalesced_group_allocator& operator=(coalesced_group_allocator&& x)=default;

  using GroupArray::control;

  std::pair<iterator,int> new_group_after(iterator first,iterator it)
  {
    assert(!control(it).match_empty_or_deleted());
    if(auto n=boost::core::countr_zero((unsigned int)
         control(top).match_empty_or_deleted());n<max_saturation){
      control(it).next()=top;
      return {top,n};
    }
    else if(top>this->begin()+address_size_){
      control(it).next()=--top;
      return {top,0};
    }
    control(it).next()=it; // close chain 

    return super::new_group_after(first,it);
  }

  auto make_prober(iterator it)const
  {
    assert(!theres_remaining_cellar());
    return super::make_prober(it);
  }

  bool theres_remaining_cellar()const
  {
    return top>this->begin()+address_size_;
  }

#ifdef FOA_UNORDERED_NWAYPLUS_STATUS
  void status()
  {
    int occupied_groups=0,
        occupied_address_groups=0,
        free_groups=0;
    long long int occupancy_used_cellar=0,
                  occupancy_used_address=0,
                  chain_length=0;

    for(auto it=this->begin(),last=this->end();it!=last;++it){
      if(control(it).match_occupied()){
        ++occupied_groups;
        if(it<this->begin()+address_size_){
          ++occupied_address_groups;
          for(
            auto it2=it;control(it2).next()&&it2!=control(it2).next();
            it2=control(it2).next()){
            ++chain_length;
          }
        }
      }
      else ++free_groups;
    }
    for(auto it=top;it<this->end();++it){
      assert(boost::core::popcount((unsigned int)control(it).match_occupied())<=N);
      occupancy_used_cellar+=boost::core::popcount((unsigned int)
        control(it).match_occupied());
    }
    for(auto it=this->begin();it<this->begin()+address_size_;++it){
      assert(boost::core::popcount((unsigned int)control(it).match_occupied())<=N);
      occupancy_used_address+=boost::core::popcount((unsigned int)
        control(it).match_occupied());
    }      
    std::cout<<"\n"
      <<"size: "<<this->size()<<"\n"
      <<"address size: "<<address_size_<<"\n"
      <<"occupied groups: "<<occupied_groups<<"\n"
      <<"occupied address groups: "<<occupied_address_groups<<"\n"
      <<"free groups: "<<free_groups<<"\n"
      <<"cellar remaining: "<<100.0*(top-(this->begin()+address_size_))/(this->size()-address_size_)<<"%\n"
      <<"occup. used cellar: "<<(double)occupancy_used_cellar/(this->begin()+this->size()-top)<<"\n"
      <<"occup. used address: "<<(double)occupancy_used_address/occupied_address_groups<<"\n"
      <<"avg. chain length: "<<(double)chain_length/occupied_address_groups<<"\n"
      ;
  }
#endif /* FOA_UNORDERED_NWAYPLUS_STATUS */

private:
  std::size_t  address_size_;
  iterator     top=this->end()-1;
};

struct regular_allocation
{
  template<typename T>
  using group_type=regular_group<T>;
  
  static constexpr float mlf=0.875;

  template<typename Group,typename Allocator>
  using allocator_type=group_allocator<
    group_array<Group,Allocator>>;
};

struct pyramid_allocation
{
  template<typename T>
  using group_type=regular_group<T>;

  static constexpr float mlf=1.0;

  template<typename Group,typename Allocator>
  using allocator_type=pyramid_allocator<
    group_array<Group,Allocator>>;
};

struct soa_allocation
{
  template<typename T>
  using group_type=soa_group<T>;

  static constexpr float mlf=0.875;

  template<typename Group,typename Allocator>
  using allocator_type=group_allocator<
    soa_group_array<Group,Allocator>>;
};

struct coalesced_allocation
{
  template<typename T>
  using group_type=coalesced_group<T>;

  static constexpr float mlf=1.0;

  template<typename Group,typename Allocator>
  using allocator_type=coalesced_group_allocator<
    group_array<Group,Allocator>>;
};

struct soa_coalesced_allocation
{
  template<typename T>
  using group_type=soa_coalesced_group<T>;

  static constexpr float mlf=1.0;

  template<typename Group,typename Allocator>
  using allocator_type=coalesced_group_allocator<
    soa_group_array<Group,Allocator>>;
};

template<typename Group,typename=void>
struct groups_are_linked:std::false_type{};

template<typename Group>
struct groups_are_linked<
  Group,
  std::void_t<decltype(std::declval<Group>().next())>
>:std::true_type{};

template<
  typename T,typename Hash=boost::hash<T>,typename Pred=std::equal_to<T>,
  typename Allocator=std::allocator<T>,
  typename SizePolicy=prime_size,
  typename HashSplitPolicy=shift_mod_hash<0>,
  typename GroupAllocationPolicy=regular_allocation
>
class foa_unordered_nwayplus_set 
{
  using size_policy=SizePolicy;
  using hash_split_policy=HashSplitPolicy;
  using group_allocation_policy=GroupAllocationPolicy;
  using alloc_traits=std::allocator_traits<Allocator>;
  using group=typename group_allocation_policy::template group_type<T>;
  using group_allocator=typename group_allocation_policy::template
    allocator_type<
      group,
      typename alloc_traits:: template rebind_alloc<group>>;
  using group_iterator=typename group_allocator::iterator;
  using linked_groups=groups_are_linked<group>;

  static constexpr auto N=group::N;

  static auto& control(group_iterator itg)
    {return group_allocator::control(itg);}
  static auto& elements(group_iterator itg)
    {return group_allocator::elements(itg);}

public:
  using key_type=T;
  using value_type=T;
  using size_type=std::size_t;
  class const_iterator:public boost::iterator_facade<
    const_iterator,const value_type,boost::forward_traversal_tag>
  {
  public:
    const_iterator()=default;

    explicit operator bool()const noexcept{return itg;}
    bool operator!()const noexcept{return !itg;}

  private:
    friend class foa_unordered_nwayplus_set;
    friend class boost::iterator_core_access;

    const_iterator(group_iterator itg,int n=0):itg{itg},n{n}{}

    const value_type& dereference()const noexcept
    {
      return elements(itg).at(n).value();
    }

    bool equal(const const_iterator& x)const noexcept
    {
      return itg==x.itg&&n==x.n;
    }

    void increment()noexcept
    {
      if((n=boost::core::countr_zero((unsigned int)(
          control(itg).match_occupied()&reset_first_bits(n+1))))<N)return;
      while((n=boost::core::countr_zero((unsigned int)(
             control(++itg).match_occupied())))>=N);
    }

    group_iterator itg={};
    int            n=0;
  };
  using iterator=const_iterator;

  foa_unordered_nwayplus_set()=default;

  ~foa_unordered_nwayplus_set()
  {
    if(!groups.empty()){
     for(auto first=begin(),last=end();first!=last;)erase(first++);
    }
  }
  
  const_iterator begin()const noexcept
  {
    auto itg=groups.begin();
    const_iterator it=itg;
    if(!(control(itg).match_occupied()&0x1u))++it;
    return it;
  }
  
  const_iterator end()const noexcept{return {groups.end()-1,N-1};}
  size_type size()const noexcept{return size_;};

  auto insert(const T& x){return insert_impl(x);}
  auto insert(T&& x){return insert_impl(std::move(x));}

  void erase(const_iterator pos)
  {
    auto [itg,n]=pos;
    destroy_element(elements(itg).at(n).data());
    control(itg).reset(n);
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
  iterator find(const Key& x)const
  {
    return find_impl(x,linked_groups());
  }

#ifdef FOA_UNORDERED_NWAYPLUS_STATUS
  void status()
  {
    groups.status();
    std::cout
      <<"succesful avg. runlength: "<<float(successful_find_runlengths)/successful_find_runs<<"\n"
      <<"unsuccesful avg. runlength: "<<float(unsuccessful_find_runlengths)/unsuccessful_find_runs<<"\n"
      <<"avg. matches per find: "<<float(num_matches)/num_finds<<"\n"
      ;
  }
#endif

private:
  // used only on rehash
  foa_unordered_nwayplus_set(std::size_t n,Allocator al):
    al{al},size_{n}{}

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

  static void prefetch_elements(group_iterator itg)
  {
    constexpr int cache_line=64;
    char *p0=(char*)elements(itg).at(0).data(),
         *p1=p0+sizeof(value_type);
    for(char* p=p0;p<p1;p+=cache_line)prefetch(p);
  }

  group_iterator group_for(std::size_t hash)const
  {
    return groups.at(size_policy::position(hash,group_size_index));
  }

  template<typename Key>
  std::pair<int,bool> find_in_group(
    const Key& x,group_iterator itg,unsigned char short_hash)const
  {
    auto mask=control(itg).match(short_hash);
    if(mask)prefetch_elements(itg);
    while(mask){
#ifdef FOA_UNORDERED_NWAYPLUS_STATUS
      ++num_matches;
#endif
        
      FXA_ASSUME(mask!=0);
      auto n=boost::core::countr_zero((unsigned int)mask);
      if(BOOST_LIKELY(pred(x,elements(itg).at(n).value())))return {n,true};
      mask&=mask-1;
    }
    return {0,false};
  }

  template<typename Key>
  iterator find_impl(const Key& x,std::true_type /* linked groups */)const
  {    
#ifdef FOA_UNORDERED_NWAYPLUS_STATUS
    ++num_finds; // TODO: this shouldn't go when !linked_groups
    int runlength=1;
#endif

    auto hash=h(x);
    auto short_hash=hash_split_policy::short_hash(hash);
    auto first=group_for(hash);

    for(auto itg=first;;){
      auto [n,found]=find_in_group(x,itg,short_hash);
      if(found){
#ifdef FOA_UNORDERED_NWAYPLUS_STATUS
        successful_find_runlengths+=runlength;
        successful_find_runs+=1;  
#endif
        return {itg,n};
      }

      auto next=control(itg).next();
      if(!next){
#ifdef FOA_UNORDERED_NWAYPLUS_STATUS
        unsuccessful_find_runlengths+=runlength;
        unsuccessful_find_runs+=1;  
#endif
        return end();
      }
      else if(itg==next)break; // chain closed, go probing
      else              itg=next;
    }
      
    for(auto pr=groups.make_prober(first);;){
      pr.next();
        
#ifdef FOA_UNORDERED_NWAYPLUS_STATUS
      ++runlength;
#endif
        
      auto itg=pr.get();
      auto [n,found]=find_in_group(x,itg,short_hash);
      if(found){
#ifdef FOA_UNORDERED_NWAYPLUS_STATUS
        successful_find_runlengths+=runlength;
        successful_find_runs+=1;  
#endif
        return {itg,n};
      }
      if(control(itg).match_empty()){
#ifdef FOA_UNORDERED_NWAYPLUS_STATUS
        unsuccessful_find_runlengths+=runlength;
        unsuccessful_find_runs+=1;  
#endif
        return end();
      }
    }
  }

  template<typename Key>
  iterator find_impl(const Key& x,std::false_type /* linked groups */)const
  {    
#ifdef FOA_UNORDERED_NWAYPLUS_STATUS
    ++num_finds;
    int runlength=1;
#endif

    auto hash=h(x);
    auto short_hash=hash_split_policy::short_hash(hash);
    auto first=group_for(hash);

    auto [n,found]=find_in_group(x,first,short_hash);
    if(found){
#ifdef FOA_UNORDERED_NWAYPLUS_STATUS
      successful_find_runlengths+=runlength;
      successful_find_runs+=1;  
#endif
      return {first,n};
    }
    if(control(first).match_empty()){
#ifdef FOA_UNORDERED_NWAYPLUS_STATUS
      unsuccessful_find_runlengths+=runlength;
      unsuccessful_find_runs+=1;  
#endif
      return end();
    }
    

    for(auto pr=groups.make_prober(first);;){        
      pr.next();
        
#ifdef FOA_UNORDERED_NWAYPLUS_STATUS
      ++runlength;
#endif
        
      auto itg=pr.get();
      auto [n,found]=find_in_group(x,itg,short_hash);
      if(found){
#ifdef FOA_UNORDERED_NWAYPLUS_STATUS
        successful_find_runlengths+=runlength;
        successful_find_runs+=1;  
#endif
        return {itg,n};
      }
      if(control(itg).match_empty()){
#ifdef FOA_UNORDERED_NWAYPLUS_STATUS
        unsuccessful_find_runlengths+=runlength;
        unsuccessful_find_runs+=1;  
#endif
        return end();
      }
    }
  }

  template<typename Value>
  std::pair<iterator,bool> insert_impl(Value&& x)
  {
    if constexpr(linked_groups::value){
      auto hash=h(x);
      auto short_hash=hash_split_policy::short_hash(hash);
      auto first=group_for(hash);
      auto [it,ita,last]=find_match_available_last(x,first,short_hash);
      if(it!=end())return {it,false};

      if(BOOST_UNLIKELY(size_+1>ml)){
        rehash(size_+1);
        return {unchecked_insert(std::forward<Value>(x),hash,short_hash),true};
      }

      auto& [itga,na]=ita;
      if(!itga){
        assert(last);
        std::tie(itga,na)=groups.new_group_after(first,last);
      }
      construct_element(std::forward<Value>(x),elements(itga).at(na).data());  
      control(itga).set(na,short_hash);
      ++size_;
      return {ita,true};
    }
    else{ // no linked groups
      auto           hash=h(x);
      auto           short_hash=hash_split_policy::short_hash(hash);
      auto           first=group_for(hash);
      group_iterator itga;
      int            maska=0;
      auto           pr=groups.make_prober(first);

      for(;;pr.next()){     
        auto itg=pr.get();
        auto [n,found]=find_in_group(x,itg,short_hash);
        if(found)return {{itg,n},false};
        if(!maska&&(maska=control(itg).match_empty_or_deleted()))itga=itg;
        if(control(itg).match_empty())break;
      }

      if(BOOST_UNLIKELY(size_+1>ml)){
        rehash(size_+1);
        return {unchecked_insert(std::forward<Value>(x),hash,short_hash),true};
      }

      if(!maska){
        for(;;){
          pr.next();
          auto itg=pr.get();
          if((maska=control(itg).match_empty_or_deleted())){
            itga=itg;
            break;
          }
        }
      }
      int n=boost::core::countr_zero((unsigned int)maska); 
      construct_element(
        std::forward<Value>(x),elements(itga).at(n).data());  
      control(itga).set(n,short_hash);
      ++size_;
      return {{itga,n},true};    
    }
  }

  void rehash(size_type new_size)
  {
    std::size_t nc =(std::numeric_limits<std::size_t>::max)();
    float       fnc=1.0f+static_cast<float>(new_size)/mlf;
    if(nc>fnc)nc=static_cast<std::size_t>(fnc);

    foa_unordered_nwayplus_set new_container{nc,al};
    std::size_t                num_tx=0;
    try{
      for(auto itg=groups.begin(),last=groups.end();itg!=last;++itg){
        auto mask=control(itg).match_really_occupied();
        while(mask){
          FXA_ASSUME(mask!=0);
          auto n=std::size_t(boost::core::countr_zero((unsigned int)mask));
          auto& x=elements(itg).at(n).value();
          new_container.unchecked_insert(std::move(x));
          destroy_element(&x);
          control(itg).reset(n);
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
    ml=max_load();   
  }

  template<typename Value>
  iterator unchecked_insert(Value&& x)
  {
    auto hash=h(x);
    return unchecked_insert(
      std::forward<Value>(x),hash,hash_split_policy::short_hash(hash));
  }

  template<typename Value>
  iterator unchecked_insert(
    Value&& x,std::size_t hash,unsigned char short_hash)
  {
    auto first=group_for(hash),
         itg=first;
    int  mask,n;

    if constexpr(linked_groups::value){
      // unchecked_insert only called just after rehashing, so
      // there are no deleted elements and we can go till end of chain
      while(control(itg).next()&&itg!=control(itg).next())itg=control(itg).next();

      // if chain's not closed see occupancy, otherwise go probing
      if(!control(itg).next()&&(mask=control(itg).match_empty())){
        FXA_ASSUME(mask!=0);
        n=boost::core::countr_zero((unsigned int)mask);
      }
      else{
        std::tie(itg,n)=groups.new_group_after(first,itg);
      }
    }
    else{ // no linked groups
      if((mask=control(itg).match_empty_or_deleted())){
        FXA_ASSUME(mask!=0);
        n=boost::core::countr_zero((unsigned int)mask);
      }
      else{
        std::tie(itg,n)=groups.new_group_after(first,itg);
      }
    }
      
    construct_element(std::forward<Value>(x),elements(itg).at(n).data());
    control(itg).set(n,short_hash);
    ++size_;
    return {itg,n};
  }

  struct find_match_available_last_return_type
  {
    iterator       it,ita={};
    group_iterator last={};
  };

  template<typename Key>
  find_match_available_last_return_type
  find_match_available_last(
    const Key& x,group_iterator first,unsigned char short_hash)const
  {
    iterator ita;
    auto     update_ita=[&](group_iterator itg)
    {
      if(!ita){
        if(auto mask=control(itg).match_empty_or_deleted()){
          FXA_ASSUME(mask!=0);
          ita={itg,boost::core::countr_zero((unsigned int)mask)};
        }
      }        
    };

   if constexpr(linked_groups::value){
      for(auto itg=first;;){
        auto [n,found]=find_in_group(x,itg,short_hash);
        if(found)return {{itg,n}};
        update_ita(itg); 

        auto next=control(itg).next();
        if(!next)         return {end(),ita,itg};
        else if(itg==next)break; // chain closed, go probing
        else              itg=next;
      }
    }
   
    for(auto pr=groups.make_prober(first);;){
      if constexpr(linked_groups::value)pr.next();
        
      auto itg=pr.get();
      auto [n,found]=find_in_group(x,itg,short_hash);
      if(found)return {{itg,n}};
      update_ita(itg); 
      if(control(itg).match_empty())return {end(),ita}; // ita must be non-null
        
      if constexpr(!linked_groups::value)pr.next();
    }
  }

  size_type max_load()const
  {
    float fml=mlf*static_cast<float>(size_policy::size(group_size_index)*N-1);
    auto res=(std::numeric_limits<size_type>::max)();
    if(res>fml)res=static_cast<size_type>(fml);
    return res;
  }  

  Hash            h;
  Pred            pred;
  Allocator       al;
  float           mlf=group_allocation_policy::mlf;
  std::size_t     size_=0;
  std::size_t     group_size_index=size_policy::size_index(size_/N+1);
  group_allocator groups{size_policy::size(group_size_index),al};
  size_type       ml=max_load();

#ifdef FOA_UNORDERED_NWAYPLUS_STATUS
  mutable long long int successful_find_runlengths=0,
                        unsuccessful_find_runlengths=0,
                        successful_find_runs=0,
                        unsuccessful_find_runs=0,
                        num_matches=0,
                        num_finds=0;
#endif
};

template<
  typename Key,typename Value,
  typename Hash=boost::hash<Key>,typename Pred=std::equal_to<Key>,
  typename Allocator=std::allocator<map_value_adaptor<Key,Value>>,
  typename SizePolicy=prime_size,
  typename HashSplitPolicy=shift_mod_hash<0>,
  typename GroupAllocationPolicy=regular_allocation
>
using foa_unordered_nwayplus_map=foa_unordered_nwayplus_set<
  map_value_adaptor<Key,Value>,
  map_hash_adaptor<Hash>,map_pred_adaptor<Pred>,
  Allocator,SizePolicy,HashSplitPolicy,GroupAllocationPolicy
>;

} // namespace nwayplus

} // namespace fxa_unordered

using fxa_unordered::foa_unordered_nway_set;
using fxa_unordered::foa_unordered_nway_map;
using fxa_unordered::nwayplus::foa_unordered_nwayplus_set;
using fxa_unordered::nwayplus::foa_unordered_nwayplus_map;

#endif
