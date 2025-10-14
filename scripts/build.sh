#!/bin/bash
mkdir ../build
cd ../build || exit

cmake .. -DIGNITE_USE_SYSTEM=ON -DCMAKE_BUILD_TYPE=Release

make -j$(nproc)
