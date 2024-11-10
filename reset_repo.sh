#!/bin/bash
git fetch origin
git reset --hard origin/main  # 원격 브랜치 이름이 'main'일 경우
git clean -fd

