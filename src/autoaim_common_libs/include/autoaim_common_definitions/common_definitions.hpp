#pragma once    // 头文件在同一个 .cpp 里只准拷贝一次”，防止重复定义导致报错

#include <stdexcept>
#include <vector>
#include <opencv2/opencv.hpp>

#include "autoaim_interfaces/msg/detections.hpp"    // 定义了识别到的目标数据结构

enum class ColorType: int {
    NONE = -1,
    BLUE = 0,
    RED = 1,
    GRAY = 2
};

enum class ArmorType: int {
    NONE = -1,
    SENTRY = 0,
    ONE = 1,
    TWO = 2,
    THREE = 3,
    FOUR = 4,
    OUTPOST = 5,
    BASE = 6
};

enum class BuffType: int {
    NONE = -1,
    INACTIVATE = 0,
    ACTIVATE = 1
};

enum class SerialMode: int {
    NONE = -1,
    IDLE = 0,
    AUTO_AIM = 1,
    SMALL_BUFF = 2,
    BIG_BUFF = 3,
    GREEN_LIGHT = 4,
    HIGH_SHOOT = 5
};

enum class DetectionMode: int {
    NONE = -1,
    ARMOR = 0,
    BUFF = 1,
    GREEN_LIGHT = 2
};

namespace defs {

using namespace autoaim_interfaces::msg;
// constexpr 意思是编译期常量。这个值在程序运行前就绝对确定了，你直接把它当成数字去替换，不要在运行时再分配内存去查它的值了
// 加上 inline 后，你告诉链接器：“不管这个文件被包含多少次，在最终生成的程序里，大家全部共享这唯一的一个变量。”
inline constexpr float OUTPOST_RADIUS = 0.273;
inline constexpr float BUFF_RADIUS = 0.7;
inline constexpr float SMALL_BUFF_PALSTANCE = 1.047197551;

// 单位: 米
inline constexpr float HEIGHT = 0.04781f;
inline constexpr float BIG_WIDTH = 0.2183f;
inline constexpr float SMALL_WIDTH = 0.1235f;
inline constexpr float BUFF_WIDTH = 0.114f;

// 装甲板坐标系：前x，左y，上z
const std::vector<cv::Point3f> SMALL_POINTS { // 小装甲板
    {0, SMALL_WIDTH / 2, HEIGHT / 2},
    {0, SMALL_WIDTH / 2, -HEIGHT / 2},
    {0, -SMALL_WIDTH / 2, -HEIGHT / 2},
    {0, -SMALL_WIDTH / 2, HEIGHT / 2}
};
const std::vector<cv::Point3f> BIG_POINTS { // 大装甲板
    {0, BIG_WIDTH / 2, HEIGHT / 2},
    {0, BIG_WIDTH / 2, -HEIGHT / 2},
    {0, -BIG_WIDTH / 2, -HEIGHT / 2},
    {0, -BIG_WIDTH / 2, HEIGHT / 2}
};
const std::vector<cv::Point3f> BUFF_POINTS { // 能量机关扇面
    {0, 0, BUFF_WIDTH},
    {0, BUFF_WIDTH, 0},
    {0, 0, -BUFF_WIDTH},
    {0, -BUFF_WIDTH, 0},
};

// 返回该装甲板类型的俯仰偏置角 
constexpr float armor_pitch(ArmorType t) {
    // [[unlikely]]：现代 CPU 执行代码不是等上一句执行完才执行下一句，而是会提前“猜” if 语句的结果，并把猜的那条路的代码提前加载
    // 加上 [[unlikely]]，“这行代码 t == ArmorType::NONE（目标为空）发生的情况非常极其罕见，你让 CPU 不要预测它，全力以赴跑正常识别到的路径”。
    if (t == ArmorType::NONE) [[unlikely]] throw std::invalid_argument("invalid armor type: NONE");
    return (t == ArmorType::OUTPOST) ? -0.2618 : 0.2618;
}

// 返回该装甲板类型的俯仰偏置角是否为负
constexpr bool is_armor_pitch_negative(ArmorType t) {
    if (t == ArmorType::NONE) [[unlikely]] throw std::invalid_argument("invalid armor type: NONE");
    return armor_pitch(t) < 0;
}

// 返回该装甲板类型是否为大装甲板
constexpr bool is_big_armor(ArmorType t) {
    if (t == ArmorType::NONE) [[unlikely]] throw std::invalid_argument("invalid armor type: NONE");
    return (t == ArmorType::ONE || t == ArmorType::BASE) ? true : false;
}

// 后的几个函数是对上述功能的函数重载（Overloading），把参数类型从 ArmorType 换成了接收到的 ROS 消息类型 ArmorDetection
inline float armor_pitch(ArmorDetection armor) {
    if (static_cast<ArmorType>(armor.label) == ArmorType::NONE) [[unlikely]] throw std::invalid_argument("invalid armor type: NONE");
    return (static_cast<ArmorType>(armor.label) == ArmorType::OUTPOST) ? -0.2618 : 0.2618;
}

inline bool is_armor_pitch_negative(ArmorDetection armor) {
    if (static_cast<ArmorType>(armor.label) == ArmorType::NONE) [[unlikely]] throw std::invalid_argument("invalid armor type: NONE");
    return armor_pitch(armor) < 0;
}

inline bool is_big_armor(ArmorDetection armor) {
    if (static_cast<ArmorType>(armor.label) == ArmorType::NONE) [[unlikely]] throw std::invalid_argument("invalid armor type: NONE");
    return (static_cast<ArmorType>(armor.label) == ArmorType::ONE || static_cast<ArmorType>(armor.label) == ArmorType::BASE) ? true : false;
}

}
