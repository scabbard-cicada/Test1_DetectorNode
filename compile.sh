#!/bin/bash
# 编译整个工作空间
#
# -G Ninja            使用 Ninja 生成器（比 make 快，也是项目既有约定）
#                     需先安装：sudo apt install ninja-build
# --symlink-install   安装时用软链接，改源码无需重新 build 就能生效
# -DCMAKE_EXPORT_COMPILE_COMMANDS=ON  生成 compile_commands.json 供 IDE 索引

colcon build --symlink-install \
  --cmake-args -G Ninja -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
