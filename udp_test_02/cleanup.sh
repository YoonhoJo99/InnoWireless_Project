#!/bin/bash

# 삭제할 파일 확장자와 파일명 정의
TARGET_EXEC="client"  # 실행 파일 이름
OBJECT_FILES="*.o"    # 객체 파일

# 실행 파일 삭제
if [ -f "$TARGET_EXEC" ]; then
    echo "Deleting executable: $TARGET_EXEC"
    rm -f "$TARGET_EXEC"
fi

# 객체 파일 삭제
if ls $OBJECT_FILES 1> /dev/null 2>&1; then
    echo "Deleting object files: $OBJECT_FILES"
    rm -f $OBJECT_FILES
fi

echo "Cleanup complete."

