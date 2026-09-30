# 自瞄检测器节点 —— 技术参考

> **任务说明在仓库根目录的 `QUESTION.md`，不在这里。** 本文档是给你查阅用的技术资料：
> 模型契约、推理后端选型、已提供代码的算法说明。

本节点用神经网络检测装甲板，输出检测框与四个关键点，再用传统视觉做一次灯条颜色
验证。推理后端是 onnxruntime（CPU）。

## 依赖

- ROS2
- OpenCV
- OpenMP
- image_transport / image_transport_plugins
- onnxruntime（`onnxruntime_vendor`，即 apt 包 `ros-$ROS_DISTRO-onnxruntime-vendor`）

## 文件结构说明

```
autoaim_detector/
├── include/autoaim_detector/
│   ├── detector_common.hpp                # [已给] 公共类型：DetectionObject、Keypoint、ColorMapper
│   ├── infer_engine.hpp                   # [已给] 模型契约常量 + InferEngine 基类 + onnxruntime 后端声明
│   └── armor_refinator.hpp                # [已给] 装甲板传统视觉灯条验证器
├── src/
│   ├── infer_engine.cpp                   # [已给] 基类公共实现（解码、NMS、绘图）+ 后端实现 + 工厂函数
│   ├── armor_refinator.cpp                # [已给] 灯条验证实现
│   └── detector_node.cpp                  # [你写] 节点，现在不存在
├── config/params.yaml                     # [部分] 推理参数已给，话题名你补
├── launch/                                # [你写] 空目录
├── models/armor.onnx                      # [已给] 模型权重
├── CMakeLists.txt                         # [部分] 取消注释
├── package.xml                            # [已给] 依赖已列全
└── docs/README.md                         # 本文档
```

任务说明 `QUESTION.md` 在仓库根目录，和 `docs/` 的四份教学文档放在一起：

```
autoaim_Test/
├── QUESTION.md                            # 任务说明
└── docs/                                  # 教学文档
```

只有一个推理后端，所以契约常量、基类和后端实现都放在 `infer_engine.hpp/cpp` 里，
文件内部按"基类公共方法 / 后端实现 / 工厂函数"三段划分。将来加 GPU 后端时把后端
那一段拆成独立文件，基类和契约留在原处。

## 参数配置

参数配置文件位于 `config/params.yaml`。

**已给好的装甲板检测参数**：

- `armor_model_name`: 模型文件名，在 `models/` 目录下查找
- `armor_confidence_threshold`: 置信度阈值
- `armor_keypoint_visibility_threshold`: 关键点可见度阈值，任一关键点低于此值时过滤候选
- `armor_nms_threshold`: NMS 阈值

**已给好的灯条验证参数**（都相对网络输出的灯条高度）：

- `armor_refinator_brightness_thres`: 红蓝通道亮度下限
- `armor_refinator_color_diff_thres`: 对应颜色与另一颜色通道的最小差值
- `armor_refinator_outward_width_ratio`: 从网络灯条内侧边缘向外搜索的宽度比例
- `armor_refinator_inward_tolerance_ratio`: 向装甲板内部保留的搜索容差比例
- `armor_refinator_vertical_margin_ratio`: 灯条上下搜索余量比例
- `armor_refinator_min_color_ratio`: 搜索区域内对应颜色像素的最小占比
- `armor_refinator_filter_unlit`: 是否丢弃验证失败（灯条未点亮）的候选

**需要你补的参数**：话题名，以及你认为该配置化的开关。参数名由你定，
在 `params.yaml` 里声明，代码里用 `declare_parameter` 读。

## 算法讲解

### 装甲板模型契约

契约常量定义在 `infer_engine.hpp` 的 `armor_model_contract` 命名空间里。

输入 `images: float32[1,3,384,640]`。相机 BGR 图像经过 letterbox（等比缩放 + 居中
贴到灰底画布）后转 RGB、除以 255 归一化到 `[0,1]`，按 NCHW 排列。

输出 `output0_bpc: float32[1,5040,23]`，每个候选的 23 个字段依次为：

1. 3 个颜色概率：`Blue, Red, Gray`
2. 8 个类别概率：`Sentry, 1, 2, 3, 4, Outpost, Base small, Base big`
3. 4 个关键点，每点为 `(x, y, visibility)`，顺序 TL / BL / BR / TR

候选置信度为最大颜色概率与最大类别概率的乘积。候选先经过
`armor_confidence_threshold` 和 `armor_keypoint_visibility_threshold` 过滤，再根据四个
关键点的外接框执行**类别无关**的 IoU NMS。visibility 仅在检测器内部使用，不写入 ROS
检测消息。

解码前会校验列数与候选框数是否符合契约，不符则该帧返回空检测。布局一旦错位，
偏移量全乱，解出来的是垃圾数据。

### 推理后端

onnxruntime 不注册任何 execution provider，走默认的 `CPUExecutionProvider`，
intra-op 线程数 4，图优化开到 `ORT_ENABLE_ALL`。单帧几十到上百毫秒，对着 rosbag
回放调试够用，达不到比赛帧率。

选 onnxruntime 而不是 OpenCV dnn 的原因：本项目的装甲板模型含
Expand / ScatterND / ConstantOfShape 等算子，OpenCV 4.10 的 ONNX importer 直接加载
失败（`ERROR [Expand]:(onnx_node!/model.23/Expand_1)`）。onnxruntime 算子覆盖完整，
同一个 `.onnx` 不需要重新导出。

上车跑实时需要 GPU 推理，那是另一个任务。`InferEngine` 是抽象基类，加后端时实现
`preprocess / infer / postprocess / set_input_image / draw_labeled_image` 五个纯虚函数，
解码与 NMS 可以直接复用基类的实现。
