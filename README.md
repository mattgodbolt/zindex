[![Build Status](https://travis-ci.org/mattgodbolt/zindex.svg?branch=master)](https://travis-ci.org/mattgodbolt/zindex)

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
a regular expression currently (JSON and field number coming soon).

By default zindex creates an index of `file.gz.zindex` when asked to index `file.gz`.

Example: create an index on lines matching a numeric regular expression. The capture group
indicates the part that's to be indexed, and the options show each line has a unique, numeric index.

```bash
$ zindex file.gz --regex 'id:([0-9]+)' --numeric --unique
```

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

`zindex` uses CMake for its basic building (though has a bootstrapping `Makefile`), and requires a C++11 compatible compiler (GCC 4.8 or above and clang 3.4 and above). It also requires `zlib`. With the relevant compiler available, building ought to be as simple as:

```bash
$ git clone https://github.com/mattgodbolt/zindex.git
$ cd zindex
$ make
```

Binaries are left in `build/`.

Additionally a static binary can be built if you're happy to dip your toe into CMake:

```bash
$ cd path/to/build/directory
$ cmake path/to/zindex/checkout/dir -DStatic:BOOL=On
$ make
```

### Issues and feature requests

See the [issue tracker](https://github.com/mattgodbolt/zindex/issues) for TODOs and known bugs. Please raise bugs there, and feel free to submit suggestions there also.

Feel free to [contact me](mailto:matt@godbolt.org) if you prefer email over bug trackers.
