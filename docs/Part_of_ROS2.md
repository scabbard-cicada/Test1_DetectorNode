<h1 align="center">Part of ROS 2</h1>

这份文档讲的是从零写一个 ROS 2 节点需要想清楚的事。

本文以 **Ubuntu 24.04 + ROS 2 Jazzy** 作为教学主线。当前仓库在
**Ubuntu 26.04 + ROS 2 Lyrical** 上验证，但节点、话题、参数、launch 和组件化的基本用法一致。
涉及系统路径时使用 `$ROS_DISTRO`，因此不需要根据发行版手动改路径。

## 先读这个

> **[07-ROS2 基础](https://hello-world-vision.github.io/Vision_Website/induction-training/07-ros2基础/)**
>
> 那份文档覆盖面很广：环境安装、工作空间与包的组成、节点与话题、参数、
> 自定义消息、launch、组件化、tf2、可视化、rosbag。语法层面该有的都有了。

那份文档是**读代码导向**的——它教你怎么读懂一个现成的工程，所以给的都是完整代码片段
供你对照。本周任务反过来，是**从零写**。写和读需要的东西不一样：

读代码时你看到 `create_subscription` 在构造函数里，知道"哦这是在订阅"就够了。
自己写的时候要回答的是：这行放在构造函数的哪个位置？放错会怎样？订阅和发布谁先建？
参数什么时候读？回调函数凭什么会被调用？

这份文档讲的就是这些。语法去看那份，这份讲**怎么排**。

## 一、节点的宏观结构

先看一个节点的骨架长什么样。下面这个例子跟本周任务无关，是一个把输入数字翻倍再发出去的
节点，只用来说明结构：

```cpp
namespace my_package {

class DoublerNode : public rclcpp::Node {
public:
    explicit DoublerNode(const rclcpp::NodeOptions& options);

private:
    // 回调函数
    void number_callback(const std_msgs::msg::Int32::SharedPtr msg);

    // 配置：构造时读一次，之后不变
    std::string input_topic_;
    std::string output_topic_;
    int factor_;

    // 订阅与发布
    rclcpp::Subscription<std_msgs::msg::Int32>::SharedPtr number_sub_;
    rclcpp::Publisher<std_msgs::msg::Int32>::SharedPtr result_pub_;
};

// 构造函数
DoublerNode::DoublerNode(const rclcpp::NodeOptions& options)
    : Node("doubler", options) {
    // 1. 读参数
    input_topic_ = declare_parameter<std::string>("input_topic");
    output_topic_ = declare_parameter<std::string>("output_topic");
    factor_ = declare_parameter<int>("factor");

    // 2. 建发布
    result_pub_ = create_publisher<std_msgs::msg::Int32>(output_topic_, rclcpp::QoS(10));

    // 3. 建订阅
    number_sub_ = create_subscription<std_msgs::msg::Int32>(
        input_topic_, rclcpp::QoS(10),
        std::bind(&DoublerNode::number_callback, this, std::placeholders::_1));
}

void DoublerNode::number_callback(const std_msgs::msg::Int32::SharedPtr msg) {
    std_msgs::msg::Int32 result;
    result.data = msg->data * factor_;
    result_pub_->publish(result);
}

}  // namespace my_package

#include "rclcpp_components/register_node_macro.hpp"
RCLCPP_COMPONENTS_REGISTER_NODE(my_package::DoublerNode)
```

这个结构有几点值得注意。

**类声明和实现可以放在同一个 `.cpp` 里。** 网站那份文档的例子把实现直接写在类里，
上面这个例子把它们分开了。ROS 2 节点通常只有一个源文件在用它，没必要单独开头文件——
本项目的建议做法是把类声明放在 `.cpp` 顶部，方法实现跟在后面。

**构造函数只有一件事要做：把节点准备好。** 读参数、创建对象、建立订阅发布，
做完就结束。它不包含任何处理逻辑——处理逻辑在回调里。

**回调是私有成员函数。** 它不会被外部调用，只由 ROS 2 在消息到达时触发。

**成员变量分两类**，建议在声明时就分组：

- **配置类**：构造时从参数读一次，之后只读。`input_topic_`、`factor_`
- **运行时对象**：订阅、发布、以及需要跨回调保持的状态

这两类分开写，读代码的人一眼就知道哪些是"启动时定下的"、哪些是"运行中在用的"。

**末尾的注册宏。** 因为节点要编成 component（为什么见网站那份的第 8 章），
没有 `main()`,靠这个宏注册。宏里写的字符串必须和 `CMakeLists.txt` 里
`rclcpp_components_register_node(... PLUGIN ...)` 完全一致。

## 二、构造函数的顺序

上面例子里标了 1、2、3。这个顺序不是随意的。

### 参数必须最先读

后面创建的东西要用参数值。话题名从参数来，发布订阅就得等参数读完；阈值从参数来，
需要阈值的对象就得等参数读完。

```cpp
input_topic_ = declare_parameter<std::string>("input_topic");   // 先
number_sub_ = create_subscription<...>(input_topic_, ...);      // 后
```

反过来写会用到未初始化的成员变量——`std::string` 默认是空串，订阅一个空话题名会抛异常。

### 发布建在订阅之前

这一条是**建议**，不是语法要求，但理由很实际。

订阅一旦建立，回调随时可能被触发。如果回调里要用发布者，而发布者还没创建，
就会访问空指针。

```cpp
// 有风险的顺序
number_sub_ = create_subscription<...>(...);   // 这之后回调可能立刻被调用
result_pub_ = create_publisher<...>(...);      // 但发布者还没建好
```

实际上组件容器通常在构造函数返回后才开始分发消息，所以这个顺序大多数时候不出问题。
但依赖这个时序是没必要的风险，把发布放前面就完全避免了。

同理，**回调里要用的所有对象，都应该在建订阅之前准备好**。如果你的回调要调用某个
处理对象，那个对象的构造要排在订阅前面。

### 构造函数里适合打日志

```cpp
RCLCPP_INFO(get_logger(), "节点初始化完成，订阅 %s", input_topic_.c_str());
```

这句日志的价值在排查时体现：节点起来了但没反应，第一个问题是"它到底订阅了哪个话题"。
把关键配置打出来，省得去翻 yaml 猜。

日志宏有几档：`RCLCPP_DEBUG` / `INFO` / `WARN` / `ERROR`。构造阶段的初始化信息用
`INFO`,出问题用 `ERROR`。

## 三、yaml 参数怎么走到代码里

网站那份讲了语法（`declare_parameter` 和 yaml 的三层结构），这里补完整的链路。

### 完整路径

```
config/params.yaml
      ↓  ament_auto_package(INSTALL_TO_SHARE config ...)
install/包名/share/包名/config/params.yaml
      ↓  launch 里 parameters=[那个路径]
节点启动时，参数被注入到节点
      ↓  declare_parameter<T>("名字")
C++ 变量
```

四个环节，每一环断了都会失败，而且症状不同：

| 断在哪 | 症状 |
|---|---|
| CMakeLists 没装 config | launch 里 `get_package_share_directory` 拼出的路径不存在 |
| launch 没传 parameters | 节点启动就抛异常，说参数未声明 |
| yaml 里参数名拼错 | 同上，因为节点要的那个名字找不到 |
| `declare_parameter` 的类型不匹配 | 抛类型转换异常 |

### yaml 的层级

```yaml
/**:
  ros__parameters:
    input_topic: "/some/topic"
    factor: 2
```

`ros__parameters` 这一层是固定的关键字，不能省也不能改名。**注意是两个下划线。**

最外层是节点名。写具体节点名（如 `doubler:`）表示只给那个节点；写 `/**:` 是通配，
表示这个文件里的参数给所有节点。本项目用 `/**:`,这样节点改名不用改 yaml。

### declare_parameter 的两种形式

```cpp
// 形式一：不带默认值
factor_ = declare_parameter<int>("factor");

// 形式二：带默认值
factor_ = declare_parameter<int>("factor", 2);
```

差别在参数缺失时的行为：形式一抛异常，节点起不来；形式二用默认值继续。

**本项目统一用形式一。** 这是刻意的：参数名打错的话，形式二会静默用默认值，
你看到节点正常启动、行为却不对，排查起来很费时间；形式一直接在启动时报错，
告诉你哪个参数没找到。

这个选择的代价是 yaml 必须写全。少一个参数节点就起不来——但这正是你想要的，
因为它让错误在最早的时刻暴露。

### 类型要对上

`declare_parameter<T>` 的 `T` 必须和 yaml 里的字面量类型匹配。

```yaml
threshold: 0.3      # 浮点
count: 5            # 整数
enable: true        # 布尔
name: "something"   # 字符串
```

常见的坑是整数和浮点：yaml 里写 `1`(而不是 `1.0`）会被解析成整数，
用 `declare_parameter<double>("x")` 读会抛类型错误。参数本意是浮点时，
yaml 里就写 `1.0`。

## 四、回调是怎么被调用的

这是新人最容易含糊的地方。网站那份文档提了回调的写法，但没讲它凭什么会被调用。

### spin 在做什么

你的节点类里没有任何循环，也没有"检查消息到了没"的代码。消息到达时回调就是被调用了——
这件事由 `spin` 完成。

独立进程的节点，`main` 里是这样：

```cpp
rclcpp::init(argc, argv);
rclcpp::spin(std::make_shared<MyNode>());   // 这里进入一个循环，直到 Ctrl+C
rclcpp::shutdown();
```

`spin` 是一个阻塞循环，它反复做：检查有没有订阅收到新消息 → 有的话调用对应回调 →
继续等。

本项目的节点是 component，你不写 `main`,`spin` 由容器进程负责。但机制一样：
**容器在某个线程里跑 spin，消息到了就调你的回调。**

理解这点能解释几件事：

- **回调不是"另一个线程在跑"**。默认情况下它就在 spin 所在的线程里被调用
- **回调执行期间，这个线程不能处理其他消息**。回调越慢，积压越多
- **构造函数返回之后，回调才可能开始**。所以构造函数里可以安全地初始化一切

### 回调耗时会怎样

假设回调里要跑一次神经网络推理，耗时 50 毫秒，而图像以 100 Hz（每 10 毫秒一帧）到来。

结果是：你处理完一帧，已经有 5 帧新的到了。处理不过来。

这种情况下 ROS 2 的行为取决于 QoS 的队列深度：

```cpp
rclcpp::QoS(10)                          // 队列深 10，积压 10 帧
rclcpp::SensorDataQoS().keep_last(1)     // 只保留最新 1 帧，旧的直接丢
```

对图像这类实时数据，**丢掉旧帧比排队处理更合理**——你处理一张 500 毫秒前的图像没有意义。
所以视觉节点的图像订阅通常用 `keep_last(1)`。

`SensorDataQoS()` 是一个预设，它除了浅队列还把可靠性设成 `BEST_EFFORT`（允许丢包，
换取低延迟）。传感器数据用它，控制指令这类不能丢的用默认的 `RELIABLE`。

**帧率低于输入频率不一定是 bug。** 本周任务里 CPU 推理跟不上录包的播放速度，
处理频率只有输入的五分之一左右，这是正常的。

### 回调里能做什么不能做什么

**不要在回调里做无限循环或长时间阻塞**。整个节点会停止响应。

**回调里可以发布消息**，这是最常见的模式：收到输入 → 处理 → 发布输出。

**多个订阅的回调之间可能并发**（取决于容器用的执行器和回调组配置）。如果两个回调
都读写同一个成员变量，需要考虑加锁。本周任务只有一个订阅，不涉及这个问题。

## 五、订阅与发布

### 一般形式

```cpp
// 发布：模板参数是消息类型
result_pub_ = create_publisher<std_msgs::msg::Int32>(话题名, QoS);
result_pub_->publish(msg);

// 订阅：模板参数是消息类型，第三个参数是回调
number_sub_ = create_subscription<std_msgs::msg::Int32>(
    话题名, QoS,
    std::bind(&DoublerNode::number_callback, this, std::placeholders::_1));
```

回调的绑定有两种写法，效果一样：

```cpp
// std::bind
std::bind(&DoublerNode::number_callback, this, std::placeholders::_1)

// lambda
[this](const std_msgs::msg::Int32::SharedPtr msg) { number_callback(msg); }
```

回调逻辑短的话直接写在 lambda 里也行；长的话单独写成成员函数更清楚。

### 图像话题要用 image_transport

普通的 `create_subscription<sensor_msgs::msg::Image>` 只能订阅未压缩图像。

如果数据源发布的是**压缩图像**（`sensor_msgs/CompressedImage`）,用普通订阅收不到——
消息类型都不一样。这时要用 `image_transport`,它的机制是：

```
你订阅 "/camera/image_raw"，传输方式指定 "compressed"
      ↓
image_transport 实际去订阅 "/camera/image_raw/compressed"
      ↓
收到 CompressedImage，自动解压
      ↓
你的回调收到解好的 Image
```

所以话题名写**不带后缀**的那个，后缀由 `image_transport` 自己拼。

这里有个容易踩的坑：如果你把话题名写成带 `/compressed` 后缀的，
`image_transport` 会去订阅 `.../compressed/compressed`,那个话题不存在。
结果是**节点正常启动、不报任何错、回调永远不触发**。没有任何诊断信息，很难查。

`image_transport` 的订阅函数签名和 `create_subscription` 不太一样，它不是节点的成员函数，
而是自由函数，需要把节点传进去。具体签名去查头文件
（`/opt/ros/$ROS_DISTRO/include/image_transport/image_transport/image_transport.hpp`），
注意**里面有多个重载，其中旧的已经标记 deprecated**,选新的那个。

编译时如果看到这样的警告：

```
warning: 'image_transport::Subscriber image_transport::create_subscription(rclcpp::Node*, ...)'
is deprecated: Use create_subscription(RequiredInterfaces node_interfaces, ..., rclcpp::QoS, ...) instead
```

说明你用的是旧重载。警告不影响运行，但既然它告诉了你新写法，照着改掉更好。
新重载第一个参数要传**引用**而不是指针，QoS 也直接传 `rclcpp::QoS` 对象。

顺带一提，`ament_index_cpp::get_package_share_directory` 也已经 deprecated，
提示用 `get_package_share_path` 代替。网站那份文档和很多现有代码还在用旧的，
能跑，但编译会告警。

### 话题名的前导斜杠

```cpp
create_publisher<T>("autoaim/detector/detections", ...)    // 相对名
create_publisher<T>("/autoaim/detector/detections", ...)   // 绝对名
```

带斜杠是绝对话题名，不受节点命名空间影响。不带斜杠是相对名，会被命名空间前缀修饰。

单个节点不加命名空间运行时，两者行为一样，所以这个区别平时看不出来。它在一个节点
跑多份时才有意义：用相对名 + 不同命名空间，两份节点的话题不会撞车。

自己发布的话题建议用相对名。订阅别人的话题，如果那个话题是绝对名，就照抄。

## 六、消息的构造与发布(自定义Interface)

### header 的时间戳

大多数消息有一个 `std_msgs/Header`:

```
std_msgs/Header header
  builtin_interfaces/Time stamp
  string frame_id
```

`stamp` 是这个数据对应的时刻，`frame_id` 是它所在的坐标系。

如果你的节点是"收到输入 → 处理 → 发布输出"这种模式，**输出的 header 应该从输入原样
复制过来**：

```cpp
output.header = msg->header;    // 对
output.header.stamp = now();    // 通常错
```

为什么？`stamp` 表示的是"这份数据描述的是哪一时刻的世界状态"。一张图像的 stamp 是
曝光时刻，你基于它算出的检测结果，描述的仍然是曝光那一刻的场景，不是你算完的时刻。

用 `now()` 的后果是下游拿不到正确的时间基准。下游节点如果要把两个话题的数据对齐
（比如图像和检测结果配对），靠的就是 stamp 相等。你改了 stamp，对齐就失效。

### 消息里的嵌套结构

自定义消息常有数组字段：

```
ArmorDetection[] armor_detections
```

对应的 C++ 用法：

```cpp
Detections msg;
for (const auto& item : 你的数据) {
    ArmorDetection d;
    d.confidence = item.conf;
    // ... 填其他字段
    msg.armor_detections.push_back(d);
}
pub_->publish(msg);
```

数组字段在 C++ 里就是 `std::vector`。填之前可以 `reserve()` 预分配，
避免反复扩容——数据量大时有意义。

### 消息字段声明顺序不代表语义顺序

这是一类很隐蔽的坑。一个消息可能这样声明：

```
geometry_msgs/Point32 tl
geometry_msgs/Point32 tr
geometry_msgs/Point32 bl
geometry_msgs/Point32 br
```

看起来顺序是 tl → tr → bl → br。但如果你的数据源给的四个点是按另一种顺序排列的
（比如 tl → bl → br → tr，绕一圈的顺序），那就不能按声明顺序对着填。

**要填对，只能去查文档或读现有代码**,不能看字段名的排列顺序猜。这类约定编译器
检查不了，填错了程序照样跑，只是结果不对。

## 七、launch 文件

节点写好了，怎么启动它？

最直接的是 `ros2 run`：

```bash
ros2 run 包名 可执行文件名
```

但这样启动的节点拿不到参数。参数在 yaml 里，得有人把 yaml 喂给它。你可以在命令行
一个个传：

```bash
ros2 run 包名 可执行文件名 --ros-args -p input_topic:=/in -p factor:=3
```

参数少的时候还行，十几个参数就没法忍了，而且每次启动都要重敲一遍。

launch 文件解决的就是这件事：**把"启动哪些节点、每个节点用什么参数"写成一份文件，
一条命令拉起来。**

### 最小的 launch 文件

launch 文件是 Python 脚本，放在包的 `launch/` 目录下，文件名以 `.launch.py` 结尾。
它必须定义一个叫 `generate_launch_description` 的函数，返回一个 `LaunchDescription`

然后：

```bash
ros2 launch my_package my.launch.py
```

### Node 的各个字段

| 字段 | 填什么 | 填错的后果 |
|---|---|---|
| `package` | 包名，和 `package.xml` 里 `<name>` 一致 | 报找不到包 |
| `executable` | `CMakeLists.txt` 里 `EXECUTABLE` 那一项的值 | 报找不到 executable |
| `name` | 节点运行时的名字 | 不报错，但会覆盖代码里的名字，见下 |
| `output` | `'screen'` 表示日志打到终端 | 不填看不到节点的日志 |
| `emulate_tty` | `True` 保留日志颜色 | 不填日志是单色的 |
| `parameters` | 参数来源，列表 | 不填则参数未初始化，节点崩 |

前两个填错会立刻报错，容易发现。后面几个值得细说。

### name 会覆盖代码里的节点名

代码里构造 Node 时写了一个名字：

```cpp
: Node("code_name", options)
```

launch 里也能写一个：

```python
Node(..., name='launch_name', ...)
```

**实际生效的是 launch 里的那个。** 我验证过：两处分别写 `code_name` 和
`launch_name`，运行时日志前缀和 `get_name()` 返回的都是 `launch_name`。

这个设计有用——同一份代码要跑两份时（比如接两个相机），用 launch 给它们起不同的名字，
不用改代码。

但对你有个实际影响：**用 `ros2 node info` 查节点时，要用 launch 里的名字。**
按代码里的名字查会说找不到节点。`name` 不填的话才用代码里的名字。

### parameters 不填会怎样

如果代码里用的是不带默认值的 `declare_parameter<T>("名字")`（本项目的做法，
理由见第三节），launch 里不传 `parameters`，节点启动就崩：

```
terminate called after throwing an instance of
'rclcpp::exceptions::UninitializedStaticallyTypedParameterException'
  what():  Statically typed parameter 'greeting' must be initialized.
[ERROR] [ltest_node-1]: process has died
```

报错会指出是哪个参数没初始化。看到这个先查两件事：launch 里 `parameters` 传了吗、
yaml 里有这个参数吗。

`parameters` 是列表，可以混用两种形式：

```python
parameters=[
    params_yaml_path,            # 从 yaml 文件读
    {'some_param': 42},          # 直接在 launch 里写死
]
```

后面的会覆盖前面的同名参数。临时调试时有用：不动 yaml，在 launch 里覆盖一个值。

### 参数文件路径为什么要用 API 拿

这是最容易写错的地方。

```python
# 错
params = 'src/my_package/config/params.yaml'
params = '/home/你的名字/ws/src/my_package/config/params.yaml'

# 对
params = os.path.join(
    get_package_share_directory('my_package'), 'config', 'params.yaml')
```

两个错法各有问题。相对路径是相对于**启动命令的工作目录**算的，你从哪个目录敲
`ros2 launch` 就从哪算，换个目录就找不到。绝对路径写死了机器和用户名，别人克隆下来
必然失效。

正确做法是用 `get_package_share_directory()`，它返回这个包安装后的 share 目录
（`install/包名/share/包名`）。前提是 `CMakeLists.txt` 里用
`ament_auto_package(INSTALL_TO_SHARE config ...)` 把 `config` 装过去了。

这里体现了一件事：**程序运行时读的是 `install/` 下的文件，不是 `src/` 下的。**

（本项目编译加了 `--symlink-install`，`install/` 下的 yaml 是指向 `src/` 的软链接，
所以改 yaml 不用重新编译。但路径本身仍然要通过 API 拿。）

### 启动多个节点

`LaunchDescription` 的参数是一个列表，往里多放几个就是：

```python
return LaunchDescription([
    Node(package='pkg_a', executable='node_a', output='screen'),
    Node(package='pkg_b', executable='node_b', output='screen'),
])
```

两个节点会同时启动。它们之间如果有话题连接，ROS 2 自己会建立，launch 不用管——
launch 只负责"把进程拉起来并配好参数"。

要启动的节点不一定在本包里。系统装的节点也能启动，只要知道它的 `package` 和
`executable` 名字。

### 命令行传参与条件启动

这两个不是必须的，但知道有它们。

`DeclareLaunchArgument` 声明一个可以从命令行传的参数：

```python
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration

def generate_launch_description():
    return LaunchDescription([
        DeclareLaunchArgument('port', default_value='8765'),
        Node(
            package='some_pkg',
            executable='some_node',
            parameters=[{'port': LaunchConfiguration('port')}],
        ),
    ])
```

启动时覆盖：

```bash
ros2 launch my_package my.launch.py port:=9000
```

`IfCondition` 让某个节点按条件启动：

```python
from launch.conditions import IfCondition

DeclareLaunchArgument('use_viz', default_value='true'),
Node(
    package='some_viz_pkg',
    executable='viz_node',
    condition=IfCondition(LaunchConfiguration('use_viz')),
),
```

这样 `use_viz:=false` 就不启动那个节点。用途是把可选的东西做成开关——
比如可视化工具，调试时要、正式跑的时候不要。


## 八、本周任务

你要写 `src/autoaim_detector/src/detector_node.cpp`,一个完整的节点。



**构造函数的顺序**按第二节说的排：参数 → 需要参数的对象 → 发布 → 订阅。

**回调里的处理顺序**要想清楚。推理引擎有固定的调用顺序（见题面），
顺序错了不会编译报错，但结果不对。

**QoS 选什么**：图像是高频传感器数据，处理跟不上输入频率。第四节讲了这种情况该用哪种。

**`header` 必须透传**，理由见第六节。

**launch 文件也要你写**，放在 `src/autoaim_detector/launch/` 下，具体要求见那个目录里的
`README.md`。参数文件路径务必用 `get_package_share_directory` 拿，别写死。

写完之后，自检顺序是这样：

```bash
# 1. 编译过了吗
./compile.sh

# 2. 节点起得来吗（另开终端先 source install/setup.bash）
ros2 launch autoaim_detector <你的launch文件>

# 3. 订阅发布对不对
ros2 node info /你的节点名

# 4. 有数据流出来吗
ros2 topic hz /你发布的话题
ros2 topic echo /你发布的话题 --once
```

第 3 步很有用：它会列出节点实际订阅和发布的所有话题。如果你以为订阅了某个话题
但这里没列出来，说明话题名或订阅代码有问题。注意节点名要用 launch 里 `name=` 写的那个
（见第七节），不确定的话先 `ros2 node list` 看一眼。

## 九、排查

### 节点起不来

**抛参数相关的异常**：yaml 里缺参数，或参数名拼错，或 launch 没把 yaml 传进去。
对照 `params.yaml` 和代码里 `declare_parameter` 的名字逐个核对。

**报找不到 component**：`CMakeLists.txt` 里 `PLUGIN` 的值和代码末尾
`RCLCPP_COMPONENTS_REGISTER_NODE` 里的不一致。这两处必须完全相同，包括命名空间。

**报找不到 executable**：launch 里 `executable=` 的值和 `CMakeLists.txt` 里
`EXECUTABLE` 的不一致。

### 节点起来了但回调不触发

按这个顺序查：

```bash
ros2 topic list                  # 上游话题存在吗
ros2 topic hz 上游话题名          # 上游真的在发数据吗
ros2 node info /你的节点名        # 你订阅的话题名对不对
```

第三步最关键。`ros2 node info` 列出的是节点**实际**订阅的话题名，如果它和上游发布的
不一致，就是话题名写错了。订阅图像时还要注意 transport 参数和 `/compressed` 后缀
那个坑（第五节）。

### 有输出但内容不对

用 `ros2 topic echo 话题名 --once` 看一条完整消息，逐字段核对。常见问题：

- 某些字段全是 0：没赋值
- 数组是空的：处理逻辑没产出结果，或者没 `push_back`
- 时间戳不对：header 没透传
- 数值明显离谱：单位或坐标系搞错了


---

祝顺利 😊😊😊

<div align="right">—— 浙江大学 RoboMaster 算法组</div>
