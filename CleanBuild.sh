#!/bin/bash
#chmod +x CleanBuild.sh && chmod +x Build.sh && ./CleanBuild.sh
BUILD_TYPE=${1:-Debug}

rm -rf build
./Build.sh $BUILD_TYPE