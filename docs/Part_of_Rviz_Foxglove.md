
<h1 align="center">Part of RViz2 & Foxglove</h1>

可视化工具的作用不是“把画面变好看”，而是把程序内部的状态变成可以观察、比较和定位的
信息。一个节点可能已经编译成功、也没有抛异常，但它订阅错了话题、坐标系错了、时间戳错了，
或者发布的数据根本不是预期内容。只看终端日志很难发现这些问题，把数据画出来通常更直接。

本文以 **Ubuntu 24.04 + ROS 2 Jazzy** 作为教学主线。当前仓库在
**Ubuntu 26.04 + ROS 2 Lyrical** 上验证，Foxglove、RViz2 和 bridge 的使用方式在两套环境中一致。

本项目主要使用两种工具：**RViz2** 和 **Foxglove**。它们都能观察 ROS 2 数据，但定位不同。

## 一、RViz2 与 Foxglove

### RViz2

RViz2 是 ROS 2 生态中的桌面可视化工具，适合观察：

- 坐标系和 TF 树
- 机器人模型、位姿、路径
- 点云、激光、相机图像
- `Marker` 和 `MarkerArray`
- 需要固定坐标系的三维数据

RViz2 通常直接运行在 ROS 2 所在的电脑上。它的布局和显示配置可以保存成 `.rviz` 文件，
下次启动时恢复。

### Foxglove

Foxglove 更像一个通用的机器人数据分析界面，适合观察：

- 图像和压缩图像
- 原始 ROS 2 消息字段
- 话题频率与延迟
- 三维数据、TF、Marker
- 多个话题在同一个时间轴上的关系

Foxglove 的界面可以在浏览器或桌面客户端中运行，布局也方便保存和分享。但它本身不是
ROS 2 节点，不能直接从 ROS 2 的 DDS 网络读取数据，需要一个桥接程序。

### 简单对比

| 对比项 | RViz2 | Foxglove |
|---|---|---|
| 运行方式 | 本机桌面程序 | 浏览器或桌面客户端 |
| ROS 2 连接 | 直接使用 ROS 2 环境 | 通过 WebSocket bridge |
| 强项 | TF、三维坐标、机器人模型 | 图像、原始消息、时间轴、频率分析 |
| 配置方式 | `.rviz` 配置文件 | Foxglove Layout |
| 适合场景 | 坐标系与三维调试 | 视觉结果和话题数据调试 |
| 是否需要 bridge | 不需要 | 需要 |

两者不是互相替代的关系。发现图像框偏了，可以先用 Foxglove 看图像和消息字段，再用 RViz2
检查坐标系；调试三维位姿时，RViz2 更直观；检查一条消息到底有没有发布时，Foxglove 的
Raw Messages 和 Topic Metrics 更方便。

## 二、Foxglove bridge

### bridge 是什么

Foxglove 的数据链路是：

```
ROS 2 节点 → DDS → foxglove_bridge → WebSocket → 浏览器或桌面客户端
```

`foxglove_bridge` 是一个 ROS 2 节点。它订阅当前 ROS 2 系统中的话题，把 ROS 2 消息转换
成 Foxglove 客户端能读取的数据，再通过 WebSocket 服务提供出来。

它不负责生成检测结果，也不负责改变消息内容。它只是把 ROS 2 网络和网页客户端连接起来。

### 启动 bridge

本项目使用系统安装的 bridge，不在仓库源码里。先确认系统能找到它：

```bash
ros2 pkg prefix foxglove_bridge
```

如果安装正常，会输出类似：

```text
/opt/ros/$ROS_DISTRO
```

最简单的启动方式是：

```bash
ros2 run foxglove_bridge foxglove_bridge
```

默认服务地址通常是：

```text
ws://localhost:8765
```

也可以指定端口：

```bash
ros2 run foxglove_bridge foxglove_bridge --ros-args -p port:=9000
```

启动日志里看到下面类似内容，说明 bridge 已经监听成功：

```text
Server listening on port 8765
```

bridge 默认绑定 `0.0.0.0`，表示监听本机所有网卡。这样同一局域网的其他设备理论上也能
连接，但默认没有鉴权。个人电脑本地调试问题不大，共享网络或实车网络中不要随意暴露。

### Foxglove 网页版如何连接

1. 先启动 ROS 2 节点和 `foxglove_bridge`
2. 浏览器打开 [Foxglove Web](https://app.foxglove.dev/)
3. 选择连接或数据源选项
4. 选择 **Foxglove WebSocket**
5. 填写 `ws://localhost:8765`
6. 点击连接

如果 bridge 在另一台电脑上，`localhost` 要换成 bridge 所在电脑的 IP，例如：

```text
ws://192.168.1.20:8765
```

连接不上时，按顺序检查：

```bash
ros2 node list
ros2 topic list
ss -ltn | grep 8765
```

`ros2 node list` 看 bridge 是否启动，`ros2 topic list` 看 ROS 2 环境是否正常，`ss` 看端口
是否处于监听状态。不要一开始就怀疑 Foxglove 面板，先确认 bridge 服务真的存在。

## 三、在 launch 中自动启动

### 自动启动 Foxglove bridge

bridge 可以和自己的节点一起写进 Python launch 文件：

```python
from launch import LaunchDescription
from launch_ros.actions import Node


def generate_launch_description():
    return LaunchDescription([
        Node(
            package='foxglove_bridge',
            executable='foxglove_bridge',
            name='foxglove_bridge',
            output='screen',
            parameters=[{'port': 8765}],
        ),
    ])
```

这样执行一次 `ros2 launch`，bridge 就会被 launch 系统一起启动。项目节点的 launch 也可以
在同一个 `LaunchDescription` 里继续添加其他 `Node`。

更适合调试的写法是增加一个开关：

```python
from launch.actions import DeclareLaunchArgument
from launch.conditions import IfCondition
from launch.substitutions import LaunchConfiguration


DeclareLaunchArgument('foxglove', default_value='true'),
Node(
    package='foxglove_bridge',
    executable='foxglove_bridge',
    parameters=[{'port': 8765}],
    condition=IfCondition(LaunchConfiguration('foxglove')),
),
```

启动时：

```bash
ros2 launch some_package some.launch.py foxglove:=false
```

注意 `DeclareLaunchArgument` 和 `Node` 必须放进返回的 `LaunchDescription([...])` 列表中，
不能只写在函数外面。具体 launch 的参数传递方法见 `Part_of_ROS2.md` 的 launch 一节。

### 自动启动 RViz2

RViz2 可以直接作为一个 launch 节点启动：

```python
from launch_ros.actions import Node


Node(
    package='rviz2',
    executable='rviz2',
    name='rviz2',
    output='screen',
),
```

这会启动 RViz2，但不会自动加载你上次保存的显示布局。要加载 `.rviz` 配置文件，给它传
`-d` 参数：

```python
Node(
    package='rviz2',
    executable='rviz2',
    name='rviz2',
    output='screen',
    arguments=['-d', rviz_config_path],
),
```

`rviz_config_path` 应该通过 `get_package_share_directory()` 拼出来，不要写死绝对路径：

```python
import os
from ament_index_python.packages import get_package_share_directory

rviz_config_path = os.path.join(
    get_package_share_directory('some_package'),
    'rviz',
    'debug.rviz',
)
```

前提是 `debug.rviz` 已经通过 CMake 安装到包的 share 目录。RViz2 的窗口能否成功弹出，
还取决于当前电脑是否有图形桌面。服务器、Docker 或 SSH 无图形转发环境中，launch 可能
启动了进程但没有窗口，这不是 ROS 话题的问题。



## 四、常见可视化话题

可视化之前先用命令行确认话题的实际类型：

```bash
ros2 topic list
ros2 topic type /话题名
ros2 interface show 消息类型
ros2 topic echo /话题名 --once
```

### 图像

常见类型：

```text
sensor_msgs/msg/Image
sensor_msgs/msg/CompressedImage
```

`Image` 是已经解码的图像，常见编码有 `bgr8`、`rgb8`、`mono8`。
`CompressedImage` 通常包含 JPEG 或 PNG 压缩数据。

在本项目的录包中，输入是：

```text
/autoaim/camera/image_raw/compressed
sensor_msgs/msg/CompressedImage
```

detector 使用 `image_transport` 订阅它，回调中拿到解压后的 `sensor_msgs/msg/Image`，再发布
标注后的普通图像。Foxglove 的 Image 面板可以直接显示 `Image`；如果看不到图，先确认
话题确实有数据，以及消息的编码是否被客户端支持。

### CameraInfo

```text
sensor_msgs/msg/CameraInfo
```

它包含相机内参矩阵 `K`、畸变参数 `D`、图像尺寸等信息。做 PnP、畸变校正或三维投影时，
不能只看图像，还要配套的 `CameraInfo`。

本周任务项目录包中的话题是：

```text
/autoaim/camera/camera_info
```

### 位姿与带时间戳的点

常见类型：

```text
geometry_msgs/msg/PoseStamped
geometry_msgs/msg/PointStamped
geometry_msgs/msg/TransformStamped
```

它们除了数值，还带 `header.stamp` 和 `header.frame_id`。在 Foxglove 或 RViz2 中看到一个
点的位置之前，先确认它属于哪个坐标系；同样的 `(1, 0, 0)` 放在不同 frame 中含义不同。

### TF

TF 常见消息类型：

```text
tf2_msgs/msg/TFMessage
```

RViz2 的 TF Display 可以把坐标系树画出来，Foxglove 的 3D 面板也能使用 TF。TF 出问题时，
先检查 frame 是否拼写一致、树是否连通、时间戳是否合理。

### Marker 与 MarkerArray

可视化算法中间结果时常用：

```text
visualization_msgs/msg/Marker
visualization_msgs/msg/MarkerArray
```

`Marker` 可以表示点、线、箭头、立方体、球、文字等。它通常包含：

- `header.frame_id`：画到哪个坐标系
- `ns` 与 `id`：标识一个可更新的图形
- `type`：图形类型
- `pose`：位置和姿态
- `scale`：大小
- `color`：颜色与透明度
- `action`：增加、修改或删除

一个常见的调试思路是：把检测点、预测点、坐标轴和轨迹分别用不同颜色画出来，然后比较
它们是否处在预期位置。不要只画“最终答案”，中间结果往往更能说明问题出在哪一步。

### ROS 2 自定义消息

本项目还有：

```text
autoaim_interfaces/msg/ArmorDetection
```

它们不是 RViz2 或 Foxglove 专门定义的可视化消息，而是业务数据。Foxglove 的 Raw Messages
可以查看它们的字段，Image 面板则不能把一个检测消息当成图像显示。

这一区别要记住：**工具能不能显示，首先取决于消息类型和面板类型是否匹配。**

## 五、本项目的最小调试流程

```bash
# 终端一：加载当前工作空间，回放录包
source install/setup.bash
bash replay.sh easy

# 终端二：启动你写的 detector launch
source install/setup.bash
ros2 launch autoaim_detector <你的launch文件>

# 终端三：确认 bridge 和话题
source install/setup.bash
ros2 node list
ros2 topic list
ros2 topic hz /你的标注图像话题
```

三个终端都要 `source install/setup.bash`，因为环境变量只在当前终端有效。

`source /opt/ros/$ROS_DISTRO/setup.bash` 这句这里没写，因为它一般已经在 `~/.bashrc` 里
（fishros 一键安装会自动加），新终端会自动执行，不需要你再敲。新终端里
`echo $ROS_DISTRO` 有输出就说明已经生效。没输出的话见
`Something.md` 的「要不要 source /opt/ros/$ROS_DISTRO/setup.bash」一节。

然后：

1. Foxglove 网页连接 `ws://localhost:8765`
2. 添加 Image 面板，选择 detector 发布的标注图像话题
3. 如果要看消息字段，添加 Raw Messages 面板
4. 如果要看频率，添加 Topic Metrics，或继续用 `ros2 topic hz`
5. 如果要看坐标系和三维数据，启动 RViz2 并设置正确的 Fixed Frame

遇到问题时，不要先反复点面板。先用 `ros2 topic type`、`ros2 topic hz`、
`ros2 topic echo --once` 确认数据确实存在，再检查可视化工具的配置。

## 六、视频与网站资料

这一部分用于补充实际操作视频。建议按下面的顺序观看，并在自己的工作空间中同步操作：

### RViz2

[在 ROS 中，使用 RViz 观测传感器数据](https://www.bilibili.com/video/BV1Vq2LYoECn/?spm_id_from=333.1007.top_right_bar_window_history.content.click&vd_source=23001627d7f74b28a446ab0c6a4d32db)

观看时重点关注：添加 Image、TF、Marker 等 Display，设置 Fixed Frame，
以及保存和加载 `.rviz` 配置。看完后，自己保存一次 RViz2 配置，再通过 launch
的 `-d` 参数加载它。

### Foxglove

[Foxglove Visualization - a 5 minute introduction（HK）](https://www.youtube.com/watch?v=jtmoPsVd2vg)

先看这段，建立 Foxglove 的整体印象：Layout、面板和数据源分别是什么。看完后，
在本项目里自己完成一次 bridge 启动、网页连接、添加 Image 面板和查看 Raw Messages。

Foxglove 官方资料：

- [Getting Started Guide](https://docs.foxglove.dev/docs/getting-started-guide)
- [Visualization Panels](https://docs.foxglove.dev/docs/visualization/panels)

先阅读 Getting Started Guide，了解连接数据源和基本界面；再查看 Visualization Panels，
按需要选择 Image、Raw Messages、3D 和 Topic Metrics 等面板。



---

祝顺利 😊😊😊

<div align="right">—— 浙江大学 RoboMaster 算法组</div>
