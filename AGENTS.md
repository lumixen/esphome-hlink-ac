# AGENTS.md

## Overview

This repository contains `hlink_ac`, an ESPHome external component that controls compatible Hitachi air conditioners over the serial H-Link protocol. It replaces proprietary cloud adapters with native Home Assistant climate integration.

H-Link is a serial protocol (9600 baud, odd parity) with two frame types: `MT P=XXXX C=YYYY` status inquiries and `ST P=XXXX,XX(XX) C=YYYY` status change requests, where `C=YYYY` is a 16-bit XOR checksum. See `README.md` and `docs/hlink_alarm_codes.csv` for protocol and configuration details.

## Commands

All build/test commands run through Docker scripts and must be executed from the `build/` directory.

```bash
cd build

# C++ unit tests (GoogleTest via ESPHome's cpp_unit_test framework)
./test all      # build image + run tests
./test build    # build test image only
./test run      # run tests (image must exist)

# Compile validation for all build/*.yml dev configs
./compile              # compile every config
./compile hlink.yml    # compile a single config
```

Test results (JUnit XML) land in `tests/.run/hlink_ac/`. Git ignores `tests/.run/`.

CI runs the same checks on every push: `.github/workflows/tests.yml` (unit tests) and `.github/workflows/compile.yml` (matrix compile of `build/*.yml`). Docker/esphome image versions are pinned in the scripts and workflows, not in the component itself.

## Project structure

- `components/hlink_ac/` — the ESPHome component
  - `climate.py` — climate platform schema and `CODEOWNERS`
  - `hlink_ac.h` / `hlink_ac.cpp` — C++ component core
  - `automation.h` — trigger classes (e.g. `on_send_hlink_cmd_result`)
  - `sensor/`, `switch/`, `text_sensor/`, `button/`, `binary_sensor/` — sub-platform schemas + C++ (`*.py`, `*.h`, `*.cpp` per platform)
- `tests/components/hlink_ac/` — unit tests
  - `*_test.cpp` — GoogleTest suites
  - `common.h`, `hlink_test_utils.h` — shared test helpers
  - `test.host.yaml`, `benchmark.yaml` — host-platform configs used by the test runner
- `build/` — dev YAML configs (`hlink*.yml`) and docker tooling (`compile`, `test`, `test-runner/`)
- `docs/` — protocol reference data
- `scripts/mqtt-tail/` — MQTT debugging utility

## Code conventions

- Format C++ with `.clang-format` (ColumnLimit 120).
- Follow ESPHome component conventions: `#ifdef USE_*` guards around optional platform code, `esphome::hlink_ac` namespace, `namespace esphome::hlink_ac::testing` for tests, `CODEOWNERS = ["@lumixen"]` in schemas.
- Keep `components/hlink_ac/__init__.py` minimal; platform config lives in sub-platform `__init__.py` files.
- Tests use GoogleTest (`TEST(HlinkXxxTest, ...)`) and cover frame parsing, utilities, and the state machine.
- H-Link addresses are 16-bit hex (`P=XXXX`); never change wire protocol behavior without updating the unit tests.

## Git workflow

- All new features are developed against the `dev` branch.
- Create a feature branch off `dev`, open a pull request targeting `dev`, and merge it there after review.
- Releases are made from `dev`: create a `Release YYYY.M.P` commit, merge `dev` into `main`, and tag it.

## Releases

Semantic versioning with `YYYY.M.P` tags (e.g. `2026.7.1`) and `Release YYYY.M.P` commit messages. `external_components` usage in README references the current release tag.
