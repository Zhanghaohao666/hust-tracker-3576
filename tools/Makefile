# servo_test 编译 Makefile
# 用法: make          (交叉编译，生成 aarch64 可执行文件)
#       make clean    (清理)

# 项目根目录（相对于 tools/）
PROJECT_ROOT := ..

# 交叉编译器
CC := aarch64-none-linux-gnu-gcc

# 头文件路径
CFLAGS := -Wall -O2
CFLAGS += -I$(PROJECT_ROOT)/RK3576_ThirdLibs/include/libservo

# 链接库路径和库
LDFLAGS := -L$(PROJECT_ROOT)/RK3576_ThirdLibs/lib
LDFLAGS += -lservo -llog -luart -lcommcore -lpthread

TARGET := servo_test
SRC := servo_test.c

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)
	@echo ""
	@echo "=== 编译完成 ==="
	@echo "将 servo_test 拷贝到 RK3576 设备上运行:"
	@echo "  scp servo_test root@<device_ip>:/tmp/"
	@echo "  ssh root@<device_ip>"
	@echo "  cd /tmp && ./servo_test"
	@echo ""

clean:
	rm -f $(TARGET)
