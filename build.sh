#!/usr/bin/env sh
mkdir -p build
dcc src/main.c -o build/bici
# dcc -fsyntax-only src/main.c -Wno-unused-command-line-argument && \
#     tcc -Wall -Werror -ggdb -b -bt4 -DBUILD_DEBUG=1 src/main.c -o build/bici
