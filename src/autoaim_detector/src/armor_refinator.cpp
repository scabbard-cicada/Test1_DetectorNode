#include "autoaim_detector/armor_refinator.hpp"

#include <cctype>

namespace {
std::string to_lower_ascii(std::string text) {
    for (auto& ch: text) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return text;
}

std::string get_name_or_unknown(const std::vector<std::string>& names, int index) {
    if (0 <= index && index < static_cast<int>(names.size())) {
        return names[index];
    }
    return "Unknown";
}

std::vector<cv::Point> to_integer_polygon(const std::array<cv::Point2f, 4>& polygon) {
    std::vector<cv::Point> points;
    points.reserve(polygon.size());
    for (const auto& point: polygon) {
        points.emplace_back(cvRound(point.x), cvRound(point.y));
    }
    return points;
}
}

ArmorRefinator::ArmorRefinator(
    int brightness_thres,
    int color_diff_thres,
    double outward_width_ratio,
    double inward_tolerance_ratio,
    double vertical_margin_ratio,
    double min_color_ratio,
    std::vector<std::string> color_names,
    std::vector<std::string> tag_names
):
    params_ {
        brightness_thres,
        color_diff_thres,
        outward_width_ratio,
        inward_tolerance_ratio,
        vertical_margin_ratio,
        min_color_ratio
    },
    color_names_(std::move(color_names)),
    tag_names_(std::move(tag_names)) {}

void ArmorRefinator::set_input_image(const cv::Mat& image) {
    img = image;
    viz_data_.clear();
    viz_data_.binary_image = cv::Mat::zeros(image.size(), CV_8UC1);

    cv::Mat blue_channel, red_channel, brightness;
    cv::extractChannel(image, blue_channel, 0);
    cv::extractChannel(image, red_channel, 2);
    cv::max(blue_channel, red_channel, brightness);

    cv::Mat brightness_mask;
    cv::compare(brightness, params_.brightness_thres, brightness_mask, cv::CMP_GT);

    cv::Mat blue_16s, red_16s, red_minus_blue;
    blue_channel.convertTo(blue_16s, CV_16S);
    red_channel.convertTo(red_16s, CV_16S);
    cv::subtract(red_16s, blue_16s, red_minus_blue);

    cv::compare(red_minus_blue, params_.color_diff_thres, red_mask_, cv::CMP_GT);
    cv::compare(red_minus_blue, -params_.color_diff_thres, blue_mask_, cv::CMP_LT);
    cv::bitwise_and(red_mask_, brightness_mask, red_mask_);
    cv::bitwise_and(blue_mask_, brightness_mask, blue_mask_);
}

ArmorRefinator::SearchPolygon ArmorRefinator::make_search_polygon(
    const cv::Point2f& top,
    const cv::Point2f& bottom,
    const cv::Point2f& armor_axis,
    bool is_left
) const {
    const float light_height = cv::norm(bottom - top);
    const cv::Point2f light_axis = (bottom - top) / light_height;
    const float outward_width =
        std::max(1.0f, static_cast<float>(params_.outward_width_ratio * light_height));
    const float inward_tolerance =
        std::max(0.0f, static_cast<float>(params_.inward_tolerance_ratio * light_height));
    const float vertical_margin =
        std::max(0.0f, static_cast<float>(params_.vertical_margin_ratio * light_height));

    const cv::Point2f outward_offset = armor_axis * (is_left ? -outward_width : outward_width);
    const cv::Point2f inward_offset = armor_axis * (is_left ? inward_tolerance : -inward_tolerance);
    const cv::Point2f top_margin = light_axis * -vertical_margin;
    const cv::Point2f bottom_margin = light_axis * vertical_margin;

    if (is_left) {
        return {
            top + outward_offset + top_margin,
            bottom + outward_offset + bottom_margin,
            bottom + inward_offset + bottom_margin,
            top + inward_offset + top_margin
        };
    }

    return {
        top + inward_offset + top_margin,
        bottom + inward_offset + bottom_margin,
        bottom + outward_offset + bottom_margin,
        top + outward_offset + top_margin
    };
}

bool ArmorRefinator::validate_search(const cv::Mat& color_mask, const SearchPolygon& polygon) {
    const auto integer_polygon = to_integer_polygon(polygon);
    const cv::Rect image_bounds(0, 0, img.cols, img.rows);
    const cv::Rect search_bounds = cv::boundingRect(integer_polygon) & image_bounds;
    if (search_bounds.empty()) {
        viz_data_.searches.push_back({polygon, false, 0.0});
        return false;
    }

    std::vector<cv::Point> local_polygon;
    local_polygon.reserve(integer_polygon.size());
    for (const auto& point: integer_polygon) {
        local_polygon.push_back(point - search_bounds.tl());
    }

    cv::Mat search_mask = cv::Mat::zeros(search_bounds.size(), CV_8UC1);
    cv::fillConvexPoly(search_mask, local_polygon, cv::Scalar(255));

    const int search_area = cv::countNonZero(search_mask);
    cv::Mat matched_pixels;
    cv::bitwise_and(color_mask(search_bounds), search_mask, matched_pixels);
    const int matched_area = cv::countNonZero(matched_pixels);
    const double color_ratio = search_area > 0
        ? static_cast<double>(matched_area) / static_cast<double>(search_area)
        : 0.0;
    const bool matched = search_area > 0 && color_ratio >= params_.min_color_ratio;

    cv::Mat binary_roi = viz_data_.binary_image(search_bounds);
    cv::bitwise_or(binary_roi, matched_pixels, binary_roi);
    viz_data_.searches.push_back({polygon, matched, color_ratio});
    return matched;
}

bool ArmorRefinator::validate(const DetectionObject& detection) {
    if (detection.kpts.size() < 4) {
        return false;
    }

    const std::string detection_color =
        to_lower_ascii(get_name_or_unknown(color_names_, detection.color));
    if (detection_color == "gray") {
        viz_data_.detections.push_back({detection, true});
        return true;
    }

    const cv::Mat* color_mask = nullptr;
    if (detection_color == "red") {
        color_mask = &red_mask_;
    } else if (detection_color == "blue") {
        color_mask = &blue_mask_;
    } else {
        viz_data_.detections.push_back({detection, false});
        return false;
    }

    const cv::Point2f left_mid = (detection.kpts[0].pt + detection.kpts[1].pt) / 2;
    const cv::Point2f right_mid = (detection.kpts[2].pt + detection.kpts[3].pt) / 2;
    const float armor_width = cv::norm(right_mid - left_mid);
    const float left_height = cv::norm(detection.kpts[1].pt - detection.kpts[0].pt);
    const float right_height = cv::norm(detection.kpts[2].pt - detection.kpts[3].pt);
    if (armor_width <= 1.0f || left_height <= 1.0f || right_height <= 1.0f) {
        viz_data_.detections.push_back({detection, false});
        return false;
    }

    const cv::Point2f armor_axis = (right_mid - left_mid) / armor_width;
    const SearchPolygon left_search =
        make_search_polygon(detection.kpts[0].pt, detection.kpts[1].pt, armor_axis, true);
    const SearchPolygon right_search =
        make_search_polygon(detection.kpts[3].pt, detection.kpts[2].pt, armor_axis, false);

    const bool left_matched = validate_search(*color_mask, left_search);
    const bool right_matched = validate_search(*color_mask, right_search);
    const bool passed = left_matched && right_matched;
    viz_data_.detections.push_back({detection, passed});
    return passed;
}

cv::Mat ArmorRefinator::draw_labeled_image() {
    cv::Mat debug_img = img.clone();

    const cv::Scalar passed_color(255, 0, 0);
    const cv::Scalar rejected_color(0, 0, 255);
    const cv::Scalar matched_search_color(0, 255, 0);
    const cv::Scalar missing_search_color(0, 165, 255);
    const std::vector<cv::Scalar> kpt_colors = {
        cv::Scalar(255, 255, 0),
        cv::Scalar(255, 0, 255),
        cv::Scalar(0, 255, 255),
        cv::Scalar(255, 128, 0)
    };

    for (const auto& detection_status: viz_data_.detections) {
        const auto& detection = detection_status.detection;
        if (detection.kpts.size() < 4) {
            continue;
        }

        std::array<cv::Point2f, 4> kpts =
            {detection.kpts[0].pt, detection.kpts[1].pt, detection.kpts[2].pt, detection.kpts[3].pt
            };
        const cv::Scalar box_color = detection_status.passed ? passed_color : rejected_color;

        for (int j = 0; j < 4; ++j) {
            cv::line(debug_img, kpts[j], kpts[(j + 1) % 4], box_color, 2);
        }
        for (size_t i = 0; i < kpts.size(); ++i) {
            cv::circle(debug_img, kpts[i], 5, kpt_colors[i], -1);
        }

        const std::string color_name =
            to_lower_ascii(get_name_or_unknown(color_names_, detection.color));
        const std::string tag_name = get_name_or_unknown(tag_names_, detection.label);
        const std::string label =
            color_name + " " + tag_name + " " + std::to_string(detection.conf).substr(0, 4);
        cv::putText(
            debug_img,
            label,
            kpts[0] - cv::Point2f(0, 10),
            cv::FONT_HERSHEY_SIMPLEX,
            0.6,
            cv::Scalar(255, 255, 255),
            2
        );
    }

    for (const auto& search: viz_data_.searches) {
        const cv::Scalar color = search.matched ? matched_search_color : missing_search_color;
        for (size_t i = 0; i < search.polygon.size(); ++i) {
            cv::line(
                debug_img,
                search.polygon[i],
                search.polygon[(i + 1) % search.polygon.size()],
                color,
                2
            );
        }
        cv::putText(
            debug_img,
            cv::format("%.2f", search.color_ratio),
            search.polygon[0],
            cv::FONT_HERSHEY_SIMPLEX,
            0.45,
            color,
            1
        );
    }

    return debug_img;
}

cv::Mat ArmorRefinator::draw_binary_image() {
    cv::Mat debug_img = viz_data_.binary_image.clone();

    for (const auto& search: viz_data_.searches) {
        for (size_t i = 0; i < search.polygon.size(); ++i) {
            const cv::Point2f& start = search.polygon[i];
            const cv::Point2f& end = search.polygon[(i + 1) % search.polygon.size()];
            cv::line(debug_img, start, end, cv::Scalar(255), 3);
            cv::line(debug_img, start, end, cv::Scalar(0), 1);
        }
    }

    return debug_img;
}
