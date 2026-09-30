# launch 目录

**这个目录现在是空的，launch 文件由你来写。**

需要做的事：启动 detector 节点，并加载 `config/params.yaml` 里的参数。
想在 Foxglove 里看图的话，还要把 `foxglove_bridge` 一起启动。

写完之后应该能这样跑起来：

```bash
ros2 launch autoaim_detector <你的文件名>.launch.py
```

## 几个必须知道的点

**参数文件路径不能写死。** 代码装到 `install/` 之后，`config/params.yaml`
的实际位置是 `install/autoaim_detector/share/autoaim_detector/config/params.yaml`。
用 `ament_index_python.packages.get_package_share_directory('autoaim_detector')`
拿到 share 目录再拼路径，不要写相对路径或绝对路径。

**executable 填什么。** 填 `CMakeLists.txt` 里
`rclcpp_components_register_node(... EXECUTABLE ...)` 那一项的值。

**foxglove_bridge 是系统装的独立节点**，不在本仓库里。它的 `package` 和
`executable` 都叫 `foxglove_bridge`，有一个 `port` 参数，默认 8765。
装好之后 `ros2 pkg prefix foxglove_bridge` 应该能查到路径。

Foxglove 连 `ws://localhost:8765`，订阅你发布的标注图像话题看效果。

## 参考

`ros2 launch` 的 Python API 用到这几个：

- `launch.LaunchDescription`
- `launch_ros.actions.Node`
- `ament_index_python.packages.get_package_share_directory`

想做得更完善可以了解 `DeclareLaunchArgument`（命令行传参）和
`IfCondition`（条件启动，比如用一个开关控制要不要起 foxglove）。这些不是必须的。
