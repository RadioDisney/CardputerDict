#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
mkdir -p build/tests
c++ -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined -g -I include tests/ui_text_test.cpp -o build/tests/ui_text_test
build/tests/ui_text_test
c++ -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined -g -I include tests/ime_layout_test.cpp -o build/tests/ime_layout_test
build/tests/ime_layout_test
c++ -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined -g -I include tests/core_test.cpp -o build/tests/core_test
build/tests/core_test sdcard/dictionaries/demo
python3 -m unittest discover -s tests -p 'test_*.py' -v
if [ -f sdcard/dictionaries/ecdict.ifo ]; then
    build/tests/core_test sdcard/dictionaries/ecdict
fi
if [ -f sdcard/dictionaries/cedict.ifo ]; then
    build/tests/core_test sdcard/dictionaries/cedict 苹果 apple
    build/tests/core_test sdcard/dictionaries/cedict 蘋果 apple
    build/tests/core_test sdcard/dictionaries/pinyin pingguo 苹果
fi
