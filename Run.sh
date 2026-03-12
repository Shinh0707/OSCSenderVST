#!/bin/bash
#chmod +x Run.sh && ./Run.sh

BUILD_TYPE=${1:-Debug}
if [ $BUILD_TYPE = "Debug" ]; then
    # フロントエンドのDevサーバーをバックグラウンドで起動
    cd Frontend
    npm run dev &
    DEV_PID=$!
    cd ..
    /path/to/AudioPluginHost
    kill $DEV_PID
else
    /path/to/AudioPluginHost
fi