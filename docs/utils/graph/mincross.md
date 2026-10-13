(mincross)=

# Crossing Minimization (MinCross)

Reimplementation of Graphviz's `mincross` algorithm for edge crossing minimization in leveled logic networks.
This algorithm iteratively reorders nodes within ranks to reduce the total number of edge crossings, using the **median**
and **transpose** heuristics originally described in Graphviz's `dot` layout engine.
It operates on networks with explicit rank information (e.g., `mutable_rank_view`) and produces a layout-friendly node
ordering that improves readability and wire routing efficiency.

The network is split into its connected components, each component is ordered on its own and the components are
placed one after the other. Within a component, the algorithm follows Graphviz's three passes:

1. **Initialization pass** — a breadth-first order from the inputs, refined by a few median and transposition
   iterations.
2. **Refinement pass** — the same from the outputs; the better of the two seeds wins.
3. **Full optimization pass** — iterative median and transposition steps until the crossing count no longer
   improves by the convergence threshold.

Each iteration alternates between upward and downward traversal through the network ranks to reorder nodes by the
weighted median positions of their fanin or fanout connections, followed by adjacent transpositions that re-visit the
neighbouring ranks after every swap. Unlike Graphviz, the order the network comes with is kept as a candidate, so the
result is never worse than the input. With fixed inputs, the components are ordered together and the input rank keeps
its order.

::::{tab-set}
:sync-group: language

:::{tab-item} C++
:sync: cpp

**Header:** `fiction/utils/graph/mincross.hpp`

```{doxygenstruct} fiction::utils::graph::mincross_params
:members:
```

```{doxygenstruct} fiction::utils::graph::mincross_stats
:members:
```

```{doxygenfunction} fiction::utils::graph::mincross(Ntk& ntk, const mincross_params& ps = {}, mincross_stats* pst = nullptr)

```

:::

::::
