# RAVE

Library to assist in dynamically transforming code through static analysis.

## To build:
* `cmake -B build`
* `cd build`
* `make`

I have a makefile here, but it's only used to generate cscope and ctags for now.
I may shift to using a pure makefile to integrate with criu-rave build system,
but cmake is significantly easier to use, so that's what I have for now.
