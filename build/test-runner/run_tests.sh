#!/bin/bash
set -e

cd /esphome

EXIT_CODE=0
python3 script/cpp_unit_test.py hlink_ac || EXIT_CODE=$?

PROGRAM=$(find /esphome -path "*/.pioenvs/*/program" -type f 2>/dev/null | head -1)
if [ -n "$PROGRAM" ] && [ -d /results ]; then
  GTEST_OUTPUT=xml:/results/hlink_ac.xml "$PROGRAM" || true
fi

exit $EXIT_CODE
