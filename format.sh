#!/bin/bash
# Shellscript that help format all project files
# Notice that you should download "clang-format" firstly
find . -type f \( -name '*.h' -or -name '*.hpp' -or -name '*.cpp' -or -name '*.c' -or -name '*.cc' \) -print | xargs clang-format -style=file --sort-includes -i