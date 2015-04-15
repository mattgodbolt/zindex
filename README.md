[![Build Status](https://travis-ci.org/mattgodbolt/zindex.svg?branch=master)](https://travis-ci.org/mattgodbolt/zindex)

`zindex` creates and queries an index on a compressed, line-based text file in a
time- and space-efficient way.

### The itch I had

Basic gist is I have a ton of multigigabye text log files gzipped and archived
on an NFS mount and I'd like to be able to access lines of that file by an index. There's
a unique key on each line, and a simple regex can pull that out. However, to pull out a
particular record requires zgrep, which then takes ages to seek through the gigabytes of
previous log to get to each record.

Enter `zindex` which builds an index and, crucially, also stores checkpoints along the way
of the compressed file which allows fast random access. Pulling out single lines by either
line number of by an index entry is then almost instant, even for huge files. The indices
themselves are small too, typically ~10% of the compressed file size for a simple unique
numeric index.

## Creating an index

zindex needs to be told what part of each line constitutes the index. This can be done by
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

### Issues and feature requests

See the [issue tracker](https://github.com/mattgodbolt/zindex/issues) for TODOs and known bugs. Please raise bugs there, and feel free to submit suggestions there also.

Feel free to [contact me](mailto:matt@godbolt.org) if you prefer email over bug trackers.
