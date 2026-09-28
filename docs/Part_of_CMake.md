<h1 align="center">Part of CMake</h1>

这份文档讲的是完成本周任务需要的 CMake 知识。

本文以 **Ubuntu 24.04 + ROS 2 Jazzy** 作为教学主线。当前仓库在
**Ubuntu 26.04 + ROS 2 Lyrical** 上验证，但两套环境的 CMake 和 ROS 2 基本用法一致。
文档中的 ROS 安装路径统一写成 `/opt/ros/$ROS_DISTRO`，不要把发行版名称写死。

## 先读这个

动手之前，先完整读一遍算法组网站上的 CMake 基础：

> **[09-CMake 基础](https://hello-world-vision.github.io/Vision_Website/induction-training/09-cmake基础/)**
>
> 那份文档讲的是通用的 CMake：为什么需要构建系统、`add_executable` 与
> `add_library` 的区别、什么是 target，`target_link_libraries` 怎么建立
> 链接关系、为什么要有 `build/` 目录。最后还有一个完整练习和三个「主动制造
> 错误」的实验。

那份文档里最重要的是 target 这个概念：

```
.cpp 文件 → 创建 Target → 给 Target 添加配置 → 建立 Target 之间的依赖关系
```

本周任务里你要写的每一行 CMake 都落在这个框架里。如果你还没建立这个概念，
先回去读，这份文档假设你已经知道 target 是什么。

## 但那份文档不够用

那份文档教的是**裸 CMake**，例子里所有源文件都是自己写的，没有外部依赖。
真实的 ROS 2 项目多了三件事，正是本周任务的考点：

| 新问题 | 裸 CMake 里没有的 |
|---|---|
| 要用别人写好的库（OpenCV、onnxruntime） | `find_package` |
| ROS 2 有自己的一套包管理 | `package.xml` 与 `ament_cmake_auto` |
| 节点要能被容器加载 | component 注册 |

下面逐个讲。讲的是规则，不是答案——怎么用在任务上，你自己映射。

## 一、find_package：用别人写好的库

那份基础文档里，`calculator_lib` 是你自己用 `add_library` 建的，所以
`target_link_libraries` 可以直接写它的名字。

但如果要链接一个**系统里装好的库**（比如 OpenCV），你没有它的 target，
得先让 CMake 去找：

```cmake
find_package(SomeLib REQUIRED)
```

这条命令会去一堆约定的路径下找 `SomeLibConfig.cmake` 或
`FindSomeLib.cmake` 这类文件。找到之后，那个文件会定义一些变量和 target，
你才能在后面用。`REQUIRED` 表示找不到就直接报错终止，不加的话找不到会静默继续，
后面才在莫名其妙的地方失败。

找到之后怎么用？现代 CMake 的推荐方式是用它导出的 **imported target**，
名字通常长这样：

```cmake
target_link_libraries(你的target
  SomeLib::SomeLib
)
```

`命名空间::目标名` 这个双冒号形式就是 imported target 的标志。它的好处是把头文件
路径、库文件、编译选项全都打包带上了，不用你再单独写
`target_include_directories`。

### 一个必须知道的坑：包名不止一个

同一个库可能有三个不同的名字，而且互不相同：

| 名字 | 例子 | 用在哪 |
|---|---|---|
| apt 包名 | `libfoo-dev` | `sudo apt install` 时 |
| CMake 包名 | `foo` 或 `Foo` | `find_package()` 的参数 |
| imported target 名 | `foo::foo` | `target_link_libraries` 的参数 |

三者经常对不上，而且**大小写敏感**。`find_package(opencv)` 和
`find_package(OpenCV)` 不是一回事。

怎么查一个库的 CMake 包名？找它安装的 config 文件：

```bash
# 看系统里有哪些 cmake 包可用
ls /usr/lib/x86_64-linux-gnu/cmake/
ls /usr/share/cmake*/Modules/ | head
```

目录名或 `xxxConfig.cmake` 里的 `xxx` 就是 `find_package` 该写的名字。
imported target 的名字可以在那个 config 文件里搜 `add_library` 找到。

本周任务中的CMakeList里有一个库正好踩了这个坑，它的 apt 包名和 CMake 包名不一致。
自己查清楚。

## 二、package.xml 与 ament_cmake_auto

ROS 2 的包多了一个 `package.xml`。新人常问：它和 `CMakeLists.txt` 都在写依赖，
是不是重复了？

不重复，两者回答不同的问题。

| | package.xml | CMakeLists.txt |
|---|---|---|
| 谁读它 | colcon，rosdep | CMake |
| 回答什么 | 这个包**是什么**，依赖**哪些包** | 这些源文件**怎么编** |
| 什么时候 | 编译**之前** | 编译**之中** |
| 决定 | 多个包的编译顺序、`rosdep install` 装什么 | 编译选项、生成什么 target、链接关系 |

`colcon build` 开始时，会先扫描所有 `package.xml`，根据 `<depend>` 算出依赖图，
排出编译顺序。这个阶段 CMake 还没启动。之后才逐个包进去执行 `CMakeLists.txt`。

### ament_cmake_auto 做了什么

本任务用的是 `ament_cmake_auto` 这套宏。下面这两行是它的入口：

```cmake
find_package(ament_cmake_auto REQUIRED)
ament_auto_find_build_dependencies()
```

第二行会读 `package.xml` 里**每一个** `<depend>`，对它们逐个调用
`find_package()`。所以你在本项目的 `CMakeLists.txt` 里看不到
`find_package(rclcpp)`、`find_package(cv_bridge)`、`find_package(OpenCV)`——
它们都在 `package.xml` 里声明过了，这一行全部处理掉。

**依赖只声明一次**，这是 `ament_cmake_auto` 的核心便利。

### ament_auto_add_library 与 add_library 的差别

那份基础文档教的是：

```cmake
add_library(my_lib src/foo.cpp)
target_include_directories(my_lib PUBLIC include)
target_link_libraries(my_lib SomeLib::SomeLib)
```

三行。用 `ament_auto_add_library` 的话，中间那行不用写：

```cmake
ament_auto_add_library(my_lib SHARED src/foo.cpp)
```

为什么？看它的实现（`/opt/ros/$ROS_DISTRO/share/ament_cmake_auto/cmake/ament_auto_add_library.cmake`）：

```cmake
add_library("${target}" ${ARG_UNPARSED_ARGUMENTS} ${_source_files})

# add include directory of this package if it exists
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/include")
  target_include_directories("${target}" PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include")
endif()
```

它内部就是调 `add_library`，然后**如果 `include/` 目录存在就自动加进去**。
再往后还会调 `ament_auto_depend_on_packages`，把 `package.xml` 里的依赖自动链接上。

所以你只需要关心两件事：target 叫什么、哪些 `.cpp` 参与编译。

但要注意，**自动链接只覆盖 `package.xml` 里声明过的依赖**。你自己手写
`find_package` 找来的库，还是得自己在 `target_link_libraries` 里链接。

### SHARED 是什么

那份基础文档提过 `.a` 静态库和 `.so` 共享库的区别。ROS 2 的节点必须编成
`SHARED`（也就是 `.so`），原因在下一节。

## 三、component 注册

这是 ROS 2 特有的，那份基础文档完全没有。

### 为什么节点要编成库

按裸 CMake 的思路，一个能跑的程序应该用 `add_executable` 编成可执行文件。
但 ROS 2 的节点通常先编成共享库，再注册成 **component**。

原因是性能。一条视觉链路上有好几个节点，节点之间要传图像。如果每个节点是独立进程，
图像要经过序列化、进程间拷贝、反序列化；如果它们在**同一个进程**里，可以直接传指针，
零拷贝。

要让多个节点装进同一个进程，就不能每个都是独立的可执行文件——必须是可以被动态加载的
共享库。这就是 `SHARED` 的原因。

### 那怎么运行？

那份基础文档里说过一句：「库本身通常没有 `main()`，所以不能直接运行。」

`rclcpp_components_register_node` 解决的就是这个。它做两件事：

1. 把你的节点类注册到 ament 索引里，让容器进程能按名字找到并加载它
2. 用一个模板生成一个含 `main()` 的 `.cpp`，编成可执行文件

第二件事可以在它的源码里看到
（`/opt/ros/$ROS_DISTRO/share/rclcpp_components/cmake/rclcpp_components_register_node.cmake`）：

```cmake
configure_file(${rclcpp_components_NODE_TEMPLATE} ...)
add_executable(${node} ${PROJECT_BINARY_DIR}/rclcpp_components/node_main_${node}.cpp)
```

所以注册之后你会得到两种用法：既能被容器加载（省拷贝），也能用
`ros2 run` 或 launch 里的 `Node` 单独跑（方便调试）。一份代码，两种形态。

### 它的两个参数

```cmake
rclcpp_components_register_node(目标名
  PLUGIN 插件名
  EXECUTABLE 可执行文件名
)
```

`PLUGIN` 要写**完整的「命名空间::类名」**，而且必须和你在节点源码末尾写的注册宏
完全一致：

```cpp
RCLCPP_COMPONENTS_REGISTER_NODE(命名空间::类名)
```

两处不一致会怎样？**编译能通过**，因为 CMake 不知道你 C++ 代码里写了什么。
但运行时容器按 `PLUGIN` 的名字去找，找不到，报
`Failed to load node ... Could not find requested resource in ament index`。

`EXECUTABLE` 是生成的可执行文件名。写 launch 文件时，`Node(executable=...)`
填的就是这个值。这里写错的话，`ros2 launch` 会报找不到 executable。

这两个参数是本周任务里最容易出错的地方，因为**错了编译不报错**。

## 四、安装：让运行时找得到文件

还有一件裸 CMake 项目通常不用操心的事。

你的代码运行时可能要读配置文件、模型权重。问题是：程序运行时该去哪找它们？

不能写 `src/autoaim_detector/config/params.yaml` 这种相对路径——程序的工作目录
不固定，从哪个目录启动就从哪算，很容易找不到。也不能写绝对路径，换台机器就失效。

ROS 2 的做法是把这些资源**安装**到一个约定位置，运行时用 API 查：

```cpp
#include <ament_index_cpp/get_package_share_directory.hpp>
std::string dir = ament_index_cpp::get_package_share_directory("包名");
```

返回的是 `install/包名/share/包名`。所以配置和模型必须被装到那里去，这是
`ament_auto_package(INSTALL_TO_SHARE ...)` 在做的事。

本项目这部分已经写好了，你不用改。但要理解一点：**改了 `src/` 下的 yaml，
生效的是 `install/` 下的那份**。本项目编译时加了 `--symlink-install`，
`install/` 下是指向 `src/` 的软链接，所以改 yaml 不用重新编译；
但如果你的 launch 文件放在了别的目录名下，就要改 `INSTALL_TO_SHARE` 的列表。

## 五、排查

### 编译报错怎么看

**`Unknown CMake command "xxx"`**
这个命令 CMake 不认识。要么拼错了，要么定义它的模块还没被 `find_package` 引入。
CMake 的宏是「先引入才能用」，顺序不能反。

**`Target "xxx" links to: yyy but the target was not found`**
你在 `target_link_libraries` 里写了一个不存在的 target。CMake 会在报错里列出
可能原因，其中一条是 `A find_package call is missing for an IMPORTED target`——
这就是提示。检查 target 名的拼写和大小写，以及有没有先 `find_package`。


**`fatal error: xxx.hpp: No such file or directory`**
找不到头文件。检查 `package.xml` 里有没有声明这个依赖，以及头文件的 include
路径写对了没有。

### 有用的命令

```bash
# 只编一个包，不用等整个工作空间
colcon build --packages-select 包名

# 看编译时实际执行的命令（排查编译选项、include 路径问题）
colcon build --packages-select 包名 --cmake-args -DCMAKE_VERBOSE_MAKEFILE=ON

# 看某个包的依赖有没有被正确解析
colcon list --packages-select 包名

# CMake 缓存有时会留下过期状态，清掉重来
rm -rf build install log
```

`build/包名/CMakeCache.txt` 里能看到所有 CMake 变量的实际取值，
怀疑某个 `find_package` 有没有生效时可以去搜。

### 一个重要提醒

搜 ROS 2 的 CMake 写法时，你会大量看到这种：

```cmake
add_executable(my_node src/my_node.cpp)
ament_target_dependencies(my_node rclcpp std_msgs)
```

**`ament_target_dependencies` 从 Lyrical 起已经被移除了**，在 Lyrical 上照抄会报
`Unknown CMake command`。Humble / Jazzy / Kilted 里它还在，能用，但本项目不用它。

想知道自己的环境有没有，跑这条：

```bash
grep -rl "macro(ament_target_dependencies" /opt/ros/$ROS_DISTRO/share/
```

Jazzy 上会列出 `ament_cmake_target_dependencies/cmake/ament_target_dependencies.cmake`，
Lyrical 上没有输出。注意别被目录骗了——Lyrical 里
`share/ament_cmake_target_dependencies/` 这个目录还在，只是定义宏的那个
`.cmake` 文件被删了，所以光看目录存不存在判断不出来。

这个宏在 ROS 2 的多数版本里存在，所以官方文档、大量教程、以及 AI 的训练数据里
它随处可见。问 AI 也大概率会给你这个写法。本项目统一用 `ament_cmake_auto`：
依赖写在 `package.xml`，用 `ament_auto_add_library` 建 target。

**遇到和本项目现有代码风格不一致的建议时，以项目里跑得通的代码为准。**

## 六、本周任务的CMake的部分

你要在 `src/autoaim_detector/CMakeLists.txt` 里补齐构建配置，让节点能编译出来
并且能被 launch 启动。

现在那个文件里已经有的部分不用改。你要加的东西，按上面讲过的顺序想一遍：

1. 有一个推理库要显式 `find_package` 一下。包名去 `package.xml` 里找
2. 有几个 `.cpp` 要编成一个 target。哪几个？去 `src/` 目录看，
   别忘了你自己写的那个
3. 有两个库需要手动链接。一个是第 1 步那个推理库——注意要链接的是它的
   **target 名**，和包名不一样，去它的 CMake 配置文件里查（上一节讲了怎么查）；
   另一个提供 OpenMP 支持，给定的源码里用了 `#pragma omp`，不链接会报
   undefined reference
4. 节点要能被 launch 启动，需要注册

每一步用什么命令、参数怎么填，上面都讲了规则。具体的名字和值，
去读 `package.xml`、看 `src/` 目录、翻你自己写的节点代码。

写完之后：

```bash
./compile.sh
```

编过之后确认一下 target 真的生成了：

```bash
ls build/autoaim_detector/*.so                    # 库编出来了吗
ls install/autoaim_detector/lib/autoaim_detector/ # 可执行文件呢
ros2 component types                              # 组件注册上了吗
```

三个都有东西，说明四段都写对了。

---

祝顺利 😊😊😊

纳新好累呀，我已累昏...

<div align="right">—— 浙江大学 RoboMaster 算法组</div>
