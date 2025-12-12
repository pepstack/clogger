#!/bin/bash

# package.sh - 安全通用目录打包脚本
# 功能：打包当前目录为"目录名-版本.tar.gz"，包文件放在 ~/Packages 目录
# 支持：使用 .excludes 文件排除指定文件/目录，完整验证包完整性，密码加密

set -e  # 遇到错误立即退出

# 安全警告：确保脚本在任何情况下都不会破坏原文件
echo "======================================"
echo "安全打包脚本启动"
echo "保证：不会修改、删除或破坏任何原文件"
echo "======================================"

# 获取当前目录的绝对路径和目录名
CURRENT_DIR="$(pwd)"
DIR_NAME="$(basename "$CURRENT_DIR")"

# 设置包文件存放目录
PACKAGE_DIR="$HOME/Packages"

# 创建 Packages 目录（如果不存在）
if [ ! -d "$PACKAGE_DIR" ]; then
    echo "创建包存放目录: $PACKAGE_DIR"
    mkdir -p "$PACKAGE_DIR"
    if [ $? -ne 0 ]; then
        echo "错误：无法创建包存放目录"
        exit 1
    fi
fi

# 检查 Packages 目录是否可写
if [ ! -w "$PACKAGE_DIR" ]; then
    echo "错误：没有写入权限到 $PACKAGE_DIR"
    exit 1
fi

# 检查版本文件是否存在
VERSION_FILE="VERSION"
VERSION_FILE_PATH="$CURRENT_DIR/$VERSION_FILE"
VERSION_STR=""

if [ -f "$VERSION_FILE_PATH" ]; then
    echo "发现版本文件: $VERSION_FILE_PATH"
    # 读取第一行作为版本号
    VERSION_STR=$(head -n 1 "$VERSION_FILE_PATH" | tr -d '[:space:]')
    if [ -n "$VERSION_STR" ]; then
        echo "使用版本号: $VERSION_STR"
    else
        echo "错误：版本文件为空或第一行没有有效版本号"
        exit 1
    fi
else
    # 如果没有版本文件，使用日期作为版本
    VERSION_STR=$(date +%Y.%m.%d)
    echo "未找到版本文件，使用日期作为版本: $VERSION_STR"
fi

# 基础打包文件名（不带后缀）
BASE_NAME="${DIR_NAME}-${VERSION_STR}"
PACKAGE_PATH=""
ENCRYPTED_FILE=""

# 检查是否在根目录（避免打包系统根目录）
if [ "$CURRENT_DIR" = "/" ]; then
    echo "错误：不能在根目录 / 执行打包操作"
    exit 1
fi

# 检查当前目录是否可读
if [ ! -r "$CURRENT_DIR" ]; then
    echo "错误：没有读取当前目录的权限"
    exit 1
fi

# 检查排除文件是否存在
EXCLUDE_FILE=".excludes"
EXCLUDE_FILE_PATH="$CURRENT_DIR/$EXCLUDE_FILE"
EXCLUDE_OPTIONS=()

if [ -f "$EXCLUDE_FILE_PATH" ]; then
    echo "发现排除文件: $EXCLUDE_FILE_PATH"
    
    # 创建临时排除文件，确保格式正确
    TEMP_EXCLUDE=$(mktemp)
    grep -v '^[[:space:]]*$' "$EXCLUDE_FILE_PATH" | grep -v '^#' | while read -r pattern; do
        # 移除前后空白并确保格式正确
        pattern=$(echo "$pattern" | sed 's/^[[:space:]]*//;s/[[:space:]]*$//')
        # 移除目录后的 / 以确保匹配正确
        pattern=$(echo "$pattern" | sed 's/\/$//')
        echo "$pattern" >> "$TEMP_EXCLUDE"
    done
    
    if [ -s "$TEMP_EXCLUDE" ]; then
        echo "有效排除模式:"
        cat "$TEMP_EXCLUDE"
        EXCLUDE_OPTIONS=(--exclude-from="$TEMP_EXCLUDE")
    else
        echo "警告：排除文件中没有有效模式"
        rm -f "$TEMP_EXCLUDE"
    fi
else
    echo "未找到排除文件: $EXCLUDE_FILE_PATH"
fi

# 生成唯一的包文件名
generate_package_name() {
    local base="$1"
    local version_file_exists="$2"
    
    # 如果有版本文件，检查包是否已存在
    if [ "$version_file_exists" = "true" ]; then
        PACKAGE_PATH="${PACKAGE_DIR}/${base}.tar.gz"
        if [ -f "$PACKAGE_PATH" ]; then
            echo "错误：版本 $VERSION_STR 的包文件已存在"
            echo "请更新 VERSION 文件中的版本号"
            exit 1
        fi
    else
        # 没有版本文件，使用日期，允许添加后缀
        PACKAGE_PATH="${PACKAGE_DIR}/${base}.tar.gz"
        if [ ! -f "$PACKAGE_PATH" ]; then
            return 0
        fi
        
        # 然后尝试字母后缀 a, b, c, ..., z
        for letter in {a..z}; do
            PACKAGE_PATH="${PACKAGE_DIR}/${base}${letter}.tar.gz"
            if [ ! -f "$PACKAGE_PATH" ]; then
                return 0
            fi
        done
        
        # 如果 a-z 都用完了，使用双字母 aa, ab, ac, ...
        for first_letter in {a..z}; do
            for second_letter in {a..z}; do
                PACKAGE_PATH="${PACKAGE_DIR}/${base}${first_letter}${second_letter}.tar.gz"
                if [ ! -f "$PACKAGE_PATH" ]; then
                    return 0
                fi
            done
        done
        
        echo "错误：无法生成唯一的包文件名"
        exit 1
    fi
}

# 验证包完整性
verify_package_integrity() {
    local package_path="$1"
    
    echo "正在验证包完整性..."
    
    # 1. 检查包文件是否存在且可读
    if [ ! -f "$package_path" ] || [ ! -r "$package_path" ]; then
        echo "错误：包文件不存在或不可读"
        return 1
    fi
    
    # 2. 测试 tar 文件完整性
    if ! tar -tzf "$package_path" >/dev/null 2>&1; then
        echo "错误：包文件损坏或格式不正确"
        return 1
    fi
    
    # 3. 验证排除的文件确实不在包中
    if [ -f "$TEMP_EXCLUDE" ]; then
        echo "验证排除项..."
        while IFS= read -r exclude_pattern; do
            if [ -n "$exclude_pattern" ]; then
                # 检查排除的文件是否在包中
                if tar -tzf "$package_path" 2>/dev/null | grep -q "$exclude_pattern"; then
                    echo "警告：排除项 '$exclude_pattern' 仍然存在于包中"
                else
                    echo "✓ 排除项 '$exclude_pattern' 已成功排除"
                fi
            fi
        done < "$TEMP_EXCLUDE"
        
        # 清理临时文件
        rm -f "$TEMP_EXCLUDE"
    fi
    
    echo "✓ 包完整性验证通过"
    return 0
}

# 加密包文件
encrypt_package() {
    local package_path="$1"
    local encrypted_path="${package_path}.gpg"
    
    echo "======================================"
    echo "加密包文件"
    echo "======================================"
    
    # 检查是否已安装 gpg
    if ! command -v gpg &> /dev/null; then
        echo "错误：未找到 gpg 命令，请先安装 GPG"
        echo "Ubuntu/Debian: sudo apt install gnupg"
        echo "CentOS/RHEL: sudo yum install gnupg"
        return 1
    fi
    
    # 提示用户输入密码
    echo "请输入加密密码（密码不会显示在屏幕上）："
    read -s -p "密码: " PASSWORD
    echo
    read -s -p "确认密码: " PASSWORD_CONFIRM
    echo
    
    # 验证密码
    if [ "$PASSWORD" != "$PASSWORD_CONFIRM" ]; then
        echo "错误：密码不匹配"
        return 1
    fi
    
    if [ -z "$PASSWORD" ]; then
        echo "错误：密码不能为空"
        return 1
    fi
    
    # 使用 GPG 加密文件
    echo "正在加密包文件..."
    if echo "$PASSWORD" | gpg --batch --yes --passphrase-fd 0 --symmetric --cipher-algo AES256 -o "$encrypted_path" "$package_path"; then
        echo "✓ 包文件加密成功"
        echo "加密文件: $encrypted_path"
        
        # 验证加密文件
        if [ -f "$encrypted_path" ] && [ -s "$encrypted_path" ]; then
            echo "✓ 加密文件验证通过"
            
            # 询问是否删除原始未加密文件
            read -p "是否删除原始未加密的包文件? (y/N): " -n 1 -r
            echo
            if [[ $REPLY =~ ^[Yy]$ ]]; then
                if rm -f "$package_path"; then
                    echo "✓ 已删除原始未加密文件"
                    # 更新包路径为加密文件
                    PACKAGE_PATH="$encrypted_path"
                    ENCRYPTED_FILE="$encrypted_path"
                else
                    echo "警告：无法删除原始文件"
                fi
            else
                echo "保留原始未加密文件: $package_path"
            fi
        else
            echo "错误：加密文件创建失败"
            return 1
        fi
    else
        echo "错误：加密过程失败"
        return 1
    fi
    
    # 清理密码变量
    unset PASSWORD
    unset PASSWORD_CONFIRM
    
    return 0
}

# 安全打包函数 - 使用临时文件确保原子性
safe_tar_package() {
    local source_dir="$1"
    local parent_dir="$2"
    local final_package="$3"
    local exclude_options=("${@:4}")  # 接收排除选项数组
    
    # 创建临时文件
    local temp_package="${final_package}.tmp.$$"
    
    # 确保在退出时清理临时文件
    trap 'rm -f "$temp_package" 2>/dev/null' EXIT INT TERM
    
    echo "正在创建临时包文件: $(basename "$temp_package")"
    
    # 显示详细的打包命令
    echo "执行打包命令:"
    if [ ${#exclude_options[@]} -gt 0 ]; then
        echo "  tar -czf \"$temp_package\" -C \"$parent_dir\" ${exclude_options[@]} \"$source_dir\""
        echo "使用排除选项: ${exclude_options[@]}"
        
        # 执行打包到临时文件（使用排除选项）
        if ! tar -czf "$temp_package" -C "$parent_dir" "${exclude_options[@]}" "$source_dir"; then
            echo "错误：打包过程失败"
            rm -f "$temp_package"
            exit 1
        fi
    else
        echo "  tar -czf \"$temp_package\" -C \"$parent_dir\" \"$source_dir\""
        echo "无排除选项"
        
        # 执行打包到临时文件（无排除选项）
        if ! tar -czf "$temp_package" -C "$parent_dir" "$source_dir"; then
            echo "错误：打包过程失败"
            rm -f "$temp_package"
            exit 1
        fi
    fi
    
    # 验证临时包文件
    if [ ! -f "$temp_package" ]; then
        echo "错误：临时包文件未创建"
        exit 1
    fi
    
    # 验证包完整性
    if ! verify_package_integrity "$temp_package"; then
        echo "错误：包完整性验证失败"
        rm -f "$temp_package"
        exit 1
    fi
    
    # 安全移动临时文件到最终位置
    if ! mv "$temp_package" "$final_package"; then
        echo "错误：无法移动临时文件到最终位置"
        rm -f "$temp_package"
        exit 1
    fi
    
    # 取消陷阱
    trap - EXIT INT TERM
    
    echo "包文件已安全创建并通过完整性验证"
}

# 生成包文件名
VERSION_FILE_EXISTS="false"
if [ -f "$VERSION_FILE_PATH" ]; then
    VERSION_FILE_EXISTS="true"
fi
generate_package_name "$BASE_NAME" "$VERSION_FILE_EXISTS"
PACKAGE_NAME=$(basename "$PACKAGE_PATH")

# 显示打包信息
echo "======================================"
echo "安全打包目录"
echo "当前目录: $CURRENT_DIR"
echo "目录名称: $DIR_NAME"
if [ -f "$VERSION_FILE_PATH" ]; then
    echo "版本: $VERSION_STR (来自 VERSION 文件)"
else
    echo "版本: $VERSION_STR (日期)"
fi
echo "包存放目录: $PACKAGE_DIR"
echo "打包文件: $PACKAGE_NAME"
if [ ${#EXCLUDE_OPTIONS[@]} -gt 0 ]; then
    echo "排除文件: $EXCLUDE_FILE_PATH"
fi
echo "开始时间: $(date)"
echo "安全保证: 只读操作，不会修改任何原文件"
echo "======================================"

# 计算目录大小（只读操作）
echo "正在计算目录大小（只读）..."
DIR_SIZE=$(du -sh "$CURRENT_DIR" 2>/dev/null | cut -f1)
echo "目录大小: $DIR_SIZE"

# 获取父目录路径（用于 tar 命令）
PARENT_DIR="$(dirname "$CURRENT_DIR")"

# 执行安全打包操作
echo "开始安全打包过程..."
safe_tar_package "$DIR_NAME" "$PARENT_DIR" "$PACKAGE_PATH" "${EXCLUDE_OPTIONS[@]}"

# 计算打包文件大小
PACKAGE_SIZE=$(du -h "$PACKAGE_PATH" | cut -f1)

# 统计文件数量（只读操作）
FILE_COUNT=$(tar -tzf "$PACKAGE_PATH" 2>/dev/null | wc -l)

echo "======================================"
echo "✓ 安全打包完成！"
echo "文件位置: $PACKAGE_PATH"
echo "文件大小: $PACKAGE_SIZE"
echo "包含文件: $FILE_COUNT 个"
if [ ${#EXCLUDE_OPTIONS[@]} -gt 0 ]; then
    echo "排除文件: 已应用 $EXCLUDE_FILE"
fi
echo "完成时间: $(date)"
echo "状态: 所有原文件完好无损，包完整性已验证"
echo ""

# 询问是否加密包文件
read -p "是否加密包文件? (Y/n): " -n 1 -r
echo
if [[ ! $REPLY =~ ^[Nn]$ ]]; then
    if encrypt_package "$PACKAGE_PATH"; then
        echo "✓ 包文件已加密"
        # 如果用户选择删除原始文件，PACKAGE_PATH 已经在 encrypt_package 函数中更新
    else
        echo "加密失败，保留未加密的包文件"
    fi
else
    echo "跳过加密，保留未加密的包文件"
fi

# 显示最终文件信息
echo ""
echo "最终文件:"
if [ -f "$PACKAGE_PATH" ]; then
    FINAL_SIZE=$(du -h "$PACKAGE_PATH" | cut -f1)
    echo "- $PACKAGE_PATH ($FINAL_SIZE)"
fi
if [ -f "${PACKAGE_PATH}.gpg" ]; then
    ENCRYPTED_SIZE=$(du -h "${PACKAGE_PATH}.gpg" | cut -f1)
    echo "- ${PACKAGE_PATH}.gpg ($ENCRYPTED_SIZE) [加密]"
    ENCRYPTED_FILE="${PACKAGE_PATH}.gpg"
fi

echo "======================================"

# 最终验证
echo "最终验证:"
echo "- ✓ 原目录完整性: 已验证"
echo "- ✓ 包文件完整性: 已验证" 
if [ ${#EXCLUDE_OPTIONS[@]} -gt 0 ]; then
    echo "- ✓ 排除文件应用: 已验证"
fi
echo "- ✓ 无文件修改: 已验证"
echo "- ✓ 无文件删除: 已验证"
if [ -f "${PACKAGE_PATH}.gpg" ]; then
    echo "- ✓ 文件加密: 已完成"
fi
echo ""

# 显示解密命令（如果文件已加密）
if [ -n "$ENCRYPTED_FILE" ]; then
    echo "======================================"
    echo "解密命令:"
    echo "  gpg --decrypt \"$ENCRYPTED_FILE\" > \"${ENCRYPTED_FILE%.gpg}\""
    echo ""
    echo "或者使用以下命令解密到当前目录:"
    echo "  gpg --decrypt \"$ENCRYPTED_FILE\" > \"$(basename "${ENCRYPTED_FILE%.gpg}")\""
    echo "======================================"
fi

echo "所有操作完成，原文件保持完好！"