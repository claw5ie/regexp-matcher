#include <cstdio>
#include <cstdlib>
#include <vector>
#include <set>
#include <cstring>
#include <cstdbool>
#include <cstdint>
#include <cassert>

using namespace std;

enum EdgeType
  {
    Edge_Eps = -1,
  };

struct GraphEdge
{
  size_t dst;
  char label;
};

using GraphNode = set<GraphEdge>;
using ENFAclosure = set<size_t>;

struct ENFA
{
  size_t start, end;
  vector<GraphNode> nodes;
  vector<ENFAclosure> closures;
};

struct DFA
{
  vector<GraphNode> states;
  set<size_t> final_states;
};

bool
operator<(GraphEdge e0, GraphEdge e1)
{
  if (e0.label == e1.label)
    return e0.dst < e1.dst;
  else
    return e0.label < e1.label;
}

size_t
push_node(ENFA &nfa)
{
  size_t node_idx = nfa.nodes.size();

  nfa.nodes.push_back({ });

  return node_idx;
}

ENFA
parse_pattern(const char *str)
{
  ENFA nfa;
  nfa.start = nfa.end = 0;
  nfa.nodes.reserve(32);
  nfa.nodes.push_back({ });

  while (*str != '\0')
    {
      char label = *str;

      if (str[1] == '*')
        {
          size_t const subexpr_start = nfa.end;
          size_t subexpr_end = push_node(nfa);
          size_t end = push_node(nfa);

          nfa.nodes[subexpr_start].insert({ subexpr_end, label });
          nfa.nodes[subexpr_end].insert({ subexpr_start, Edge_Eps });
          nfa.nodes[subexpr_start].insert({ end, Edge_Eps });
          nfa.nodes[subexpr_end].insert({ end, Edge_Eps });
          nfa.end = end;

          while (*++str == '*')
            ;
        }
      else
        {
          size_t end = push_node(nfa);

          nfa.nodes[nfa.end].insert({ end, label });
          nfa.end = end;
          ++str;
        }
    }

  nfa.closures.resize(nfa.nodes.size());

  return nfa;
}

void
compute_closures_aux(ENFA &nfa, vector<bool> &visited, size_t node_idx)
{
  visited[node_idx] = true;

  GraphNode &edges = nfa.nodes[node_idx];

  for (auto node: edges)
    if (!visited[node.dst] && node.label == Edge_Eps)
      compute_closures_aux(nfa, visited, node.dst);

  ENFAclosure &closure = nfa.closures[node_idx];
  closure.insert(node_idx);

  for (auto node: edges)
    {
      if (node.label == Edge_Eps)
        {
          auto &set = nfa.closures[node.dst];
          closure.insert(set.begin(), set.end());
        }
    }
}

void
compute_closures(ENFA &nfa)
{
  vector<bool> visited;
  visited.resize(nfa.nodes.size());

  for (size_t i = 0; i < nfa.nodes.size(); i++)
    if (!visited[i])
      compute_closures_aux(nfa, visited, i);
}

void
insert_closure(std::set<size_t> &set, std::set<size_t> &closure)
{
  set.insert(closure.begin(), closure.end());
}

bool
are_sets_equal(set<size_t> &left, set<size_t> &right)
{
  if (left.size() != right.size())
    return false;

  for (auto lit = left.begin(), rit = right.begin();
       lit != left.end();
       lit++, rit++)
    if (*lit != *rit)
      return false;

  return true;
}

size_t
find_repeating_state(vector<set<size_t>> &states)
{
  for (size_t i = 0; i + 1 < states.size(); i++)
    if (are_sets_equal(states[i], states.back()))
      return i;

  return -1;
}

DFA
create_dfa_from_regexp(const char *pattern)
{
  ENFA nfa = parse_pattern(pattern);
  compute_closures(nfa);
  DFA dfa;
  dfa.states.push_back({ });

  vector<set<size_t>> dfa_states;
  dfa_states.push_back({ });
  insert_closure(dfa_states[0], nfa.closures[nfa.start]);

  for (size_t i = 0; i < dfa_states.size(); i++)
    {
      set<char> labels;

      for (size_t node_idx: dfa_states[i])
        for (auto node: nfa.nodes[node_idx])
          if (node.label != Edge_Eps)
            labels.insert(node.label);

      for (char label: labels)
        {
          dfa_states.push_back({ });

          for (size_t node_idx: dfa_states[i])
            {
              auto &set = nfa.nodes[node_idx];

              for (auto it = set.lower_bound({ 0, label });
                   it != set.end() && it->label == label;
                   it++)
                insert_closure(dfa_states.back(), nfa.closures[it->dst]);
            }

          size_t repeating_state
            = find_repeating_state(dfa_states);

          if (repeating_state != (size_t)-1)
            {
              dfa_states.pop_back();
              dfa.states[i].insert({ repeating_state, label });
            }
          else
            {
              dfa.states.push_back({ });
              dfa.states[i].insert({ dfa.states.size() - 1, label });
            }
        }

      for (size_t node_idx: dfa_states[i])
        {
          if (node_idx == nfa.end)
            {
              dfa.final_states.insert(i);
              break;
            }
        }
    }

  return dfa;
}

int
main(void)
{
  DFA dfa = create_dfa_from_regexp("a*");

  for (size_t i = 0; i < dfa.states.size(); i++)
    for (auto edge: dfa.states[i])
      printf("%zu - %c -> %zu\n", i, edge.label, edge.dst);
}
