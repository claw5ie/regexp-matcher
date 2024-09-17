#include <iostream>
#include <iomanip>
#include <vector>
#include <set>
#include <limits>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdbool>
#include <cstdint>
#include <cassert>

#include "main.hpp"

StateIndex push_state(ENFA &nfa)
{
  auto state_idx = nfa.states.size();
  nfa.states.push_back({ });
  return state_idx;
}

void insert_closure(StateSubset &set, const StateSubset &closure)
{
  set.insert(closure.begin(), closure.end());
}

void compute_closures(ENFA &nfa)
{
  nfa.closures.resize(nfa.states.size());
  // Closure of a set contains the set itself.
  for (StateIndex idx = 0; idx < nfa.states.size(); idx++)
    nfa.closures[idx].insert(idx);

  auto was_changed = false;

  // Perform fixed point iteration method: insert closures until all sets are stabilized.
  do
  {
    was_changed = false;

    for (StateIndex idx = 0; idx < nfa.states.size(); idx++)
    {
      auto &state = nfa.states[idx];
      for (auto it = state.lower_bound({ .dst = lowest_state_index, .label = Edge_Eps });
           it != state.end() && it->label == Edge_Eps;
           it++)
      {
        auto &closure = nfa.closures[idx];
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

StatePair parse_option(ENFA &nfa);

StatePair parse_highest_level(ENFA &nfa)
{
  StatePair left;

  if (*at == '(')
  {
    ++at;
    left = parse_option(nfa);
    if (*at != ')')
    {
      std::cerr << "error: expected closing parenthesis at \'"
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
    nfa.states[left.start].insert({ .dst = left.end, .label = *at });
    ++at;
  }
  else
  {
    std::cerr << "error: invalid expression starting at \'"
              << at
              << "\'.\n";
    exit(EXIT_FAILURE);
  }

  do
  {
    switch (*at)
    {
    case '*':
    {
      ++at;

      auto new_start = push_state(nfa);
      auto new_end = push_state(nfa);
      nfa.states[new_start].insert({ .dst = left.start, .label = Edge_Eps });
      nfa.states[new_start].insert({ .dst = new_end,    .label = Edge_Eps });
      nfa.states[left.end ].insert({ .dst = new_end,    .label = Edge_Eps });
      nfa.states[new_end  ].insert({ .dst = new_start,  .label = Edge_Eps });

      left.start = new_start;
      left.end = new_end;
    } break;
    case '?':
    {
      ++at;

      auto new_start = push_state(nfa);
      auto new_end = push_state(nfa);
      nfa.states[new_start].insert({ .dst = left.start, .label = Edge_Eps });
      nfa.states[new_start].insert({ .dst = new_end,    .label = Edge_Eps });
      nfa.states[left.end ].insert({ .dst = new_end,    .label = Edge_Eps });

      left.start = new_start;
      left.end = new_end;
    } break;
    case '+': // Maybe should replace "r+?" by "r*"?
    {
      ++at;

      auto new_start = push_state(nfa);
      auto new_end = push_state(nfa);
      nfa.states[new_start].insert({ .dst = left.start, .label = Edge_Eps });
      nfa.states[left.end ].insert({ .dst = new_end,    .label = Edge_Eps });
      nfa.states[new_end  ].insert({ .dst = new_start,  .label = Edge_Eps });

      left.start = new_start;
      left.end = new_end;
    }
    default:
      goto finish_parsing_postfix_unary_ops;
    }
  } while (true);
finish_parsing_postfix_unary_ops: {}

  return left;
}

StatePair parse_concat(ENFA &nfa)
{
  auto left = parse_highest_level(nfa);

  while (*at != '\0' && *at != '|' && *at != ')')
  {
    auto right = parse_highest_level(nfa);
    nfa.states[left.end].insert({ .dst = right.start, .label = Edge_Eps });
    left.end = right.end;
  }

  return left;
}

StatePair parse_option(ENFA &nfa)
{
  auto left = parse_concat(nfa);

  while (*at == '|')
  {
    ++at;

    auto right = parse_concat(nfa);

    auto start = push_state(nfa);
    auto end = push_state(nfa);

    nfa.states[start    ].insert({ .dst = left.start,  .label = Edge_Eps });
    nfa.states[start    ].insert({ .dst = right.start, .label = Edge_Eps });
    nfa.states[left.end ].insert({ .dst = end,         .label = Edge_Eps });
    nfa.states[right.end].insert({ .dst = end,         .label = Edge_Eps });
    left.start = start;
    left.end = end;
  }

  return left;
}

StatePair parse_pattern(ENFA &nfa, const char *str)
{
  StatePair regexp;

  if (*str != '\0')
    regexp = parse_option(nfa);
  else
    regexp.start = regexp.end = push_state(nfa);

  return regexp;
}

ENFA parse_pattern(const char *str)
{
  at = str;

  ENFA nfa;
  nfa.states.reserve(32);
  auto regexp = parse_pattern(nfa, str);
  assert(*at == '\0');
  nfa.start = regexp.start;
  nfa.end = regexp.end;

  return nfa;
}

DFA convert_enfa_to_dfa(ENFA &nfa, StateIndex initial_state)
{
  using Subsets = std::vector<StateSubset>;

  DFA dfa;
  Subsets subsets; // Subsets of NFA states corresponding to each DFA states.

  compute_closures(nfa);

  dfa.states.push_back({ });
  subsets.push_back({ });

  insert_closure(subsets[0], nfa.closures[initial_state]);

  auto find_repeating_state =
    [&subsets]() -> StateIndex
    {
      for (StateIndex idx = 0; idx + 1 < subsets.size(); idx++)
        if (subsets[idx] == subsets.back())
          return idx;

      return -1;
    };

  for (StateIndex idx = 0; idx < subsets.size(); idx++)
  {
    // All possible transitions from current DFA state.
    std::set<char> labels;

    for (auto state_idx: subsets[idx])
      for (auto state: nfa.states[state_idx])
        if (state.label != Edge_Eps)
          labels.insert(state.label);

    for (auto label: labels)
    {
      subsets.push_back({ });

      for (auto state_idx: subsets[idx])
      {
        auto &set = nfa.states[state_idx];

        for (auto it = set.lower_bound({ .dst = lowest_state_index, .label = label });
             it != set.end() && it->label == label;
             it++)
          insert_closure(subsets.back(), nfa.closures[it->dst]);
      }

      auto repeating_state = find_repeating_state();

      if (repeating_state != (size_t)-1)
      {
        subsets.pop_back();
        dfa.states[idx].insert({ .dst = repeating_state, .label = label });
      }
      else
      {
        dfa.states.push_back({ });
        dfa.states[idx].insert({ .dst = dfa.states.size() - 1, .label = label });
      }
    }

    for (auto state_idx: subsets[idx])
    {
      if (state_idx == nfa.end)
      {
        dfa.final_states.insert(idx);
        break;
      }
    }
  }

  return dfa;
}

DFA convert_enfa_to_dfa(ENFA &nfa)
{
  return convert_enfa_to_dfa(nfa, nfa.start);
}

bool match(DFA &dfa, const char *string)
{
  size_t current = 0;

  for (; *string != '\0'; string++)
  {
    auto it = dfa.states[current].lower_bound({ .dst = lowest_state_index, .label = *string });
    if (it == dfa.states[current].end() || it->label != *string)
      return false;

    current = it->dst;
  }

  return dfa.final_states.find(current) != dfa.final_states.end();
}

const char *label_to_cstring(char label)
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

int main(int argc, char **argv)
{
  const char *regex_string = argc <= 1 ? "b|a" : argv[1];
  auto nfa = parse_pattern(regex_string);
  auto dfa = convert_enfa_to_dfa(nfa);

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
      std::cout << std::left << std::setw(max_len) << argv[i] << ": "
                << (results[i] ? "accepted" : "rejected")
                << "\n";

    std::cout << '\n';
  }

  std::cout << "Regex: " << regex_string << '\n';
  std::cout << "DFA:\n    initial state: 0"
    "\n    final states:  ";
  for (auto state: dfa.final_states)
    std::cout << state << ' ';
  std::cout << "\n    state count:   "
            << dfa.states.size()
            << "\n\n";

  for (StateIndex idx = 0; idx < dfa.states.size(); idx++)
    for (auto edge: dfa.states[idx])
      printf("    %-2zu - %c -> %zu\n", idx, edge.label, edge.dst);

  std::cout << "ENFA:\n    initial state: "
            << nfa.start
            << "\n    final state:   "
            << nfa.end
            << "\n    state count:   "
            << nfa.states.size()
            << "\n\n";

  for (StateIndex idx = 0; idx < nfa.states.size(); idx++)
    for (auto edge: nfa.states[idx])
      printf("    %-2zu - %-3s -> %zu\n", idx, label_to_cstring(edge.label), edge.dst);
}
