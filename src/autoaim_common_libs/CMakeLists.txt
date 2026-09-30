cmake_minimum_required(VERSION 3.10)
project(autoaim_common_libs)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(ament_cmake_auto REQUIRED)
ament_auto_find_build_dependencies()

# colcon 会为所有 ament 包传入这两个 catkin 兼容变量。有编译目标的包会在
# ament 的安装宏里用到它们，而本包是纯头文件库，没有目标，CMake 就会报
#   "Manually-specified variables were not used by the project: CATKIN_..."
# 显式读取一次即可消除该警告。
if(DEFINED CATKIN_INSTALL_INTO_PREFIX_ROOT)
  set(CATKIN_INSTALL_INTO_PREFIX_ROOT "${CATKIN_INSTALL_INTO_PREFIX_ROOT}")
endif()
if(DEFINED CATKIN_SYMLINK_INSTALL)
  set(CATKIN_SYMLINK_INSTALL "${CATKIN_SYMLINK_INSTALL}")
endif()
if(DEFINED CMAKE_EXPORT_COMPILE_COMMANDS)
  set(CMAKE_EXPORT_COMPILE_COMMANDS "${CMAKE_EXPORT_COMPILE_COMMANDS}")
endif()

ament_auto_package(
  USE_SCOPED_HEADER_INSTALL_DIR
  INSTALL_TO_SHARE
)
# CMakeLists 里链接了依赖，但在 XML 里还要声明的原因：
# package.xml（给 ROS 2 调度员看，发生在编译前）：比如你的自瞄库依赖 autoaim_interfaces。colcon 读取 xml 后，会强制先去编译接口包，然后再编译你的自瞄库。
# CMakeLists.txt（给 C++ 编译器看，发生在编译中）：它告诉编译器（GCC/Clang），“当把代码变成二进制文件时，去系统哪个具体路径下找 OpenCV 的动态库进行绑定”。

# CMakeLists 的项目名称需要和 package.xml 的 <name> 标签一致，否则 colcon 会报错。原因有点麻烦，不需要关心
# ament_cmake_auto 本身也是一个包（ROS 2 官方提供的）。通过 find_package 加载它之后，它里面封装的一大堆用 CMake 语言写的高级宏
# CMakeLists.txt，其实就是一个用 CMake 语言写的脚本程序，用来指挥 C++ 编译器的
# 在 CMake 语言里，函数传参不像 Python 那样 func(flag=True)。它把所有传入的东西都当作字符串列表。
# 全大写的词叫 Flags（标志位）。当 ament_auto_package 这个函数在底层执行时，它会扫描你传入的单词列表。它发现有 INSTALL_TO_SHARE，就进入一条 if 分支，执行相关的安装逻辑。
# 在 C++ 的机制中，头文件（.hpp / .h）绝对不会被直接编译！

# ROS 2 环境路径：当你用 colcon build 编译一个工作空间后，会生成一个 install/ 文件夹，这就是构建出的环境。
# 静态资源（Static Resources / Plain text）：不需要经过编译器（如 g++）翻译处理，直接原封不动地从源码文件夹复制（Copy）到安装文件夹（install）就可以起作用的文件
# C++ 的头文件（.h / .hpp），配置文件（如 package.xml, .yaml 参数文件，ROS 的启动文件（.launch.py），机器人的 3D 模型文件（.urdf）

# 静态资源：指的是编译后的二进制文件（.so / .dll / .exe）。包括可执行文件（Executables）和 动态链接库（Shared Libraries, Linux下后缀是 .so，Windows下是 .dll）