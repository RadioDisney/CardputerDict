#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
mkdir -p build/tests
c++ -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined -g -I include tests/core_test.cpp -o build/tests/core_test
build/tests/core_test sdcard/dictionaries/demo
python3 -m unittest discover -s tests -p 'test_*.py' -v
if [ -f sdcard/dictionaries/ecdict.ifo ]; then
    build/tests/core_test sdcard/dictionaries/ecdict
fi
