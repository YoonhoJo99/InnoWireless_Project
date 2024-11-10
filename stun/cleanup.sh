#!/bin/bash

# 색상 정의
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${YELLOW}cleanup.sh: 프로젝트 정리를 시작합니다...${NC}"

# 오브젝트 파일 삭제
echo -e "${GREEN}오브젝트 파일 검색 중...${NC}"
find . -name "*.o" -type f -print -delete | while read file; do
    echo -e "${RED}삭제됨: ${NC}$file"
done

# 실행 파일 삭제
echo -e "${GREEN}실행 파일 검색 중...${NC}"
if [ -f "./client_A" ]; then
    rm -f ./client_A
    echo -e "${RED}삭제됨: ${NC}client_A"
fi

if [ -f "./client_B" ]; then
    rm -f ./client_B
    echo -e "${RED}삭제됨: ${NC}client_B"
fi

# 임시 파일 삭제
echo -e "${GREEN}임시 파일 검색 중...${NC}"
find . -name "*~" -type f -print -delete | while read file; do
    echo -e "${RED}삭제됨: ${NC}$file"
done

find . -name "*.swp" -type f -print -delete | while read file; do
    echo -e "${RED}삭제됨: ${NC}$file"
done

# 권한 설정 확인
echo -e "${GREEN}권한 설정 확인 중...${NC}"
if [ ! -x "$0" ]; then
    chmod +x "$0"
    echo -e "${YELLOW}cleanup.sh 실행 권한이 추가되었습니다.${NC}"
fi

echo -e "${YELLOW}정리가 완료되었습니다.${NC}"
