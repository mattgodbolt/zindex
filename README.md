[![CI](https://github.com/mattgodbolt/zindex/actions/workflows/ci.yml/badge.svg)](https://github.com/mattgodbolt/zindex/actions/workflows/ci.yml)


`zindex` creates and queries an index on a compressed, line-based text file in a
time- and space-efficient way.

### The itch I had

I have many multigigabyte text gzipped log files and I'd like to be able to find data in them by an index.
There's a key on each line that a simple regex can pull out. However, to find a
particular record requires `zgrep`, which takes ages as it has to seek through
gigabytes of previous data to get to each record.

Enter `zindex` which builds an index and also stores decompression checkpoints along the way
which allows lightning fast random access. Pulling out single lines by either
line number of by an index entry is then almost instant, even for huge files. The indices
themselves are small too, typically ~10% of the compressed file size for a simple unique
numeric index.

## Creating an index

`zindex` needs to be told what part of each line constitutes the index. This can be done by
a regular expression, by field, or by piping each line through an external program.

By default zindex creates an index of `file.gz.zindex` when asked to index `file.gz`.

Example: create an index on lines matching a numeric regular expression. The capture group
indicates the part that's to be indexed, and the options show each line has a unique, numeric index.

```bash
$ zindex file.gz --regex 'id:([0-9]+)' --numeric --unique
```

Example: create an index on the second field of a CSV file:

```bash
$ zindex file.gz --delimiter , --field 2
```

Example: create an index on a JSON field `orderId.id` in any of the items in the document root's `actions` array (requires [jq](http://stedolan.github.io/jq/)).
The `jq` query creates an array of all the `orderId.id`s, then `join`s them with a space to ensure each individual line piped to jq creates a single line of output,
with multiple matches separated by spaces (which is the default separator).

```bash
$ zindex file.gz --pipe "jq --raw-output --unbuffered '[.actions[].orderId.id] | join(\" \")'"
```

Multiple indices, and configuration of the index creation by JSON configuration file are supported, see below.

## Querying the index

The `zq` program is used to query an index.  It's given the name of the compressed file and a list of queries. For example:

```bash
$ zq file.gz 1023 4443 554
```

It's also possible to output by line number, so to print lines 1 and 1000 from a file:

```bash
$ zq file.gz --line 1 1000
```

## Building from source

`zindex` uses CMake for its basic building (though has a bootstrapping `Makefile`), and requires CMake 3.19 or later and a C++17 compatible compiler (GCC 8 or above, clang 7 or above). It also requires `zlib`. With the relevant compiler available, building ought to be as simple as:

```bash
$ git clone https://github.com/mattgodbolt/zindex.git
$ cd zindex
$ make
```

Binaries are left in `build/Release`.

Additionally a static binary can be built if you're happy to dip your toe into CMake:

```bash
$ cd path/to/build/directory
$ cmake path/to/zindex/checkout/dir -DStatic:BOOL=On -DCMAKE_BUILD_TYPE=Release
$ make
```

## Multiple indices

To support more than one index, or for easier configuration than all the command-line flags that might be
needed, there is a JSON configuration format. Pass the `--config <yourconfigfile>.json` option and put something like this in the configuration file:

    {
        "indexes": [
            {
                "type": "field",
                "delimiter": "\t",
                "fieldNum": 1
            },
            {
                "name": "secondary",
                "type": "field",
                "delimiter": "\t",
                "fieldNum": 2
            }
        ]
    }

This creates two indices, one on the first field and one on the second field, as delimited by tabs. One can
then specify which index to query with the `-i <index>` option of `zq`. An index with no `"name"` is called
`default`, and is the one `zq` queries when no `-i` is given.

Fields are always selected by their 1-based number (`--field` or `fieldNum`); selecting a field by the name in a
header line is not supported.

## Advanced usage

### Useful indexing options

* `--tab-delimiter` is shorthand for a tab `--delimiter`, which is awkward to type in a shell.
* `--index-file <path>` (on both `zindex` and `zq`) stores or reads the index at `<path>` instead of `file.gz.zindex`.
* `-o`/`--sparseLineOffsets` only stores line offsets for lines that have an index key. Lines without one can
  still be fetched by line number (`zq --line`), but the option is aimed at cases where you only ever look up lines
  by key.

```bash
$ zindex file.gz --tab-delimiter --field 3 --index-file /somewhere/else.idx
$ zq file.gz --index-file /somewhere/else.idx blue
```

### Custom queries with `--raw`

An index is a SQLite database, and `zq --raw "<sql>"` runs your own query against it instead of a simple
key lookup. Each index is a table called `index_<name>` (so `index_default` for an unnamed index) with, among
others, a `key` column and a `line` column. **The first column of your query must be the line number**: `zq`
prints those lines from the file and ignores any other columns. (A query such as `select count(*) ...` therefore
prints the line numbered by the count, not the count itself.)

For a tab-separated file of word, number and colour, indexed (via a configuration file as above) with indices
named `word` (field 1) and `colour` (field 3), this finds every line whose word starts with `al` (a wildcard
search):

```bash
$ zq file.gz --raw "select a.line from index_word a where a.key like 'al%';"
```

To combine indices, join them on `line`: lines whose word starts with `al` and whose colour is `red`:

```bash
$ zq file.gz --raw "select a.line from index_word a, index_colour b
    where a.line == b.line and a.key like 'al%' and b.key == 'red';"
```

### Issues and feature requests

See the [issue tracker](https://github.com/mattgodbolt/zindex/issues) for TODOs and known bugs. Please raise bugs there, and feel free to submit suggestions there also.

Feel free to [contact me](mailto:matt@godbolt.org) if you prefer email over bug trackers.
