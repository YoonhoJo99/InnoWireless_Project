CC = gcc
CFLAGS = -Wall -Wextra
LDFLAGS = 
INCLUDES = -I.

# 실행 파일 이름
TARGETS = client_A client_B

# 소스 파일들
SRCS_A = client_A.c STUNExternalIP.c
SRCS_B = client_B.c STUNExternalIP.c

# Object 파일들
OBJS_A = $(SRCS_A:.c=.o)
OBJS_B = $(SRCS_B:.c=.o)

# 기본 타겟
all: $(TARGETS)

# Client A 빌드 룰
client_A: $(OBJS_A)
	$(CC) $(OBJS_A) -o $@ $(LDFLAGS)

# Client B 빌드 룰
client_B: $(OBJS_B)
	$(CC) $(OBJS_B) -o $@ $(LDFLAGS)

# Object 파일 빌드 룰
%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# clean 타겟
clean:
	rm -f $(TARGETS) *.o

# 의존성 파일 포함
-include $(SRCS_A:.c=.d)
-include $(SRCS_B:.c=.d)

# 헤더 파일 의존성 생성
%.d: %.c
	@set -e; rm -f $@; \
	$(CC) -MM $(INCLUDES) $< > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

.PHONY: all clean
