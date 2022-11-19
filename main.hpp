#ifndef MAIN_HPP
#define MAIN_HPP

#include <vector>
#include <set>
#include <cstddef>

enum EdgeType
  {
    Edge_Eps = -1,
  };

struct GraphEdge
{
  size_t dst;
  char label;
};

using GraphNode = std::set<GraphEdge>;
using ENFAclosure = std::set<size_t>;

struct ENFA
{
  size_t start, end;
  std::vector<GraphNode> states;
  std::vector<ENFAclosure> closures;
};

struct DFA
{
  std::vector<GraphNode> states;
  std::set<size_t> final_states;
};

struct StatePair
{
  size_t start, end;
};

#endif // MAIN_HPP
