/* -*- coding: utf-8 -*- マルチバイト */

#ifndef _SYMTREE_UTIL_HPP_
#define _SYMTREE_UTIL_HPP_

#include <type_traits>

#define UNUSED(x) (void)x

namespace symtree
{
/*
 * iterator実装の為の便利クラス。boostやfollyのiterator_facadeと似てる。
 * iteratorを実装したい人はこのクラスを継承して以下のメソッドを実装すると、iter++, ++iter、など必要なモノをすべて実装してくれる。
 * 
 *   void increment();
 *   void decrement(); // オプショナル、--したい人だけ実装
 *   V& dereference() const;
 *   bool equal( const D& other) const;
 * 
 * Templateパラメータ:
 * D: 継承先クラス (CRTP)
 * V: 値の型
*/
template<class D, typename V>
class iterator_facade
{
public:
  const V& operator*() const
  {
    return as_derived_const().dereference();
  }

  V& operator*() 
  {
    return as_derived().dereference();
  }

  D& operator++()
  {
    as_derived().increment();
    return as_derived();
  }

  D operator++(int)
  {
    auto res = as_derived(); // 進める前をコピー
    as_derived().increment();
    return res;
  }

  D& operator--()
  {
    as_derived().decrement();
    return as_derived();
  }

  D operator--(int)
  {
    auto res = as_derived(); // 戻す前をコピー
    as_derived().decrement();
    return res;
  }

  bool operator==(const D &other) const
  {
    return as_derived_const().equal( other );
  }
  bool operator!=(const D &other) const
  { 
    return !as_derived_const().equal( other );
  }

 private:
  D& as_derived() {
    return static_cast<D&>( *this );
  }

  const D & as_derived_const() const {
    return static_cast<const D&>( *this );
  }
};

///////////////////////////////////
// irange
///////////////////////////////////

/*
countable_range: irangeのヘルパークラス。irangeからしか使わない。（ユーザーはautoで受け取るので）
*/
template<typename T>
class countable_range
{
  T _rangeBegin;
  T _rangeEnd;

  class range_iterator : public iterator_facade<range_iterator, T>
  {
    T _current;

  public:
    T& dereference() { return _current; }
    void increment() { ++_current; }
    bool equal(const range_iterator &other) const { return _current == other._current; }

    range_iterator(T start) : _current (start) { }
  };


public:
  countable_range( T beg, T end ) : _rangeBegin(beg),_rangeEnd(end) {}
  range_iterator begin() const { return range_iterator(_rangeBegin); }
  range_iterator end() const { return range_iterator(_rangeEnd); }
};

/*
beg から end-1 までの数字を順番に返すiteratorを返す。(endは含まない)
range for文で使う。

例:
for (auto i : irange(0, 100))
{
  ...
}
*/
template<typename T>
countable_range<T> irange( T beg, T end ) { return countable_range<T>( beg, end ); }

/*
begに0を即値で指定すると、endがunsignedだったりint64_tだった時に型違いが発生するので、そのケースの型解決用のヘルパー。
*/
template<typename T>
countable_range<typename std::enable_if<!std::is_same<T, int>::value, T>::type> irange( int beg, T end ) { return countable_range<T>( (T) beg, end ); }

/*
irange(0, end)と同じ振る舞いをするヘルパー。０からend-1までのiteratorを返す。
*/
template<typename T>
countable_range<T> irange( T end ) { return countable_range<T>( 0, end ); }
}
#endif