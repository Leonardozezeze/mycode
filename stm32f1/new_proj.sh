#!/bin/bash
# Program:
#   This script creates a new STM32 project from MXTemplate
# History:
# 2026/9/29  Cassian  First release

set -e  # 遇到错误立即退出

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# 获取脚本所在目录
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TEMPLATE_DIR="${SCRIPT_DIR}/MXTemplate"

# 检查模板目录是否存在
if [ ! -d "${TEMPLATE_DIR}" ]; then
    echo -e "${RED}错误：找不到模板目录 MXTemplate${NC}"
    echo "请确保在正确的目录下运行此脚本"
    exit 1
fi

# 获取用户输入并验证
read -p "请输入新项目名称：" projname

# 检查输入是否为空
if [ -z "${projname}" ]; then
    echo -e "${RED}错误：项目名称不能为空${NC}"
    exit 1
fi

# 检查项目名称是否包含非法字符
if [[ ! "${projname}" =~ ^[a-zA-Z0-9_-]+$ ]]; then
    echo -e "${RED}错误：项目名称只能包含字母、数字、下划线和连字符${NC}"
    exit 1
fi

# 检查目标目录是否已存在
TARGET_DIR="${SCRIPT_DIR}/${projname}"
if [ -d "${TARGET_DIR}" ]; then
    echo -e "${YELLOW}警告：目录 '${projname}' 已存在${NC}"
    read -p "是否覆盖？(y/N): " confirm
    if [[ "${confirm}" != "y" && "${confirm}" != "Y" ]]; then
        echo "操作已取消"
        exit 0
    fi
    echo "正在删除已存在的目录..."
    rm -rf "${TARGET_DIR}"
fi

# 复制模板项目
echo "正在复制模板项目..."
cp -r "${TEMPLATE_DIR}" "${TARGET_DIR}"

if [ $? -ne 0 ]; then
    echo -e "${RED}错误：复制模板失败${NC}"
    exit 1
fi

# 进入新项目目录
cd "${TARGET_DIR}"

# 复制并重命名 .ioc 文件
echo "正在配置项目文件..."
cp "MXTemplate.ioc" "${projname}.ioc"

if [ $? -ne 0 ]; then
    echo -e "${RED}错误：复制 .ioc 文件失败${NC}"
    exit 1
fi

# 删除原始的模板 .ioc 文件
rm -f "MXTemplate.ioc"

# 更新 .mxproject 文件中的项目名称（如果存在）
if [ -f ".mxproject" ]; then
    # 备份原文件
    cp ".mxproject" ".mxproject.bak"
    # 替换项目名称
    sed -i "s/MXTemplate/${projname}/g" ".mxproject"
fi

# 更新 CMakeLists.txt 中的项目名称（如果存在）
if [ -f "CMakeLists.txt" ]; then
    sed -i "s/MXTemplate/${projname}/g" "CMakeLists.txt"
fi

# 更新 CMakePresets.json 中的项目名称（如果存在）
if [ -f "CMakePresets.json" ]; then
    sed -i "s/MXTemplate/${projname}/g" "CMakePresets.json"
fi

echo -e "${GREEN}✓ 项目 '${projname}' 创建成功！${NC}"
echo ""
echo "下一步："
echo "  cd ${projname}"
echo "  # 使用 STM32CubeMX 打开 ${projname}.ioc 文件进行配置"
