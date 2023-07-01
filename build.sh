#!/bin/sh
cd gambatte-core
bash scripts/build_shlib.sh
cd ..
mkdir dtv_build
cd dtv_build
cmake ../direct-to-video
cmake --build .
cd ..
make