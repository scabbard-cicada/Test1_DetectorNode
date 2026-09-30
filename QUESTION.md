# DetectorNode

## 一、任务目标

本任务的目标是：在已经给出神经网络推理和灯条验证实现的基础上，完成一个可以运行的
ROS 2 detector 节点。

本任务不考察模型内部算法。模型加载、预处理、推理、后处理、NMS 和灯条验证代码已经
提供完成。你需要完成的是 ROS 2 工程框架：

- 设计节点类和构造函数
- 读取 yaml 参数
- 创建订阅者和发布者
- 编写图像回调函数
- 调用已提供的推理和验证接口
- 完成 CMake 构建配置
- 编写 launch 文件
- 使用 Foxglove 查看运行结果

上一道 `Car_Detector_Test` 主要考察 letterbox、模型输出解码和 NMS；本任务主要考察
ROS 2 节点设计和工程组织，两者考察重点不同，你可以继续学习这个任务中神经网络推理的部分，如果你对此存在疑问的话。

## 二、开始之前

请先阅读仓库根目录 `docs/` 中与本任务对应的材料：

- `docs/Something.md`：如何阅读一个工程、目录结构、脚本、git、接口、封装和排查方法
- `docs/Part_of_CMake.md`：本项目 CMake、`ament_cmake_auto`、依赖和 component 注册
- `docs/Part_of_ROS2.md`：节点、构造函数、回调、参数、订阅发布和 launch
- `docs/Part_of_Rviz_Foxglove.md`：Foxglove、RViz2、bridge 和可视化调试

这些文档是本任务的学习材料。题面只说明任务边界，不重复教程内容。

## 三、已提供内容

以下内容已经给出，不需要修改。下面第三、第四节里的路径都相对
`src/autoaim_detector/`，只有 `compile.sh`、`replay.sh` 和 `docs/` 在仓库根目录：

- `package.xml`：依赖已经列好
- `include/autoaim_detector/infer_engine.hpp`：推理引擎接口
- `include/autoaim_detector/armor_refinator.hpp`：灯条颜色验证接口
- `include/autoaim_detector/detector_common.hpp`：检测结果结构体
- `src/infer_engine.cpp`：模型推理和检测结果后处理
- `src/armor_refinator.cpp`：灯条颜色验证实现
- `models/armor.onnx`：训练好的模型
- `autoaim_common_libs`：公共类型和枚举
- `autoaim_interfaces`：`Detections` 和 `ArmorDetection` 消息
- `compile.sh`：工作空间编译脚本
- `replay.sh`：录包回放脚本

## 四、需要完成的内容

### 1. `src/detector_node.cpp`

这个文件目前不存在，需要从零创建。

节点至少需要完成以下功能：

- 继承 `rclcpp::Node`
- 使用 `rclcpp_components` 注册 component
- 从 `config/params.yaml` 读取任务要求的参数
- 使用 `image_transport` 订阅输入图像
- 在图像回调中完成图像转换、推理、结果转换和发布
- 发布 `autoaim_interfaces/msg/Detections`
- 发布标注图像 `sensor_msgs/msg/Image`
- 发布灯条验证二值图 `sensor_msgs/msg/Image`

输入录包中的图像话题固定为：

```text
/autoaim/camera/image_raw/compressed
```

使用 `image_transport` 订阅时，基础话题名应为：

```text
/autoaim/camera/image_raw
```

传输方式使用 `compressed`。输出话题名由你在 yaml 中设计。

### 2. `config/params.yaml`

推理和灯条验证参数已经给出。你需要补充并在代码中读取以下参数：

- `enable_labeled_image`
- `enable_fps`
- `input_image_topic`
- `labeled_image_topic`
- `binary_image_topic`
- `detections_topic`
- `input_image_transport`

其中输入话题和传输方式的值必须与录包匹配，输出话题名可以按照项目命名习惯自行设计。

### 3. `CMakeLists.txt`

在现有 CMake 配置基础上完成节点的构建配置，使以下内容都能正常工作：

- 推理和验证源码与 `detector_node.cpp` 一起编译
- 正确链接模型推理和 OpenMP 相关依赖
- 将节点注册为 `rclcpp_components` component
- 生成可供 `ros2 run` 和 launch 使用的可执行文件

具体 CMake 知识和本项目使用的构建方式见 `docs/Part_of_CMake.md`。

### 4. `launch/` 中的 launch 文件

在 `launch/` 目录中创建一个 Python launch 文件，完成以下功能：

- 启动 detector 节点
- 加载 `config/params.yaml`
- 启动 `foxglove_bridge`
- 能够通过 Foxglove 查看 detector 发布的标注图像

可选：将 Foxglove bridge 的启动开关和端口做成 launch 参数；也可以将 RViz2 作为可选的
调试节点启动。具体写法见 `docs/Part_of_ROS2.md` 和 `docs/Part_of_Rviz_Foxglove.md`。

## 五、运行验收

编译：

```bash
./compile.sh
```

回放录包：

```bash
bash replay.sh easy
```

启动你写的 launch 文件：

```bash
ros2 launch autoaim_detector <你的launch文件>.launch.py
```

Foxglove 连接：

```text
ws://localhost:8765
```

至少完成以下验证：

- 节点可以正常启动
- 输入图像回调能够被触发
- 检测结果话题持续发布
- 标注图像可以在 Foxglove 中显示
- 图像中能看到装甲板检测结果
- `easy` 和 `normal` 录包都可以运行
- launch 文件可以正确加载 yaml 参数

## 六、提交要求

完成所有部分后，在项目根目录建一个 `result/` 目录，里面放两样东西：

**1. 录屏视频（`.mp4`）**

需要展示：

- 你的节点正在运行
- Foxglove 已经连接到 bridge
- Foxglove 中能够看到 detector 发布的可视化结果
- 注意，不要拍屏，劳累呀。

**2. 一份报告**

格式上Markdown，PDF都可以。回答下面三个问题：

1. 你觉得现在的培训怎么样，难度如何？
2. 对后续的培训有什么建议？
3. 你学到了什么？是否能看出这个任务想考察什么？做的过程中压力如何？

这三个问题没有标准答案，写你真实的想法。觉得难、觉得某部分讲得不清楚
都可以直接说。这份反馈会影响后面几周的培训怎么安排。

如果有没做完的部分，也在报告里写清楚卡在哪里、尝试过什么。

### 发送

压缩打包时注意：

- 整体不超过 20 MB
- 保留 `result/` 目录及其中的录屏和报告
- 不要打包 `docs/`
- 不要打包 `build/`、`install/`、`log/` 等编译产物
- 视频太大就先压一下，或者降低录屏分辨率和码率

提交前建议自己从零走一遍（`rm -rf build install log` 后重新编译），确认能跑通。

把整个.zip发到：

```text
scabbardcicada@gmail.com
```
祝顺利 😊😊😊

纳新好累呀，我已累昏...

<div align="right">—— 浙江大学 RoboMaster 算法组</div>
