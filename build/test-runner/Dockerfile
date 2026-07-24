FROM python:3.12-slim

RUN apt-get update && apt-get install -y --no-install-recommends \
    git build-essential && rm -rf /var/lib/apt/lists/*

RUN pip install --no-cache-dir esphome==2026.7.2 platformio
RUN pio platform install "native"

RUN git clone --depth 1 --branch 2026.7.2 \
    https://github.com/esphome/esphome.git /esphome

# Component sources (layer invalidated only on change)
COPY components/hlink_ac /esphome/esphome/components/hlink_ac
RUN cd /esphome && pip install -e . --quiet --no-deps 2>&1 | tail -1

# Test sources (layer invalidated only on change)
COPY tests/components/hlink_ac /esphome/tests/components/hlink_ac

COPY build/test-runner/run_tests.sh /usr/local/bin/
RUN chmod +x /usr/local/bin/run_tests.sh

WORKDIR /esphome
ENTRYPOINT ["run_tests.sh"]
