#ifndef MAIN_HPP
#define MAIN_HPP

enum EdgeType
{
  Edge_Eps = -1,
};

struct GraphEdge
{
  size_t dst;
  char label;
};

using GraphState = std::set<GraphEdge>;
using ENFAclosure = std::set<size_t>;

struct ENFA
{
  size_t start, end;
  std::vector<GraphState> states;
  std::vector<ENFAclosure> closures;
};

struct DFA
{
  std::vector<GraphState> states;
  std::set<size_t> final_states;
};

struct StatePair
{
  size_t start, end;
};

#endif // MAIN_HPP
