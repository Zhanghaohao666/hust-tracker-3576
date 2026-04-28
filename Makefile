CUR_DIR := $(shell pwd)
SDK_ROOT := $(CUR_DIR)

MOUDLE_NAME :=
INCLUDE_PATH :=
TARGET := hust_tracker_3576

DEFAULT_PLATFORM ?= rk3576
DEFAULT_GLIBCVersion ?= 200

ifneq ($(platform),)
    DEFAULT_PLATFORM=$(platform)
endif

ifneq ($(glibc),)
    DEFAULT_GLIBCVersion=$(glibc)
endif

include $(SDK_ROOT)/config.mk

$(info ================ 编译说明 =================)
$(info )
$(info Current platform is :  $(DEFAULT_PLATFORM))
$(info Current compiler is :  $(CORSS))
$(info Current glibc version is :  $(DEFAULT_GLIBCVersion))
$(info )
$(info ===========================================)

TARGET_DIR=$(SDK_ROOT)/target
OBJ_DIR=$(SDK_ROOT)/obj
MK_DIR=$(SDK_ROOT)/mk

AllDIRS := $(SDK_ROOT)
AllDIRS += $(shell ls -R . | grep '^\./.*:$$' | awk '{gsub(":","");print}' | \
                    grep -v '^./src$$' | grep -v '^./src/' | grep -v '^./tools')
AllDIRS += $(shell ls -R ./src | grep '^\./.*:$$' | awk '{gsub(":","");print}')

LDFLAGS += -lsample_comm -lrtsp -llog -lcommcore -lservo

# ==========================================
# 编译选项配置 (已添加 -O3 优化)
# ==========================================
CFLAGS += $(foreach dir, $(AllDIRS), -I$(dir))
CFLAGS += -O3
CFLAGSCXX := $(CFLAGS)

SRC_C := $(foreach dir,$(AllDIRS),$(wildcard $(dir)/*.c))

SRC_CXX := $(foreach dir, $(AllDIRS), $(wildcard $(dir)/*.cpp))

OBJ_C := $(SRC_C:%.c=%.o)
MK_C := $(OBJ_C:%.o=%.d)
OBJ_CXX := $(SRC_CXX:%.cpp=%.o)
MK_CXX := $(OBJ_CXX:%.o=%.d)

# rules
.PHONY: prepare all install clean distclean

all: prepare $(TARGET) install

prepare:
	mkdir -p $(OBJ_DIR)
	mkdir -p $(MK_DIR)
	mkdir -p $(TARGET_DIR)

install:
	@mv $(TARGET) $(TARGET_DIR)
ifneq ($(OBJ_C), )
	@mv $(OBJ_C) $(OBJ_DIR)
	@mv $(MK_C) $(MK_DIR)
endif
ifneq ($(OBJ_CXX), )
	@mv $(OBJ_CXX) $(OBJ_DIR)
	@mv $(MK_CXX) $(OBJ_DIR)
endif

clean:
	find . -name "*.o" -delete
	find . -name "*.d" -delete
	rm -rf $(OBJ_DIR)
	rm -rf $(MK_DIR)
	rm -rf $(TARGET_DIR)

distclean:clean

# $(TARGET): $(OBJ_C) $(OBJ_CXX)
#   $(CC) $(CFLAGS) $(CFLAGSCXX) -o $@ $^ $(LDFLAGS)
$(TARGET): $(OBJ_C) $(OBJ_CXX)
	$(CXX) $(CFLAGS) $(CFLAGSCXX) -o $@ $^ $(LDFLAGS)

%.o:%.c
	$(CC) -c $< -MM -MT $@ -MF $(@:.o=.d) $(CFLAGS)
	$(CC) -c $< -o $@ -fPIC $(CFLAGS)

%.o:%.cpp
	$(CXX) -c $< -MM -MT $@ -MF $(@:.o=.d) $(CFLAGSCXX)
	$(CXX) -c $< -o $@ -fPIC $(CFLAGSCXX)