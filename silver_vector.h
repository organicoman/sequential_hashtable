// silver_vector.h header -*- C++ -*-

// Copyright (C) 2007-2024 Free Software Foundation, Inc.
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

/**
 * @file bits/silver_vector.h
 * This is an internal header file, included by other headers.
 * Do not attempt to use it directly.
 * @headername{hasher}
 */

#ifndef SILVER_VECTOR_H
#define SILVER_VECTOR_H 1

#pragma GCC system_header

#include <memory> // allocator_traits
#include <optional>
#include <initializer_list>
#include <cstring> // memcpy
#include <utility> // pair

namespace stl _GLIBCXX_VISIBILITY(default)
{
_GLIBCXX_BEGIN_NAMESPACE_VERSION

/**
 * @defgroup svector implementation classes
 * @ingroup _ContainerHasher-detail
 * @{
 */
  template<typename AdaptedIter, typename Allocator>
    class svector;

namespace __detail
{
  /**
   * @brief The _Silver_value_base class is the type of elements
   * the silver_vector will allocate.
   * @param _Container_iterator an index into _ContainerHasher's inner
   * container. If the inner container models a random access container,
   * the index is of type 'long long' (a simple index into an array).
   * Otherwise it is an iterator to the elements of the _ContainerHasher
   * elements. This design avoids the unecessary handling of iterator
   * invalidation for random access containers.
   */

  template<typename _Container_iterator>
  using __type_id = std::conditional_t<
                      std::is_same_v<
                        typename std::iterator_traits<_Container_iterator>
                                      ::iterator_category
                      , std::random_access_iterator_tag>
                      , long long
                      , _Container_iterator>;

  template<typename _Container_iterator>
  struct alignas(sizeof(__type_id<_Container_iterator>))
    _Silver_value
  {
    using __type = _Silver_value<_Container_iterator>;

    using __container_iterator_tag =
        typename std::iterator_traits<_Container_iterator>::iterator_category;

    using __index_type = __type_id<_Container_iterator>;
    using __value_type = std::optional<__index_type>;

    __value_type _M_idx;

    constexpr
    _Silver_value(const __index_type &before) noexcept
      : _M_idx(before) {}

    constexpr
    _Silver_value(const __type& othr) noexcept = default;

    constexpr
    _Silver_value(__type&& othr) noexcept = default;

    constexpr
    __type&
    operator=(const __type &) noexcept = default;

    constexpr
    __type&
    operator=(__type &&) noexcept = default;

    constexpr
    void
    swap(__type& other) noexcept
    { std::swap(_M_idx, other._M_idx); }

    constexpr
    __value_type&
    _M_val() noexcept
    { return _M_idx; }

    constexpr
    __value_type
    _M_val() const noexcept
    { return _M_idx; }

  };

  /**
   * @brief The _Silver_vector_base class
   * @param _Container_iterator The iterator type fo the adapted container.
   * We call the _ContainerHasher inner container, the 'adapted container'.
   * @param _Rebound_alloc Allocator type got from rebinding the allocator
   * of the _ContainerHasher's into _Silver_value.
   * We inherit from _Rebound_alloc to benefit from the Zero size base struct
   * optimization.
   */
  template<typename _Container_iterator, typename _Rebound_alloc>
  class _Silver_vector_base : public _Rebound_alloc
  {
    using __alloc_base_type = _Rebound_alloc;
    using __alloc_traits = std::allocator_traits<__alloc_base_type>;
    using __alloc_value_type = typename __alloc_traits::value_type;
    using __alloc_size_type = typename __alloc_traits::size_type;
    using __silver_value_type = _Silver_value<_Container_iterator>;

    static_assert(std::is_same_v<__alloc_value_type, __silver_value_type>
                  , "_Alloc value_type must match the vector element type");

    using __pointer = typename __alloc_traits::pointer;
    using __iterator_tag =
          typename std::iterator_traits<_Container_iterator>::iterator_category;

  protected:
    __pointer _M_beg;

    // forward declaraction of iterators.
    struct __iterator;
    struct __iterator_const;

  public:
    using size_type = __alloc_size_type;
    using value_type = __alloc_value_type;
    using reference = typename __alloc_traits::reference;
    using allocator_type = __alloc_base_type;
    using iterator = __iterator;
    using const_iterator = __iterator_const;

  public:
    ~_Silver_vector_base()
    {
      if(_M_beg)
      {
          static_cast<__alloc_base_type*>(this)->deallocate(_M_capacity());
          _M_beg = nullptr;
      }
    }

    constexpr
    _Silver_vector_base()
        noexcept(std::is_nothrow_default_constructible_v<__alloc_base_type>)
    : __alloc_base_type()
    {
      _M_beg = _M_get_alloc().allocate(2);
      _M_get_alloc().construct((_M_beg + 1), _M_end_tag());
    }

    constexpr
    _Silver_vector_base(const allocator_type& othr)
        noexcept(std::is_nothrow_copy_constructible_v<allocator_type>)
    : __alloc_base_type(__alloc_traits::select_on_container_copy_construction(othr))
    {
      _M_beg = _M_get_alloc().allocate(2);
      _M_get_alloc().construct((_M_beg + 1), _M_end_tag());
    }

    constexpr
    _Silver_vector_base(allocator_type&& othr)
        noexcept(std::is_nothrow_move_constructible_v<allocator_type>)
    : __alloc_base_type(std::move(othr))
    {
      _M_beg = _M_get_alloc().allocate(2);
      _M_get_alloc().construct((_M_beg + 1), _M_end_tag());
    }

    constexpr
    _Silver_vector_base(_Silver_vector_base&& othr)
        noexcept(std::is_nothrow_move_constructible_v<allocator_type>)
    : __alloc_base_type(std::move(othr))
    , _M_beg(std::move(othr._M_beg))
    { }

    // allocate enough capacity to hold the engaged elements only
    // maybe the source svector is sparsly filled.
    constexpr
    _Silver_vector_base(const _Silver_vector_base& othr)
        noexcept(std::is_nothrow_copy_constructible_v<allocator_type>)
    : __alloc_base_type(__alloc_traits::select_on_container_copy_construction(othr))
    {
      static_assert(std::is_trivially_constructible_v<value_type>);

      const size_type _sz = othr.size();
      *this = std::move(_Silver_vector_base(_sz));
      _M_byte_blit(othr._M_begin(), _sz);
      _M_fill_disengaged(_sz);
    }

    constexpr
    _Silver_vector_base&
    operator=(_Silver_vector_base&& rght)
            noexcept(std::is_nothrow_move_constructible_v<allocator_type>) = default;


    constexpr
    _Silver_vector_base&
    operator=(const _Silver_vector_base& rght)
               noexcept(std::is_nothrow_move_constructible_v<allocator_type>)
    {
      if(*this == rght)
        return *this;
      *this = std::move(_Silver_vector_base(rght));
      return *this;
    }

    // only used by derived class
  protected:

    // may throw { std::bad_array_new_length, std::bad_alloc }
    // allocates 2^k only where 2^k >= n. then tags the end of
    // the buffer
    constexpr
    explicit
    _Silver_vector_base(size_type n)
      : _Silver_vector_base()
    {
      size_type _cap = 2;
      while( _cap < n )
        _cap <<= 1;
      auto& _M_alloc = _M_get_alloc();
      _M_beg = _M_alloc.allocate(_cap);
      // tag the buffer
      _M_alloc.construct((_M_beg + _cap - 1), _M_end_tag());
    }

    constexpr
    explicit
    _Silver_vector_base(__pointer _ptr, const allocator_type& _alloc = allocator_type{})
      : __alloc_base_type(_alloc), _M_beg(std::move(_ptr))
    { }

    constexpr
    allocator_type&
    _M_get_alloc() noexcept
    {
      return *static_cast<allocator_type*>(this);
    }

    constexpr
    inline
    __pointer
    _M_begin() noexcept
    {
      return _M_beg;
    }

    // the end of the Element count
    // Time complexity Linear (_hintPos - lastElem)
    constexpr
    __pointer
    _M_end(size_type _hintPos) const noexcept
    {
      auto _M_start = _M_begin() + _hintPos;
      while(*_M_start or *_M_start != _M_end_tag())
        ++_M_start;
      return _M_start;
    }

    constexpr
    __pointer
    _M_end(const __pointer& _hintPos) const noexcept
    {
      auto _M_start = _M_begin();
      std::ptrdiff_t _idx = _hintPos - _M_start;
      if(_idx < 0)
      // assuming memory on always grows toward higher addresses
      // maybe we should create a type trait to detect this property?
        return nullptr;
      return _M_end(_idx);
    }

    // a tag value used to mark the end of the allocated space(buffer capacity)
    constexpr
    inline
    value_type
    _M_end_tag() const noexcept
    {
      if constexpr(std::is_same_v<__iterator_tag
                                , std::random_access_iterator_tag>)
        return {-1};
      else
        return {nullptr};
    }

    // Capacity of this container, 2^k
    // Time complexity Log2(capacity)
    constexpr
    size_type
    _M_capacity() const noexcept
    {
      // the minimal capacity is 2;
      size_type _cap = 2;
      auto begin = _M_beg;
      while(*(begin + _cap - 1) != _M_end_tag())
        _cap >>= 1;
      return _cap;
    }

    /**
     * @brief _M_find_end
     *    Finds the One past the end index. Also the adequate capacity.
     * @param _startHint
     *    A hint position to narrow the range of search.
     * @return
     *    One past the End position and Adequate minimal capacity to hold
     *    that count of elements.
     */
    constexpr
    std::pair<size_type, size_type>
    _M_find_end(size_type _startHint) noexcept
    {
      auto _arr_ptr = _M_begin();
      size_type _idx = 0, _cap = 2, _inc = 1;
      while(_arr_ptr[_cap - 1] != _M_end_tag())
      {
        if(_arr_ptr[_idx] == std::nullopt)
          break;
        _cap = 2 << _inc;
        _idx = 1 << _inc;
        ++_inc;
      }
      // a new constructed container with maybe a value.
      if(_idx == 0)
      {
        if(_arr_ptr[_idx] == std::nullopt)
          return {_idx, _cap};
        else
          return {_idx + 1, _cap};
      }

      size_type _upr = _cap - 1, _lwr = 0;
      if(_startHint < _cap)
      {
        _upr = std::max(_startHint, _idx);
        _lwr = std::min(_startHint, _idx);
      }

      size_type _bfr = _arr_ptr[_idx] == std::nullopt ? _lwr : _idx;
      size_type _aft = _arr_ptr[_idx] == std::nullopt ? _idx : _upr;

      if(_arr_ptr[_bfr] == std::nullopt)
      {
        _aft = _lwr;
        _bfr = 0;
      }
      if(_arr_ptr[_aft] != std::nullopt)
      {
        _bfr = _upr;
        _aft = _cap - 1;
      }

      while(true) // converging algorithm
      {
          if((_aft - _bfr) == 1)
          {
            if(_arr_ptr[_idx] == std::nullopt)
              return {_bfr + 1, _cap};
            else
              return {_idx + 1, _cap};
          }
          _idx = _bfr + (_aft - _bfr) / 2;
          if(_arr_ptr[_idx] == std::nullopt)
            _aft = _idx;
          else
            _bfr = _idx;
      }
    }

    /**
     * @brief _M_cap_size
     * @return the size is calculated by finding the first disengaged value
     *    Also returns the minimal capacity adequate to hold that much elements
     *    Time complexity is Amortized O(log2(n))
     */
    constexpr
    std::pair<size_type, size_type>
    _M_cap_size() const noexcept
    {
      auto [_sz, _cap] = _M_find_end(0);
      return {_cap, _sz};
    }

    /**
     * @brief _M_clear
     *    destroyes all elements. Pointers are not invalidated,
     *    but the dereferenced value is an empty optional<T>
     */
    constexpr
    void _M_clear() noexcept
    {
      auto _begin = _M_begin();
      while(*_begin)
      {
        // std::optional::reset
        *_begin.reset();
        ++_begin;
      }
    }

    /**
     * @brief _M_val_at
     *    It is the responsability of the caller to give an index
     *    less than the capacity of the buffer.
     * @param _idx
     *    positive integer must be less than _M_capacity()
     * @return a reference into a std::optional, either engaged or
     *    disengaged.
     */
    constexpr
    reference
    _M_val_at(size_t _idx) noexcept
    {
      return _M_begin()[_idx];
    }

    /**
     * @brief _M_fill_disengaged
     *    Fills the buffer with disengaged std::optional until the position
     *    one less the capacity.
     *    It is the caller responsability to give a hint position less than
     *    the capacity of the buffer.
     * @param _hintPos
     *    starting position at which the first disengaged std::optional is
     *    created.
     */
    constexpr
    void
    _M_fill_disengaged(size_type _hintPos) noexcept
    {
      auto _M_start = _M_begin() + _hintPos;
      auto& _M_alloc = _M_get_alloc();
      while(*_M_start != _M_end_tag())
        _M_alloc.construct(_M_start++, value_type{});
    }

    /**
     * @brief _M_byte_blit
     *    Copy construct the _count values from source into destination,
     *    for trivially constructible types like pointers and integers
     *    it is equivalent to calling memcpy.
     * @param _dst
     *    the destination buffer.
     * @param _src
     *    the source buffer
     * @param _count
     *    number of element to copy construct at destination.
     */
    constexpr
    void
    _M_byte_blit(__pointer _dst, __pointer _src, size_t _count)
    {
      if constexpr( std::is_trivially_constructible_v<value_type>)
        std::memcpy(_dst, _src, _count * sizeof(value_type));
      else
        std::uninitialized_copy_n(_src, _count, _dst);
    }

    constexpr
    void
    _M_byte_blit(__pointer _src, size_t _count) noexcept
    {
      _M_byte_blit(_M_begin(), _src, _count);
    }

    /**
     * @brief _M_construct_at
     *    constructs a value_type at position. It is the responsability of
     *    the caller to give a position inside the buffer.
     *    If the _pos is at capacity, but the buffer is still empty then no
     *    reallocation heppens, otherwise reallocate and copy.
     * @param _pos
     *    Must be inside the buffer
     * @param _val
     *    The value to copy into the position
     * @return
     *    A pointer into the position of the inserted value, and boolean
     *    to indicate if reallocation happended.
     */
    std::pair<__pointer, bool>
    _M_construct_at(size_type _pos, const value_type& _val) noexcept
    {
      auto& _M_alloc = _M_get_alloc();
      if(_M_begin()[_pos] == _M_end_tag())
      {
        // reallocate if necessary
        size_type _new_cap = (_pos + 1) << 1;
        auto [_cap, _n_elem] = _M_cap_size();
        __pointer _tmp = nullptr;
        if (_cap < _new_cap)
          // no need to reallocate, just insert at the end

        try
        {
          _tmp = _M_alloc.allocate(_new_cap);
          _tmp[_new_cap - 1] = _M_end_tag();
          _M_byte_blit(_tmp, _M_begin(), _n_elem);
          _M_fill_disengaged(_n_elem);
        }
        catch (...)
        {
          throw;
        }
        _M_alloc.deallocate(_pos);
        *this = std::move(_Silver_vector_base(_tmp));
      }

      __pointer _ret = _M_begin() + _pos;
      _M_beg[_pos] = _val;
      return _ret;
    }
  };

  /**
   * @brief The _Silver_vector_base::__iterator class
   * out of class definition
   */
  template<typename _Container_iterator, typename _Rebound_alloc>
  struct
    _Silver_vector_base<_Container_iterator, _Rebound_alloc>
    ::__iterator
  {
    using __vector_type = _Silver_vector_base<_Container_iterator
                                            , _Rebound_alloc>;
    using __pointer = typename __vector_type::__pointer;
    using __ref = typename __vector_type::value_type&;
    using __const_ref = const typename __vector_type::value_type&;

  private:
    __pointer _M_ptr;

  public:
    constexpr
    __iterator(__pointer ptr) noexcept
      : _M_ptr(ptr)
    { }

    constexpr __iterator(const __iterator& othr) noexcept = default;
    constexpr __iterator(__iterator&& othr) noexcept = default;

    constexpr
    __iterator&
    operator=(const __iterator& rght) noexcept = default;

    constexpr
    __iterator&
    operator=(__iterator&& rght) noexcept = default;

    constexpr
    __iterator&
    operator++() noexcept
    {
      ++_M_ptr;
      return *this;
    }

    constexpr
    __iterator
    operator++(int) noexcept
    {
      auto tmp = __iterator(*this);
      ++_M_ptr;
      return tmp;
    }

    constexpr
    __ref
    operator*() noexcept
    {
      return *_M_ptr;
    }

    constexpr
    __const_ref
    operator*() const noexcept
    {
      return *_M_ptr;
    }
  };

  /**
   * @brief The _Silver_vector_base::__iterator_const class
   * out of class definition
   */
  template<typename _Container_iterator, typename _Rebound_alloc>
  struct
    _Silver_vector_base<_Container_iterator, _Rebound_alloc>
    ::__iterator_const
  {
    using __vector_type = _Silver_vector_base<_Container_iterator
                                            , _Rebound_alloc>;
    using __pointer = typename __vector_type::__pointer;
    using __ref = typename __vector_type::value_type&;
    using __const_ref = const typename __vector_type::value_type&;

  private:
    __pointer _M_ptr;

  public:
    constexpr
    __iterator_const(__pointer ptr) noexcept
      : _M_ptr(ptr)
    { }

    constexpr __iterator_const(const __iterator_const& othr) noexcept = default;
    constexpr __iterator_const(__iterator_const&& othr) noexcept = default;

    constexpr
    __iterator_const&
    operator=(const __iterator_const& rght) noexcept = default;

    constexpr
    __iterator_const&
    operator=(__iterator_const&& rght) noexcept = default;

    constexpr
    __iterator_const&
    operator++() noexcept
    {
      ++_M_ptr;
      return *this;
    }

    constexpr
    __iterator_const
    operator++(int) noexcept
    {
      auto tmp = __iterator_const(*this);
      ++_M_ptr;
      return tmp;
    }

    constexpr
    __const_ref
    operator*() noexcept
    {
      return *_M_ptr;
    }

    constexpr
    __const_ref
    operator*() const noexcept
    {
      return *_M_ptr;
    }
  };

  /**
   * @brief The svector class
   * A vector class with minimal memory foot print.
   * capacity growth is 2^k where k >= 1
   * the buffer is marked with a bogus value at the end of capacity
   * size() and capacity() calculate their value, i.e. not stored values
   * begin() is O(1)
   * erase() bubbles the value to the end of the buffer then disengage it.
   * push_back() is O(n)
   * pop_back() is O(n)
   * insert() is O(n-pos)
   */
  template<typename AdaptedIter
           , typename Allocator = std::allocator<AdaptedIter>>
  class svector : public _Silver_vector_base<AdaptedIter, Allocator>
  {
    using __base_type = _Silver_vector_base<AdaptedIter, Allocator>;
  public:
    using value_type = typename __base_type::value_type;
    using allocator_type = typename __base_type::allocator_type;
    using size_type = typename __base_type::size_type;
    using difference_type =
          typename std::allocator_traits<allocator_type>::difference_type;
    using reference = value_type&;
    using const_reference =  const value_type&;
    using pointer = typename std::allocator_traits<allocator_type>::pointer;
    using const_pointer =
          typename std::allocator_traits<allocator_type>::const_pointer;
    using iterator = typename __base_type::iterator;
    using const_iterator = typename __base_type::const_iterator;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const iterator>;

    using __base_type::__base_type
         ,__base_type::operator=;

    ~svector() = default;

    // allocate enough capacity to hold the elements count
    constexpr svector(std::initializer_list<value_type> const& il) noexcept
      : __base_type(il.size())
    {
      using __base_type::_M_byte_blit
          , __base_type::_M_fill_disengaged;

      static_assert(std::is_trivially_constructible_v<value_type>);

      _M_byte_blit(std::data(il), il.size());

      // finish the rest till the capacity with disengaged value_type
      _M_fill_disengaged(il.size());
    }

    // should be used sparsly
    constexpr
    size_type
    size() const noexcept
    {
      using __base_type::_M_size;
      return _M_size();
    }

    // relys on size(),i.e should be used sparsly
    constexpr
    bool
    empty() const noexcept
    { return size() == 0; }

    // recyle container instead of clearing it
    // Linear in time.
    constexpr
    void
    clear() noexcept
    {
      using __base_type::_M_clear;
      _M_clear();
    }

    constexpr
    reference
    operator[](size_t index) noexcept
    {
      using __base_type::_M_val_at;
      return _M_val_at(index);
    }

    constexpr
    const_reference
    operator[](size_t index) const noexcept
    {
      using __base_type::_M_val_at;
      return _M_val_at(index);
    }

  };

///@} _Silver_vector
} // namespace __detail
_GLIBCXX_END_NAMESPACE_VERSION
} // namespace stl


#endif // SILVER_VECTOR_H
