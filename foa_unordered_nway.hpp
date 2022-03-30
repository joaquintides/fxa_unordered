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

#if __SSE2__
#include <immintrin.h>
#endif

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

#if __SSE2__

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
    return ~match_empty();
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

  std::size_t match_empty()const{return ~match_non_empty();}
  
#endif /* __SSE2__ */

  element& at(std::size_t n){return storage[n];}
  node_type*& extra(){return extra_;}

private:
#if __SSE2__
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

template<typename T>
struct nwayplus_group
{
  static constexpr int N=16;
  struct element
  {
    T* data(){return reinterpret_cast<T*>(&storage);}
    T& value(){return *data();}

  private:
    std::aligned_storage_t<sizeof(T),alignof(T)> storage;
  };

#if __SSE2__

  void set(std::size_t pos,std::size_t hash)
  {
    reinterpret_cast<unsigned char*>(&mask)[pos]=hash&0x7Fu;
  }

  void set_sentinel()
  {
    reinterpret_cast<unsigned char*>(&mask)[N-1]=sentinel_;
  }

  void reset(std::size_t pos)
  {
    assert(pos<N);
    reinterpret_cast<unsigned char*>(&mask)[pos]=deleted_;
  }

  int match(std::size_t hash)const
  {
    auto m=_mm_set1_epi8(hash&0x7Fu);
    return _mm_movemask_epi8(_mm_cmpeq_epi8(mask,m));
  }

  int match_empty()const
  {
    auto m=_mm_set1_epi8(empty_);
    return _mm_movemask_epi8(_mm_cmpeq_epi8(mask,m));
  }

  int match_empty_or_deleted()const
  {
    auto m=_mm_set1_epi8(-1);
    return _mm_movemask_epi8(_mm_cmpgt_epi8_fixed(m,mask));    
  }

  int match_occupied()const
  {
    return ~match_empty_or_deleted();
  }

  int match_really_occupied()const // exclusing sentinel
  {
    return ~_mm_movemask_epi8(mask);
  }

#else

#error non-SSE2 code not written yet

#endif /* __SSE2__ */

  element& at(std::size_t n){return storage[n];}
  nwayplus_group*& next(){return next_;}

private:
#if __SSE2__
  // exact values as per Abseil rationale
  static constexpr int8_t empty_=-128,
                          deleted_=-2,
                          sentinel_=-1;

  //ripped from Abseil raw_hash_set.h
  static __m128i _mm_cmpgt_epi8_fixed(__m128i a, __m128i b) {
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
  uint64_t  lowmask=0,himask=0;
#endif /* __SSE2__ */

  element        storage[N];
  nwayplus_group *next_=nullptr;
};

template<typename T,typename Allocator>
class nwayplus_group_array
{
  using group=nwayplus_group<T>;
  static constexpr auto N=group::N;

public:
  static constexpr auto address_factor=0.86f;
  static constexpr int  max_saturation=(int)(1+N*(1-address_factor));

  nwayplus_group_array(std::size_t n,const Allocator& al):
    address_size_{n/N+1},v{std::size_t(address_size_/address_factor),al}
  {
    v.back().set_sentinel();
  }

  nwayplus_group_array(nwayplus_group_array&&)=default;
  nwayplus_group_array& operator=(nwayplus_group_array&&)=default;

  auto begin()const{return const_cast<group*>(v.data());}
  auto end()const{return const_cast<group*>(v.data()+v.size());}
  auto at(std::size_t n)const{return const_cast<group*>(&v[n]);}

  auto& back()const{return const_cast<group&>(v.back());}

  // only in moved-from state when rehashing
  bool empty()const{return v.empty();}

  auto address_size()const{return address_size_;}

  bool in_cellar(group* p)const
  {
    return std::size_t(p-&v.front())>=address_size_;
  }

  group* new_group_after(group* p)
  {
    if(boost::core::popcount((unsigned int)p->match_occupied())<max_saturation){
      return p->next()=top;    
    }
    else{
      if(top>=v.data()+address_size_)return p->next()=--top;
    }

    // probe after cellar exhaustion
    // TODO: maybe better to start probing at start of chain
    auto pr=make_prober(p);
    while(!pr.get()->match_empty_or_deleted())pr.next();
    return pr.get();
  }

  struct prober
  {        
    group* get()const noexcept{return data+n;}

    void next()noexcept
    {
      for(;;){
        n=(n+i)&pow2mask;
        i+=1;
        if(n<size)break;
      }
    }

  private:
    friend class nwayplus_group_array;

    prober(group* data,std::size_t size,group* p):
      data{data},size{size},n{(std::size_t)(p-data)}
      {next();}

    group       *data;
    std::size_t size,
                n,
                i=1,
                pow2mask=boost::core::bit_ceil(size)-1;
  };

  prober make_prober(group* p)const
  {
    return {const_cast<group*>(v.data()),v.size(),p};
  }
  
private:
  using group_allocator_type=std::allocator_traits<Allocator>::
    template rebind_alloc<group>;

  std::size_t                   address_size_;
  std::vector<
    group,group_allocator_type> v;
  group                         *top=&v.back();
};

template<
  typename T,typename Hash=boost::hash<T>,typename Pred=std::equal_to<T>,
  typename Allocator=std::allocator<T>,
  typename SizePolicy=prime_size
>
class foa_unordered_nwayplus_set 
{
  using size_policy=SizePolicy;
  using group=nwayplus_group<T>;
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
    friend class foa_unordered_nwayplus_set;
    friend class boost::iterator_core_access;

    const_iterator(group* p,int n=0):
      p{p},n{n}{}

    const value_type& dereference()const noexcept
    {
      return p->at(n).value();
    }

    bool equal(const const_iterator& x)const noexcept
    {
      return p==x.p&&n==x.n;
    }

    void increment()noexcept
    {
      if((n=boost::core::countr_zero((unsigned int)(
          p->match_occupied()&reset_first_bits(n+1))))<N)return;
      while((n=boost::core::countr_zero((unsigned int)(
             (++p)->match_occupied())))>=N);
    }

    group     *p=nullptr;
    int       n=0;
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
    auto p=groups.begin();
    const_iterator it=p;
    if(!(p->match_occupied())&0x1u)++it;
    return it;
  }
  
  const_iterator end()const noexcept{return {&groups.back(),N-1};}
  size_type size()const noexcept{return size_;};

  auto insert(const T& x){return insert_impl(x);}
  auto insert(T&& x){return insert_impl(std::move(x));}

  void erase(const_iterator pos)
  {
    auto [p,n]=pos;
    p->reset(n);
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
    auto hash=h(x);
    auto p=groups.at(size_policy::position(hash,size_index)/N);
    auto cp=p;
    do{
      auto mask=cp->match(hash),
           n=boost::core::countr_zero((unsigned int)mask);
      while(n<N){
        if(BOOST_LIKELY(pred(x,cp->at(n).value())))return {cp,n};
        mask&=mask-1;
        n=boost::core::countr_zero((unsigned int)mask);
      }

      if(cp->match_empty())return end();
    }while((cp=cp->next()));

    // chain traversed, go probing
    auto pr=groups.make_prober(p);
    for(;;){
      auto cp=pr.get();
      auto mask=cp->match(hash),
           n=boost::core::countr_zero((unsigned int)mask);
      while(n<N){
        if(BOOST_LIKELY(pred(x,cp->at(n).value())))return {cp,n};
        mask&=mask-1;
        n=boost::core::countr_zero((unsigned int)mask);
      }
      if(cp->match_empty())return end();
      pr.next();
    }
  }

private:
  using alloc_traits=std::allocator_traits<Allocator>;
  using group_array_type=nwayplus_group_array<
    T,
    typename alloc_traits::template rebind_alloc<group>>;

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

  template<typename Value>
  std::pair<iterator,bool> insert_impl(Value&& x)
  {
    auto hash=h(x);
    auto p=groups.at(size_policy::position(hash,size_index)/N);
    auto [ita,it]=find_match_or_available(x,p,hash);
    if(it!=end())return {it,false};

    if(BOOST_UNLIKELY(size_+1>ml)){
      rehash(size_+1);
      p=groups.at(size_policy::position(hash,size_index)/N);
      ita={};
    }

    auto [pa,na]=ita;
    if(pa){
      construct_element(std::forward<Value>(x),pa->at(na).data());  
      pa->set(na,hash);
      ++size_;
      return {ita,true};
    }
    else return {unchecked_insert(std::forward<Value>(x),p,hash),true};
  }

  void rehash(size_type new_size)
  {
    std::size_t nc =(std::numeric_limits<std::size_t>::max)();
    float       fnc=1.0f+static_cast<float>(new_size)/mlf;
    if(nc>fnc)nc=static_cast<std::size_t>(fnc);

    foa_unordered_nwayplus_set new_container{nc,al};
    std::size_t                num_tx=0;
    try{
      for(auto& b:groups){
        auto mask=b.match_really_occupied();
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
    auto hash=h(x);
    auto p=groups.at(size_policy::position(hash,size_index)/N);
    return unchecked_insert(std::move(x),p,hash);
  }

  template<typename Value>
  iterator unchecked_insert(Value&& x,group* p,std::size_t hash)
  {
    int mask;
    while(!(mask=p->match_empty_or_deleted())){
      if(!(p=p->next())){
        p=groups.new_group_after(p);
      }
    }
      
    auto n=boost::core::countr_zero((unsigned int)mask);
    construct_element(std::forward<Value>(x),p->at(n).data());
    p->set(n,hash);
    ++size_;
    return {p,n};
  }

  template<typename Key>
  std::pair<iterator,iterator> find_match_or_available(
    const Key& x,group* p,std::size_t hash)const
  {
    group* pa=nullptr;
    int    na=0;
    auto   cp=p;
    do{
      if(!pa){
        if(auto mask=cp->match_empty_or_deleted()){
          pa=cp;
          na=boost::core::countr_zero((unsigned int)mask);
        }
      }
      auto mask=cp->match(hash),
           n=boost::core::countr_zero((unsigned int)mask);
      while(n<N){
        if(BOOST_LIKELY(pred(x,cp->at(n).value())))return {{pa,na},{cp,n}};
        mask&=mask-1;
        n=boost::core::countr_zero((unsigned int)mask);
      }

      if(cp->match_empty())return {{pa,na},end()};
    }while((cp=cp->next()));

    // chain traversed, go probing
    auto pr=groups.make_prober(p);
    for(;;){
      auto cp=pr.get();
      if(!pa){
        if(auto mask=cp->match_empty_or_deleted()){
          pa=cp;
          na=boost::core::countr_zero((unsigned int)mask);
        }
      }        
      auto mask=cp->match(hash),
           n=boost::core::countr_zero((unsigned int)mask);
      while(n<N){
        if(BOOST_LIKELY(pred(x,cp->at(n).value())))return {{pa,na},{cp,n}};
        mask&=mask-1;
        n=boost::core::countr_zero((unsigned int)mask);
      }
      if(cp->match_empty())return {{pa,na},end()};
      pr.next();
    }
  }

  size_type max_load()const
  {
    float fml=mlf*static_cast<float>(size_policy::size(size_index));
    auto res=(std::numeric_limits<size_type>::max)();
    if(res>fml)res=static_cast<size_type>(fml);
    return res;
  }  

  Hash             h;
  Pred             pred;
  Allocator        al;
  float            mlf=0.875f;
  std::size_t      size_=0;
  std::size_t      size_index=size_policy::size_index(size_);
  group_array_type groups{size_policy::size(size_index),al};
  size_type        ml=max_load();
};

template<
  typename Key,typename Value,
  typename Hash=boost::hash<Key>,typename Pred=std::equal_to<Key>,
  typename Allocator=std::allocator<map_value_adaptor<Key,Value>>,
  typename SizePolicy=prime_size
>
using foa_unordered_nwayplus_map=foa_unordered_nwayplus_set<
  map_value_adaptor<Key,Value>,
  map_hash_adaptor<Hash>,map_pred_adaptor<Pred>,
  Allocator,SizePolicy
>;

} // namespace fxa_unordered

using fxa_unordered::foa_unordered_nway_set;
using fxa_unordered::foa_unordered_nway_map;
using fxa_unordered::foa_unordered_nwayplus_set;
using fxa_unordered::foa_unordered_nwayplus_map;

#endif