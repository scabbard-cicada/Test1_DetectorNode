#pragma once

#include <memory>
#include <vector>
#include <unordered_map>

struct Keypoint {
    cv::Point2f pt;
    float conf = 0.0f;
};

struct DetectionObject {
    float bbox[4];
    float conf;
    int color;
    int label;
    std::vector<Keypoint> kpts;
};

class ColorMapper {
public:

    static const std::unordered_map<std::string, cv::Scalar>& get_color_map() {
        static const std::unordered_map<std::string, cv::Scalar> color_map = {
            {"Blue", cv::Scalar(255, 0, 0)},
            {"Red", cv::Scalar(0, 0, 255)},
            {"Gray", cv::Scalar(114, 114, 114)},
            {"Green", cv::Scalar(0, 255, 0)},
            {"Yellow", cv::Scalar(0, 255, 255)},
            {"Cyan", cv::Scalar(255, 255, 0)},
            {"Magenta", cv::Scalar(255, 0, 255)},
            {"White", cv::Scalar(255, 255, 255)},
            {"Black", cv::Scalar(0, 0, 0)}
        };
        return color_map;
    }

    static cv::Scalar get_color(const std::string& color_name) {
        const auto& color_map = get_color_map();
        auto it = color_map.find(color_name);
        if (it != color_map.end()) {
            return it->second;
        }

        std::cerr << "Warning: Color '" << color_name << "' not found in color map. Using default purple color." << std::endl;
        return cv::Scalar(255, 0, 255);
    }

    static std::vector<cv::Scalar> get_colors(const std::vector<std::string>& color_names) {
        std::vector<cv::Scalar> colors;
        colors.reserve(color_names.size());
        for (const auto& name : color_names) {
            colors.push_back(get_color(name));
        }
        return colors;
    }
};
