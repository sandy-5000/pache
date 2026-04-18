#!/bin/bash

if [[ "$1" == "-b" ]]; then
    bash scripts/build.sh
fi

bash scripts/run.sh
