#!/bin/bash
BUILD_TYPE=${1:-Debug}
LOG_FILE="check_result.log"
MAX_LINES=300

> "$LOG_FILE"

echo "Starting static analysis. Results will be saved to $LOG_FILE..."

{
    echo "========== [1/3] TypeScript Check (Frontend) =========="
    cd Frontend

    echo "[Running tsc (Syntax & Type Check)...]"
    npx tsc --noEmit

    echo ""
    echo "[Running eslint (Linter Check)...]"
    npm run lint 

    cd ..
    echo ""

    echo "========== [2/3] C++ Check (Syntax & Memory) =========="
    
    if [ ! -f "build/compile_commands.json" ]; then
        echo "Error: build/compile_commands.json not found."
        echo "Please run Build.sh first to generate the compilation database."
        exit 1
    fi

    cmake -DCMAKE_BUILD_TYPE:STRING=$BUILD_TYPE \
      -DCMAKE_EXPORT_COMPILE_COMMANDS:BOOL=TRUE \
      -DCMAKE_C_COMPILER:FILEPATH=/usr/bin/clang \
      -DCMAKE_CXX_COMPILER:FILEPATH=/usr/bin/clang++ \
      --no-warn-unused-cli \
      -S . \
      -B build \
      -G "Unix Makefiles"

    if ! command -v clang-tidy &> /dev/null; then
        echo "Error: clang-tidy is not installed."
        echo "For macOS: brew install llvm"
        exit 1
    fi

    echo "[Running clang-tidy...]"
    find ./Source -type f -name "*.cpp" | xargs clang-tidy -p build/ \
        -checks='-*,clang-diagnostic-*,clang-analyzer-core.*,clang-analyzer-cplusplus.NewDelete,clang-analyzer-cplusplus.NewDeleteLeaks,bugprone-*,cppcoreguidelines-owning-memory'

    echo ""

    echo "========== [3/3] File Length Check =========="
    echo "[Checking for files exceeding $MAX_LINES lines...]"
    
    find ./Frontend -type f \( -name "*.ts" -o -name "*.tsx" \) -not -path "*/node_modules/*" 2>/dev/null > /tmp/check_files.tmp
    find ./Source -type f \( -name "*.cpp" -o -name "*.h" \) 2>/dev/null >> /tmp/check_files.tmp

    WARNING_COUNT=0
    while read -r file; do
        lines=$(wc -l < "$file" | tr -d ' ')
        if [ "$lines" -gt "$MAX_LINES" ]; then
            echo "Warning: $file is too long ($lines lines). Please consider code separation, class definitions, or method extraction."
            WARNING_COUNT=$((WARNING_COUNT + 1))
        fi
    done < /tmp/check_files.tmp
    rm /tmp/check_files.tmp

    if [ "$WARNING_COUNT" -eq 0 ]; then
        echo "All files are within the $MAX_LINES lines limit."
    fi

    echo ""
    echo "========== All Checks Completed =========="

} 2>&1 | tee -a "$LOG_FILE"