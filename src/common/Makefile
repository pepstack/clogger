###############################################################
# Makefile - Build Static Library
#   这个文件基本不需要修改!
# Author: 350137278@qq.com
# @since: 2025-08-25 13:22:09
# @date: 2025-11-30 18:12:00
###############################################################
# 引入平台检测配置
include ../../Makefile.target

.DEFAULT_GOAL := all

# 根据编译的平台架构和依赖更改: Makefile.config
include Makefile.config

CC ?= $(COMPILER_C)

# 模块名称
MODULE ?= $(MODULE_DIR)
MODULE_AUTHOR := $(shell cat $(WORKSPACE_FOLDER)/AUTHOR)
MODULE_VERSION := $(shell cat $(SOURCE_PREFIX)/$(MODULE)/VERSION)
MAJOR_VERSION := $(shell echo $(MODULE_VERSION) | cut -d. -f1)

# 源代码路径前缀
SOURCE_PREFIX ?= .

# 构建输出路径前缀
TARGET_PREFIX ?= .

# 安装目录(默认: /usr/local)
INSTALL_PREFIX ?= $(WORKSPACE_FOLDER)/target

# Installationn headers
INSTALL_HEADERS = $(SOURCE_PREFIX)/$(MODULE_DIR)/HEADERS

# 库安装目录
INSTALL_LIB_DIR = $(INSTALL_PREFIX)/lib

# 头文件安装目录
INSTALL_INCLUDE_DIR = $(INSTALL_PREFIX)/include

# 编译源和目的路径设置
OBJS_DIR := $(TARGET_PREFIX)/objs/$(MODULE)
LIBS_DIR := $(TARGET_PREFIX)/libs

# Set all dirs for C source: './src/a ./src/b'
ALLCDIRS += $(SOURCE_PREFIX)/$(MODULE_DIR)

# Given dirs for all header (*.h) files
INCDIRS += -I$(SOURCE_PREFIX) \
		-I$(SOURCE_PREFIX)/$(MODULE_DIR)

# Get pathfiles for C source files: './src/a/1.c ./src/b/2.c'
CSRCS := $(foreach cdir, $(ALLCDIRS), $(wildcard $(cdir)/*.c))
CSRCS += $(MINGW_CSRCS)

# 剔除排除列表中的 C 文件(EXCLUDE_CSRCS 在 Makefile.config 中定义)
CSRCS := $(filter-out $(foreach f,$(EXCLUDE_CSRCS),%/$(f)), $(CSRCS))

# 对象文件应该放在构建目录中，而不是源目录
COBJS = $(addprefix $(OBJS_DIR)/, $(notdir $(CSRCS:.c=.o)))

# 编译目标文件
# Static Library
STATIC_LIB = lib$(MODULE).a

# Dynamic Library
SHARED_LIB = lib$(MODULE).so
SHARED_LIB_MAJOR = $(SHARED_LIB).$(MAJOR_VERSION)

###############################################################
# 打印信息
GOALS := $(if $(MAKECMDGOALS),$(MAKECMDGOALS),all)
$(shell printf "\033[1;36m[%s] <%s> (build=%s, os=%s, arch=%s, bits=%s)\033[0m\n" \
	"$(MODULE)-$(MODULE_VERSION)" "$(GOALS)" "$(BUILD_TYPE)" "$(OS_NAME)" "$(ARCH)" "$(BITS)" >&2)

###############################################################
# Build Target Configuration
.PHONY: all static clean install uninstall csrcs

all: static

# 列出源文件
csrcs:
	@echo "=== CSRCS ==="
	@for src in $(CSRCS); do \
		echo "  $$src"; \
	done
	@echo "=== ($(words $(CSRCS)) files) ==="

static: $(LIBS_DIR)/$(STATIC_LIB)

# http://www.gnu.org/software/make/manual/make.html#Eval-Function
define COBJS_template =
$(OBJS_DIR)/$(basename $(notdir $(1))).o: $(1) | $(OBJS_DIR)
	$(CC) $(CFLAGS) $(TARGET_CFLAGS) -c $(1) $(INCLUDES) $(INCDIRS) -o $(OBJS_DIR)/$(basename $(notdir $(1))).o
endef

$(foreach src,$(CSRCS),$(eval $(call COBJS_template,$(src))))
###############################################################
# 创建目录的规则
$(OBJS_DIR) $(LIBS_DIR):
	@mkdir -p $@

# 静态库构建规则
$(LIBS_DIR)/$(STATIC_LIB): $(COBJS) | $(LIBS_DIR)
	$(AR) rcs $@ $(COBJS)
	@printf "\033[32m[build static $(BUILD_TYPE) success]\033[0m $(STATIC_LIB)\n"

clean:
	-rm -f *.stackdump *.o *.exe
	-rm -rf $(OBJS_DIR)
	-rm -rf $(LIBS_DIR)/$(STATIC_LIB)
	@printf "\033[32mclean success.\033[0m\n"

install:
	@printf "\033[32m$@: target ignored.\033[0m\n"

uninstall:
	@printf "\033[32m$@: target ignored.\033[0m\n"