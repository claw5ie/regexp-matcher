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
    Edge_Eps = -2,
    Edge_Any = -1
  };

struct ENFAedge
{
  size_t dst;
  char label;
};

using ENFAnode = set<ENFAedge>;
using ENFAclosure = set<size_t>;

struct ENFA
{
  size_t start, end;
  vector<ENFAnode> nodes;
  vector<ENFAclosure> closures;
};

bool
operator<(ENFAedge e0, ENFAedge e1)
{
  if (e0.label == e1.label)
    return e0.dst < e1.dst;
  else
    return e0.label < e1.label;
}

size_t
push_node(ENFA *nfa)
{
  size_t node_idx = nfa->nodes.size();

  nfa->nodes.push_back({ });

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
      char label = *str == '?' ? (char)Edge_Any : *str;

      if (str[1] == '*')
        {
          size_t const subexpr_start = nfa.end;
          size_t subexpr_end = push_node(&nfa);
          size_t end = push_node(&nfa);

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
          size_t end = push_node(&nfa);

          nfa.nodes[nfa.end].insert({ end, label });
          nfa.end = end;
          ++str;
        }
    }

  nfa.closures.resize(nfa.nodes.size());

  return nfa;
}

void
compute_closures_aux(ENFA *nfa, vector<bool> &visited, size_t node_idx)
{
  visited[node_idx] = true;

  ENFAnode *edges = &nfa->nodes[node_idx];

  for (auto node: *edges)
    if (!visited[node.dst] && node.label == Edge_Eps)
      compute_closures_aux(nfa, visited, node.dst);

  ENFAclosure *closure = &nfa->closures[node_idx];
  closure->insert(node_idx);

  for (auto node: *edges)
    {
      if (node.label == Edge_Eps)
        {
          auto *set = &nfa->closures[node.dst];
          closure->insert(set->begin(), set->end());
        }
    }
}

void
compute_closures(ENFA *nfa)
{
  vector<bool> visited;
  visited.resize(nfa->nodes.size());

  for (size_t i = 0; i < nfa->nodes.size(); i++)
    if (!visited[i])
      compute_closures_aux(nfa, visited, i);
}

void
insert_closure(std::set<size_t> &set, std::set<size_t> &closure)
{
  set.insert(closure.begin(), closure.end());
}

bool
isMatch(char *s, char *p)
{
  ENFA nfa = parse_pattern(p);
  compute_closures(&nfa);

  set<size_t> front, back;

  insert_closure(front, nfa.closures[nfa.start]);

  while (*s != '\0')
    {
      for (auto node: front)
        {
          auto *set = &nfa.nodes[node];
          auto end = set->end();
          auto it = set->lower_bound({ 0, *s });

          for (; it != end && it->label == *s; it++)
            insert_closure(back, nfa.closures[it->dst]);

          it = set->lower_bound({ 0, Edge_Any });

          for (; it != end && it->label == Edge_Any; it++)
            insert_closure(back, nfa.closures[it->dst]);
        }

      front.clear();
      front.swap(back);
      s++;
    }

  for (auto node: front)
    if (node == nfa.end)
      return true;

  return false;
}

int
main(int argc, char **argv)
{
  assert(argc == 3);

  printf("%s\n", isMatch(argv[1], argv[2]) ? "accepted" : "rejected");
}
