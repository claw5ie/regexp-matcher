#include "main.hpp"
#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdbool>
#include <cstdint>
#include <cassert>

using namespace std;

bool
operator<(GraphEdge e0, GraphEdge e1)
{
  if (e0.label == e1.label)
    return e0.dst < e1.dst;
  else
    return e0.label < e1.label;
}

size_t
push_state(ENFA &nfa)
{
  size_t state_idx = nfa.states.size();

  nfa.states.push_back({ });

  return state_idx;
}

void
compute_closures_aux(ENFA &nfa, vector<bool> &visited, size_t state_idx)
{
  visited[state_idx] = true;

  GraphState &edges = nfa.states[state_idx];

  for (auto state: edges)
    if (!visited[state.dst] && state.label == Edge_Eps)
      compute_closures_aux(nfa, visited, state.dst);

  ENFAclosure &closure = nfa.closures[state_idx];
  closure.insert(state_idx);

  for (auto state: edges)
    {
      if (state.label == Edge_Eps)
        {
          auto &set = nfa.closures[state.dst];
          closure.insert(set.begin(), set.end());
        }
    }
}

void
compute_closures(ENFA &nfa)
{
  nfa.closures.resize(nfa.states.size());
  vector<bool> visited;
  visited.resize(nfa.states.size());

  for (size_t i = 0; i < nfa.states.size(); i++)
    if (!visited[i])
      compute_closures_aux(nfa, visited, i);
}

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
      left.start = left.end = push_state(nfa);

      while (*at != '\0' && *at != '|' && *at != ')')
        {
          char label = *at;

          if (at[1] == '*')
            {
              size_t const subexpr_start = left.end;
              size_t subexpr_end = push_state(nfa);
              size_t end = push_state(nfa);

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
              size_t end = push_state(nfa);

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

      size_t start = push_state(nfa);
      size_t end = push_state(nfa);

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
  compute_closures(nfa);

  return nfa;
}

void
insert_closure(std::set<size_t> &set, const std::set<size_t> &closure)
{
  set.insert(closure.begin(), closure.end());
}

size_t
find_repeating_state(const vector<set<size_t>> &states)
{
  for (size_t i = 0; i + 1 < states.size(); i++)
    if (states[i] == states.back())
      return i;

  return -1;
}

DFA
convert_enfa_to_dfa(const ENFA &nfa, const set<size_t> &initial_states)
{
  DFA dfa;
  dfa.states.push_back({ });
  vector<set<size_t>> dfa_states;
  dfa_states.push_back({ });

  for (size_t state_idx: initial_states)
    insert_closure(dfa_states[0], nfa.closures[state_idx]);

  for (size_t i = 0; i < dfa_states.size(); i++)
    {
      set<char> labels;

      for (size_t state_idx: dfa_states[i])
        for (auto state: nfa.states[state_idx])
          if (state.label != Edge_Eps)
            labels.insert(state.label);

      for (char label: labels)
        {
          dfa_states.push_back({ });

          for (size_t state_idx: dfa_states[i])
            {
              auto &set = nfa.states[state_idx];

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

      for (size_t state_idx: dfa_states[i])
        {
          if (state_idx == nfa.end)
            {
              dfa.final_states.insert(i);
              break;
            }
        }
    }

  return dfa;
}

DFA
create_dfa_from_regexp(const char *pattern)
{
  ENFA nfa = parse_pattern(pattern);
  DFA dfa = convert_enfa_to_dfa(nfa, { nfa.start });

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
