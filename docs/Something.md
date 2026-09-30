<h1 align="center">Some important About engineering</h1>

## 引言
了解一门语言之后，你大概能写出几百行能跑的程序：一个 `main` 函数，几个自定义函数，
也许分了两三个文件。但是现在你拿到的是一个项目，`tree` 一下，看到几十个文件分布在七八层目录里，
有 `CMakeLists.txt`、`package.xml`、`.sh` 脚本、`.yaml` 配置，有 `include/` 和 `src/`
两套并行的目录，还有你没见过的 `build/`、`install/`、`log/`。

你会的语法在这里一行都用不上——或者说，用得上，但那不是难点。难点是：这些东西为什么
存在？我该改哪个文件？改完怎么让它生效？

这份文档讲的就是这些。它们没有语法那么容易归纳，教材里也很少写，因为写的人早已习惯，
不觉得需要解释。但它们恰恰是新人卡住最久的地方。

## 一、查看项目架构&Tree

在任何项目里干活之前，先花多一点时间摸清它的结构。这不是浪费时间——你要改的东西在哪，
取决于这个项目怎么组织。

看目录结构用 `tree`。这个命令**不是系统自带的**，多数 Ubuntu 装完是没有的，
先装一下：

```bash
sudo apt install tree
```

然后在项目根目录执行：

```bash
tree -L 2 -I 'build|install|log'
```

`-L 2` 是只看两层深度，`-I` 是排除指定目录。被排除的三个是编译产物，不是源码，
看它们只会干扰判断。你会看到：

```
.
├── AGENTS.md
├── bag
│   └── bag
├── compile.sh
├── docs
│   ├── Part_of_CMake.md
│   ├── Part_of_ROS2.md
│   ├── Part_of_Rviz_Foxglove.md
│   └── Something.md
├── LICENSE
├── QUESTION.md
├── reference
│   ├── detector_node.cpp
│   └── README.md
├── replay.sh
└── src
    ├── autoaim_common_libs
    ├── autoaim_detector
    └── autoaim_interfaces
```

源码在 `src/`，编译和运行用的脚本在根目录，数据在 `bag/`，文档在 `docs/`，
题面 `QUESTION.md` 也在根目录。
这种分法不是这个项目独创的，你在别的 ROS 2 项目里会看到几乎一样的布局。

`reference/` 是参考答案，下发给你的版本里没有这个目录。

不想装 `tree` 也可以用自带的 `ls`，加 `-F` 会在目录名后面带一个斜杠，
方便区分目录和文件：

```bash
ls -F
```

只是看不到嵌套结构，要一层层 `cd` 进去看。装 `tree` 更省事。

### 为什么源码要放在 src/ 里

这不是习惯问题，是 `colcon` 的硬性要求。

`colcon` 是 ROS 2 的构建工具。你在项目根目录执行 `colcon build` 时，它做的第一件事是
**递归扫描 `src/` 目录，寻找每一个含有 `package.xml` 的文件夹**。找到一个，就认为这是
一个「包」，读取里面的 `<name>` 作为包名、`<depend>` 作为依赖清单，详细关于colcon以及
package.xml的内容，在**Vision_Website** 中有所提到。

试一下：

```bash
colcon list --topological-order
```

```
autoaim_interfaces      src/autoaim_interfaces  (ros.ament_cmake)
autoaim_common_libs     src/autoaim_common_libs (ros.catkin)
autoaim_detector        src/autoaim_detector    (ros.ament_cmake)
```

注意这个顺序不是字母序。`autoaim_interfaces` 排在最前面，因为另外两个包都依赖它——它
定义了 ROS 消息，必须先把 `.msg` 文件编译成 C++ 头文件，后面的包才能 `#include`。
colcon 从每个 `package.xml` 里读出依赖关系，自己算出了这个顺序。

所以如果你把 `src/` 改名，colcon 就什么都找不到了。三个包之间的编译顺序也不需要你操心，
声明清楚依赖就行。

### build install log 

这三个目录是 `colcon build` 生成的，源码里没有它们。

| 目录 | 装什么 |
|---|---|
| `build/` | 编译中间产物：CMake 缓存、`.o` 目标文件、Ninja 的依赖记录 |
| `install/` | 编译结果：`.so` 动态库、可执行文件、头文件、配置与模型等资源 |
| `log/` | 每次编译的日志，按时间戳分目录 |

三个都可以整个删掉，重新编译即可。事实上这是解决「编译出现莫名其妙错误」的第一招：

```bash
rm -rf build install log
./compile.sh
```

它们也都写在 `.gitignore` 里，不进版本库。原因很直接：这些文件能从源码重建，而且里面
存了大量绝对路径（比如 `/home/cicada/Autoaim_Test/...`），拷到别人电脑上根本用不了。

**一个关键事实**：程序运行时读的是 `install/` 下的文件，不是 `src/` 下的。比如代码里
用 `get_package_share_directory("autoaim_detector")` 拿模型路径，得到的是
`install/autoaim_detector/share/autoaim_detector`。理解这一点，才能理解下面两件事。

### 为什么每次开新终端都要 source

```bash
source install/setup.bash
```

不执行这句，`ros2 launch autoaim_detector ...` 会告诉你找不到包，即使你刚刚编译成功。

原因是 `install/setup.bash` 修改了几个环境变量。自己看一眼变化：

```bash
echo $AMENT_PREFIX_PATH                # source 之前
source install/setup.bash
echo $AMENT_PREFIX_PATH                # source 之后
```

source 之前通常只有 `/opt/ros/$ROS_DISTRO`，之后前面多了本项目 `install/` 下的包路径：

```
/home/cicada/autoaim_Test/install/autoaim_detector:/home/cicada/autoaim_Test/install/autoaim_interfaces:/opt/ros/$ROS_DISTRO
```

`ros2` 命令和CMakeList中的 `get_package_share_directory()` 就是靠这个变量找包的。同时被修改的还有
`LD_LIBRARY_PATH`（动态链接器靠它找 `libautoaim_detector.so`）和 `PYTHONPATH`。

环境变量只在当前终端有效。所以每开一个新终端，都要重新 source——这也是为什么这个项目
需要两个终端时，两个都得先 source。

### 要不要 source /opt/ros/$ROS_DISTRO/setup.bash

**大多数人不需要，只 `source install/setup.bash` 就够了。**

ROS 2 装完之后这句一般已经写进 `~/.bashrc` 了——用 fishros 一键安装的话它会自动加上，
手动照官方教程装的人通常也会被提示加。写进 `.bashrc` 的内容每开一个新终端都会自动执行，
所以你打开终端时 ROS 的环境已经在了，再敲一遍只是重复一次，没有坏处也没有必要。

自己确认一下，在新终端里执行：

```bash
echo $ROS_DISTRO
```

有输出（比如 `jazzy`）说明已经自动 source 过了，那么在本项目里你只需要：

```bash
source install/setup.bash
```

如果没有任何输出，说明 `.bashrc` 里没写。两个办法，选一个：

```bash
# 办法一：写进 .bashrc，以后每个新终端自动生效（推荐）
echo 'source /opt/ros/'$ROS_DISTRO'/setup.bash' >> ~/.bashrc

# 办法二：每个新终端手动敲，注意必须在 install/setup.bash 之前
source /opt/ros/$ROS_DISTRO/setup.bash
source install/setup.bash
```

顺序不能反：ROS 的系统环境要先在，本项目 `install/` 的路径才能叠在它前面。
写进 `.bashrc` 的做法天然满足这个顺序，所以后面的文档里只会写
`source install/setup.bash` 这一句。

### 改了配置要不要重新编译

看 `compile.sh` 里的 `--symlink-install` 这个参数。

加了它之后，`install/` 里的资源文件不是拷贝，而是**软链接**。验证一下：

```bash
ls -la install/autoaim_detector/share/autoaim_detector/config/
```

```
params.yaml -> /home/cicada/autoaim_Test/src/autoaim_detector/config/params.yaml
```

目录本身是真实目录，目录里的文件是指向 `src/` 的软链接。也就是说
`install/` 下那个 `params.yaml` 就是 `src/` 下那个，改一处等于改两处。所以：

- 改 `params.yaml`、改 launch 文件：**不用重新编译**，重启节点就生效
- 改 `.cpp` 或 `.hpp`：**必须重新编译**，因为 C++ 代码要编成 `.so` 才能运行

`.so` 文件不是软链接（它是编译产物，没有源文件可指向），这也印证了上面的区别。

如果编译时没加 `--symlink-install`，`install/` 里就是拷贝，改了 yaml 不重新编译就不生效。
这种「我明明改了却没反应」的情况，排查起来很费时间。

## 二、为什么要写 .sh 脚本

项目根目录有两个脚本。先看 `compile.sh`，去掉注释只有两行：

```bash
colcon build --symlink-install \
  --cmake-args -G Ninja -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
```

你可能会想：这不就是一条命令吗，为什么要包一层？

因为**这三个参数一个都不能漏**，而且漏了不会报错，只会在别的地方给你制造麻烦：

- 漏 `--symlink-install`：改 yaml 后不生效，就是上一节说的那个坑
- 漏 `-G Ninja`：改用 make 编译，慢一些，更麻烦的是切换生成器必须先清空 `build/`，
  否则 CMake 直接报错。你和队友用不同生成器，互相拿到对方的 `build/` 就编不动
- 漏 `-DCMAKE_EXPORT_COMPILE_COMMANDS=ON`：不生成 `compile_commands.json`，
  编辑器的跳转定义、自动补全、错误提示全部失效

手敲这条命令是 87 个字符，`./compile.sh` 是 12 个。但节省打字不是重点，**保证每次编译
用的都是同一组参数**才是。团队里每个人、每一次编译都一致，出问题时才好排查。

`replay.sh` 更能说明脚本的价值。它的核心也只有一行 `ros2 bag play`，但外面包了这些：

```bash
BAG_NAME="bag/bag"

if [ ! -d "$BAG_NAME" ] || [ ! -f "$BAG_NAME/metadata.yaml" ]; then
    echo "找不到录包: $BAG_NAME"
    echo "请先解压 bag.7z 到项目根目录：7z x bag.7z -o bag"
    exit 1
fi

ros2 bag play "$BAG_NAME" --loop
```

三件事，每件都是手敲时容易漏的：

1. **存在性检查和可操作的提示**。录包没解压时，你看到的不是 `ros2 bag play` 抛出的一串
   Python 异常，而是一句话告诉你该干什么，还附了命令。这里检查的是 `metadata.yaml`
   而不只是目录：目录建出来了但内容没解压，`ros2 bag play` 的报错会难懂得多
2. **`--loop`**。录包只有 85 秒，不循环的话调试时要一遍遍手动重启
3. **`exit 1`**。失败时返回非零退出码，这样别的脚本调用它时能判断出失败了

写脚本的判断标准很简单：**一条命令你会执行超过三次，或者它有超过两个容易记错的参数，
就值得写成脚本。** 顺手把注释和错误提示也写进去，几个月后你自己回来看也用得上。

## 三、一个包是由什么组成的

打开 `src/autoaim_detector/`，除了 `include/` 和 `src/`，有两个文件决定了这个包怎么编译：
`package.xml` 和 `CMakeLists.txt`。

新人常问：这两个文件都在写依赖，是不是重复了？

不重复，它们回答的是两个不同的问题。

| | package.xml | CMakeLists.txt |
|---|---|---|
| 谁读它 | colcon，rosdep | CMake |
| 回答什么 | 这个包**是什么**，依赖**哪些包** | 这些源文件**怎么编** |
| 什么时候 | 编译**之前** | 编译**之中** |
| 决定 | 编译顺序、`rosdep install` 装什么 | 编译选项、生成什么目标、链接什么库 |

`package.xml` 是给「调度员」看的。colcon 在编译开始前读它，知道三个包的依赖关系，
排出编译顺序。`rosdep install` 也读它，知道该 apt 装哪些系统库。

`CMakeLists.txt` 是给编译器的指挥者 CMake 看的。它管的是具体怎么编：用 C++20、
开 `-O3` 优化、把哪几个 `.cpp` 编成一个库、这个库要链接哪些外部库。

这个项目的 `CMakeLists.txt` 只有 30 行，而且**看不到一行 `find_package(rclcpp)`**。
依赖去哪了？在这两行：

```cmake
find_package(ament_cmake_auto REQUIRED)
ament_auto_find_build_dependencies()
```

`ament_auto_find_build_dependencies()` 会去读 `package.xml` 里的每一个 `<depend>`，
自动为它们调用 `find_package()`。之后用 `ament_auto_add_library()` 建立目标时，
这些依赖会被自动链接上。

所以这两个文件是有分工也有配合的：**依赖只在 `package.xml` 里声明一次，
`CMakeLists.txt` 通过 `ament_cmake_auto` 取用。**

这也解释了一个现象。网上大多数 ROS 2 教程会教你这样写：

```cmake
add_executable(my_node src/my_node.cpp)
ament_target_dependencies(my_node rclcpp std_msgs)   # 本项目不用这个
```
本项目统一用 `ament_auto_*` 系列，依赖自动处理。

`CMakeLists.txt` 里有一个手写的 `find_package`，值得看一眼：

```cmake
find_package(onnxruntime_vendor REQUIRED)
```

严格说这行是冗余的——`onnxruntime_vendor` 已经写在 `package.xml` 里，
`ament_auto_find_build_dependencies()` 会找到它，删掉也能编过。保留它是为了
显式：这个包提供的 CMake target 名字叫 `onnxruntime::onnxruntime`，
和包名 `onnxruntime_vendor` 对不上，后面 `target_link_libraries` 要写的是
target 名，不是包名。

这种"包名一个样、target 名另一个样"的情况在真实项目里很常见。判断该写哪个的
办法是去看包安装的 CMake 配置文件，比如
`/opt/ros/$ROS_DISTRO/share/onnxruntime_vendor/cmake/onnxruntime_vendor-extras.cmake`，
里面 `add_library(... IMPORTED)` 那行的名字就是你要链接的 target。

### 漏写依赖会怎样

三种情况，症状不同：

- `package.xml` 漏了，`CMakeLists.txt` 里也没有兜底的 `find_package`：编译时报
  `fatal error: cv_bridge/cv_bridge.hpp: No such file or directory`
- `package.xml` 漏了但 CMakeLists 里手写了 `find_package`：你自己能编过，但别人
  `rosdep install` 装不上这个依赖，克隆下来编译失败
- 漏的是本地包（比如 `autoaim_interfaces`）：colcon 算错编译顺序，detector 可能先于
  interfaces 编译，找不到生成的消息头文件

## 四、接口、封装、调用

这三个词在语法课上是关键字，在工程里是如何把工作分开。

接下来的项目作业里有个现成的例子。你要写的节点需要调用神经网络做推理，而推理的实现已经给你了。
你需要知道的全部内容是这几行（`infer_engine.hpp`）：

```cpp
class InferEngine {
public:
    virtual void preprocess() = 0;
    virtual void infer() = 0;
    virtual void postprocess() = 0;
    virtual void set_input_image(const cv::Mat img) = 0;
    virtual cv::Mat draw_labeled_image() = 0;

    std::vector<DetectionObject> get_detection_objects() const;
    std::vector<DetectionObject>& get_detection_objects();
    // ...
};
```

加上工厂函数：

```cpp
std::unique_ptr<InferEngine> create_infer_engine(
    std::string model_path, int num_color, int num_tag, /* ... */);
```

调用方式是固定的四步：

```cpp
engine->set_input_image(img);
engine->preprocess();
engine->infer();
engine->postprocess();
auto& results = engine->get_detection_objects();
```

**你不需要知道的**：letterbox 的缩放系数怎么算、ONNX Runtime 的 session 怎么创建、
网络输出的 5040×23 张量怎么解码成检测框、NMS 怎么比较 IoU、OpenMP 怎么并行加速。当然，这些我其实也已经在笔试的一个Task中有所考察了，如果你认为这一块你有所不理解的话，我的建议是直接看这个项目里面的封装好的

这些全在 `infer_engine.cpp` 的具体实现里，你一行都不用读(当然，如果你对模型接入，预处理和后处理还不是很了解，可以以这个为媒介进行针对学习)。

**接口 6 行，实现 464 行。** 封装的收益在这里是可以数出来的：你今天少读了 458 行代码，
而且以后这 458 行怎么改都不影响你写的节点。


### OOP 中 public 和 private 的界线画在哪

`ArmorRefinator`（灯条颜色验证器）是个好例子。它的 public 只有五个成员：

```cpp
class ArmorRefinator {
public:
    ArmorRefinator(int brightness_thres, /* ... */);
    void set_input_image(const cv::Mat& image);
    bool validate(const DetectionObject& detection);
    cv::Mat draw_labeled_image();
    cv::Mat draw_binary_image();
```

private 里藏着这些：

```cpp
private:
    using SearchPolygon = std::array<cv::Point2f, 4>;
    SearchPolygon make_search_polygon(...);   // 搜索区域怎么构造
    bool validate_search(...);                // 颜色占比怎么统计
    cv::Mat red_mask_, blue_mask_;
```

判断标准很实际：**调用方需要做决定的东西放 public，实现机制放 private。**

调用方需要决定「这个检测结果是真的吗」，所以 `validate` 是 public。调用方不需要决定
「搜索多边形的四个顶点按什么顺序排」，所以 `make_search_polygon` 是 private。

这条线画对了有什么好处？假如以后要把搜索区域从四边形改成椭圆，改 private 部分不会影响
任何外部代码。如果 `make_search_polygon` 当初写成了 public，就可能已经有人在用它，
改动就会连带破坏别处。

## 五、数据契约

上面讲的接口，编译器会帮你检查——参数类型不对、少传一个参数，都编不过。

但有一类约定编译器完全管不了。


```cpp
struct DetectionObject {
    float bbox[4];
    float conf;
    int color;
    int label;
    std::vector<Keypoint> kpts;   // 4 个关键点
};
```

`kpts` 是一个长度为 4 的数组，装着装甲板的四个角点。问题是：哪个下标对应哪个角？

答案是 `kpts[0]`=左上、`kpts[1]`=左下、`kpts[2]`=右下、`kpts[3]`=右上（TL/BL/BR/TR）。
这个顺序写在文档里，**不在类型里**。

三个互不相识的模块都依赖这个约定：

- 推理引擎按这个顺序填进去（`infer_engine.cpp`）
- 灯条验证器按 `kpts[0]`、`kpts[1]` 取左灯条，`kpts[2]`、`kpts[3]` 取右灯条
  （`armor_refinator.cpp:167`）
- 你要写的节点，按这个顺序转成 ROS 消息的 `tl`/`bl`/`br`/`tr` 字段

如果你把顺序填错了会发生什么？**编译通过，运行不报错，检测框照样画出来**，只是下游
用这四个点做位姿解算时，会算出一个翻转的结果。而你要花很长时间才能发现问题出在这里。

这就是「数据契约」：类型系统表达不了、只能靠约定和文档维持的接口。识别它们的方法是问
自己一个问题——**这个数据的含义，能从类型声明里看出来吗？** `std::vector<Keypoint>`
看不出顺序，`float bbox[4]` 看不出是「中心点+宽高」还是「左上+右下」。看不出来的地方，
就是必须去查文档的地方。

真实项目里这类约定很多。这个项目还有几个：

- `Detections` 消息的 `header` 必须从输入图像原样透传，不能自己取当前时间，
  否则下游的时间对齐会全乱
- `ArmorDetection.msg` 里字段声明顺序是 `tl, tr, bl, br`，但语义顺序是 TL/BL/BR/TR，
  照着声明顺序填就错了
- 图像话题名不能带 `/compressed` 后缀，`image_transport` 会自己拼

写代码时遇到这种约定，最好的做法是**在代码旁边留一句注释说明它**。

## 六、用 git 管理项目

git 不只是 **备份代码** 的工具。在这个项目里，它承担了几件具体的事。


### 让删除变得安全

这个项目最近的一个提交删掉了 7315 行、70 个文件——五个 ROS 包、九个消息定义。
敢这么删，是因为前面每一步都提交了。

要找回某个被删的文件：

```bash
git show 4a3048e:src/autoaim_detector/src/detector_node.cpp
```

`git show <提交>:<路径>` 把那个时刻的文件内容打印出来，加个重定向就能存成文件。
验证一下这个文件确实已经不在工作区了，但在历史里完好：

下面的代码是举例，你们无所实现：
```bash
$ ls src/autoaim_detector/src/
armor_refinator.cpp  infer_engine.cpp        # 没有 detector_node.cpp

$ git show 4a3048e:src/autoaim_detector/src/detector_node.cpp | wc -l
370                                          # 但历史里有，370 行
```

要找回整个目录：

```bash
git checkout ef851a0 -- src/autoaim_selector src/autoaim_locator
```

所以规矩是：**删代码之前先提交。** 提交过的东西永远能找回，没提交的删了就真没了。
不用把注释掉的代码留在文件里「以防万一」——那是没有 git 时代的做法。

### 提交信息要写清改了
你的每次git commit后面跟上的messgae中需要详细说明自己在这次修改中都做了什么

从这两句话看不出具体改了什么。半年后你想找「那次改推理后端的提交」，
翻到这两条只能一个个点开看 diff。

写提交信息的标准是：**让以后的人（包括你自己）不打开 diff 就知道这次改了什么。**

### 什么该进版本库，什么不该

看 `.gitignore`：

```gitignore
build/
install/
log/
bag/
compile_commands.json
*.engine
*.trt
__pycache__/
```

前三个是编译产物，能重建，而且含绝对路径。`compile_commands.json` 同理。

`bag/` 是录包数据。git 存大二进制文件的效率很差，而且每次修改都会
让仓库永久变大（历史里的旧版本删不掉）。这类数据单独打包传。

有意思的是 `models/armor.onnx`——它有 47 MB，却**被 git 跟踪**了。为什么和 `bag/`
待遇不同？

判断标准是：**这个文件是「代码的一部分」，还是「代码的输入」？**

模型权重决定了程序的行为。`infer_engine.hpp` 里那些常量（输入 640×384、
输出 5040×23）都是照着这个模型写的，换个模型代码就跑不通。它和代码是绑定的，
版本必须一致，所以要跟踪。

录包只是测试用的输入数据，换一个录包代码照样跑，所以不跟踪。

`.gitignore` 里还有一对可以对照看：`.onnx` 跟踪，`.engine` 和 `.trt` 忽略。后两者是
TensorRT 针对特定型号显卡编译出来的，在别人机器上用不了，必须在目标设备上重新生成——
所以它们属于「产物」，不属于「源码」。

## 七、命名和组织

项目的 detector 包，头文件的路径举例是：

```
src/autoaim_detector/include/autoaim_detector/infer_engine.hpp
                     ^^^^^^^ ^^^^^^^^^^^^^^^^
                     固定     和包名一样
```

`include/` 下面又套了一层跟包名相同的目录，代码里的 include 写法也跟着变长：

```cpp
#include "autoaim_detector/infer_engine.hpp"
```

为什么不直接放 `include/infer_engine.hpp`？

因为**避免不同包的头文件重名**。假如 A 包和 B 包都有一个 `utils.hpp`，两个包的
`include/` 都在编译器的搜索路径里，那 `#include "utils.hpp"` 到底该用哪个？加上包名
前缀之后，`#include "a_pkg/utils.hpp"` 和 `#include "b_pkg/utils.hpp"` 不会混。


### ROS2的话题命名

这个项目的话题名有固定模式：

```
autoaim/detector/labeled_image
autoaim/detector/detections
^^^^^^^ ^^^^^^^^ ^^^^^^^^^^^^
项目名   模块名    内容
```

三段式。你自己起话题名时照这个模式来，别人一看就知道这是谁发的、装的是什么。

还有一个细节：有些话题名带前导斜杠（`/autoaim/camera/image_raw`），有些不带
（`autoaim/detector/detections`）。这不是随手写的：

- **带斜杠**是绝对话题名，不受节点命名空间影响。相机话题带斜杠，因为它由录包发布，
  位置固定
- **不带斜杠**是相对话题名，会被节点的命名空间前缀修饰。节点自己发布的话题用相对名，
  这样同一个节点跑两份（比如接两个相机）时可以用不同命名空间隔开，话题不会撞

### 其他惯例

这些约定光看代码就能总结出来，不需要专门的文档：

**成员变量加下划线后缀**：

```cpp
bool enable_fps_;
std::string input_image_topic_;
rclcpp::Publisher<Detections>::SharedPtr detections_pub_;
```

作用是一眼区分成员变量和局部变量。构造函数里 `enable_fps_ = declare_parameter<bool>("enable_fps")`，
左边带下划线是成员，右边是参数名，不会看混。

**常量全大写，并收进 namespace**：

```cpp
namespace armor_model_contract {
inline constexpr int INPUT_HEIGHT = 384;
inline constexpr int INPUT_WIDTH = 640;
inline constexpr int OUTPUT_CANDIDATES = 5040;
}
```

用 `armor_model_contract::INPUT_WIDTH` 比一个裸的全局 `INPUT_WIDTH` 清楚得多，
也不会和别处的同名常量冲突。

**枚举用 `enum class`**：

```cpp
enum class ColorType: int {
    NONE = -1,
    BLUE = 0,
    RED = 1,
    GRAY = 2
};
```

三个细节：用 `enum class` 而不是裸 `enum`（枚举值不会污染全局命名空间）；
显式写 `: int`（因为要 `static_cast<int>` 塞进 ROS 消息的 int32 字段）；
统一有个 `NONE = -1` 表示「未设置」。

**函数名全小写加下划线**：`to_armor_detections`、`get_fps`、`make_search_polygon`。

不过这些写法上的惯例，在Agent或者LLM的帮助下，对新人来说上手是非常容易的。

## 八、几个一定会踩的坑

这些都是有明确原因的，遇到了对照着查。

**编译成功了，但 `ros2 launch` 说找不到包。**
新终端没 source。执行 `source install/setup.bash`。注意每个新终端都要执行。

**改了 params.yaml 没反应。**
先确认改的是 `src/` 下的文件而不是 `build/` 里的副本。如果确实改对了还没反应，
检查节点是否重启了——yaml 只在节点启动时读一次。


**节点启动了，日志正常，但回调一次都没进。**
最常见的原因是话题名不对。按顺序查：

```bash
ros2 topic list                    # 录包在放吗
ros2 topic hz /autoaim/camera/image_raw/compressed   # 有数据吗
ros2 node info /你的节点名          # 你订阅的话题名对吗
```

如果订阅的是压缩图像话题，确认话题名**不带** `/compressed` 后缀，
传输方式参数是 `compressed`。写成带后缀的话，`image_transport` 会去订阅
`.../compressed/compressed`，那个话题不存在——**节点正常启动、不报任何错、
回调永不触发**。

**启动时刷上百行 `Schema error: Trying to register schema with name ...`。**
不是崩了，不影响推理。这是 Ubuntu 自己打包的那份 onnxruntime
（`libonnxruntime-dev`）重复注册算子的已知问题，用上一节说的 vendor 包不会出现。
不管有没有这堆噪声，往下翻能看到
`[ONNXRuntimeInferEngine] loaded ... input 640x384`，就说明模型正常加载。

**帧率只有 20 来 Hz，录包是 100 Hz，是不是有 bug？**
不是。CPU 推理单帧要 40~60 毫秒，处理不过来，丢帧是正常的。具体数字跟机器和
onnxruntime 版本都有关，20~40 Hz 都算正常。QoS 设成
`keep_last(1)` 就是为了只保留最新一帧，不让旧帧排队积压——实时系统里，
过期的数据不如丢掉。

**红蓝装甲板认反了。**
OpenCV 用 BGR 通道顺序，不是 RGB。`cv_bridge::toCvShare(msg, ...)` 的第二个参数
要写 `"bgr8"`。写成 `"rgb8"` 的话检测框照样出来，但颜色分类是反的。

**`apt install libonnxruntime-dev` 报"无法定位软件包"。**
不要用这个包名。Ubuntu 官方源从 25.10 才开始收录 `libonnxruntime-dev`，
22.04 和 24.04 都没有。本项目用 ROS 官方的 vendor 包，四个发行版都有：

```bash
sudo apt install ros-$ROS_DISTRO-onnxruntime-vendor
```

它已经写进 `package.xml`，所以正常情况下 `rosdep install --from-paths src -y`
会自动装上，不需要单独敲这条。

---

祝顺利 😊😊😊

终于写完了第一个，劳累...

<div align="right">—— 浙江大学 RoboMaster 算法组</div>
