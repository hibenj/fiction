# Synthesis I/O

## Truth Tables

**Header:** `fiction/synthesis/io/tt_reader.hpp`

```{doxygenclass} fiction::synthesis::io::tt_reader
:members:
```

## Planar Cell Databases

**Header:** `fiction/synthesis/io/planar_cell_database_json.hpp`

A planar cell database (see {ref}`planar cells <planar-cells>`) is written as one JSON document: the format name and
version, the largest support size, the enumeration parameters if given, and one object per cell with its support
size, its function (the negation-class representative as a hexadecimal truth table over the cell's inputs), its cost
and depth, its nodes (gate, fanins, which fanins are inverted, level) and whether its output is inverted. The reader
checks every cell against the function it claims.

```{doxygenfunction} fiction::synthesis::io::write_planar_cell_database(const planar_cell_database& db, std::ostream& os, const planar_cell_database_params* ps)

```

```{doxygenfunction} fiction::synthesis::io::write_planar_cell_database(const planar_cell_database& db, const std::string& filename, const planar_cell_database_params* ps)

```

```{doxygenfunction} fiction::synthesis::io::read_planar_cell_database(std::istream& is)

```

```{doxygenfunction} fiction::synthesis::io::read_planar_cell_database(const std::string& filename)

```

```{doxygenclass} fiction::synthesis::io::planar_cell_database_parsing_error

```
