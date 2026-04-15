ifeq ($(DEFAULT_PLATFORM), rk3576)
# export CORSS := aarch64-buildroot-linux-gnu-
export CORSS := aarch64-none-linux-gnu-
SUBMODULEDIR := RK3576_Glibc$(DEFAULT_GLIBCVersion)_Libs
THIRDDIR := RK3576_ThirdLibs
DEFS := -DDEBUG
else  ifeq ($(DEFAULT_PLATFORM), ubuntu)
export CORSS := 
SUBMODULEDIR := ubuntu_Libs
endif

export CC := $(CORSS)gcc
export CXX := $(CORSS)g++
export AR := $(CORSS)ar

EXTEN_CFLAGS  := $(foreach dir,$(shell [ -d ./$(SUBMODULEDIR)/include ] && find $(SUBMODULEDIR)/include -type d),-I$(dir))
EXTEN_CFLAGS  += $(foreach dir,$(shell [ -d ./$(THIRDDIR)/include ] && find $(THIRDDIR)/include -type d),-I$(dir))
EXTEN_LDFLAGS := $(foreach dir,$(shell [ -d ./$(SUBMODULEDIR)/lib ] && find $(SUBMODULEDIR)/lib -type d),-L$(dir))
EXTEN_LDFLAGS += $(foreach dir,$(shell [ -d ./$(THIRDDIR)/lib ] && find $(THIRDDIR)/lib -type d),-L$(dir))
EXTEN_CFLAGS += $(DEFS)

$(info Current glibc version is :  $(EXTEN_LDFLAGS))

ifeq ($(DEBUG),y)
	export CFLAGS := -Wall -g -O0 -fPIC -fno-strict-aliasing $(EXTEN_CFLAGS)
else
	export CFLAGS := -Wall -O2 -fPIC -fno-strict-aliasing $(EXTEN_CFLAGS)
endif

export LDFLAGS := $(EXTEN_LDFLAGS)

ifneq ($(DEFAULT_PLATFORM), "")
	LDFLAGS += -lpthread -lm -lrockit -lrga -ldrm -lasound -lrkaiq -lmali -lgraphic_lsf -lrockchip_mpp -lrkAlgoDis -lRkSwCac
endif

# RKNN 库路径和链接
LDFLAGS += -L/mnt/A/yt/hust-tracker-3576/src/libs/rknn/rknpu2/Linux/aarch64
LDFLAGS += -lrknnrt

# Tracker 库路径
LDFLAGS += -L/mnt/A/yt/hust-tracker-3576/src/libs
LDFLAGS += -lTrackerLib

LDFLAGS += -lopencv_imgproc -lopencv_core -lopencv_imgcodecs -lopencv_videoio -ldl 

# turbojpeg 库文件所在的目录
LDFLAGS += -L/mnt/A/yt/hust-tracker-3576/src/libs/rknn/jpeg_turbo/Linux/aarch64
LDFLAGS += -lturbojpeg  
LDFLAGS += -lopencv_video

CFLAGS += -fopenmp
LDFLAGS += -lgomp