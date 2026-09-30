#pragma once

#include <array>
#include <opencv2/opencv.hpp>
#include "autoaim_detector/detector_common.hpp"

class ArmorRefinator {
public:

    ArmorRefinator(
        int brightness_thres,
        int color_diff_thres,
        double outward_width_ratio,
        double inward_tolerance_ratio,
        double vertical_margin_ratio,
        double min_color_ratio,
        std::vector<std::string> color_names,
        std::vector<std::string> tag_names
    );

    void set_input_image(const cv::Mat& image);

    bool validate(const DetectionObject& detection);

    cv::Mat draw_labeled_image();

    cv::Mat draw_binary_image();

private:

    using SearchPolygon = std::array<cv::Point2f, 4>;

    cv::Mat img;
    cv::Mat red_mask_;
    cv::Mat blue_mask_;

    struct ValidationParams {
        int brightness_thres;
        int color_diff_thres;
        double outward_width_ratio;
        double inward_tolerance_ratio;
        double vertical_margin_ratio;
        double min_color_ratio;
    } params_;

    std::vector<std::string> color_names_;
    std::vector<std::string> tag_names_;

    struct VisualizationData {

        struct DetectionStatus {
            DetectionObject detection;
            bool passed;
        };

        struct SearchStatus {
            SearchPolygon polygon;
            bool matched;
            double color_ratio;
        };

        std::vector<DetectionStatus> detections;
        std::vector<SearchStatus> searches;
        cv::Mat binary_image;

        void clear() {
            detections.clear();
            searches.clear();
            binary_image.release();
        }
    } viz_data_;

    SearchPolygon make_search_polygon(
        const cv::Point2f& top,
        const cv::Point2f& bottom,
        const cv::Point2f& armor_axis,
        bool is_left
    ) const;

    bool validate_search(const cv::Mat& color_mask, const SearchPolygon& polygon);
};
