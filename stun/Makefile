# 컴파일러 설정
CC = gcc
CFLAGS = -Wall -g

# 소스 파일들
COMMON_SRC = STUNExternalIP.c
CLIENT_A_SRC = client_A.c
CLIENT_B_SRC = client_B.c

# 오브젝트 파일들
COMMON_OBJ = $(COMMON_SRC:.c=.o)
CLIENT_A_OBJ = $(CLIENT_A_SRC:.c=.o)
CLIENT_B_OBJ = $(CLIENT_B_SRC:.c=.o)

# 실행 파일들
CLIENT_A = client_A
CLIENT_B = client_B

# 기본 타겟
all: $(CLIENT_A) $(CLIENT_B)

# Client A 빌드 규칙
$(CLIENT_A): $(COMMON_OBJ) $(CLIENT_A_OBJ)
	$(CC) $(CFLAGS) -o $@ $^

# Client B 빌드 규칙
$(CLIENT_B): $(COMMON_OBJ) $(CLIENT_B_OBJ)
	$(CC) $(CFLAGS) -o $@ $^

# .c 파일을 .o 파일로 컴파일하는 규칙
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# 'clean' 타겟 - 생성된 파일들을 삭제
clean:
	rm -f $(COMMON_OBJ) $(CLIENT_A_OBJ) $(CLIENT_B_OBJ) $(CLIENT_A) $(CLIENT_B)

# 'rebuild' 타겟 - 전체 리빌드
rebuild: clean all

# 종속성 정의
$(COMMON_OBJ): STUNExternalIP.h
$(CLIENT_A_OBJ): STUNExternalIP.h client_common.h
$(CLIENT_B_OBJ): STUNExternalIP.h client_common.h

.PHONY: all clean rebuild
