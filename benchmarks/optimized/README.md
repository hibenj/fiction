# Optimized AIGs

The ISCAS85 and the small EPFL benchmarks after AIG optimization, in AIGER format, one directory per set with the
names of the Verilog benchmarks. They are the inputs of the crossing experiments (`experiments/crossing_decomposition`,
`experiments/planar_cells`), which read them by default and fall back to the Verilog benchmarks where a file is
missing.

Produced on 2026-10-13 with ABC (`cda-tum/abc`, branch `ac_dc_tm`, `abc.rc` loaded) by

```
read <benchmark>.v; strash; dfraig; resyn; resyn2; resyn2rs; write_aiger <set>/<name>.aig
```

which is the optimization script of Tempia Calvino et al., "Practical Boolean Decomposition for Delay-driven LUT
Mapping" (IWLS 2024) without its lazy-man's-synthesis step (`if -y -K 6; resyn2rs`), whose recorded library is not
available here. `optimization.log` lists the AND count and depth of every benchmark before and after.
