#!/bin/bash
#  此脚本用于修复文件和目录的权限，自动更改文件的修改时间和版本。
#  2025-11-24: 增加对 python 的处理
#######################################################################
_thisdir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
_thisname=$(basename "${BASH_SOURCE[0]}")
_thisfile="$(cd "$_thisdir" && pwd)/$_thisname"

if [ -n "$SUDO_USER" ]; then
    # 如果脚本是通过 sudo 运行的，那么`SUDO_USER`就是调用 sudo 的普通用户
    # root1:root1
    CURRENT_USER="$SUDO_USER"
    CURRENT_GROUP=$(id -gn "$SUDO_USER")
else
    # 如果脚本是以 root 身份直接运行的（没有通过sudo），那么`SUDO_USER`为空
    # root:root
    CURRENT_USER="$USER"
    CURRENT_GROUP=$(id -gn "$USER")
fi

set -o nounset  # Treat unset variables as an error
#set -o errexit  # Treat any error as exit

# 配置全局变量
AUTHOR=$(cat "$_thisdir"/AUTHOR)
COPYRIGHT=$(cat "$_thisdir"/COPYRIGHT)

###############################################################
# 配置：需要忽略的目录列表
IGNORE_DIRS=(
    ".git"          # Git仓库目录
    ".svn"          # SVN仓库目录
    ".hg"           # Mercurial仓库目录
    ".idea"         # JetBrains IDE配置
    ".DS_Store"     # macOS系统文件
    ".spec-workflow" # spec-workflow目录
    "node_modules"  # Node.js依赖
    "__pycache__"   # Python缓存
    ".vscode"       # vscode配置目录
    "build"         # 构建目录
    "dist"          # 分发目录
    "target"        # Maven构建目录
    "deps"          # 依赖的库路径
    "packages"      # 备份软件包路径
    "logs"          # 日志文件路径
)

# 配置：定义需要设置为755的文件扩展名
EXEC_EXTS=("sh" "so" "exe" "bat")

# 配置：定义需要版本修订的文件扩展名
SRC_EXTS=("h" "c" "cpp" "cxx" "hpp" "java" "py")

###############################################################
# 构建find排除参数
build_ignore_prune() {
    local prune_args=()
    prune_args+=("(")
    
    local first=true
    for dir in "${IGNORE_DIRS[@]}"; do
        if [ "$first" = true ]; then
            first=false
        else
            prune_args+=(-o)
        fi
        prune_args+=(-path "./$dir" -prune)
    done
    
    prune_args+=(")" -o)
    echo "${prune_args[@]}"
}

revise_repo_priv() {
    echo " + 正在修复目录权限为 755..."
    find . $(build_ignore_prune) -type d -exec chmod 755 {} \;

    echo " + 正在设置所有文件和目录的所有者为: $CURRENT_USER:$CURRENT_GROUP"
    find . $(build_ignore_prune) -exec chown "$CURRENT_USER:$CURRENT_GROUP" {} \;

    echo " + 设置所有文件默认权限为 644..."
    find . $(build_ignore_prune) -type f -exec chmod 644 {} \;

    echo " + 为可执行文件设置 755 权限..."

    # 构建可执行文件扩展名条件
    exec_conditions=()
    exec_conditions+=("(")

    first=true
    for ext in "${EXEC_EXTS[@]}"; do
        if [ "$first" = true ]; then
            first=false
        else
            exec_conditions+=(-o)
        fi
        exec_conditions+=(-name "*.$ext")
    done
    exec_conditions+=(")")

    find . $(build_ignore_prune) -type f "${exec_conditions[@]}" -exec chmod 755 {} \;

    echo " + 所有者: $CURRENT_USER:$CURRENT_GROUP"
    echo " + 目录权限: 755 (drwxr-xr-x)"
    echo " + 文件默认权限: 644 (-rw-r--r--)"
    echo " + 可执行文件扩展名: ${EXEC_EXTS[*]} 设置为 755 (-rwxr-xr-x)"
    echo " + 已忽略目录: ${IGNORE_DIRS[*]}"
    echo -e "✅ 权限修复完成。\n"
}

###############################################################
# 全局变量声明
declare -g FileName Author Copyright
declare -g accessTimeFmt modifyTimeFmt createTimeFmt
declare -g fileLastDate fileLastVersion hasSince

# 处理单个 py 文件
revise_py() {
    # 获取文件时间信息
    get_file_time "$file"

    # 提取元数据
    extract_meta_py "$file"

    # 更新元数据
    update_metadata_py "$file"

    # 恢复文件时间？
    restore_filetime "$file"

    # 检查并升级版本
    check_upgrade_version_py "$file"

    # 清理文件格式
    cleanup_file "$file"

    # 恢复文件时间？
    restore_filetime "$file"
}

# 处理单个文件
revise_file() {
    local file="$1"
    FileName=$(basename "$file")

    # 如果是 py 文件，则调用 revise_py 函数
    if [[ "$file" == *.py ]]; then
        revise_py "$file"
        return
    fi

    # 获取文件时间信息
    get_file_time "$file"

    # 提取元数据
    extract_meta "$file"

    # 更新元数据
    update_metadata "$file"

    # 恢复文件时间？
    restore_filetime "$file"

    # 检查并升级版本
    check_upgrade_version "$file"

    # 清理文件格式
    cleanup_file "$file"

    # 恢复文件时间？
    restore_filetime "$file"
}

restore_filetime() {
    local file="$1"
    touch -a -c -d "${accessTimeFmt}" "$file"
    touch -m -c -d "${modifyTimeFmt}" "$file"
}

# 获取文件时间信息
get_file_time() {
    local file="$1"
    local access_time modify_time create_time
    
    # 获取原始时间
    if stat -f "%Sm" "$file" &>/dev/null; then
        # macOS
        access_time=$(stat -f "%Sa" "$file")
        modify_time=$(stat -f "%Sm" "$file")
        create_time=$(stat -f "%Sc" "$file")
    else
        # Linux
        access_time=$(stat -c "%x" "$file")
        modify_time=$(stat -c "%y" "$file")
        create_time=$(stat -c "%w" "$file")
    fi
    
    # 格式化时间
    format_time() {
        local time_str="$1"
        if [[ "$time_str" == "-" ]]; then
            date -d "$2" +"%Y-%m-%d %H:%M:%S"
        else
            date -d "$time_str" +"%Y-%m-%d %H:%M:%S"
        fi
    }

    # 设置全局变量
    accessTimeFmt=$(format_time "$access_time" "$modify_time")
    modifyTimeFmt=$(date -d "$modify_time" +"%Y-%m-%d %H:%M:%S")
    createTimeFmt=$(format_time "$create_time" "$modify_time")
}

# 从文件头提取元数据
extract_meta() {
    local file="$1"
    local header_content=$(sed -n '1,100p' "$file")

    # 重置全局变量
    fileLastDate=""
    fileLastVersion=""
    hasSince=0
    
    # 尝试匹配C风格注释 (/** ... */)
    fileLastDate=$(echo "$header_content" | 
        grep -m1 -E '^\s*\*\s*@date\>' | 
        awk '{print $3" "$4}')

    fileLastVersion=$(echo "$header_content" | 
        grep -m1 -E '^\s*\*\s*@version\>' | 
        awk '{print $3}')

    hasSince=$(echo "$header_content" | 
        grep -c -E '^\s*\*\s*@since\>')
}

# 清理文件格式
cleanup_file() {
    local file="$1"
    
    # 统一换行符为LF
    sed -i 's/\r//g' "$file"
    
    # 删除行尾空白
    sed -i 's/[[:space:]]*$//' "$file"
}

# 检查并补充缺失的元数据内容
ensure_metadata_content() {
    local file="$1"
    
    # 1. 确保@file有文件名
    if grep -q '^\s*\*\s*@file\>\s*$' "$file"; then
        sed -i '1,100 {
            /^\s*\*\s*@file\>\s*$/ {
                s/@file\s*$/@file '"$FileName"'/
            }
        }' "$file"
    fi
    
    # 2. 确保@author有内容
    if grep -q '^\s*\*\s*@author\>\s*$' "$file"; then
        sed -i '1,100 {
            /^\s*\*\s*@author\>\s*$/ {
                s/@author\s*$/@author '"$AUTHOR"'/
            }
        }' "$file"
    fi
    
    # 3. 确保@copyright有内容
    if grep -q '^\s*\*\s*@copyright\>\s*$' "$file"; then
        sed -i '1,100 {
            /^\s*\*\s*@copyright\>\s*$/ {
                s/@copyright\s*$/@copyright '"$COPYRIGHT"'/
            }
        }' "$file"
    fi
    
    # 4. 确保@version有内容
    if grep -q '^\s*\*\s*@version\>\s*$' "$file"; then
        # 如果已经有版本号但被清空，使用0.0.1
        sed -i '1,100 {
            /^\s*\*\s*@version\>\s*$/ {
                s/@version\s*$/@version 0.0.1/
            }
        }' "$file"
    fi
    
    # 5. 确保@since有内容
    if grep -q '^\s*\*\s*@since\>\s*$' "$file"; then
        sed -i '1,100 {
            /^\s*\*\s*@since\>\s*$/ {
                s/@since\s*$/@since '"$createTimeFmt"'/
            }
        }' "$file"
    fi
    
    # 6. 确保@date有内容
    if grep -q '^\s*\*\s*@date\>\s*$' "$file"; then
        sed -i '1,100 {
            /^\s*\*\s*@date\>\s*$/ {
                s/@date\s*$/@date '"$modifyTimeFmt"'/
            }
        }' "$file"
    fi
}

# 更新文件头元数据
update_metadata() {
    local file="$1"
    
    # 首先确保所有元数据标签都有内容
    ensure_metadata_content "$file"
    
    # 1. 更新@file（总是设置正确的文件名）
    sed -i '1,100 {
        /^\s*\*\s*@file\>/ {
            s/@file\s\+[^ ]*/@file '"$FileName"'/
        }
    }' "$file"
    
    # 2. 添加@author（如果不存在）
    if ! grep -q '^\s*\*\s*@author\>' "$file"; then
        sed -i '1,100 {
            /^\s*\*\s*@file\>/ a\
 * @author '"$AUTHOR"'
        }' "$file"
    fi
    
    # 3. 添加@copyright（如果不存在）
    if ! grep -q '^\s*\*\s*@copyright\>' "$file"; then
        sed -i '1,100 {
            /^\s*\*\s*@author\>/ a\
 * @copyright '"$COPYRIGHT"'
        }' "$file"
    fi
    
    # 4. 添加@version（如果不存在）
    if ! grep -q '^\s*\*\s*@version\>' "$file"; then
        sed -i '1,100 {
            /^\s*\*\s*@copyright\>/ a\
 * @version 0.0.1
        }' "$file"
    fi
    
    # 5. 添加@since（如果不存在）
    if ! grep -q '^\s*\*\s*@since\>' "$file"; then
        sed -i '1,100 {
            /^\s*\*\s*@version\>/ a\
 * @since '"$createTimeFmt"'
        }' "$file"
    fi
    
    # 6. 更新@date（总是设置正确的修改时间）
    if grep -q '^\s*\*\s*@date\>' "$file"; then
        sed -i '1,100 {
            /^\s*\*\s*@date\>/ {
                s/@date\s\+[0-9:-]\+ [0-9:]\+/@date '"$modifyTimeFmt"'/
            }
        }' "$file"
    else
        sed -i '1,100 {
            /^\s*\*\s*@since\>/ a\
 * @date '"$modifyTimeFmt"'
        }' "$file"
    fi
}

# 检查并升级版本
check_upgrade_version() {
    local file="$1"
    
    # 检查是否需要升级版本
    if [[ -n "$fileLastVersion" && "$modifyTimeFmt" > "$fileLastDate" ]]; then
        # 自动升级版本号
        upgrade_version() {
            IFS='.' read -r major minor revision <<< "$1"
            major=${major:-0}
            minor=${minor:-0}
            revision=${revision:-0}
            
            if ((revision < 99)); then
                revision=$((revision + 1))
            elif ((minor < 9)); then
                minor=$((minor + 1))
                revision=0
            else
                major=$((major + 1))
                minor=0
                revision=0
            fi
            echo "${major}.${minor}.${revision}"
        }
        
        newVersion=$(upgrade_version "$fileLastVersion")
        
        # 更新文件中的版本号
        sed -i '1,100 {
            /^\s*\*\s*@version\>/ {
                s/@version\s\+[0-9.]\+/@version '"$newVersion"'/
            }
        }' "$file"
        
        echo "版本已升级: $fileLastVersion => $newVersion ($file)"
    fi
}

# 处理 py 文件
extract_meta_py() {
    local file="$1"
    local header_content=$(sed -n '1,100p' "$file")

    # 重置全局变量
    fileLastDate=""
    fileLastVersion=""
    hasSince=0
    
    # 尝试匹配C风格注释 (/** ... */)
    fileLastDate=$(echo "$header_content" | 
        grep -m1 -E '^\s*#\s@date\>' | 
        awk '{print $3" "$4}')

    fileLastVersion=$(echo "$header_content" | 
        grep -m1 -E '^\s*#\s@version\>' | 
        awk '{print $3}')

    hasSince=$(echo "$header_content" | 
        grep -c -E '^\s*#\s@since\>')
}

# 检查并补充缺失的元数据内容
ensure_metadata_content_py() {
    local file="$1"
    
    # 1. 确保@file有文件名
    if grep -q '^\s*#\s@file\>\s*$' "$file"; then
        sed -i '1,100 {
            /^\s*#\s@file\>\s*$/ {
                s/@file\s*$/@file '"$FileName"'/
            }
        }' "$file"
    fi
    
    # 2. 确保@author有内容
    if grep -q '^\s*#\s@author\>\s*$' "$file"; then
        sed -i '1,100 {
            /^\s*#\s@author\>\s*$/ {
                s/@author\s*$/@author '"$AUTHOR"'/
            }
        }' "$file"
    fi
    
    # 3. 确保@copyright有内容
    if grep -q '^\s*#\s@copyright\>\s*$' "$file"; then
        sed -i '1,100 {
            /^\s*#\s@copyright\>\s*$/ {
                s/@copyright\s*$/@copyright '"$COPYRIGHT"'/
            }
        }' "$file"
    fi
    
    # 4. 确保@version有内容
    if grep -q '^\s*#\s@version\>\s*$' "$file"; then
        # 如果已经有版本号但被清空，使用0.0.1
        sed -i '1,100 {
            /^\s*#\s@version\>\s*$/ {
                s/@version\s*$/@version 0.0.1/
            }
        }' "$file"
    fi
    
    # 5. 确保@since有内容
    if grep -q '^\s*#\s@since\>\s*$' "$file"; then
        sed -i '1,100 {
            /^\s*#\s@since\>\s*$/ {
                s/@since\s*$/@since '"$createTimeFmt"'/
            }
        }' "$file"
    fi
    
    # 6. 确保@date有内容
    if grep -q '^\s*#\s@date\>\s*$' "$file"; then
        sed -i '1,100 {
            /^\s*#\s@date\>\s*$/ {
                s/@date\s*$/@date '"$modifyTimeFmt"'/
            }
        }' "$file"
    fi
}

update_metadata_py() {
    local file="$1"
    
    # 首先确保所有元数据标签都有内容
    ensure_metadata_content_py "$file"
    
    # 1. 更新@file（总是设置正确的文件名）
    sed -i '1,100 {
        /^\s*#\s@file\>/ {
            s/@file\s\+[^ ]*/@file '"$FileName"'/
        }
    }' "$file"
    
    # 2. 添加@author（如果不存在）
    if ! grep -q '^\s*#\s@author\>' "$file"; then
        sed -i '1,100 {
            /^\s*#\s@file\>/ a\
 * @author '"$AUTHOR"'
        }' "$file"
    fi
    
    # 3. 添加@copyright（如果不存在）
    if ! grep -q '^\s*#\s@copyright\>' "$file"; then
        sed -i '1,100 {
            /^\s*#\s@author\>/ a\
 * @copyright '"$COPYRIGHT"'
        }' "$file"
    fi
    
    # 4. 添加@version（如果不存在）
    if ! grep -q '^\s*#\s@version\>' "$file"; then
        sed -i '1,100 {
            /^\s*#\s@copyright\>/ a\
 * @version 0.0.1
        }' "$file"
    fi
    
    # 5. 添加@since（如果不存在）
    if ! grep -q '^\s*#\s@since\>' "$file"; then
        sed -i '1,100 {
            /^\s*#\s@version\>/ a\
 * @since '"$createTimeFmt"'
        }' "$file"
    fi
    
    # 6. 更新@date（总是设置正确的修改时间）
    if grep -q '^\s*#\s@date\>' "$file"; then
        sed -i '1,100 {
            /^\s*#\s@date\>/ {
                s/@date\s\+[0-9:-]\+ [0-9:]\+/@date '"$modifyTimeFmt"'/
            }
        }' "$file"
    else
        sed -i '1,100 {
            /^\s*#\s@since\>/ a\
 * @date '"$modifyTimeFmt"'
        }' "$file"
    fi
}

check_upgrade_version_py() {
    local file="$1"
    
    # 检查是否需要升级版本
    if [[ -n "$fileLastVersion" && "$modifyTimeFmt" > "$fileLastDate" ]]; then
        # 自动升级版本号
        upgrade_version() {
            IFS='.' read -r major minor revision <<< "$1"
            major=${major:-0}
            minor=${minor:-0}
            revision=${revision:-0}
            
            if ((revision < 99)); then
                revision=$((revision + 1))
            elif ((minor < 9)); then
                minor=$((minor + 1))
                revision=0
            else
                major=$((major + 1))
                minor=0
                revision=0
            fi
            echo "${major}.${minor}.${revision}"
        }
        
        newVersion=$(upgrade_version "$fileLastVersion")

        # 更新文件中的版本号
        sed -i '1,100 {
            /^\s*#\s@version\>/ {
                s/@version\s\+[0-9.]\+/@version '"$newVersion"'/
            }
        }' "$file"
        
        echo "版本已升级: $fileLastVersion => $newVersion ($file)"
    fi
}

###############################################################
echo "1) 修复目录和文件权限..."
revise_repo_priv

echo "2) 修订源文件时间和版本..."
# 构建源码文件扩展名条件
src_conditions=()
src_conditions+=("(")
first=true
for ext in "${SRC_EXTS[@]}"; do
    if [ "$first" = true ]; then
        first=false
    else
        src_conditions+=(-o)
    fi
    src_conditions+=(-name "*.$ext")
done
src_conditions+=(")")

# 执行查找并处理
# files=$(find . $(build_ignore_prune) -type f "${src_conditions[@]}" -print0 | xargs -0 -I{} echo "{}")
mapfile -d '' fileslist < <(find . $(build_ignore_prune) -type f "${src_conditions[@]}" -print0)
for fileitem in "${fileslist[@]}"; do
    echo " + 修订: $fileitem"
    revise_file "$fileitem"
done
echo -e "✅ 源文件版本修订完成。\n"
