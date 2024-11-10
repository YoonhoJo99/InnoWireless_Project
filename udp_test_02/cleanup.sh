#!/bin/bash

# 목적 파일(.o) 삭제
echo "Removing object files..."
rm -f *.o

# 의존성 파일(.d) 삭제
echo "Removing dependency files..."
rm -f *.d

# 실행 파일 삭제
echo "Removing executable files..."
rm -f client_A client_B

echo "Cleanup completed!"
