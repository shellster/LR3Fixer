set shell := ["bash", "-c"]

build:
    source .venv/bin/activate && pio run

upload:
    source .venv/bin/activate && pio run --target upload
