#!/usr/bin/env bash
# 从 c_ffi_template 模板生成一个新项目。
#
# 用法:
#   new-project.sh                 # 交互式输入项目名
#   new-project.sh MyBoard         # 生成到本脚本同级目录 ./MyBoard
#   new-project.sh MyBoard /path/MyBoard
#
# 可用 TEMPLATE_DIR 指定模板目录（默认：本脚本同级的 c_ffi_template）。
#
# 会做的事：
#   1. 复制模板（排除 target/、c_src/build/、.git/）
#   2. 把 c_src/*.ioc 重命名为 <项目名>.ioc
#   3. 把 MXTemplate（CMake/.ioc）和 c_ffi_template（Cargo/Makefile/build.rs/文档）
#      统一替换为 <项目名>
#
# 不会做的事：git init、编译。生成后 cd 进去 make 即可。

set -euo pipefail

script_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
template_root=${TEMPLATE_DIR:-"$script_dir/c_ffi_template"}

if [ ! -d "$template_root" ]; then
    echo "错误: 找不到模板目录: $template_root" >&2
    echo "      可用 TEMPLATE_DIR=/path/to/c_ffi_template 指定" >&2
    exit 1
fi
template_root=$(cd "$template_root" && pwd)

if [ $# -ge 1 ]; then
    name=$1
else
    read -rp "项目名称: " name
fi

if ! [[ $name =~ ^[A-Za-z][A-Za-z0-9_-]*$ ]]; then
    echo "错误: 项目名只能包含字母、数字、- 和 _，且必须以字母开头: '$name'" >&2
    exit 1
fi

dest=${2:-"$(dirname "$template_root")/$name"}

# launch.json 用 ${workspaceFolderBasename} 找 ELF，目录名必须等于 Cargo 包名
if [ "$(basename "$dest")" != "$name" ]; then
    echo "错误: 目标目录名必须和项目名一致: $dest" >&2
    exit 1
fi

# 防止目标在模板内部（tar 复制会自我递归）
case "$dest" in
    "$template_root" | "$template_root"/*)
        echo "错误: 目标目录不能位于模板目录内部: $dest" >&2
        exit 1
        ;;
esac

if [ -e "$dest" ]; then
    echo "错误: 目标已存在: $dest" >&2
    exit 1
fi

for f in Cargo.toml build.rs c_src/CMakeLists.txt; do
    if [ ! -e "$template_root/$f" ]; then
        echo "错误: $template_root 不是模板根目录（缺少 $f）" >&2
        exit 1
    fi
done

echo "模板: $template_root"
echo "目标: $dest"

# ---------- 1. 复制 ----------
mkdir -p "$dest"
tar -C "$template_root" \
    --exclude='./target' \
    --exclude='./c_src/build' \
    --exclude='./.git' \
    -cf - . | tar -C "$dest" -xf -

# ---------- 2. 重命名 .ioc ----------
ioc=$(find "$dest/c_src" -maxdepth 1 -name '*.ioc' -print -quit)
if [ -n "$ioc" ]; then
    mv "$ioc" "$dest/c_src/$name.ioc"
fi

# ---------- 3. 替换项目名 ----------
# 跳过可能被复制进来的生成脚本自身，避免改掉里面的 sed 模式
while IFS= read -r -d '' file; do
    [ "$(basename "$file")" = "new-project.sh" ] && continue
    sed -i "s/MXTemplate/$name/g; s/c_ffi_template/$name/g" "$file"
done < <(
    grep -rlIZ -e 'MXTemplate' -e 'c_ffi_template' "$dest" \
        --exclude-dir=target --exclude-dir=build --exclude-dir=.git || true
)

echo
echo "已生成项目: $dest"
echo
echo "下一步:"
echo "  cd \"$dest\""
echo "  make                              # 构建并生成 hex/bin"
echo "  # 用 STM32CubeMX 打开 c_src/$name.ioc 继续配置外设"
