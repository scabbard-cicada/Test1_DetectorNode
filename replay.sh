#!/bin/bash
# 回放录包，供 detector 节点测试用
#
# 用法：
#   bash replay.sh
#
# 录包内容：单机器人平动 + 旋转，约 85 秒，640x384 压缩图像

BAG_NAME="bag/bag"

if [ ! -d "$BAG_NAME" ] || [ ! -f "$BAG_NAME/metadata.yaml" ]; then
    echo "找不到录包: $BAG_NAME"
    echo "请先解压 bag.7z 到项目根目录：7z x bag.7z -o bag"
    exit 1
fi

echo "回放录包: $BAG_NAME"
# 录包里存的是 compressed 图像话题，detector 订阅时用 image_transport 自动解压
ros2 bag play "$BAG_NAME" --loop
