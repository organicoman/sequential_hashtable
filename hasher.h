// Internal policy header for unordered_set and unordered_map -*- C++ -*-

// Copyright (C) 2010-2024 Free Software Foundation, Inc.
//
// This file is part of the GNU ISO C++ Library.  This library is free
// software; you can redistribute it and/or modify it under the
// terms of the GNU General Public License as published by the
// Free Software Foundation; either version 3, or (at your option)
// any later version.

// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// Under Section 7 of GPL version 3, you are granted additional
// permissions described in the GCC Runtime Library Exception, version
// 3.1, as published by the Free Software Foundation.

// You should have received a copy of the GNU General Public License and
// a copy of the GCC Runtime Library Exception along with this program;
// see the files COPYING3 and COPYING.RUNTIME respectively.  If not, see
// <http://www.gnu.org/licenses/>.
/** @file bits/hasher.h
 *  This is an internal header file, included by other library headers.
 *  Do not attemp to use it directly.
 *  @headername{hashed_stack, hashed_queue}
 */

#ifndef HASHER_H
#define HASHER_H 1

#include <tuple>		          // for std::tuple, std::forward_as_tuple
#include <bits/functional_hash.h> // for __is_fast_hash
#include <bits/stl_algobase.h>	  // for std::min, std::is_permutation.
#include <bits/stl_pair.h>	      // for std::pair
#include <ext/aligned_buffer.h>	  // for __gnu_cxx::__aligned_buffer
#include <ext/alloc_traits.h>	  // for std::__alloc_rebind
#include <ext/numeric_traits.h>	  // for __gnu_cxx::__int_traits
#include <limits>                 // for std::numeric_limits
#include <cstdint>                // for std::uintptr_t

namespace std
{
  // specialization of std::get for identity
  template<std::size_t _Ip, typename _Tp>
    constexpr
    _Tp&&
    get(_Tp&& __x) noexcept
    {
      return std::forward<_Tp>(__x);
    }
}

namespace stl _GLIBCXX_VISIBILITY(default)
{
_GLIBCXX_BEGIN_NAMESPACE_VERSION
    /// @cond undocumented

  // default container forward declaration
  template<typename _Tp, typename _Allocator = std::allocator<_Tp>>
    class revolver;

  template<typename _Tp, typename _Alloc, typename _Key = _Tp
          ,typename _Container = stl::revolver<_Tp>
          ,typename _Hash = std::hash<_Key>>
    class _ContainerHasher;

namespace __detail
{
/**
  *  @defgroup _ContainerHasher-detail Base and Implementation Classes
  *  @ingroup unordered_associative_containers
  *  @{
  */
  template<typename _Key, typename _Alloc, typename _Equal
          ,typename _Hash, typename _RangeHash, typename _RehashPolicy>
    class _ContainerHasher_base;

  struct _Select1st
  {
    template<typename _Tuple>
      struct __1st_type
      { using __key_type = _Tuple; };

    template<typename _Key, typename... _Up>
      struct __1st_type<std::tuple<_Key, _Up...>>
      { using __key_type = _Key; };

    template<typename _Key, typename... _Up>
      struct __1st_type<const std::tuple<_Key, _Up...>>
      { using __key_type = const _Key; };

    template<typename _Tuple>
      struct __1st_type<_Tuple&>
      { using __key_type = typename __1st_type<_Tuple>::__key_type&; };

    // std::pair is a special case of std::tuple
    template<typename _Tp, typename _Up>
      struct __1st_type<std::pair<_Tp, _Up>>
      { using __key_type = typename __1st_type<std::tuple<_Tp,_Up>>::__key_type; };

    template<typename _Tp>
      constexpr
      typename __1st_type<_Tp>::__key_type&&
      operator()(_Tp&& __x) const noexcept
      { return std::get<0>(std::forward<_Tp>(__x)); }
  };

  template<typename _HashtableAlloc, typename _NodePtr>
    struct _NodePtrGuard
    {
      _HashtableAlloc& _M_h;
      _NodePtr _M_ptr;

      ~_NodePtrGuard()
      {
        if (_M_ptr)
          _M_h._M_deallocate_node_ptr(_M_ptr);
      }
    };

  template<typename _NodeAlloc>
    struct _Hashtable_alloc;

   // Functor recycling a pool of nodes and using allocation once the pool is
   // empty.
  template<typename _NodeAlloc>
    struct _ReuseOrAllocNode
    {
    private:
      using __node_alloc_type = _NodeAlloc;
      using __hashtable_alloc = _Hashtable_alloc<__node_alloc_type>;
      using __node_alloc_traits =
        typename __hashtable_alloc::__node_alloc_traits;

    public:
      using __node_ptr = typename __hashtable_alloc::__node_ptr;

    private:
      mutable __node_ptr _M_nodes;
      __hashtable_alloc& _M_h;

    public:
      _ReuseOrAllocNode(__node_ptr __nodes, __hashtable_alloc& __h)
        : _M_nodes(__nodes), _M_h(__h) { }
      _ReuseOrAllocNode(const _ReuseOrAllocNode&) = delete;

      ~_ReuseOrAllocNode()
      { _M_h._M_deallocate_nodes(_M_nodes); }

    template<typename _Index>
      __node_ptr
      operator()(_Index&& __index, std::size_t __hash_code) const
      {
        if (!_M_nodes)
          return _M_h._M_allocate_node(std::forward<_Index>(__index)
                                     , __hash_code);

        __node_ptr __node = _M_nodes;
        _M_nodes = _M_nodes->_M_next();
        __node->_M_nxt = nullptr;
        auto& __a = _M_h._M_node_allocator();
        _NodePtrGuard<__hashtable_alloc, __node_ptr> __guard { _M_h, __node };
        __node_alloc_traits::construct(__a, __node, std::forward<_Index>(__index)
                                     , __hash_code);
        __guard._M_ptr = nullptr;
        return __node;
      }
    };

   // Functor similar to the previous one but without any pool of nodes to
   // recycle.
  template<typename _NodeAlloc>
    struct _AllocNode
    {
    private:
      using __hashtable_alloc = _Hashtable_alloc<_NodeAlloc>;

    public:
      using __node_ptr = typename __hashtable_alloc::__node_ptr;

      _AllocNode(__hashtable_alloc& __h)
        : _M_h(__h) { }

    template<typename _Index>
      __node_ptr
      operator()(_Index&& __index, std::size_t __hash_code) const
      { return _M_h._M_allocate_node(std::forward<_Index>(__index), __hash_code); }

    private:
      __hashtable_alloc& _M_h;
    };

    /**
    *  struct _Hashtable_hash_traits
    *
    *  Important traits for hash tables depending on associated hasher.
    *
    */
  template<typename _Hash>
    struct _Hashtable_hash_traits
    {
      static constexpr std::size_t
      __small_size_threshold() noexcept
      { return std::__is_fast_hash<_Hash>::value ? 0 : 20; }
    };

    /**
    *  struct _Hash_node_base
    *
    *  Nodes, used to wrap elements stored in the hash table.  A policy
    *  template parameter of class template _ContainerHasher
    *  controls whether nodes also store a hash code.
    *  In some cases (e.g. strings) this may be a performance win.
    */
    struct _Hash_node_base
    {
      _Hash_node_base* _M_nxt;
      _Hash_node_base() noexcept : _M_nxt() { }
      _Hash_node_base(_Hash_node_base* __next) noexcept : _M_nxt(__next) { }
    };

    /**
    *  base struct _Hash_node_value_base
    *
    *  Node type, with either a pointer type or index type
    */

  template<typename _Iterator_tag>
    struct _Hash_node_index_base
    {
        using __random_iterator = std::false_type;
        std::size_t    _M_hash_code;
        std::uintptr_t _M_index;
    };

  template<>
    struct _Hash_node_index_base<std::random_access_iterator_tag>
    {
        using __random_iterator = std::true_type;
        std::size_t _M_hash_code;
        std::size_t _M_index;
    };

  template<typename _Node_index_base>
    struct
      alignas(sizeof(_Node_index_base))
      _Hash_node_value_base
    {
      using __base_type = _Node_index_base;
      using __random_access_iterator = typename __base_type::__random_iterator;
      using __index_type = std::uintptr_t;

      __base_type _M_value;

      inline
      _Hash_node_value_base*
      _M_valptr() noexcept
      { return this; }

      inline
      const _Hash_node_value_base*
      _M_valptr() const noexcept
      { return this; }

    };

  /**
   *  Primary template struct _Hash_node.
   */
  template<typename _Iterator_tag>
    struct _Hash_node
      : _Hash_node_base
      , _Hash_node_index_base<_Iterator_tag>
    {
      _Hash_node*
      _M_next() const noexcept
      { return static_cast<_Hash_node*>(this->_M_nxt); }
    };

    /// Base class for node iterators.
  template<typename _Iterator_tag>
    struct _Node_iterator_base
    {
      using __node_type = _Hash_node<_Iterator_tag>;
      using __value_type = typename __node_type::__value_type;

      __node_type* _M_cur;

      _Node_iterator_base() : _M_cur(nullptr) { }
      _Node_iterator_base(__node_type* __p) noexcept
        : _M_cur(__p) { }

      void
      _M_incr() noexcept
      { _M_cur = _M_cur->_M_next(); }

      friend bool
      operator==(const _Node_iterator_base& __x, const _Node_iterator_base& __y)
        noexcept
      { return __x._M_cur == __y._M_cur; }

#if __cpp_impl_three_way_comparison < 201907L
      friend bool
      operator!=(const _Node_iterator_base& __x, const _Node_iterator_base& __y)
        noexcept
      { return __x._M_cur != __y._M_cur; }
#endif
    };

    /// Node iterators, used to iterate through all the hashtable.
  template<typename _Iterator_tag>
    struct _Node_iterator
      : public _Node_iterator_base<_Iterator_tag>
    {
    private:
      using __base_type = _Node_iterator_base<_Iterator_tag>;
      using __node_type = typename __base_type::__node_type;

    public:
      using value_type = typename __base_type::__value_type;
      using difference_type = std::ptrdiff_t;
      using iterator_category = std::forward_iterator_tag;
      using pointer = value_type*;
      using reference = value_type&;

        _Node_iterator() = default;

        explicit
            _Node_iterator(__node_type* __p) noexcept
            : __base_type(__p) { }

        reference
        operator*() const noexcept
        { return this->_M_cur->_M_v(); }

        pointer
        operator->() const noexcept
        { return this->_M_cur->_M_valptr(); }

        _Node_iterator&
        operator++() noexcept
        {
            this->_M_incr();
            return *this;
        }

        _Node_iterator
        operator++(int) noexcept
        {
            _Node_iterator __tmp(*this);
            this->_M_incr();
            return __tmp;
        }
    };

    /// Node const_iterators, used to iterate through all the hashtable.
    template<typename _Iterator_tag>
    struct _Node_const_iterator
        : public _Node_iterator_base<_Value, __cache>
    {
    private:
        using __base_type = _Node_iterator_base<_Value, __cache>;
        using __node_type = typename __base_type::__node_type;

    public:
        typedef _Value					value_type;
        typedef std::ptrdiff_t				difference_type;
        typedef std::forward_iterator_tag			iterator_category;

        typedef const value_type*				pointer;
        typedef const value_type&				reference;

        _Node_const_iterator() = default;

        explicit
            _Node_const_iterator(__node_type* __p) noexcept
            : __base_type(__p) { }

        _Node_const_iterator(const _Node_iterator<_Value, __constant_iterators,
                                                  __cache>& __x) noexcept
            : __base_type(__x._M_cur) { }

        reference
        operator*() const noexcept
        { return this->_M_cur->_M_v(); }

        pointer
        operator->() const noexcept
        { return this->_M_cur->_M_valptr(); }

        _Node_const_iterator&
        operator++() noexcept
        {
            this->_M_incr();
            return *this;
        }

        _Node_const_iterator
        operator++(int) noexcept
        {
            _Node_const_iterator __tmp(*this);
            this->_M_incr();
            return __tmp;
        }
    };
   ///@} _ContainerHasher-detail
} // namespace __detail
    /// @endcond
_GLIBCXX_END_NAMESPACE_VERSION
} // namespace std


#endif // HASHER_H
