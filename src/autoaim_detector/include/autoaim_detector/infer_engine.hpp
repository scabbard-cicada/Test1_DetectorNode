#pragma once
#include <memory>
#include <string>
#include <vector>
#include <algorithm>
#include <onnxruntime_cxx_api.h>
#include <opencv2/opencv.hpp>
#include "autoaim_detector/detector_common.hpp"
#include "autoaim_common_definitions/common_definitions.hpp"

namespace armor_model_contract {
inline constexpr char INPUT_NAME[] = "images";
inline constexpr char OUTPUT_NAME[] = "output0_bpc";
inline constexpr int INPUT_BATCH = 1;
inline constexpr int INPUT_CHANNELS = 3;
inline constexpr int INPUT_HEIGHT = 384;
inline constexpr int INPUT_WIDTH = 640;
inline constexpr int OUTPUT_BATCH = 1;
inline constexpr int OUTPUT_CANDIDATES = 5040;
inline constexpr int OUTPUT_COLUMNS = 23;
inline constexpr int NUM_COLORS = 3;
inline constexpr int NUM_CLASSES = 8;
inline constexpr int NUM_KEYPOINTS = 4;
inline constexpr int KEYPOINT_DIMS = 3;
}

class InferEngine {
public:

    virtual ~InferEngine() = default;

    virtual void preprocess() = 0;
    virtual void infer() = 0;
    virtual void postprocess() = 0;

    virtual void set_input_image(const cv::Mat img) = 0;

    virtual cv::Mat draw_labeled_image() = 0;

    std::vector<DetectionObject> get_detection_objects() const;
    std::vector<DetectionObject>& get_detection_objects();

protected:
    DetectionMode mode_ = DetectionMode::NONE;

    std::vector<DetectionObject> detection_arr_;

    static float calculate_iou(const float lbox[4], const float rbox[4]);

    static void apply_nms(std::vector<int>& picked, std::vector<DetectionObject>& proposals, float nms_thresh);

    static void sort_proposals_by_confidence(std::vector<DetectionObject>& proposals);

    cv::Mat draw_detections_on_image(
        const cv::Mat& image,
        const std::vector<DetectionObject>& detection_objects,
        const std::vector<std::string>& tag_names,
        const std::vector<std::string>& color_names
    ) const;

    std::vector<DetectionObject> generate_proposals_from_output(
        const float* output_data,
        int output_size,
        int num_color,
        int num_tag,
        int num_keypoints,
        int dim_keypoints,
        float conf_threshold,
        float keypoint_visibility_threshold,
        float scale_factor = 1.0f,
        float offset_x = 0.0f,
        float offset_y = 0.0f
    );
};

class ONNXRuntimeInferEngine : public InferEngine {
public:
    ONNXRuntimeInferEngine(
        std::string model_path,
        int num_color,
        int num_tag,
        int num_keypoints,
        int dim_keypoints,
        float conf_threshold,
        float keypoint_visibility_threshold,
        float nms_threshold,
        std::vector<std::string> color_names,
        std::vector<std::string> tag_names,
        DetectionMode mode
    );
    ~ONNXRuntimeInferEngine() override = default;

    void set_input_image(const cv::Mat image) override;

    void preprocess() override;

    void infer() override;

    void postprocess() override;

    cv::Mat draw_labeled_image() override;

private:
    const int NUM_COLOR;
    const int NUM_TAG;
    const int NUM_KEYPOINTS;
    const int DIM_KEYPOINTS;
    const float conf_threshold;
    const float keypoint_visibility_threshold;
    const float nms_threshold;
    const std::vector<std::string> color_names;
    const std::vector<std::string> tag_names;

    std::string model_path;

    int input_image_width = armor_model_contract::INPUT_WIDTH;
    int input_image_height = armor_model_contract::INPUT_HEIGHT;

    Ort::Env env_{ORT_LOGGING_LEVEL_WARNING, "autoaim_detector"};
    Ort::SessionOptions session_options_;
    std::unique_ptr<Ort::Session> session_;

    Ort::MemoryInfo memory_info_{Ort::MemoryInfo::CreateCpu(
        OrtArenaAllocator, OrtMemTypeDefault)};

    std::string input_name_;
    std::string output_name_;
    std::vector<int64_t> input_shape_;

    cv::Mat image;
    cv::Mat preprocessed_image;
    std::vector<float> input_tensor_values_;
    std::vector<float> output_values_;
    int output_element_count_ = 0;

    float scale_factor_ = 1.0f;
    float offset_x_ = 0.0f;
    float offset_y_ = 0.0f;

    bool contract_valid_ = false;
    bool frame_valid_ = false;
    bool output_error_reported_ = false;
};

std::unique_ptr<InferEngine> create_infer_engine(
    std::string model_path,
    int num_color,
    int num_tag,
    int num_keypoints,
    int dim_keypoints,
    float conf_threshold,
    float keypoint_visibility_threshold,
    float nms_threshold,
    std::vector<std::string> color_name,
    std::vector<std::string> tag_name,
    DetectionMode mode
);
