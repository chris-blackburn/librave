# RAVE
Library to assist in dynamically transforming code through static analysis.

## DynamoRIO:
rave uses [DynamoRIO's](https://dynamorio.org/) standalone disassembler, so you
need to download the release. I've tested with [version
8.0.0](https://dynamorio.org/page_releases.html). I don't have this process
automated yet, so you need to do the following manually:

* Create a `deps` directory in the project root.
* Extract the DynamoRIO release and put it in `deps/DynamoRIO`

## To build:
* `cmake -B build`
* `cd build`
* `make`

I have a makefile here, but it's only used to generate cscope and ctags for now.
I may shift to using a pure makefile to integrate with criu-rave build system,
but cmake is significantly easier to use, so that's what I have for now.
