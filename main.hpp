using StateIndex = size_t;

constexpr StateIndex lowest_state_index = std::numeric_limits<StateIndex>::lowest();
constexpr char Edge_Eps = -1;

struct Edge
{
  StateIndex dst;
  char label;

  bool operator<(Edge e1) const
  {
    Edge e0 = *this;
    if (e0.label == e1.label)
      return e0.dst < e1.dst;
    else
      return e0.label < e1.label;
  }
};

using State = std::set<Edge>;
using StateSubset = std::set<StateIndex>;

struct ENFA
{
  StateIndex start, end;
  std::vector<State> states;
  std::vector<StateSubset> closures;
};

struct DFA
{
  std::vector<State> states;
  StateSubset final_states;
};

struct StatePair
{
  StateIndex start, end;
};
