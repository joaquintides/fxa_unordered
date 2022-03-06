# fca_unordered
Proof of concept of closed-addressing unordered associative containers.
* [Development Plan for Boost.Unordered](https://pdimov.github.io/articles/unordered_dev_plan.html)
* [Benchmark results](https://github.com/joaquintides/fca_unordered/actions) for this PoC

```cpp
template<
  typename T,typename Hash=boost::hash<T>,typename Pred=std::equal_to<T>,
  typename Allocator=std::allocator<T>,
  typename SizePolicy=prime_size,typename BucketArrayPolicy=grouped_buckets,
  typename EmbedNode=std::false_type
>
class fca_unordered_set;

template<
  typename Key,typename Value,
  typename Hash=boost::hash<Key>,typename Pred=std::equal_to<Key>,
  typename Allocator=std::allocator</* equivalent to std::pair<const Key,Value> */>,
  typename SizePolicy=prime_size,typename BucketArrayPolicy=grouped_buckets,
  typename EmbedNode=std::false_type
>
class fca_unordered_map;
```

**`SizePolicy`**

Specifies how the bucket array grows and the algorithm used for determining the position
of an element with hash value `h` in the array.
* `prime_size`: Sizes are prime numbers in an approximately doubling sequence. `position(h) = h % size`,
modulo operations are sped up by keeping a function pointer table with `fpt[i](h) == h % size(i)`,
where `i` ranges over the size sequence.
* `prime_switch_size`: Same as before, but instead of a table a `switch` statement over `i` is used.
* `prime_fmod_size`: `position(h) = fastmod(high32bits(h) + low32bits(h), size)`.
[fastmod](https://github.com/lemire/fastmod) is a fast implementation of modulo for 32-bit numbers
by Daniel Lemire.
* `prime_frng_size`: `position(h) = fastrange(h, size)`. Daniel Lemire's [fastrange](https://github.com/lemire/fastrange)
maps a uniform distribution of values in the range `[0, size)`. This policy is ill behaved for
low quality hash functions because it ignores the low bits of the hash value.
* `prime_frng_fib_size`: Fixes pathological situations of `prime_frng_size` by doing
`positon(h) = fastrange(h * F, size)`, where `F` is the
[Fibonacci hashing](https://en.wikipedia.org/wiki/Hash_function#Fibonacci_hashing) constant.
* `pow2_size`: Sizes are consecutive powers of two. `position(h)` returns the higher bits of the
hash value, which, as it happens with `prime_frng_size`, works poorly for low quality hash functions.
* `pow2_fib_size`: `h` is Fibonacci hashed before calculating the position.

**`BucketArrayPolicy`**
* `simple_buckets`: The bucket array is a plain vector of node pointers without additional metadata.
The resulting container deviates from the C++ standard requirements for unordered associative
containers in three aspects:
  * Iterator increment is not constant but gets slower as the number of empty buckets grow;
    see [N2023](http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2006/n2023.pdf) for details.
  * Because of the former, `erase(iterator)` returns `void` instead of an iterator to the next
    element.
  * `begin()` is not constant time (hopping to the first occupied bucket is required).
<div style="list-style-type: none;margin-left: 40px;">
  <ul>This policy is used to simulate <code>unordered_bucket_map</code> as specified in the
    <a href="https://pdimov.github.io/articles/unordered_dev_plan.html">Development Plan for Boost.Unordered</a>.
  </ul>
</div>

* `bcached_simple_buckets`: Same as `simple_buckets`, but a reference to the first occupied bucket
is kept and updated so as to provide constant-time `begin()`.
* `grouped_buckets`: The resulting container is fully standards-compliant, including constant
iteration and average constant `erasure(iterator)`. Besides the usual bucket array, a vector
of *bucket group* metadata is kept. Buckets are logically grouped in 32/64 (depending on the
architecture) consecutive buckets: the associated bucket group metadata consists of a pointer
to the first bucket of the group, a bitmask signalling which buckets are occupied,
and `prev` and `next` pointers to link non-empty bucket groups in a bidirectional list.
Going from a given bucket to the next occupied one is implemented as follows:
  * Use [bit counting](https://www.boost.org/libs/core/doc/html/core/bit.html) operations to
    determine, in constant time, the following occupied bucket within the group.
  * If there are no further occupied buckets in the group, go the `next` group (which is
    guaranteed to be non-empty).
<div style="list-style-type: none;margin-left: 40px;">
  <ul>The memory overhead added by bucket groups is 4 bits per bucket.</ul>
</div>

**`EmbedNode`**

If `EmbedNode::value` is true, buckets are extended to hold space for a node, so that
no additional memory allocation is needed to insert the first element in the bucket.
The resulting container deviates in a number of important aspects from the C++ standard
requirements for unordered associative containers:
* Pointer stability is not mantained on rehashing.
* The elements of the container must be movable.
* It is not possible to provide [node extraction](https://en.cppreference.com/w/cpp/container/node_handle)
  capabilities.

Currently, `grouped_buckets` is the only bucket array policy supporting node embedding.

```cpp
template<
  typename T,typename Hash=boost::hash<T>,typename Pred=std::equal_to<T>,
  typename Allocator=std::allocator<T>
>
class fca_simple_unordered_set;

template<
  typename Key,typename Value,
  typename Hash=boost::hash<Key>,typename Pred=std::equal_to<Key>,
  typename Allocator=std::allocator</* equivalent to std::pair<const Key,Value> */>
>
class fca_simple_unordered_map;
```

Abandoned experiment where individual occupied buckets where linked in a bidirectional
list. Outperformed by `fca_unordered_[set|map]` with `grouped_buckets`.
