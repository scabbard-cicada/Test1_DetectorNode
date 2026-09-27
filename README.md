# Test1 DetectorNode 教学文档

本周任务的学习材料，共四份。建议按顺序读。

## 文档说明

| 文档 | 内容 |
|---|---|
| [Something.md](docs/Something.md) | 工程习惯。目录结构为什么这么分、脚本的作用、包由什么组成、接口与封装、数据契约、git 的用法、命名约定，以及常见的坑和排查方法 |
| [Part_of_CMake.md](docs/Part_of_CMake.md) | 本项目的 CMake。`find_package` 找外部库、`package.xml` 与 `ament_cmake_auto` 的分工、component 注册、资源安装 |
| [Part_of_ROS2.md](docs/Part_of_ROS2.md) | 从零写一个节点。节点结构、构造函数的顺序、yaml 参数怎么进到代码里、回调机制、订阅发布、消息构造、launch 文件 |
| [Part_of_Rviz_Foxglove.md](docs/Part_of_Rviz_Foxglove.md) | 可视化调试。RViz2 与 Foxglove 的区别、bridge 的作用与连接方式、在 launch 里自动启动、常见可视化话题 |

前三份对应任务要写的三类东西（节点、CMake、launch），第四份用来看运行效果。

## 环境

文档以 **Ubuntu 24.04 + ROS 2 Jazzy** 为主线。命令里的 ROS 路径统一用
`$ROS_DISTRO`，不需要按发行版手改。

## 反馈

文档有讲得不清楚的地方、或者任务卡住了，直接来问。
