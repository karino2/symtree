/* -*- coding: utf-8 -*- マルチバイト */

#ifndef _FOREST_HPP_
#define _FOREST_HPP_

#include <map>
#include <memory>
#include <set>
#include "util.hpp"


namespace symtree
{


enum class edge_dir
{
  leading,
  trailing
};

enum class prior_next
{
  prior,
  next
};


template<typename T> class forest_iterator;
template<typename T> class child_iterator;

extern int g_node_alloc_count;

/*
forestのノード。ノードの集合体がforestで、集合体自身を表すclassは無い。
*/
template<typename T>
class forest
{
  friend class forest_iterator<T>;

  // _edge[dir][prior_next]の順番。
  forest<T>* _edge[2][2]; 

  void init_edge()
  {
    /*
    leafはこの２つはthis
    */
    _edge[size_t(edge_dir::leading)][size_t(prior_next::next)] = this;
    _edge[size_t(edge_dir::trailing)][size_t(prior_next::prior)] = this;

    /*
    親は無し
    */
    _edge[size_t(edge_dir::leading)][size_t(prior_next::prior)] = nullptr;
    _edge[size_t(edge_dir::trailing)][size_t(prior_next::next)] = nullptr;
  }

public:
  using iterator = forest_iterator<T>;
  using const_iterator= forest_iterator<const T>;

  using ch_iterator = child_iterator<T>;
  using const_ch_iterator = child_iterator<const T>;

  explicit forest( const T& data ) : _data( data ) 
  {
    g_node_alloc_count++;
    init_edge();
  }

  explicit forest( T&& data ) : _data( std::move( data ) )
  {
    g_node_alloc_count++;
    init_edge();
  }

  ~forest()
  {
    g_node_alloc_count--;
    if(is_root())
    {
      // 自分を除く子どもたちを削除。
      begin().erase( begin().trailing_of() );
      assert( !begin().has_children() );
    }
  }

  bool is_root() const
  {
    return _edge[size_t(edge_dir::leading)][size_t(prior_next::prior)] == nullptr
      &&  _edge[size_t(edge_dir::trailing)][size_t(prior_next::next)] == nullptr;
  }

  T _data;

  forest<T>*& get_link(edge_dir dir, prior_next link) { return _edge[size_t(dir)][size_t(link)]; }
  
  forest<T>* get_link(edge_dir dir, prior_next link) const { return _edge[size_t(dir)][size_t(link)]; }

  iterator begin() { return iterator( this, edge_dir::leading ); }

  /*
    const_iteratorにわたすthisがconst forest<T>*になってしまうので強制的にconstを取るキャストをしている。
    本来はforest_iteratorのノードを表す型をテンプレートにし forest<T>* と const forest<T>*を選べるようにするのが正しいが、
    そうするとforest_iteratorの中でインテリセンスが効かなくなってしまうので、お行儀の悪いキャストで乗り切る事にする。
  */
  const_iterator begin() const { return const_iterator( (forest<const T>*)this, edge_dir::leading ); }

  iterator end() { return iterator( this, edge_dir::trailing ).next_of(); }
  const_iterator end() const { return const_iterator( (forest<const T>*)this, edge_dir::trailing ).next_of(); }

  ch_iterator begin_child() { return ch_iterator( begin() ); }
  ch_iterator end_child() { return ch_iterator( begin() ).end(); }
  const_ch_iterator cbegin_child() const { return const_ch_iterator( begin() ); }
  const_ch_iterator cend_child() const { return const_ch_iterator( begin() ).end(); }

  /*
    すべてのエッジに対して関数fnを実行。
    fnはiterを引数に取る。iterをいじっても良い（to_leading()とか）
  */
  template<typename F>
  void for_each( F fn )
  {
    for( auto iter = begin(); iter != end(); iter++ )
    {
      fn( iter );
    }
  }

  /*
    すべてのエッジに対して、Leadingの時だけ関数fnを実行。
    fnはiterを引数に取る。iterをいじっても良い（to_leading()とか）
  */
  template<typename F>
  void for_each_leading( F fn )
  {
    for( auto iter = begin(); iter != end(); iter++ )
    {
      if (iter.is_leading())
        fn( iter );
    }
  }

  /*
  nth番目の子供を返す。子供の数より多い場合はnullptrを返す。
  */
  forest<T>*
  nth_child( int nth )
  {
    auto iter = child_iterator<T>( this );
    for(auto i : irange( nth ))
    {
      UNUSED( i );
      iter++;
    }
    if (iter == iter.end())
      return nullptr;
    return iter.get_node();
  }

  /*
  childを末っ子として追加。サブツリーもOK。
  */
  void
  append_child( forest<T>* child )
  {
    begin().to_trailing().chain( child );    
  }

  bool
  has_children() const
  {
    return begin().has_children();
  }

  /*
  ツリーをクローンする。
  要素のTに対し、以下の関数が存在する場合だけ使えるメソッド。
  T C::clone(const T&);
  各ノードはC::cloneを使ってクローンしていく。
  Tがunique_ptrなどの時の為、メソッドでは無くテンプレートで指定する事にした。
  */
  template<typename C>
  typename std::enable_if<std::is_same<T, decltype(C::clone( std::declval<T>() ))>::value, forest<T>*>::type
  clone() const
  {
    using new_iterator = forest_iterator<typename std::remove_const<T>::type>;

    std::map<forest<T>*, forest<T>*> alloced;
    auto newRoot = new forest<T>( C::clone( _data ) );

    // 単なるポインタの値をキーとして使いたいだけなのだが、
    // うまくconstつけてコンパイル通せなかったのでキャスト…
    alloced[ (forest<T>*)this] = newRoot;

    auto prev = newRoot->begin();
    for( auto iter = begin().next_of(); iter != end(); iter++ )
    {
      auto newNode = alloced[(forest<T>*)iter.get_node() ];
      if ( newNode == nullptr )
      {
        newNode = new forest<T>( C::clone( iter.get_node()->_data ) );
        alloced[(forest<T>*)iter.get_node() ] = newNode;
      }

      // 新しいツリーの方の, iterと同じ場所を指すiterator
      // new_iterator newiter {iter};
      // newiter.set_node( newNode );
      new_iterator newiter ( newNode, iter._edge._direction );

      prev.set_next( newiter );
      prev++;
      assert( newiter == prev );
    }

    return newRoot;
  }
};

template<typename T>
struct edge
{
  forest<T> *_node;
  edge_dir _direction;
  edge( forest<T>* node, edge_dir dir ) : _node( node ), _direction( dir ) {}

  bool equal( const edge<T>& other ) const
  {
    return _node == other._node && _direction == other._direction;
  }

  bool is_leading() const { return _direction == edge_dir::leading; }
  bool is_trailing() const { return _direction == edge_dir::trailing; }

  T& operator*() const { return _node->_data; }
};

template<typename T>
class forest_iterator : public iterator_facade<forest_iterator<T>, edge<T>>
{
  friend class forest<T>;
  static const auto next = prior_next::next;
  static const auto prior = prior_next::prior;
  static const auto leading = edge_dir::leading;
  static const auto trailing = edge_dir::trailing;

  forest<T>*& get_link(edge_dir dir, prior_next link) { return _edge._node->get_link( dir, link ); }  
  forest<T>* get_link(edge_dir dir, prior_next link) const { return _edge._node->get_link( dir, link ); }

  void set_link( edge_dir dir, prior_next link, forest<T> *node )
  {
    // みなし子のrootはnullptr。その場合は更新しない。
    if (get_node() != nullptr)
      get_link( dir, link ) = node;
  }

  void set_next( forest_iterator<T>& y )
  {
    set_link( _edge._direction, prior_next::next, y.get_node() );
    y.set_link( y._edge._direction, prior_next::prior, get_node() );
  }

  bool _rootDeleteing = false;


public:
  edge<T> _edge;

  forest_iterator( forest<T>* node, edge_dir dir ) : _edge( node, dir ) {}

  forest_iterator( const forest_iterator& x ) : _edge( x._edge ) {}

  forest<T>* get_node() const { return _edge._node; }
  T& content() { return _edge._node->_data; }
  void set_node( forest<T> *node ) { _edge._node = node; }
  void set_direction( edge_dir dir ) { _edge._direction = dir; }

  ////////////////////////////
  // iterator_facade関連
  ////////////////////////////


  bool equal( const forest_iterator<T>& other ) const
  {
    return _edge.equal( other._edge );
  }

  edge<T>& dereference() { return _edge; }
  const edge<T>& dereference() const { return _edge; }

  /*
    ASLのドキュメントのiterationを見ながら読むとわかりやすい。
    https://stlab.adobe.com/group__asl__tutorials__forest.html
  */
  void increment()
  {
    forest<T>* nextNode = get_link( _edge._direction, next );

    if ( _edge.is_leading() )
    {
      // leafだったら反転。それ以外ならleadingのまま。
      _edge._direction = ( nextNode == get_node() ? trailing : leading );
    }
    else
    {
      // 兄弟に移動する場合は反転、親に戻る場合はそのまま。
      // 兄弟に移動したかどうかは、nextのleading-priorが移動元かどうかで判定
      // nextがendまで来るとnullptrになるが、その場合は反転はしない。
      if (nextNode != nullptr )
        _edge._direction = ( nextNode->get_link( leading, prior ) == get_node() ? leading : trailing );
    }

    _edge._node = nextNode;
  }

  /*
    end()から--は出来ない（nulllptrからは戻り方が分からないので）
  */
  void decrement()
  {
    forest<T>* prev = get_link( _edge._direction, prior );

    if ( _edge.is_leading() )
    {
      // 兄弟に移動する場合は反転、親に戻る場合はそのまま。
      // 兄弟に移動したかどうかは、prevのtrailing-nextが移動元かどうかで判定
      // prevがnullptrの場合はrootの場合なので兄弟はいないからそのまま。

      _edge._direction = ( prev!= nullptr && prev->get_link( trailing, next ) == get_node() ? trailing : leading );
    }
    else
    {
      // leafだったら反転。それ以外ならtrailingのまま。
      _edge._direction = ( prev == get_node() ? leading : trailing );
    }

    _edge._node = prev;
  }

  ////////////////////////////
  // Iteratorのそのほかのメソッド
  ////////////////////////////

  // 自身をTrailにする
  forest_iterator& to_trailing()
  {
    _edge._direction = trailing;
    return *this;
  }

  // 自身をLeadingにする
  forest_iterator& to_leading()
  {
    _edge._direction = leading;
    return *this;
  }

  // Leadingのコピーを返す
  forest_iterator leading_of() const
  {
    auto res = *this;
    res._edge._direction = leading;
    return res;
  }

  // Trailingのコピーを返す
  forest_iterator trailing_of() const
  {
    auto res = *this;
    res.to_trailing();
    return res;
  }

  // nextのコピーを返す。nextは++で求める。
  forest_iterator next_of() const
  {
    auto res = *this;
    res++;
    return res;
  }

  // priorのコピーを返す。priorは--で求める。
  forest_iterator prior_of() const
  {
    auto res = *this;
    res--;
    return res;
  }

  bool is_leading() const { return _edge._direction == leading; }
  bool is_trailing() const { return _edge._direction == trailing; }

  /*
    num個の子供を飛ばして、次はnum+1個目の子供に行く状態にする。
    ようするにnum個目の子供のTrailingの状態にする。
  */
  void skip_n_children( int num )
  {
    for( auto i : irange( num ))
    {
      UNUSED( i );
      (*this)++;
      to_trailing();
    }
  }

  /*
  現在のエッジにノードを挿入する。

  insertをiteratorのメソッドとするのはSTL的では無いが、
  free floating関数にするとxの型のADLが効いてしまって使いづらいかったので
  iteratorのメソッドとした。

  insertの実装は、
  https://github.com/pixiv/polygon/issues/220#issuecomment-715770985
  の4パターンを見ながら読むとわかりやすい。
  */
  forest_iterator<T> insert( const T& x )
  {
    return chain( new forest<T>(x) );
  }

  /*
  insertのmoveバージョン。
  詳細はコピーコンストラクタバージョンを参照のこと。
  */
  forest_iterator<T> insert( T&& x )
  {
    return chain( new forest<T>( std::move( x ) ) );
  }

  /*
    今さしているiteratorのノードに子供が要るかを返す。

    nodeに実装する方がいいか？
  */
  bool has_children() const
  {
    return get_node() != leading_of().next_of().get_node();
  }

  /*
    自身の指しているノードを削除し、次の有効なイテレータを返す。指しているノードが葉の時しか呼んではいけない。thisの指すイテレータは以後使わない事。
  */
  forest_iterator erase()
  {
    forest_iterator leading_prior( leading_of().prior_of() );
    forest_iterator trailing_next( trailing_of().next_of() );

    assert( !has_children() );
    leading_prior.set_next( trailing_next );

    // nullにすると誤ってend()と一致してしまうかもしれないので、deleteするだけにする。
    delete _edge._node;

    return  (_edge._direction == leading)  ? leading_prior.next_of() : trailing_next;
  }

  /*
    現在の位置からlastまでのノードを削除する。
    lastが現在よりも上まで続いている場合はiteratorが2回通るノードのみ削除。

    詳細は以下
    https://stlab.adobe.com/group__asl__tutorials__forest.html
    の Node Deletionが参考になる。
  */
  forest_iterator erase( const forest_iterator& last )
  {
    // eraseの開始であるthisはleadingで無くてはいけない。
    assert( is_leading() );

    int stack_depth = 0;
    forest_iterator cur( *this );

    while (cur != last)
    {
      if(cur.is_leading())
      {
        stack_depth++;
        cur++;
      }
      else // 戻り
      {
        // 二度通っていたら削除
        if (stack_depth > 0)
        {          
          cur = cur.erase();
        } 
        else
        {
          ++cur;
        }
        stack_depth = std::max(0, stack_depth - 1);
      }
    }
    return last;
  }

  /*
    現在の位置にサブツリーを挿入。
    挿入のルールはinsertと同様。
    引数のsubtreeの寿命は以後このツリーが管理するから呼び出し元で削除しない事。
  */
  forest_iterator<T> chain( forest<T>* subtree )
  {
    forest_iterator<T> result( subtree, edge_dir::leading );

    forest_iterator<T> prev( prior_of() );

    forest_iterator<T> newTrail( result.trailing_of() );

    prev.set_next( result );
    newTrail.set_next( *this );

    return result;  
  }

  /*
    現在指しているノードとその子孫のサブツリーを、現在のツリーから切り離す。
    返されるサブツリーのルートの寿命管理は、呼び出し側が行う。
    iteratorは次に進む。簡単のため、thisはleadingじゃないと駄目としておく。
  */
  forest<T>* unchain()
  {
    assert( is_leading() );
    assert( !get_node()->is_root() );

    forest_iterator leading_prior( prior_of() );
    forest_iterator trailing_next( trailing_of().next_of() );

    leading_prior.set_next( trailing_next );

    // unchainするノードの親をnullptrに。
    get_link( leading, prior ) = nullptr;
    get_link( trailing, next ) = nullptr;

    auto ret = get_node();

    // thisのイテレータを次に進める。_nodeを更新するだけでいいはず。
    _edge._node = trailing_next.get_node();
    assert( _edge._direction == leading );

    return ret;
  }

  /*
    現在指しているノードを、引数のnewNodeに差し替える。
    現在指しているノードはunchainされてuniqu_ptrとして返される。
    thisは新しいノードのtrailingを指す。
  */
  std::unique_ptr<forest<T>> replace( forest<T>* newNode )
  {
    auto oldNode = _edge._node;
    auto prevLead = oldNode->get_link( leading, prior );
    auto nextTrail = oldNode->get_link( trailing, next );
    newNode->get_link( leading, prior ) = prevLead;
    newNode->get_link( trailing, next ) = nextTrail;

    // prevLeadとnexttrailがnullptrならそもそもこのノードがルートなので差し替える必要は無い。
    // その場合はoldNodeをunique_ptrで返せば十分。
    if (prevLead != nullptr)
    {
      if (prevLead->get_link( leading, next ) == oldNode)
        prevLead->get_link( leading, next ) = newNode;
      else
        prevLead->get_link( trailing, next ) = newNode;
    }

    if (nextTrail != nullptr)
    {
      if (nextTrail->get_link( trailing, prior ) == oldNode)
        nextTrail->get_link( trailing, prior ) = newNode;
      else
        nextTrail->get_link( leading, prior ) = newNode;
    }

    oldNode->get_link(leading, prior) = nullptr;
    oldNode->get_link(trailing, next) = nullptr;

    _edge._node = newNode;
    _edge._direction = trailing;
    
    return std::unique_ptr<forest<T>>( oldNode );
  }
};

template<typename T>
class child_iterator : public iterator_facade<child_iterator<T>, T>
{
  forest_iterator<T> _curIterator;
  forest_iterator<T> _endIterator;

  explicit child_iterator( const forest_iterator<T>& cur, const forest_iterator<T>& end ) : _curIterator( cur ), _endIterator( end )
  {
  }

public:
  child_iterator( const child_iterator& x ) = default;

  explicit child_iterator( forest<T> *node ) : child_iterator( node->begin() ) {}
  explicit child_iterator( const forest_iterator<T>& parentIter ) : _curIterator( parentIter ), _endIterator( parentIter.trailing_of() )
  {
    _curIterator++;
  }

  /*
    iterator_facade関連
  */

  void increment()
  {
    if ( _curIterator == _endIterator )
      return;    
    _curIterator.to_trailing();
    _curIterator++;
  }

  T& dereference() const { return _curIterator.get_node()->_data; }

  bool equal( const child_iterator<T>& other ) const
  {
    return other._curIterator == _curIterator;
  }

  child_iterator<T> end() const
  {
    return child_iterator( _endIterator, _endIterator );
  }

  /*
  そのほかのpublic method。
  */

  forest<T>* get_node()
  {
    return _curIterator.get_node();
  }

  /*
    現在指しているノードを差し替える。
    forest_iteratorのreplaceと違い、指しているエッジはnewNodeのLeadingとなる（同じ場所）
  */
  std::unique_ptr<forest<T>> replace( forest<T>* newNode )
  {
    auto curIter = _curIterator;
    auto ret = _curIterator.replace( newNode );

    // 現在の指している場所を復元。
    _curIterator = curIter;
    _curIterator._edge._node = newNode;

    return ret;
  }

};

}

#endif

