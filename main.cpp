#include <iostream>
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
  vector<GraphNode> states;
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
  size_t node_idx = nfa.states.size();

  nfa.states.push_back({ });

  return node_idx;
}

struct StatePair
{
  size_t start, end;
};

static const char *at;

StatePair
parse_pattern_aux(ENFA &nfa);

StatePair
parse_next_level(ENFA &nfa)
{
  StatePair left;

  if (*at == '(')
    {
      ++at;
      left = parse_pattern_aux(nfa);
      assert(*at == ')');
      ++at;
    }
  else
    {
      left.start = left.end = push_node(nfa);

      while (*at != '\0' && *at != '|' && *at != ')')
        {
          char label = *at;

          if (at[1] == '*')
            {
              size_t const subexpr_start = left.end;
              size_t subexpr_end = push_node(nfa);
              size_t end = push_node(nfa);

              nfa.states[subexpr_start].insert({ subexpr_end, label });
              nfa.states[subexpr_end].insert({ subexpr_start, Edge_Eps });
              nfa.states[subexpr_start].insert({ end, Edge_Eps });
              nfa.states[subexpr_end].insert({ end, Edge_Eps });
              left.end = end;

              while (*++at == '*')
                ;
            }
          else
            {
              size_t end = push_node(nfa);

              nfa.states[left.end].insert({ end, label });
              left.end = end;
              ++at;
            }
        }
    }

  if (*at == '*')
    {
      nfa.states[left.start].insert({ left.end, Edge_Eps });
      nfa.states[left.end].insert({ left.start, Edge_Eps });

      while (*++at == '*')
        ;
    }

  return left;
}

StatePair
parse_pattern_aux(ENFA &nfa)
{
  StatePair left = parse_next_level(nfa);

  while (*at == '|')
    {
      ++at;
      StatePair right = parse_next_level(nfa);

      size_t start = push_node(nfa);
      size_t end = push_node(nfa);

      nfa.states[start].insert({ left.start, Edge_Eps });
      nfa.states[start].insert({ right.start, Edge_Eps });
      nfa.states[left.end].insert({ end, Edge_Eps });
      nfa.states[right.end].insert({ end, Edge_Eps });
      left.start = start;
      left.end = end;
    }

  return left;
}

ENFA
parse_pattern(const char *str)
{
  ENFA nfa;
  nfa.states.reserve(32);

  at = str;
  StatePair regexp = parse_pattern_aux(nfa);
  nfa.start = regexp.start;
  nfa.end = regexp.end;

  nfa.closures.resize(nfa.states.size());

  return nfa;
}

void
compute_closures_aux(ENFA &nfa, vector<bool> &visited, size_t node_idx)
{
  visited[node_idx] = true;

  GraphNode &edges = nfa.states[node_idx];

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
  visited.resize(nfa.states.size());

  for (size_t i = 0; i < nfa.states.size(); i++)
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
        for (auto node: nfa.states[node_idx])
          if (node.label != Edge_Eps)
            labels.insert(node.label);

      for (char label: labels)
        {
          dfa_states.push_back({ });

          for (size_t node_idx: dfa_states[i])
            {
              auto &set = nfa.states[node_idx];

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
main(int argc, char **argv)
{
  DFA dfa = create_dfa_from_regexp(argc <= 1 ? "b|a" : argv[1]);

  cout << "final states: ";
  for (size_t state: dfa.final_states)
    cout << state << ' ';
  cout << '\n' << '\n';

  for (size_t i = 0; i < dfa.states.size(); i++)
    for (auto edge: dfa.states[i])
      printf("%zu - %c -> %zu\n", i, edge.label, edge.dst);

  // Code for ENFA debugging
  // ENFA nfa = parse_pattern(argc <= 1 ? "b|a" : argv[1]);

  // cout << "initial state: "
  //      << nfa.start
  //      << "\nfinal state:   "
  //      << nfa.end
  //      << "\nstate count:   "
  //      << nfa.states.size()
  //      << "\n\n";

  // for (size_t i = 0; i < nfa.states.size(); i++)
  //   for (auto edge: nfa.states[i])
  //     printf("%zu - %c -> %zu\n", i, edge.label == -1 ? 'e' : edge.label, edge.dst);
}
