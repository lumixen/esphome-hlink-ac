#!/bin/bash
set -e

cd /esphome

if [ -d /results ]; then
  export GTEST_OUTPUT="${GTEST_OUTPUT:-xml:/results/hlink_ac.xml}"
fi

python3 script/cpp_unit_test.py hlink_ac
