#include <iostream>
#include <iomanip>
#include <vector>
#include <set>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdbool>
#include <cstdint>
#include <cassert>

#include "main.hpp"

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
insert_closure(std::set<size_t> &set,
               const std::set<size_t> &closure)
{
  set.insert(closure.begin(), closure.end());
}

void
compute_closures(ENFA &nfa)
{
  nfa.closures.resize(nfa.states.size());
  for (size_t i = 0; i < nfa.states.size(); i++)
    nfa.closures[i].insert(i);

  bool was_changed = false;

  // Perform fixed point iteration method: insert closures
  // until all sets are stabilized.
  do
    {
      was_changed = false;

      for (size_t i = 0; i < nfa.states.size(); i++)
        {
          auto &state = nfa.states[i];
          for (auto it = state.lower_bound({ 0, Edge_Eps });
               it != state.end() && it->label == Edge_Eps;
               it++)
            {
              auto &closure = nfa.closures[i];
              size_t old_count = closure.size();
              insert_closure(closure, nfa.closures[it->dst]);
              if (old_count != closure.size())
                was_changed = true;
            }
        }
    }
  while (was_changed);
}

static const char *at;

StatePair
parse_option(ENFA &nfa);

StatePair
parse_highest_level(ENFA &nfa)
{
  StatePair left;

  if (*at == '(')
    {
      ++at;
      left = parse_option(nfa);
      if (*at != ')')
        {
          cerr << "error: expected closing parenthesis at \'"
               << at
               << "\'.\n";
          exit(EXIT_FAILURE);
        }
      ++at;
    }
  else if (*at != '\0' && *at != ')' && *at != '|' && *at != '*')
    {
      at += (*at == '\\');
      left.start = push_state(nfa);
      left.end = push_state(nfa);
      nfa.states[left.start].insert({ left.end, *at });
      ++at;
    }
  else
    {
      cerr << "error: invalid expression starting at \'"
           << at
           << "\'.\n";
      exit(EXIT_FAILURE);
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
parse_concat(ENFA &nfa)
{
  StatePair left = parse_highest_level(nfa);

  while (*at != '\0' && *at != '|' && *at != ')')
    {
      StatePair right = parse_highest_level(nfa);
      nfa.states[left.end].insert({ right.start, Edge_Eps });
      left.end = right.end;
    }

  return left;
}

StatePair
parse_option(ENFA &nfa)
{
  StatePair left = parse_concat(nfa);

  while (*at == '|')
    {
      ++at;
      StatePair right = parse_concat(nfa);

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

StatePair
parse_pattern(ENFA &nfa, const char *str)
{
  StatePair regexp;

  if (*str != '\0')
    regexp = parse_option(nfa);
  else
    regexp.start = regexp.end = push_state(nfa);

  return regexp;
}

ENFA
parse_pattern(const char *str)
{
  ENFA nfa;
  nfa.states.reserve(32);

  at = str;
  StatePair regexp = parse_pattern(nfa, str);
  assert(*at == '\0');
  nfa.start = regexp.start;
  nfa.end = regexp.end;
  compute_closures(nfa);

  return nfa;
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
convert_enfa_to_dfa(const ENFA &nfa, size_t initial_state)
{
  DFA dfa;
  dfa.states.push_back({ });
  vector<set<size_t>> dfa_states;
  dfa_states.push_back({ });

  insert_closure(dfa_states[0], nfa.closures[initial_state]);

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
convert_enfa_to_dfa(ENFA &nfa)
{
  return convert_enfa_to_dfa(nfa, nfa.start);
}

bool
match(DFA &dfa, const char *string)
{
  size_t current = 0;

  for (; *string != '\0'; string++)
    {
      auto it = dfa.states[current].lower_bound({ 0, *string });
      if (it == dfa.states[current].end() || it->label != *string)
        return false;

      current = it->dst;
    }

  return dfa.final_states.find(current) != dfa.final_states.end();
}

const char *
label_to_cstring(char label)
{
  static char buffer[4];

  if (label != Edge_Eps)
    {
      buffer[0] = label;
      buffer[1] = '\0';
    }
  else
    {
      buffer[0] = 'e';
      buffer[1] = 'p';
      buffer[2] = 's';
      buffer[3] = '\0';
    }

  return buffer;
}

int
main(int argc, char **argv)
{
  const char *regex_string = argc <= 1 ? "b|a" : argv[1];
  ENFA nfa = parse_pattern(regex_string);
  DFA dfa = convert_enfa_to_dfa(nfa);

  if (argc >= 3)
    {
      argv += 2;
      argc -= 2;

      std::vector<bool> results;
      results.resize(argc);

      size_t max_len = 0;
      for (int i = 0; i < argc; i++)
        {
          results[i] = match(dfa, argv[i]);

          size_t len = strlen(argv[i]);
          if (max_len < len)
            max_len = len;
        }

      for (size_t i = 0; i < results.size(); i++)
        cout << left << setw(max_len) << argv[i] << ": "
             << (results[i] ? "accepted" : "rejected")
             << "\n";

      cout << '\n';
    }

  cout << "Regex: " << regex_string << '\n';
  cout << "DFA:\n    initial state: 0\n    final states:  ";
  for (size_t state: dfa.final_states)
    cout << state << ' ';
  cout << "\n\n";

  for (size_t i = 0; i < dfa.states.size(); i++)
    for (auto edge: dfa.states[i])
      printf("    %-2zu - %c -> %zu\n", i, edge.label, edge.dst);

  cout << "ENFA:\n    initial state: "
       << nfa.start
       << "\n    final state:   "
       << nfa.end
       << "\n    state count:   "
       << nfa.states.size()
       << "\n\n";

  for (size_t i = 0; i < nfa.states.size(); i++)
    for (auto edge: nfa.states[i])
      printf("    %-2zu - %-3s -> %zu\n", i, label_to_cstring(edge.label), edge.dst);
}
