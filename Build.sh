#!/bin/bash
#chmod +x Build.sh && ./Build.sh
BUILD_TYPE=${1:-Debug}

echo "Building in $BUILD_TYPE mode..."
# buildディレクトリが存在しない場合のみ作成
mkdir -p build

cd Frontend
npm run build
cd ..

# CMakeの構成
cmake -DCMAKE_BUILD_TYPE:STRING=$BUILD_TYPE \
      -DCMAKE_EXPORT_COMPILE_COMMANDS:BOOL=TRUE \
      -DCMAKE_C_COMPILER:FILEPATH=/usr/bin/clang \
      -DCMAKE_CXX_COMPILER:FILEPATH=/usr/bin/clang++ \
      --no-warn-unused-cli \
      -S . \
      -B build \
      -G "Unix Makefiles"

# 8スレッドでビルドを実行
cmake --build build -j 8