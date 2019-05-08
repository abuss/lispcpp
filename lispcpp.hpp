// A simple Scheme Interpreter in 90 lines of C++
// based on the Peter Norvig's Lis.py.

// Created by Antal Buss (2019).

#pragma once

#include <string>
#include <vector>
#include <list>
#include <map>
#include <variant>
#include <regex>
#include <functional>

#include <iostream>

using Program = std::string;
using Tokens = std::list<std::string>;

struct Expression;
struct Environment;

using Symbol = std::string;
using Number = double;
using Atom = std::variant<Symbol, Number>;
using Func = std::function<Expression(Expression)>;


enum class ET { Symbol, Number, Func, Lambda, List, None };


struct Expression
{
  std::deque<Expression> elems;
  Atom atom;
  Func func;
  std::shared_ptr<Environment> env; // = nullptr;

  ET exp_type;

  Expression(ET et=ET::None) : exp_type{et} { }
  Expression(Atom atm, ET et=ET::Number) : atom{atm}, exp_type{et} { }
  Expression(Func fn, ET et=ET::Symbol) : func{fn}, exp_type{et} { }

  void append(Expression expr) { elems.push_back(expr); }
  void set_env(std::shared_ptr<Environment> e) { env = e; }

  bool is_list() const { return exp_type==ET::List; }
  bool is_symbol() const {
    if (auto s = std::get_if<Symbol>(&atom))
      return s->size()>0;
    return false;
  }
  bool is_number() const {
    return std::get_if<Number>(&atom);
  }
  Symbol get_symbol() const { return std::get<Symbol>(atom); }
  Number get_number() const { return std::get<Number>(atom); }
  Expression get_first() const { 
    if (elems.empty()) 
      return Expression(); 
    return elems[0]; }
  bool elems_size() const { return elems.size(); }
};


using VarMap = std::map<std::string, Expression>;

struct Environment
{
  VarMap vars;
  std::shared_ptr<Environment> global = nullptr;

  Environment(const Environment&) = default;
  Environment(Environment* other= nullptr) : global{other} { }
  Environment(Expression parms, Expression args, std::shared_ptr<Environment> e)
  {
    for(size_t i=0;i<parms.elems.size();++i) {
      vars.insert({parms.elems[i].get_symbol(),args.elems[i]});
    }
    global = e;
  }

  Environment(Environment& e) : vars{e.vars}, global{e.global} { }

  auto contains(Symbol s) const
  { return vars.find(s)!=vars.end();}

  void update(std::initializer_list<VarMap::value_type> vals)
  {
    for(auto kv: vals)
      vars.insert(kv);
  }

  void update(VarMap::value_type kv) { vars.insert(kv); }

  void update(std::string k, Expression v) { vars[k] = v; }

  Expression& operator[](Symbol s)
  {
    if (contains(s))
      return vars[s];
    else if (global!=nullptr)
      return (*global)[s];
    throw std::invalid_argument("Symbol '"+s+"' not defined");
  }
};


Expression eval(Expression x, std::shared_ptr<Environment> e);
Program lispstr(const Expression& e);

template<typename F>
Func fn_expr(F fn) {
  return [fn](Expression expr) -> Expression {
      auto res = expr.elems[0].get_number();
      for (size_t i=1;i<expr.elems.size();++i) {
        auto x = expr.elems[i].get_number();
        res = fn(res,x);
      }
      return Expression(Atom(res),ET::Number);
  };
}

Expression fn_bool(bool val) {
  return (val) ? Expression(Atom("#t"),ET::Symbol) : Expression(Atom("#f"),ET::Symbol);
}

template<typename F>
Func fn_logic(F fn) {
  return [fn](Expression expr) -> Expression {
      auto x = expr.elems[0];
      auto y = expr.elems[1];
      return fn_bool(fn(x.get_number(),y.get_number()));
  };
}

std::shared_ptr<Environment> standard_env()
{
  using Expr = Expression;
  auto env = std::make_shared<Environment>();
  env->update({
      {"+",      fn_expr([](auto x, auto y) { return x+y;}) },
      {"-",      fn_expr([](auto x, auto y) { return x-y;}) },
      {"*",      fn_expr([](auto x, auto y) { return x*y;}) },
      {"/",      fn_expr([](auto x, auto y) { return x/y;}) },
      {"=",      fn_logic([](auto x, auto y) { return x==y;}) },
      {"<",      fn_logic([](auto x, auto y) { return x<y;}) },
      {">",      fn_logic([](auto x, auto y) { return x>y;}) },
      {"<=",     fn_logic([](auto x, auto y) { return x<=y;}) },
      {">=",     fn_logic([](auto x, auto y) { return x>=y;}) },
      {"abs",    Expr([](auto x){ return Atom(abs(x.elems[0].get_number())); }, ET::Func) },
      {"not",    Expr([](auto x){ return fn_bool(x.elems[0].get_symbol()=="#f"); }, ET::Func) },
      {"list",   Expr([](auto x){ x.exp_type=ET::List; return x; }, ET::Func) },
      {"car",    Expr([](auto x){ return x.elems[0].get_first(); }, ET::Func) },
      {"cdr",    Expr([](auto x){ x.elems[0].elems.pop_front(); return x.get_first(); }, ET::Func) },
      {"cons",   Expr([](auto x){ x.elems[1].elems.push_front(x.elems[0]); return x.elems[1]; },ET::Func) },
      {"length", Expr([](auto x){ return Atom(x.elems[0].elems.size()); },ET::Func) },
      {"list?",  Expr([](auto x){ return fn_bool(x.elems[0].is_list()); }, ET::Func) },
      {"begin",  Expr([](auto x){ return x.elems[x.elems.size()-1]; }) },
      {"append", Expr([](auto x) { for(auto v: x.elems[1].elems) x.elems[0].elems.push_back(v); return x.elems[0];}) },
      {"null?",  Expr([](auto x){ return fn_bool(x.elems[0].elems.empty()); }, ET::Func) },
      {"nil",    Expr(ET::List) },
  });
  return env;
}

// Parsing

// Convert a string into a list of tokens.
template<typename Expr>
auto tokenize(Expr s)
{
  auto res = std::regex_replace(s, std::regex("[(]"), " ( ");
  res = std::regex_replace(res, std::regex("[)]"), " ) ");
  std::list<std::string> matches;
  std::regex e("[^ ]+");
  for(auto it = std::sregex_iterator(res.begin(), res.end(), e); 
           it != std::sregex_iterator(); ++it) {
    matches.push_back(it->str());
  }
  return matches;
}


Expression atom(std::string token)
{
  try {
    Number val = std::stod(token);
    return Expression(Atom(val),ET::Number);
  }
  catch (std::invalid_argument) {
      return Expression(Atom(token),ET::Symbol);
  }
}


// Read an expression from a sequence of tokens.
Expression read_from_tokens(Tokens& tokens)
{
  if (tokens.empty())
    throw std::length_error("unexpected EOF while reading");
  auto token = tokens.front();
  tokens.pop_front();
  if (token == "(") {
    Expression L(ET::Func);
    while (tokens.front() != ")") {
      auto res = read_from_tokens(tokens);
      L.append(res);
    }
    if (L.elems_size()==0)
      L.exp_type = ET::List;
    tokens.pop_front();
    return L;
  } else if (token == ")")
      throw std::invalid_argument("unexpected )");
  return atom(token);
}

// Read a Scheme expression from a string.
auto parse(Program program)
{
  auto res = tokenize(program);
  return read_from_tokens(res);
}

Program lispstr(const Expression& e)
{
  std::ostringstream out;
  if (e.is_list() or e.exp_type==ET::Func) {
    out << "(";
    for (auto v : e.elems)
      out << " " << lispstr(v);
    out << " )";
  }
  else if (e.is_symbol())
    out << e.get_symbol();
  else if (e.is_number())
    out << (double)e.get_number();
  return out.str();
}


// Evaluate an expression in an environment.
Expression eval(Expression x, std::shared_ptr<Environment> e)
{
  if (x.is_symbol())
    return  (*e)[x.get_symbol()];
  else if (x.is_number())
    return x;
  else if (x.is_list() && x.elems_size()==0)
    return x;
  else if (x.get_first().get_symbol()=="quote")
    return x.elems[1];
  else if (x.get_first().get_symbol()=="if") {
    auto [test,then,alt] = std::tuple{x.elems[1],x.elems[2],x.elems[3]};
    if (eval(test,e).get_symbol()=="#t")
      return eval(then,e);
    else
      return eval(alt,e);
  }
  else if (x.get_first().get_symbol()=="define") {
    auto [var,expr] = std::tuple{x.elems[1],x.elems[2]};
    e->update({var.get_symbol(), eval(expr, e)});
    x.set_env(e);
    return Expression();
  }
  else if (x.get_first().get_symbol()=="lambda") {
    x.exp_type = ET::Lambda;
    x.set_env(e);
    return x;
  }
  else {
    auto fun = eval(x.get_first(), e);
    x.elems.pop_front();
    Expression args;
    for (auto v : x.elems)
      args.append(eval(v, e));
    if (fun.exp_type==ET::Lambda) {
      auto [parms,body] = std::tuple{fun.elems[1],fun.elems[2]};
      auto nenv = std::make_shared<Environment>(parms,args,fun.env);
      return eval(body, nenv);
    }
    return fun.func(args);
  }
}


// the default read-eval-print-loop
void repl(const std::string & prompt="lispcpp> ")
{
  auto env = standard_env();
  std::string cmd;
  while (cmd!="quit") {
    std::cout << prompt;
    std::getline(std::cin, cmd);
    try {
      std::cout << lispstr(eval(parse(cmd), env)) << std::endl;
    } catch (std::exception& e) {
       std::cout << e.what() << '\n';
    }
  }
}
