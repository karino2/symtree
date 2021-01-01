
#define _NFIFTEST_SUBTEST_
#include "nfiftest.hpp"

#include "symtree.hpp"
#include <string>
#include <iostream>
#include <sstream>
#include <tuple>

using namespace std;
using namespace symtree;
using namespace nfiftest;

// インテリセンスの効きがいまいちなので同じファイルに再定義
#undef REQUIRE
#define REQUIRE(expr) if(!(expr)) throw nfiftest::assert_fail_error(__FILE__, __LINE__, #expr)

int symtree::g_node_alloc_count = 0;

void check_string_equals( string str1, string str2 )
{
  if (str1.size() != str2.size())
    cout << "size differ: len1(" << str1.size() << "), len2(" << str2.size() << ")" << endl;
  int range = std::min( str1.size(), str2.size() );
  for( auto i : irange( range ))
  {
    if (str1[i] != str2[i])
    {
      cout << "differ at " << i << endl;
      cout << "src1: " << str1.substr( i, 100 ) << endl;
      cout << "src2: " << str2.substr( i, 100 ) << endl;
      return;
    }
  }
}

enum class test_sym
{
  int_imm,
  variable,
  sub,
  add
};

struct enum_formatter
{
  static std::string enum_to_str(test_sym sym)
  {
    switch(sym)
    {
      case test_sym::int_imm:
        return "int";
      case test_sym::variable:
        return "var";
      case test_sym::sub:
        return "sub";
      case test_sym::add:
        return "add";
    }
  }
};

using ttree = stree<test_sym>;
using ttree_builder = stree_builder<test_sym>;
using tatom = atom<test_sym>;

template<test_sym eid, typename... CHLDS>
using taccessor = accessor<test_sym, eid, CHLDS...>;

inline std::string ttree_dump(ttree& root)
{
  return stree_dump<test_sym, enum_formatter>(root);
} 

using int_imm = taccessor<test_sym::int_imm, int64_t>;
using add_op = taccessor<test_sym::add, ttree, ttree>;
using sub_op = taccessor<test_sym::sub, ttree, ttree>;


static std::vector<TestPair> test_cases1 = {
{"stree_builderの簡単なテスト", []{
  auto expect = R"(<enum:add>
  <enum:var>
    <string:x>
    </string:x>
  </enum:var>
  <enum:sub>
    <enum:int>
      <int:7>
      </int:7>
    </enum:int>
    <enum:int>
      <int:4>
      </int:4>
    </enum:int>
  </enum:sub>
</enum:add>
)";

  ttree_builder builder;

  builder.create_root(test_sym::add);
  {
    auto with_guard = builder.append_with(test_sym::variable);
    builder.append("x");
  }
  {
    auto with_guard = builder.append_with(test_sym::sub);
    {
      auto with2 = builder.append_with(test_sym::int_imm);
      builder.append(7);
    }
    {
      auto with2 = builder.append_with(test_sym::int_imm);
      builder.append(4);
    }
  }

  auto actual = ttree_dump(*builder._root);
  // check_string_equals( expect, actual );
  // cout << actual << endl;
  REQUIRE( expect == actual );
}},
{"accessorのテスト", []{
  ttree_builder builder;

  builder.create_root(test_sym::add);
  {
    auto with_guard = builder.append_with(test_sym::int_imm);
    builder.append(3);
  }
  {
    auto with_guard = builder.append_with(test_sym::sub);
    {
      auto with2 = builder.append_with(test_sym::int_imm);
      builder.append(7);
    }
    {
      auto with2 = builder.append_with(test_sym::int_imm);
      builder.append(4);
    }
  }

  auto root = builder._root;
  add_op op1(*root);
  auto& left = get<0>(op1);
  auto& right = get<1>(op1);
  
  REQUIRE( tatom::enumval == left._data._type);
  REQUIRE( test_sym::int_imm == left._data._value._enumval);
  
  int_imm op2(left);
  auto actual1 = get<0>(op2);
  REQUIRE( actual1 == 3);


  REQUIRE( tatom::enumval == right._data._type);
  REQUIRE( test_sym::sub == right._data._value._enumval);

  sub_op op3(right);
  int_imm op3_left(get<0>(op3));
  int_imm op3_right(get<1>(op3));
  REQUIRE( 7 == get<0>(op3_left) );
  REQUIRE( 4 == get<0>(op3_right) );


}}
};

void register_symtree_test(std::vector<TestPair>& testCases)
{
    tuple<int, std::string> test;
  testCases.insert(testCases.end(), test_cases1.begin(), test_cases1.end());
}