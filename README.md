# zindex - create an index on a compressed text file

Under development; example usage below subject to change and doesn't yet work!

### Creating an index

zindex needs to be told what part of each line constitutes the index. This can be done by
a regular expression, JSON, or field number.

By default zindex creates an index of `file.gz.zindex` when asked to index `file.gz`.

Example: create an index on lines matching a numeric regular expression. The capture group
indicates the part that's to be indexed, and the options show each line has a unique, numeric index.

    $ zindex file.gz --regex 'id:([0-9]+)' --numeric --unique

### Querying the index

Output the lines matching these indices:

    $ zq file.gz 1023 4443 554


## TODO

* Verbosity/logging
* Support for non-numeric indices
* Integration tests
* Optimizations: for multiple accesses in order, can skip the re-scan from the last index point and just keep reading continuously.

