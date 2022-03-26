/* Proof of concept of closed- and open-addressing
 * unordered associative containers.
 *
 * Copyright 2022 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 */
 
#ifndef FCA_UNORDERED_HPP
#define FCA_UNORDERED_HPP

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

  allocator_type get_allocator(){return buckets.get_allocator();}

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

  allocator_type get_allocator(){return buckets.get_allocator();}

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
  
  dynamic_node_allocator(std::size_t /*n*/,const Allocator& al):al{al}{}

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
  node_type* relocate_node(
    node_type* p,RawBucketArray,Bucket&,
    dynamic_node_allocator&,RawBucketArray,Bucket&)
  {return p;}    

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
struct hybrid_node_allocator_bucket:bucket
{
  hybrid_node_allocator_bucket(){reset_payload();}

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
    if(auto pb=find_hosting_bucket(p,buckets)){
      alloc_traits::destroy(this->get_allocator(),&pb->data()->value);
      pb->reset_payload();
    }
    else super::delete_node(p,buckets,b);
  }
  
  template<typename RawBucketArray,typename Bucket>
  node_type* relocate_node(
    node_type* p,RawBucketArray buckets,Bucket&,
    hybrid_node_allocator& new_node_allocator,
    RawBucketArray new_buckets,Bucket& newb)
  {
    if(auto pb=find_hosting_bucket(p,buckets)){
      auto newp=new_node_allocator.new_node(
        std::move(pb->data()->value),new_buckets,newb);
      alloc_traits::destroy(al,&pb->data()->value);
      pb->reset_payload();
      return newp;
    }
    else return p;
  }    

private:
  using alloc_traits=std::allocator_traits<Allocator>;
  
  template<std::ptrdiff_t N,typename Bucket>
  Bucket* find_available_bucket(Bucket* pb,Bucket* pbend)
  {
    if constexpr(N==0)return nullptr;
    else return
      !pb->has_payload()?
        pb:
        pb+1==pbend?
          nullptr:
          find_available_bucket<N-1>(pb+1,pbend);
  }

  template<typename RawBucketArray,typename Bucket>
  Bucket* find_available_bucket(RawBucketArray buckets,Bucket& b)
  {
    return find_available_bucket<LINEAR_PROBE_N>(&b,buckets.end());
  }
  
  template<typename RawBucketArray>
  auto find_hosting_bucket(node_type* p,RawBucketArray buckets)
  {
    using bucket=std::remove_reference_t<decltype(*buckets.begin())>;
    
    std::uintptr_t u=(std::uintptr_t)p,
                   ubegin=(std::uintptr_t)buckets.begin(),
                   uend=(std::uintptr_t)buckets.end();
    if(ubegin<=u&&u<uend){
      return (bucket*)(ubegin+(u-ubegin)/sizeof(bucket)*sizeof(bucket));
    }
    else return (bucket*)nullptr;
  }

  Allocator al; 
};

struct hybrid_node_allocation
{
  template<typename Node>
  using bucket_type=hybrid_node_allocator_bucket<Node>;
  
  template<typename Node,typename Allocator>
  using allocator_type=hybrid_node_allocator<Node,Allocator>;
};

enum struct quadratic_prober_variant
{
  // ways to handle the group for the first location tried
  standard, // every slot in the group considered
  forward,  // only the slots from the location onwards
  exact,    // only the exact slot for the location
};

template<typename Allocator,quadratic_prober_variant Variant>
class quadratic_prober
{
public:
  static constexpr std::size_t N=sizeof(std::size_t)*8;
  using allocator_type=Allocator;

  quadratic_prober(std::size_t n,const Allocator& al):
    pow2mask(boost::core::bit_ceil((n+N-1)/N)-1),
    bitmask((n+N-1)/N,al)
  {
    std::size_t nmod=n%N;
    bitmask.back()=~std::size_t(0)&reset_first_bits(nmod);  
  }

  std::size_t allocate(std::size_t n)
  {
    std::size_t ndiv=n/N,
                nmod=n%N;

    if constexpr(Variant==quadratic_prober_variant::standard){
      nmod=std::size_t(boost::core::countr_one(bitmask[ndiv]));      
    }
    else if constexpr(Variant==quadratic_prober_variant::forward){
      nmod=std::size_t(boost::core::countr_one(
        nmod==0?
          bitmask[ndiv]:
          bitmask[ndiv]|set_first_bits(nmod)));
    }
    else{
      static_assert(Variant==quadratic_prober_variant::exact);
      if(bitmask[ndiv]&set_bit(nmod))nmod=N;
    }
    if(nmod>=N){ // first probe failed
      std::size_t i=1;
      for(;;){
        ndiv=(ndiv+i)&pow2mask;
        i+=1;
        if(ndiv<bitmask.size()&&
           (nmod=std::size_t(boost::core::countr_one(bitmask[ndiv])))<N)break;
      }
    }

    bitmask[ndiv]|=set_bit(nmod);
    return ndiv*N+nmod;
  }

  void deallocate(std::size_t n)
  {
    std::size_t ndiv=n/N,
                nmod=n%N;
    bitmask[ndiv]&=reset_bit(nmod);      
  }

private:
  std::size_t                        pow2mask;
  std::vector<std::size_t,Allocator> bitmask;
};

template<typename T>
struct uninitialized
{
  T* data(){return reinterpret_cast<T*>(&storage);}

  std::aligned_storage_t<sizeof(T),alignof(T)> storage;
};

template<typename Node,typename Allocator>
class linear_node_allocator
{
public:
  using allocator_type=Allocator;
  using node_type=Node;
  
  linear_node_allocator(std::size_t n,const Allocator& al):
    al(al),
    nodes(n,al),
    prober(n,al)
  {}

  allocator_type get_allocator(){return al;}

  template<typename Value,typename RawBucketArray,typename Bucket>
  node_type* new_node(Value&& x,RawBucketArray buckets,Bucket& b)
  {  
    auto p=nodes[prober.allocate(&b-buckets.begin())].data();
    try{
      alloc_traits::construct(al,&p->value,std::forward<Value>(x));
      return p;
    }
    catch(...){
      prober.deallocate(p-reinterpret_cast<node_type*>(nodes.data()));
      throw;
    }
  }
  
  template<typename RawBucketArray,typename Bucket>
  void delete_node(node_type* p,RawBucketArray,Bucket&)
  {
    alloc_traits::destroy(al,&p->value);
    prober.deallocate(p-reinterpret_cast<node_type*>(nodes.data()));
  }
  
  template<typename RawBucketArray,typename Bucket>
  node_type* relocate_node(
    node_type* p,
    RawBucketArray buckets,Bucket& b,
    linear_node_allocator& new_node_allocator,
    RawBucketArray new_buckets,Bucket& newb)
  {
    auto newp=new_node_allocator.new_node(
      std::move(p->value),new_buckets,newb);
    delete_node(p,buckets,b);
    return newp;
  }    

private:
  using alloc_traits=std::allocator_traits<Allocator>;
  using uninitialized_node=uninitialized<Node>;
  using uninitialized_node_allocator_type=typename alloc_traits::
    template rebind_alloc<uninitialized_node>;
  using size_t_allocator_type=typename alloc_traits::
    template rebind_alloc<std::size_t>;

  allocator_type                                                    al;
  std::vector<uninitialized_node,uninitialized_node_allocator_type> nodes;
  quadratic_prober<
    size_t_allocator_type,quadratic_prober_variant::standard>       prober;
};

struct linear_node_allocation
{
  template<typename Node>
  using bucket_type=bucket;
  
  template<typename Node,typename Allocator>
  using allocator_type=linear_node_allocator<Node,Allocator>;
};

template<typename Node,typename Allocator>
class pool_node_allocator
{
public:
  using allocator_type=Allocator;
  using node_type=Node;
  
  pool_node_allocator(std::size_t n,const Allocator& al):
    al(al),
    nodes(n,al)
  {}

  allocator_type get_allocator(){return al;}

  template<typename Value,typename RawBucketArray,typename Bucket>
  node_type* new_node(Value&& x,RawBucketArray,Bucket&)
  {  
    auto p=allocate_node();
    try{
      alloc_traits::construct(al,&p->value,std::forward<Value>(x));
      return p;
    }
    catch(...){
      deallocate_node(p);
      throw;
    }
  }
  
  template<typename RawBucketArray,typename Bucket>
  void delete_node(node_type* p,RawBucketArray,Bucket&)
  {
    alloc_traits::destroy(al,&p->value);
    deallocate_node(p);
  }
  
  template<typename RawBucketArray,typename Bucket>
  node_type* relocate_node(
    node_type* p,
    RawBucketArray buckets,Bucket& b,
    pool_node_allocator& new_node_allocator,
    RawBucketArray new_buckets,Bucket& newb)
  {
    auto newp=new_node_allocator.new_node(
      std::move(p->value),new_buckets,newb);
    delete_node(p,buckets,b);
    return newp;
  }    

private:
  using alloc_traits=std::allocator_traits<Allocator>;
  using uninitialized_node=uninitialized<Node>;
  using uninitialized_node_allocator_type=typename alloc_traits::
    template rebind_alloc<uninitialized_node>;
  using size_t_allocator_type=typename alloc_traits::
    template rebind_alloc<std::size_t>;

  node_type* allocate_node()
  {
    if(free){
      auto p=free;
      free=static_cast<node_type*>(free->next);
      return p;
    }
    else{
      return reinterpret_cast<node_type*>(&nodes[top++]);
    }
  }

  void deallocate_node(node_type* p)
  {
    p->next=free;
    free=p;
  }

  allocator_type                                          al;
  std::size_t                                             top=0;
  node_type                                               *free=nullptr;
  std::vector<
    uninitialized_node,uninitialized_node_allocator_type> nodes;
};

struct pool_node_allocation
{
  template<typename Node>
  using bucket_type=bucket;
  
  template<typename Node,typename Allocator>
  using allocator_type=pool_node_allocator<Node,Allocator>;
};

template<typename Node>
struct embedded_node_allocator_bucket:bucket
{
  Node* data(){return reinterpret_cast<Node*>(&storage);}
  const Node* data()const{return reinterpret_cast<const Node*>(&storage);}
  Node& node(){return *data();}
  const Node& value()const{return *data();}
  
  std::aligned_storage_t<sizeof(Node),alignof(Node)> storage;
};

template<typename Node,typename Allocator>
class embedded_node_allocator
{
public:
  using allocator_type=Allocator;
  using node_type=Node;
  
  embedded_node_allocator(std::size_t n,const Allocator& al):
    al(al),
    prober(n,al)
  {}

  allocator_type get_allocator(){return al;}

  template<typename Value,typename RawBucketArray,typename Bucket>
  node_type* new_node(Value&& x,RawBucketArray buckets,Bucket& b)
  {  
    auto p=(buckets.begin()+prober.allocate(&b-buckets.begin()))->data();
    try{
      alloc_traits::construct(al,&p->value,std::forward<Value>(x));
      return p;
    }
    catch(...){
      deallocate_node(p,buckets);
      throw;
    }
  }
  
  template<typename RawBucketArray,typename Bucket>
  void delete_node(node_type* p,RawBucketArray buckets,Bucket&)
  {
    alloc_traits::destroy(al,&p->value);
    deallocate_node(p,buckets);
  }
  
  template<typename RawBucketArray,typename Bucket>
  node_type* relocate_node(
    node_type* p,
    RawBucketArray buckets,Bucket& b,
    embedded_node_allocator& new_node_allocator,
    RawBucketArray new_buckets,Bucket& newb)
  {
    auto newp=new_node_allocator.new_node(
      std::move(p->value),new_buckets,newb);
    delete_node(p,buckets,b);
    return newp;
  }    

private:
  using alloc_traits=std::allocator_traits<Allocator>;
  using size_t_allocator_type=typename alloc_traits::
    template rebind_alloc<std::size_t>;

  template<typename RawBucketArray>
  void deallocate_node(node_type* p,RawBucketArray buckets)
  {
    using bucket=std::remove_reference_t<decltype(*buckets.begin())>;
      
    std::uintptr_t u=(std::uintptr_t)p,
                   ubegin=(std::uintptr_t)buckets.begin();
    prober.deallocate((u-ubegin)/sizeof(bucket));      
  }

  allocator_type                                           al;
  quadratic_prober<
    size_t_allocator_type,quadratic_prober_variant::exact> prober;
};

struct embedded_node_allocation
{
  template<typename Node>
  using bucket_type=embedded_node_allocator_bucket<Node>;
  
  template<typename Node,typename Allocator>
  using allocator_type=embedded_node_allocator<Node,Allocator>;
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
    node_type* p,bucket& b,
    node_allocator_type& new_node_allocator,bucket_array_type& new_buckets)
  {
    auto itnewb=new_buckets.at(new_buckets.position(h(p->value)));
    p=node_allocator.relocate_node(
      p,buckets.raw(),b,
      new_node_allocator,new_buckets.raw(),*itnewb);
    new_buckets.insert_node(itnewb,p);
  }

  template<typename Value>
  std::pair<iterator,bool> insert_impl(Value&& x)
  {
    auto hash=h(x);
    auto itb=buckets.at(buckets.position(hash));
    auto it=find(x,itb);
    if(it!=end())return {it,false};
        
    if(BOOST_UNLIKELY(size_+1>ml)){
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

    bucket_array_type   new_buckets(bc,buckets.get_allocator());
    node_allocator_type new_node_allocator(
                          new_buckets.capacity(),new_buckets.get_allocator());
    try{
      for(auto& b:buckets.raw()){            
        for(auto p=b.next;p;){
          auto  next_p=p->next;
          transfer_node(static_cast<node_type*>(p),b,new_node_allocator,new_buckets);
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
    node_allocator=std::move(new_node_allocator);
    ml=max_load();   
  }
  
  template<typename Key>
  iterator find(const Key& x,bucket_iterator itb)const
  {
    for(auto p=itb->next;p;p=p->next){
      if(BOOST_LIKELY(pred(x,static_cast<node_type*>(p)->value))){
        return {static_cast<node_type*>(p),itb};
      }
    }
    return end();
  }
  
  template<typename Key>
  std::pair<fxa_unordered::bucket**,bucket_iterator>
  find_prev(const Key& x)const
  {
    auto itb=buckets.at(buckets.position(h(x)));
    for(auto pp=&itb->next;*pp;pp=&(*pp)->next){
      if(BOOST_LIKELY(pred(x,static_cast<node_type*>(*pp)->value))){
        return {pp,itb};
      }
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
  float               mlf=1.0f;
  size_type           size_=0;
  bucket_array_type   buckets{0,Allocator()};
  node_allocator_type node_allocator{
                        buckets.capacity(),buckets.get_allocator()};
  size_type           ml=max_load();  
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

} // namespace fxa_unordered

using fxa_unordered::fca_unordered_set;
using fxa_unordered::fca_unordered_map;

#endif
