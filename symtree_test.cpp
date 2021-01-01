
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
  add,
  let
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
      case test_sym::let:
        return "let";
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

using var_op = taccessor<test_sym::variable, string>;
using int_imm = taccessor<test_sym::int_imm, int64_t>;
using add_op = taccessor<test_sym::add, ttree, ttree>;
using sub_op = taccessor<test_sym::sub, ttree, ttree>;

// var_op=tree1 in ttree2.
using let_op = taccessor<test_sym::let, var_op, ttree, ttree>;

struct expr
{
    ttree& _node;
    test_sym _type;
    expr(ttree& node) : _node(node), _type(_node._data._value._enumval)
    {
        assert(node._data._type == tatom::enumval);    
    }

    template<typename OP, typename FN>
    int eval_binop(FN fn)
    {
        OP op(_node);
        expr left(get<0>(op));
        expr right(get<1>(op));
        return fn(left.eval(), right.eval());

    }

    int eval()
    {
        switch(_type)
        {
            case test_sym::int_imm:
            {
                int_imm op(_node);
                return get<0>(op);
            }
            case test_sym::add:
                return eval_binop<add_op>( [](int a, int b) { return a+b; } );

            case test_sym::sub:
                return eval_binop<sub_op>( [](int a, int b) { return a-b; } );

            default:
                assert(false);
                throw std::runtime_error("never reached here");
        }
    }
};

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

  expr root_expr(*root);
  REQUIRE( 6 == root_expr.eval() );

}},
{"let accessorのテスト", []{
  ttree_builder builder;

  // let x = 3+4 in x+5
  builder.create_root(test_sym::let);
  {
    auto with_guard = builder.append_with(test_sym::variable);
    builder.append("x");
  }
  // value
  {
    auto with_guard = builder.append_with(test_sym::add);
    {
      auto with2 = builder.append_with(test_sym::int_imm);
      builder.append(3);
    }
    {
      auto with2 = builder.append_with(test_sym::int_imm);
      builder.append(4);
    }
  }
  // body
  {
    auto with_guard = builder.append_with(test_sym::add);
    {
      auto with2 = builder.append_with(test_sym::variable);
      builder.append("x");
    }
    {
      auto with2 = builder.append_with(test_sym::int_imm);
      builder.append(5);
    }
  }

  auto root = builder._root;
  let_op op1(*root);
  auto v = get<0>(op1);
  auto& value = get<1>(op1);
  auto& body = get<2>(op1);

  REQUIRE( "x" == get<0>(v) );


}}
};

void register_symtree_test(std::vector<TestPair>& testCases)
{
    tuple<int, std::string> test;
  testCases.insert(testCases.end(), test_cases1.begin(), test_cases1.end());
}