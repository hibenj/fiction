# Network Conversion

**Header:** `fiction/synthesis/network_conversion.hpp`

```{doxygenfunction} fiction::synthesis::convert_network

```

## Network Balancing

::::{tab-set}
:sync-group: language

:::{tab-item} C++
:sync: cpp

**Header:** `fiction/synthesis/network_balancing.hpp`

```{doxygenstruct} fiction::synthesis::network_balancing_params
:members:
```

```{doxygenfunction} fiction::synthesis::network_balancing

```

:::

:::{tab-item} Python
:sync: python

```{eval-rst}
.. autoclass:: mnt.pyfiction.synthesis.network_balancing_params
   :members:

.. autofunction:: mnt.pyfiction.synthesis.network_balancing

.. autofunction:: mnt.pyfiction.synthesis.is_balanced
```

:::

::::

## Fanout Substitution

::::{tab-set}
:sync-group: language

:::{tab-item} C++
:sync: cpp

**Header:** `fiction/synthesis/fanout_substitution.hpp`

```{doxygenstruct} fiction::synthesis::fanout_substitution_params
:members:
```

```{doxygenfunction} fiction::synthesis::fanout_substitution

```

:::

:::{tab-item} Python
:sync: python

```{eval-rst}
.. autoclass:: mnt.pyfiction.synthesis.fanout_substitution_params
   :members:

.. autoclass:: mnt.pyfiction.synthesis.substitution_strategy
   :members:

.. autofunction:: mnt.pyfiction.synthesis.fanout_substitution

.. autofunction:: mnt.pyfiction.synthesis.is_fanout_substituted
```

:::

::::

## Crossing-Aware LUT Decomposition

Two LUTs of one level that share inputs cross each other's input wires in every drawing: k shared inputs cost
k(k-1)/2 crossings. If a function h over some of the shared inputs is a sub-function of both LUTs, a shared node
computing h replaces those k wires per LUT by a single one {cite:p}`nano_2024_5`. `crossing_decomposition` ranks a
balanced LUT network, orders it with {ref}`mincross <mincross>`, and then walks the levels from the inputs upwards:
for every pair of LUTs with shared inputs it searches a shared node by truth-table cofactors, builds the network with
the shared node in a level inserted below the pair, re-sorts the levels around it locally, and keeps the result only
if the crossing count drops. The rest of the order is left as `mincross` produced it; the pass never reorders the
whole network again. The experiment in `experiments/crossing_decomposition` runs the pass on the benchmark sets of
the paper and checks every result for equivalence.

The structure of the balanced network matters more than the pass: with one private buffer chain per edge, most of
the crossings of a wide network are between chain edges, and two LUTs that read the same signal through different
chains do not count as sharing it. Balancing with shared chains (`network_balancing_params::share_buffers`) removes
those chain edges and lets the pass see the sharing; the experiment uses it by default and reports the crossings of
the private-chain structure next to it.

Every shared node that cannot sit in an existing level inserts one, which buffers every other signal between the two
levels and adds to the depth. `level_cost` and `node_cost` price an inserted level and every added node in
crossings: a candidate is accepted only when its crossings plus these charges stay below the current count. With
both at zero the pass minimizes crossings alone and may double the depth of deep networks; a node cost of one
accepts a decomposition only when every buffer it adds removes at least one crossing.

A shared node is found for a pair, but it serves a group: every other LUT of the level that reads the same inputs
and admits the same inner function joins the pair (`group_shared_nodes`), so k readers of one function share one
node instead of one node per pair.

::::{tab-set}
:sync-group: language

:::{tab-item} C++
:sync: cpp

**Header:** `fiction/synthesis/crossing_decomposition.hpp`

```{doxygenstruct} fiction::synthesis::crossing_decomposition_params
:members:
```

```{doxygenstruct} fiction::synthesis::crossing_decomposition_stats
:members:
```

```{doxygenfunction} fiction::synthesis::crossing_decomposition

```

:::

::::

## Planar Cells

A planar cell is a small gate structure, AND, OR, XOR and three-input majority with inverting edges, that computes
one Boolean function of a few inputs and can be drawn level by level without any crossing wire. Replacing every LUT
of a ranked network by its cell turns the LUT network into a gate network whose only crossings are the ones between
cells. `generate_planar_cells` enumerates such cells: structures are grown from the inputs one level at a time, a new
node reads two or three signals that neighbour each other on the frontier of unconsumed signals (signals in between
are enclosed if they have exactly one consumer already), every signal feeds at most two nodes, and the last level
joins what is still unconsumed. For every structure, every allowed gate per node and every inversion on the edges
between nodes is evaluated. Inversions on input edges and on the output need no enumeration: the database is keyed by
negation classes, functions that differ only by negating inputs or the output, and `planar_cell_database::lookup`
serves a concrete function with the cell of its class and the negations folded into the cell's edges, choosing the
negations that need the fewest inverters. Per class the database keeps the cell that serves the whole class cheapest,
with gates and inversions counted alike, because both are nodes in field-coupled nanotechnologies. One table per
support size lets a function of fewer inputs use the smaller cell.

The enumeration evaluates node types rather than gates and edges: a node is a conjunction with some fanins negated, an
exclusive disjunction, or a majority with at most one fanin negated, and the phase of its output is left to its
consumers, so every assignment of gates and inversions is covered with a few choices per node. Small structures are
evaluated first, and a function whose class already has a cell with fewer gates is skipped. With three levels and
{AND, XOR, MAJ}, every function of up to three inputs has a cell, and 1070 of the 2191 negation classes of four-input
functions that depend on all inputs; 34 of their 208 NPN classes have no cell for any input order. Four levels cover
2190 of the 2191 classes in a few seconds, every NPN class for at least one input order and all but one for every
order. OR adds no coverage over AND with inversions and only lowers the cost of some cells.

::::{tab-set}
:sync-group: language

:::{tab-item} C++
:sync: cpp

**Header:** `fiction/synthesis/planar_cell_database.hpp`

```{doxygenenum} fiction::synthesis::planar_gate

```

```{doxygenstruct} fiction::synthesis::planar_cell_database_params
:members:
```

```{doxygenstruct} fiction::synthesis::planar_cell_database_stats
:members:
```

```{doxygenstruct} fiction::synthesis::planar_cell_node
:members:
```

```{doxygenstruct} fiction::synthesis::planar_cell
:members:
```

```{doxygenclass} fiction::synthesis::planar_cell_database
:members:
```

```{doxygenfunction} fiction::synthesis::generate_planar_cells

```

:::

::::

## Technology Mapping

::::{tab-set}
:sync-group: language

:::{tab-item} C++
:sync: cpp

**Header:** `fiction/synthesis/technology_mapping.hpp`

```{doxygenclass} fiction::synthesis::missing_required_gates_exception
:members:
```

```{doxygenstruct} fiction::synthesis::technology_mapping_params
:members:
```

```{doxygenfunction} fiction::synthesis::and_or_not

```

```{doxygenfunction} fiction::synthesis::and_or_not_maj

```

```{doxygenfunction} fiction::synthesis::all_standard_2_input_functions

```

```{doxygenfunction} fiction::synthesis::all_standard_3_input_functions

```

```{doxygenfunction} fiction::synthesis::all_supported_standard_functions

```

```{doxygenstruct} fiction::synthesis::technology_mapping_stats
:members:
```

```{doxygenfunction} fiction::synthesis::technology_mapping

```

:::

:::{tab-item} Python
:sync: python

```{eval-rst}
.. autoclass:: mnt.pyfiction.synthesis.missing_required_gates_exception
   :members:

.. autoclass:: mnt.pyfiction.synthesis.technology_mapping_params
   :members:

.. autofunction:: mnt.pyfiction.synthesis.and_or_not

.. autofunction:: mnt.pyfiction.synthesis.and_or_not_maj

.. autofunction:: mnt.pyfiction.synthesis.all_standard_2_input_functions

.. autofunction:: mnt.pyfiction.synthesis.all_standard_3_input_functions

.. autofunction:: mnt.pyfiction.synthesis.all_supported_standard_functions

.. autoclass:: mnt.pyfiction.synthesis.technology_mapping_stats
   :members:

.. autofunction:: mnt.pyfiction.synthesis.technology_mapping
```

:::

::::

(planarization)=

## Planarization

A ranked logic network is planar if its edges can be drawn between adjacent ranks without crossings. The
planarization pipeline removes the crossings of a balanced, ranked network by duplicating nodes, by crossing gates
that swap two signals, or by a per-level choice between the two {cite:p}`nano_2026_3`, then restores
fanout nodes and the minimum buffering while keeping ranks and planarity. The stages below are also available on
their own.

`planarization` runs the whole pipeline: node duplication with the chosen strategy, crossing gates for the levels the
hybrid strategy left crossed, planar fanout substitution, and planar rebalancing. The pipeline defaults to the hybrid
strategy with the lookahead criterion and XOR crossing gates; the planarization experiment in `experiments/` runs
this default configuration on the benchmark sets and checks every result for equivalence.

::::{tab-set}
:sync-group: language

:::{tab-item} C++
:sync: cpp

**Header:** `fiction/synthesis/planarization.hpp`

```{doxygenstruct} fiction::synthesis::planarization_params
:members:
```

```{doxygenstruct} fiction::synthesis::planarization_stats
:members:
```

```{doxygenfunction} fiction::synthesis::planarization

```

:::

:::{tab-item} Python
:sync: python

The Python function works on a `technology_network` and reports duplicated inputs through an index list instead of
virtual inputs; see its docstring below.

```{eval-rst}
.. autoclass:: mnt.pyfiction.synthesis.planarization_params
   :members:

.. autoclass:: mnt.pyfiction.synthesis.node_duplication_planarization_params
   :members:

.. autoclass:: mnt.pyfiction.synthesis.planarization_strategy
   :members:

.. autoclass:: mnt.pyfiction.synthesis.output_order
   :members:

.. autoclass:: mnt.pyfiction.synthesis.decision_criterion
   :members:

.. autoclass:: mnt.pyfiction.synthesis.duplication_cost_model
   :members:

.. autoclass:: mnt.pyfiction.synthesis.planarization_stats
   :members:

.. autofunction:: mnt.pyfiction.synthesis.planarization
```

:::

::::

## Node Duplication Planarization

::::{tab-set}
:sync-group: language

:::{tab-item} C++
:sync: cpp

**Header:** `fiction/synthesis/node_duplication_planarization.hpp`

```{doxygenstruct} fiction::synthesis::node_duplication_planarization_params
:members:
```

```{doxygenstruct} fiction::synthesis::node_duplication_planarization_stats
:members:
```

```{doxygenfunction} fiction::synthesis::node_duplication_planarization

```

:::

::::

## Crossing Gate Planarization

::::{tab-set}
:sync-group: language

:::{tab-item} C++
:sync: cpp

**Header:** `fiction/synthesis/crossing_gate_planarization.hpp`

```{doxygenstruct} fiction::synthesis::crossing_gate_planarization_params
:members:
```

```{doxygenstruct} fiction::synthesis::crossing_gate_planarization_stats
:members:
```

```{doxygenfunction} fiction::synthesis::crossing_gate_planarization

```

:::

::::

## Planar Fanout Substitution

::::{tab-set}
:sync-group: language

:::{tab-item} C++
:sync: cpp

**Header:** `fiction/synthesis/planar_fanout_substitution.hpp`

```{doxygenstruct} fiction::synthesis::planar_fanout_substitution_params
:members:
```

```{doxygenfunction} fiction::synthesis::planar_fanout_substitution

```

:::

::::

## Planar Rebalancing

::::{tab-set}
:sync-group: language

:::{tab-item} C++
:sync: cpp

**Header:** `fiction/synthesis/planar_rebalancing.hpp`

```{doxygenstruct} fiction::synthesis::planar_rebalancing_params
:members:
```

```{doxygenfunction} fiction::synthesis::planar_rebalancing

```

:::

::::

## Delete Virtual PIs

::::{tab-set}
:sync-group: language

:::{tab-item} C++
:sync: cpp

**Header:** `fiction/synthesis/delete_virtual_pis.hpp`

```{doxygenfunction} fiction::synthesis::delete_virtual_pis

```

:::

::::

## Truth Tables

::::{tab-set}
:sync-group: language

:::{tab-item} C++
:sync: cpp

**Header:** `fiction/synthesis/truth_tables.hpp`

```{doxygenfunction} fiction::synthesis::create_id_tt

```

```{doxygenfunction} fiction::synthesis::create_not_tt

```

```{doxygenfunction} fiction::synthesis::create_and_tt

```

```{doxygenfunction} fiction::synthesis::create_or_tt

```

```{doxygenfunction} fiction::synthesis::create_nand_tt

```

```{doxygenfunction} fiction::synthesis::create_nor_tt

```

```{doxygenfunction} fiction::synthesis::create_xor_tt

```

```{doxygenfunction} fiction::synthesis::create_xnor_tt

```

```{doxygenfunction} fiction::synthesis::create_lt_tt

```

```{doxygenfunction} fiction::synthesis::create_gt_tt

```

```{doxygenfunction} fiction::synthesis::create_le_tt

```

```{doxygenfunction} fiction::synthesis::create_ge_tt

```

```{doxygenfunction} fiction::synthesis::create_and3_tt

```

```{doxygenfunction} fiction::synthesis::create_xor_and_tt

```

```{doxygenfunction} fiction::synthesis::create_or_and_tt

```

```{doxygenfunction} fiction::synthesis::create_onehot_tt

```

```{doxygenfunction} fiction::synthesis::create_maj_tt

```

```{doxygenfunction} fiction::synthesis::create_gamble_tt

```

```{doxygenfunction} fiction::synthesis::create_dot_tt

```

```{doxygenfunction} fiction::synthesis::create_ite_tt

```

```{doxygenfunction} fiction::synthesis::create_and_xor_tt

```

```{doxygenfunction} fiction::synthesis::create_xor3_tt

```

```{doxygenfunction} fiction::synthesis::create_double_wire_tt

```

```{doxygenfunction} fiction::synthesis::create_crossing_wire_tt

```

```{doxygenfunction} fiction::synthesis::create_fan_out_tt

```

```{doxygenfunction} fiction::synthesis::create_half_adder_tt

```

:::

:::{tab-item} Python
:sync: python

```{eval-rst}
.. autofunction:: mnt.pyfiction.synthesis.create_id_tt

.. autofunction:: mnt.pyfiction.synthesis.create_not_tt

.. autofunction:: mnt.pyfiction.synthesis.create_and_tt

.. autofunction:: mnt.pyfiction.synthesis.create_or_tt

.. autofunction:: mnt.pyfiction.synthesis.create_nand_tt

.. autofunction:: mnt.pyfiction.synthesis.create_nor_tt

.. autofunction:: mnt.pyfiction.synthesis.create_xor_tt

.. autofunction:: mnt.pyfiction.synthesis.create_xnor_tt

.. autofunction:: mnt.pyfiction.synthesis.create_lt_tt

.. autofunction:: mnt.pyfiction.synthesis.create_gt_tt

.. autofunction:: mnt.pyfiction.synthesis.create_le_tt

.. autofunction:: mnt.pyfiction.synthesis.create_ge_tt

.. autofunction:: mnt.pyfiction.synthesis.create_and3_tt

.. autofunction:: mnt.pyfiction.synthesis.create_xor_and_tt

.. autofunction:: mnt.pyfiction.synthesis.create_or_and_tt

.. autofunction:: mnt.pyfiction.synthesis.create_onehot_tt

.. autofunction:: mnt.pyfiction.synthesis.create_maj_tt

.. autofunction:: mnt.pyfiction.synthesis.create_gamble_tt

.. autofunction:: mnt.pyfiction.synthesis.create_dot_tt

.. autofunction:: mnt.pyfiction.synthesis.create_ite_tt

.. autofunction:: mnt.pyfiction.synthesis.create_and_xor_tt

.. autofunction:: mnt.pyfiction.synthesis.create_xor3_tt

.. autofunction:: mnt.pyfiction.synthesis.create_double_wire_tt

.. autofunction:: mnt.pyfiction.synthesis.create_crossing_wire_tt

.. autofunction:: mnt.pyfiction.synthesis.create_fan_out_tt

.. autofunction:: mnt.pyfiction.synthesis.create_half_adder_tt
```

:::

::::
