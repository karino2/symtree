/* -*- coding: utf-8 -*- マルチバイト */

#ifndef _SYMTREE_HPP_
#define _SYMTREE_HPP_

#include "util.hpp"
#include "forest.hpp"
#include <iostream>
#include <string>
#include <sstream>

namespace symtree
{

struct typed_num
{
    enum num_type
    {
        signed_int,
        unsigned_int
    };
    num_type _type;
    uint64_t _value;
    typed_num(num_type ntype, uint64_t val) : _type(ntype), _value(val) {}
    typed_num(const typed_num&) = default;
    typed_num(typed_num&&) = default;
    typed_num &operator=(const typed_num&) = default;
    typed_num &operator=(typed_num&&) = default;
};

template<typename ENUMTYPE>
struct atom
{
    enum class atom_type
    {
        numval,
        enumval,
        stringval
    };

    union value
    {
        ENUMTYPE _enumval;
        typed_num _numval;
        std::string* _stringval;

        value(ENUMTYPE eval) : _enumval(eval) {}
        value(int val) : _numval(typed_num::signed_int, val) {}
        value(unsigned int val) : _numval(typed_num::unsigned_int, val) {}
        value(const std::string& str)
        {
            _stringval = new std::string(str);
        }

        void destroy(atom_type type)
        {
            switch(type)
            {
                case atom_type::enumval:
                case atom_type::numval:
                    return;
                case atom_type::stringval:
                    delete _stringval;
            }
        }
    };

    atom_type _type;
    value _value;

    atom(ENUMTYPE eval) : _type(atom_type::enumval), _value(eval) {}
    atom(int val) : _type(atom_type::numval), _value(val) {}
    atom(unsigned int val) : _type(atom_type::numval), _value(val) {}
    atom(const std::string& str) : _type(atom_type::stringval), _value(str) {} 

    atom(atom<ENUMTYPE>&& other) : _type(other._type), _value(other._value)
    {
        other._type = atom_type::numval;
        other._value._numval = typed_num(typed_num::signed_int, 0);
    }

    ~atom()
    {
        _value.destroy(_type);
    }

    /*
        デバッグ用。
        ETOSは std::string enum_to_str(ENUMTYPE e)をstatic methodに持つstruct
    */
    template<typename ETOS>
    std::string display_string()
    {
        switch(_type)
        {
            case atom_type::enumval:
                return "enum:" + ETOS::enum_to_str(_value._enumval);
            case atom_type::numval:
            {
                auto value = std::to_string(_value._numval._value);
                switch(_value._numval._type)
                {
                    case typed_num::signed_int:
                        return "int:" + value;
                    case typed_num::unsigned_int:
                        return "uint:" + value;
                }
            }
            case atom_type::stringval:
                return "string:" + *_value._stringval;
        }
    }


};

template<typename ENUMTYPE>
using stree = forest<atom<ENUMTYPE>>;

inline void indent( std::stringstream &out, int level )
{
  for (auto i: irange( level ))
  {
    UNUSED( i );
    out << "  ";
  }
}


/*
  ETOSは std::string enum_to_str(ENUMTYPE e)をstatic methodに持つstruct
*/
template<typename E, typename ETOS>
std::string stree_dump(stree<E>& root)
{
    std::stringstream buf;
    int level = 0;
    for (auto& edge : root)
    {
        if ( edge._direction == edge_dir::leading )
        {
            indent(buf, level);
            level++;
            std::string str = (*edge).template display_string<ETOS>();
            buf << "<" << str << ">" << std::endl;
        }
        else
        {
            level--;
            indent(buf, level);
            std::string str = (*edge).template display_string<ETOS>();
            buf << "</" << str << ">" << std::endl;
        }
    }
  return buf.str();

}


template<typename ENUMTYPE> struct stree_builder;

template<typename ENUMTYPE>
struct scoped_iterator
{
    using iterator = typename stree<ENUMTYPE>::iterator;

    stree_builder<ENUMTYPE> *tree;
    iterator iter;

    scoped_iterator(stree_builder<ENUMTYPE>* tr, const iterator& itr) : tree(tr), iter(itr) {}
    ~scoped_iterator();

};


template<typename ENUMTYPE>
struct stree_builder
{
    using stree_ = stree<ENUMTYPE>;
    using siterator_ = typename stree_::iterator;
    using atom_ = atom<ENUMTYPE>;

    stree_ *_root;
    siterator_ _iter;

    stree_builder() : _root(nullptr), _iter(nullptr, edge_dir::trailing) {}
    ~stree_builder()
    {
        if (_root != nullptr)
        {
            delete _root;
        }
    }

    void create_root_by_atom(atom_&& atm)
    {
        assert(_root == nullptr);
        _root = new stree_(std::move(atm));
        _iter = _root->begin().trailing_of();
    }

    template<typename T>
    void create_root(T value)
    {
        create_root_by_atom(atom_(value));
    }

    siterator_ append_atom(atom_&& atm)
    {
        assert(_root != nullptr);
        return _iter.insert(std::move(atm));
    }

    template<typename T>
    siterator_ append(T value)
    {
        return append_atom(atom_(value));
    }

    template<typename T>
    siterator_ append_and_down(T value)
    {
        auto iter = append_atom(atom_(value));
        _iter = iter.trailing_of();
        return _iter;
    }

    void go_up()
    {
        // いつもparentには兄弟はいない前提。
        // appendなどのAPIだけでツリーを作っていればその前提は正しい。
        // その前提が正しくないような事をやっている人は_iterを直接いじれば良い。
        _iter++;
    }

    /*
        子供を追加し、そこにiteratorを移動する。
        returnしたscoped_iteratorがdestructされる時にgo_upする。
    */
    template<typename T>
    scoped_iterator<ENUMTYPE> append_with(T value)
    {
        auto iter = append_and_down(value);
        return scoped_iterator<ENUMTYPE>(this, iter);
    }
};


template<typename ENUMTYPE>
scoped_iterator<ENUMTYPE>::~scoped_iterator()
{
    tree->go_up();
}


}
#endif