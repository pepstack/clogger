###############################################################################
# Makefile - 总控 Makefile：不需要更改！
#
#   如果配置构建项目，请更改：Makefile.modules
#
# Author: 350137278@qq.com
# @since: 2025-08-25 13:22:09
# @date: 2025-08-30 18:12:00
###############################################################################
# 引入平台检测配置
include Makefile.target
.DEFAULT_GOAL := all

# 构建类型（可被命令行覆盖）
BUILD_TYPE ?= release

# 导出变量给子Makefile
export WORKSPACE_FOLDER INSTALL_PREFIX BUILD_TYPE

###############################################################
# 定义构建的项目(请仅更改下面的配置文件的内容)
include Makefile.modules

###############################################################

# 默认目标
.PHONY: all clean cleanall install uninstall info debug release

# 构建所有项目
all: $(MODULES)

# 定义构建项目模板
define build_repo_template =
.PHONY: $(1)
$(1): $$($(1)_DEPS)
	@printf "$(COLOR_CYAN)[构建 $(1)...]$(COLOR_CLR)\n"
	$$(MAKE) -C $$($(1)_DIR)
endef

# 定义清理项目模板
define clean_repo_template =
.PHONY: clean-$(1)
clean-$(1):
	@printf "$(COLOR_CYAN)[清理 $(1)...]$(COLOR_CLR)\n"
	$$(MAKE) -C $$($(1)_DIR) clean
endef

# 定义安装项目模板
define install_repo_template =
.PHONY: install-$(1)
install-$(1): $(1)
	@printf "$(COLOR_CYAN)[安装 $(1)...]$(COLOR_CLR)\n"
	$$(MAKE) -C $$($(1)_DIR) install
endef

# 定义卸载项目模板
define uninstall_repo_template =
.PHONY: uninstall-$(1)
uninstall-$(1):
	@printf "$(COLOR_CYAN)[卸载 $(1)...]$(COLOR_CLR)\n"
	$$(MAKE) -C $$($(1)_DIR) uninstall
endef

# 统计代码文件行数
define loc_repo_template =
.PHONY: loc-$(1)
loc-$(1):
	@printf "$(COLOR_CYAN)[统计 $(1) 代码行数...]$(COLOR_CLR)\n"
	@find $$($(1)_DIR) -name "*.h" -o -name "*.c" -o -name "*.cpp" -o -name "*.cxx" -not -path "*test*" | xargs wc -l | tail -1
endef

# 应用所有模板到所有项目
$(foreach repo,$(MODULES),$(eval $(call build_repo_template,$(repo))))
$(foreach repo,$(MODULES),$(eval $(call clean_repo_template,$(repo))))
$(foreach repo,$(MODULES),$(eval $(call install_repo_template,$(repo))))
$(foreach repo,$(MODULES),$(eval $(call uninstall_repo_template,$(repo))))
$(foreach repo,$(MODULES),$(eval $(call loc_repo_template,$(repo))))

loc:
	@echo 执行统计代码命令: cloc $(SOURCE_PREFIX) $(CLOC_EXCLUDE_DIRS)
	@cloc $(SOURCE_PREFIX) $(CLOC_EXCLUDE_DIRS)

# 定义全局目标
clean: $(addprefix clean-,$(MODULES))
	@printf "$(COLOR_GREEN)所有项目清理完成！$(COLOR_CLR)\n"

cleanall:
	-rm -rf $(BUILD_PREFIX)
	-rm -rf $(WORKSPACE_FOLDER)/msvc/build
	-rm -rf $(WORKSPACE_FOLDER)/msvc/.vs
	@printf "\033[32mcleanall success.\033[0m\n"

install: $(addprefix install-,$(MODULES))
	@printf "$(COLOR_GREEN)所有项目安装完成！$(COLOR_CLR)\n"

uninstall: $(addprefix uninstall-,$(shell echo $(MODULES) | tr ' ' '\n' | tac | tr '\n' ' '))
	@printf "$(COLOR_GREEN)所有项目卸载完成！$(COLOR_CLR)\n"

# 显示项目信息
info:
	@printf "$(COLOR_CYAN)=== 项目结构 ===$(COLOR_CLR)\n"
	@echo "WORKSPACE_FOLDER: $(WORKSPACE_FOLDER)"
	@echo "INSTALL_PREFIX: $(INSTALL_PREFIX)"
	@echo "BUILD_TYPE: $(BUILD_TYPE)"
	@echo ""
	@printf "$(COLOR_CYAN)项目列表:$(COLOR_CLR) $(MODULES)\n"
	@echo ""
	@printf "$(COLOR_CYAN)可用目标:$(COLOR_CLR)\n"
	@echo "  all       - 构建所有项目 (默认)"
	@echo "  clean     - 清理所有项目"
	@echo "  install   - 安装所有项目"
	@echo "  uninstall - 卸载所有项目"
	@echo "  info      - 显示项目信息"
	@echo "  debug     - 设置为调试构建"
	@echo "  release   - 设置为发布构建"
	@echo "  loc       - 统计源代码行数"

# 设置调试构建
debug:
	@printf "$(COLOR_CYAN)设置为 debug 构建模式$(COLOR_CLR)\n"
	$(MAKE) BUILD_TYPE=debug
	@printf "$(COLOR_CYAN)debug 构建模式成功完成。$(COLOR_CLR)\n"

# 设置发布构建
release:
	@printf "$(COLOR_CYAN)设置为 release 构建模式$(COLOR_CLR)\n"
	$(MAKE) BUILD_TYPE=release
	@printf "$(COLOR_CYAN)release 构建模式成功完成。$(COLOR_CLR)\n"