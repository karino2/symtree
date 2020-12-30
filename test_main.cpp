#include "nfiftest.hpp"
#include "forest.hpp"
#include <string>
#include <iostream>
#include <sstream>

using namespace std;
using namespace symtree;
using namespace nfiftest;

// インテリセンスの効きがいまいちなので同じファイルに再定義
#undef REQUIRE
#define REQUIRE(expr) if(!(expr)) throw nfiftest::assert_fail_error(__FILE__, __LINE__, #expr)

int symtree::g_node_alloc_count = 0;


string dump_tree( forest<string>& node )
{
  stringstream actual;
  for (auto& edge : node)
  {
    if ( edge._direction == edge_dir::leading )
    {
      actual << "<" << *edge << ">" << endl;
    }
    else
    {
      actual << "</" << *edge << ">" << endl;
    }
  }
  return actual.str();
}

struct destructor_tracker
{
  destructor_tracker( bool& isDestructorCalled ) : _destructorCalled( isDestructorCalled ) {}
  destructor_tracker( destructor_tracker&& src ) : _destructorCalled( src._destructorCalled )
  {
    src._isMoved = true;
  } 
  ~destructor_tracker()
  { 
    if (!_isMoved)
      _destructorCalled = true;
  }
  bool& _destructorCalled;
  bool _isMoved = false;
};

struct string_cloner
{
  static string clone( const string& src )
  {
    return std::string( src );
  }
};


std::vector<TestPair> test_cases = {
{"forestの少し複雑なツリーのテスト", []{
  /*
  https://stlab.adobe.com/group__asl__tutorials__forest.html のDefault Construction and insertと同じ例。
  */
  forest<string> node( "grandmother" );
  auto i = node.begin().to_trailing();
  {
    auto p = i.insert( "mother" ).to_trailing();
    p.insert( "me" );
    p.insert( "sister" );
    p.insert( "brother" );
  }

  {
    auto p = i.insert( "aunt" ).to_trailing();
    p.insert( "cousin" );
  }
  i.insert( "uncle" );

  if (SECTION("ツリーが出来ているかをダンプして確認")) {SG g;
    auto expect = R"(<grandmother>
<mother>
<me>
</me>
<sister>
</sister>
<brother>
</brother>
</mother>
<aunt>
<cousin>
</cousin>
</aunt>
<uncle>
</uncle>
</grandmother>
)";
    auto actual = dump_tree( node );

    REQUIRE( actual == expect );
  }

  if (SECTION("skip_n_childrenのテスト、2つ飛ばす")) {SG g;
    auto iter = node.begin();
    iter.skip_n_children( 2 ); // motherとauntをスキップ、auntのTrailing（つまり次の++でuncleに）。
    
    REQUIRE( iter.content() == "aunt" );
    REQUIRE( iter.is_trailing() );
  }

  if (SECTION("長男を削除")) {SG g;
    auto iter = node.begin();
    iter++; // mother
    iter++; //me
    iter.erase();

    auto expect = R"(<grandmother>
<mother>
<sister>
</sister>
<brother>
</brother>
</mother>
<aunt>
<cousin>
</cousin>
</aunt>
<uncle>
</uncle>
</grandmother>
)";
    auto actual = dump_tree( node );

    REQUIRE( actual == expect );
  }

  if (SECTION("まんなかの子を削除")) {SG g;
    auto iter = node.begin();
    iter++; // mother
    iter++; //me
    iter++; //me-trail
    iter++; // sister
    iter.erase();

    auto expect = R"(<grandmother>
<mother>
<me>
</me>
<brother>
</brother>
</mother>
<aunt>
<cousin>
</cousin>
</aunt>
<uncle>
</uncle>
</grandmother>
)";
    auto actual = dump_tree( node );

    REQUIRE( actual == expect );
  }

  if (SECTION("末子を削除")) {SG g;
    auto iter = node.begin();
    iter++; // mother
    iter++; //me
    iter++; //me-trail
    iter++; // sister
    iter++; // sister-trail
    iter++; // brother
    iter.erase();

    auto expect = R"(<grandmother>
<mother>
<me>
</me>
<sister>
</sister>
</mother>
<aunt>
<cousin>
</cousin>
</aunt>
<uncle>
</uncle>
</grandmother>
)";
    auto actual = dump_tree( node );

    REQUIRE( actual == expect );
  }

  if (SECTION("一人っ子を削除")) {SG g;
    auto iter = node.begin();
    iter++; // mother
    iter++; //me
    iter++; //me-trail
    iter++; // sister
    iter++; // sister-trail
    iter++; // brother
    iter++; // brother
    iter++; // mother-trail
    iter++; // aunt
    iter++; // cousin
    iter.erase();

    auto expect = R"(<grandmother>
<mother>
<me>
</me>
<sister>
</sister>
<brother>
</brother>
</mother>
<aunt>
</aunt>
<uncle>
</uncle>
</grandmother>
)";
    auto actual = dump_tree( node );

    REQUIRE( actual == expect );
  }

  if (SECTION("範囲削除、motherの子供全削除")) {SG g;
    auto iter = node.begin();
    iter++; // mother
    auto last = iter.trailing_of();
    iter++; // first-child.
    iter.erase( last );

    auto expect = R"(<grandmother>
<mother>
</mother>
<aunt>
<cousin>
</cousin>
</aunt>
<uncle>
</uncle>
</grandmother>
)";
    auto actual = dump_tree( node );

    REQUIRE( actual == expect );
  }

  if (SECTION("範囲削除、motherの子供全削除、lastが少し先のケース")) {SG g;
    auto iter = node.begin();
    iter++; // mother

    auto last = iter.trailing_of(); // mother-trail
    last++; // aunt-leading 

    iter++; // first-childl
    iter.erase( last );

    auto expect = R"(<grandmother>
<mother>
</mother>
<aunt>
<cousin>
</cousin>
</aunt>
<uncle>
</uncle>
</grandmother>
)";
    auto actual = dump_tree( node );

    REQUIRE( actual == expect );
  }

  if (SECTION("範囲削除、真ん中のサブツリーを削除")) {SG g;
    auto iter = node.begin();
    iter++; // mother
    iter.to_trailing(); // mother-trail
    iter++; // aunt-leading 

    auto last = iter.trailing_of();
    last++; // uncle-lead

    iter.erase( last );

    auto expect = R"(<grandmother>
<mother>
<me>
</me>
<sister>
</sister>
<brother>
</brother>
</mother>
<uncle>
</uncle>
</grandmother>
)";
    auto actual = dump_tree( node );

    REQUIRE( actual == expect );
  }

  if (SECTION("auntをunchain")) {SG g;
    auto iter = node.begin();
    iter++; // mother
    iter.to_trailing(); // mother-trail
    iter++; // aunt-leading 

    auto auntTree = iter.unchain();

    REQUIRE( iter.is_leading() );
    REQUIRE( *(iter._edge) == "uncle");
    REQUIRE( auntTree->is_root() );

    auto expect1 = R"(<grandmother>
<mother>
<me>
</me>
<sister>
</sister>
<brother>
</brother>
</mother>
<uncle>
</uncle>
</grandmother>
)";
    auto expect2 = R"(<aunt>
<cousin>
</cousin>
</aunt>
)";
    auto actual1 = dump_tree( node );
    auto actual2 = dump_tree( *auntTree );

    REQUIRE( actual1 == expect1 );
    REQUIRE( actual2 == expect2 );

    delete auntTree;
  }
  if (SECTION("サブツリーをchain")) {SG g;
    auto subtree = new forest<string>( "A" );
    auto i = subtree->begin().to_trailing();
    i.insert( "B" ).to_trailing();
    i.insert( "C" ).to_trailing();

    auto iter = node.begin();
    iter.to_trailing(); // grand-trail
    iter--; // uncle-trail
    iter.chain( subtree );

    auto expect = R"(<grandmother>
<mother>
<me>
</me>
<sister>
</sister>
<brother>
</brother>
</mother>
<aunt>
<cousin>
</cousin>
</aunt>
<uncle>
<A>
<B>
</B>
<C>
</C>
</A>
</uncle>
</grandmother>
)";
    auto actual = dump_tree( node );

    REQUIRE( actual == expect );
  }

  if (SECTION("サブツリーをreplace、兄弟あり")) {SG g;
    auto subtree = new forest<string>( "A" );
    auto i = subtree->begin().to_trailing();
    i.insert( "B" ).to_trailing();
    i.insert( "C" ).to_trailing();

    auto iter = node.begin();
    iter.to_trailing(); // grand-trail
    iter--; // uncle-trail
    iter--; // uncle-lead
    iter--; // aunt-trail
    iter.to_leading();
    auto ret = iter.replace( subtree );

    REQUIRE( ret->is_root() );
    REQUIRE( iter.is_trailing() );
    REQUIRE( iter.content() == "A" );

    auto expect1 = R"(<grandmother>
<mother>
<me>
</me>
<sister>
</sister>
<brother>
</brother>
</mother>
<A>
<B>
</B>
<C>
</C>
</A>
<uncle>
</uncle>
</grandmother>
)";
    auto expect2 = R"(<aunt>
<cousin>
</cousin>
</aunt>
)";

    auto actual1 = dump_tree( node );
    REQUIRE( actual1 == expect1 );

    auto actual2 = dump_tree( *(ret.get()) );
    REQUIRE( actual2 == expect2 );
  }

  if (SECTION("サブツリーをreplace、一人っ子")) {SG g;
    auto subtree = new forest<string>( "A" );
    auto i = subtree->begin().to_trailing();
    i.insert( "B" ).to_trailing();
    i.insert( "C" ).to_trailing();

    auto iter = node.begin();
    iter.to_trailing(); // grand-trail
    iter--; // uncle-trail
    iter--; // uncle-lead
    iter--; // aunt-trail
    iter--; // cousin-trail
    iter.to_leading();
    auto ret = iter.replace( subtree );

    REQUIRE( ret->is_root() );

    auto expect1 = R"(<grandmother>
<mother>
<me>
</me>
<sister>
</sister>
<brother>
</brother>
</mother>
<aunt>
<A>
<B>
</B>
<C>
</C>
</A>
</aunt>
<uncle>
</uncle>
</grandmother>
)";
    auto expect2 = R"(<cousin>
</cousin>
)";

    auto actual1 = dump_tree( node );
    REQUIRE( actual1 == expect1 );

    auto actual2 = dump_tree( *(ret.get()) );
    REQUIRE( actual2 == expect2 );
  }

  if (SECTION("cloneのテスト")) {SG g;
    auto expect = R"(<grandmother>
<mother>
<me>
</me>
<sister>
</sister>
<brother>
</brother>
</mother>
<aunt>
<cousin>
</cousin>
</aunt>
<uncle>
</uncle>
</grandmother>
)";

    auto cloned = node.clone<string_cloner>();

    auto actual = dump_tree( *cloned );

    REQUIRE( actual == expect );
    REQUIRE( cloned != &node );
    delete cloned;
  }

  if (SECTION("child_iteratorのテスト")) {SG g;
    auto iter = child_iterator<string>( &node );
    auto end = iter.end();
    REQUIRE( *iter == "mother" );
    REQUIRE( iter != end );

    iter++;
    REQUIRE( *iter == "aunt" );
    REQUIRE( iter != end );

    iter++;
    REQUIRE( *iter == "uncle" );
    REQUIRE( iter != end );

    iter++;
    REQUIRE( iter == end );
  }

  if (SECTION("nth_childのテスト")) {SG g;
    auto child = node.nth_child( 0 );
    REQUIRE( child->_data  == "mother" );

    child = node.nth_child( 1 );
    REQUIRE( child->_data == "aunt" );

    child = node.nth_child( 2 );
    REQUIRE( child->_data == "uncle" );

    child = node.nth_child( 3 );
    REQUIRE( child == nullptr );
  }
}},
{"forestのinsertのテスト", []{
  forest<string> node( "A" );
  auto i = node.begin().to_trailing();
  i.insert( "B" ).to_trailing();
  i.insert( "C" ).to_trailing();

  if (SECTION("ABCの親子関係が正しく出来ているかテスト")) {SG g;
    auto expect = R"(<A>
<B>
</B>
<C>
</C>
</A>
)";
    auto actual = dump_tree( node );

    REQUIRE( actual == expect );
  }
  /*
  https://github.com/pixiv/polygon/issues/220#issuecomment-715770985
  にある4通りのテスト。
  */
  if (SECTION("パターン1のテスト")) {SG g;
    auto iter = node.begin().next_of();
    iter.insert( "N" );

    auto expect = R"(<A>
<N>
</N>
<B>
</B>
<C>
</C>
</A>
)";
    auto actual = dump_tree( node );

    REQUIRE( actual == expect );
  }

  if (SECTION("パターン2のテスト")) {SG g;
    auto iter = node.begin();
    iter++;
    iter++;
    iter.insert( "N" );

    auto expect = R"(<A>
<B>
<N>
</N>
</B>
<C>
</C>
</A>
)";
    auto actual = dump_tree( node );

    REQUIRE( actual == expect );
  }

  if (SECTION("パターン3のテスト")) {SG g;
    auto iter = node.begin();
    iter++;
    iter++;
    iter++;
    iter.insert( "N" );

    auto expect = R"(<A>
<B>
</B>
<N>
</N>
<C>
</C>
</A>
)";
    auto actual = dump_tree( node );

    REQUIRE( actual == expect );
  }

  if (SECTION("パターン4のテスト")) {SG g;
    auto iter = node.begin();
    iter++;
    iter++;
    iter++;
    iter++;
    iter++;
    iter.insert( "N" );

    auto expect = R"(<A>
<B>
</B>
<C>
</C>
<N>
</N>
</A>
)";
    auto actual = dump_tree( node );

    REQUIRE( actual == expect );
  }


}},
{"forestでノードのデストラクタがちゃんと呼ばれるかのテスト", []{
  bool aCalled = false;
  bool bCalled = false;
  bool cCalled = false;
  auto root = new forest<destructor_tracker>( destructor_tracker( aCalled ) );
  auto i = root->begin().to_trailing();
  i.insert( destructor_tracker( bCalled ) ).to_trailing();
  i.insert( destructor_tracker( cCalled ) ).to_trailing();

  REQUIRE( !aCalled );
  REQUIRE( !bCalled );
  REQUIRE( !cCalled );

  delete root;

  REQUIRE( aCalled );
  REQUIRE( bCalled );
  REQUIRE( cCalled );
}},
{"forestのノード一つ一つの条件を定めたテスト", []{
  forest<string> node( "A" );
  if (SECTION("一つのノードのクローン")) {SG g;
    auto newNode = node.clone<string_cloner>();

    auto expect = "<A>\n</A>\n";
    auto actual = dump_tree( node );

    REQUIRE( actual == expect );

    delete newNode;
  }

  if (SECTION("子供が居ない時のchild_iteratorのテスト")) {SG g;
    auto iter = child_iterator<string>( &node );
    auto end = iter.end();
    REQUIRE( iter == end );
  }

  if (SECTION("子供が一人だけの時のテスト")) {SG g;
    auto i = node.begin().to_trailing();
    i.insert( "B" );

    if (SECTION("子供が一人だけの時のクローン")) {SG g;
      auto expect = R"(<A>
<B>
</B>
</A>
)";

      auto newNode = node.clone<string_cloner>();
      auto actual = dump_tree( node );

      REQUIRE( actual == expect );

      delete newNode;
    }

    if (SECTION("子供が一人だけの時のchild_iteratorのテスト")) {SG g;
      auto iter = child_iterator<string>( &node );
      auto end = iter.end();

      REQUIRE( *iter == "B" );
      REQUIRE( iter != end );

      iter++;
      REQUIRE( iter == end );
    }

  }  
}}
};

int main()
{
    RunTests(test_cases);
    return 0;
}