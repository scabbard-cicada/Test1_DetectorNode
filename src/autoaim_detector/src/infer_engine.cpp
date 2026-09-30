#include "autoaim_detector/infer_engine.hpp"
#include <autoaim_common_definitions/common_definitions.hpp>
#include <omp.h>
#include <array>
#include <cmath>
#include <iostream>
#include <stdexcept>

std::vector<DetectionObject> InferEngine::get_detection_objects() const {
    return detection_arr_;
}

std::vector<DetectionObject>& InferEngine::get_detection_objects() {
    return detection_arr_;
}

float InferEngine::calculate_iou(const float lbox[4], const float rbox[4]) {
    float interBox[] = {
        std::max(lbox[0] - lbox[2] / 2.f, rbox[0] - rbox[2] / 2.f),
        std::min(lbox[0] + lbox[2] / 2.f, rbox[0] + rbox[2] / 2.f),
        std::max(lbox[1] - lbox[3] / 2.f, rbox[1] - rbox[3] / 2.f),
        std::min(lbox[1] + lbox[3] / 2.f, rbox[1] + rbox[3] / 2.f)
    };

    if (interBox[2] > interBox[3] || interBox[0] > interBox[1])
        return 0.0f;

    float interBoxS = (interBox[1] - interBox[0]) * (interBox[3] - interBox[2]);
    return interBoxS / (lbox[2] * lbox[3] + rbox[2] * rbox[3] - interBoxS);
}

void InferEngine::apply_nms(
    std::vector<int>& picked,
    std::vector<DetectionObject>& proposals,
    float nms_thresh
) {
    picked.clear();
    const int n = proposals.size();
    if (n == 0)
        return;

    std::vector<uint8_t> suppressed(n, 0);

    for (int i = 0; i < n; i++) {
        if (suppressed[i])
            continue;

        picked.push_back(i);
        const DetectionObject& a = proposals[i];

#pragma omp parallel for schedule(dynamic, 32) if (n > 100)
        for (int j = i + 1; j < n; j++) {
            if (!suppressed[j]) {
                const DetectionObject& b = proposals[j];
                if (calculate_iou(a.bbox, b.bbox) > nms_thresh)
                {
                    suppressed[j] = true;
                }
            }
        }
    }
}

void InferEngine::sort_proposals_by_confidence(std::vector<DetectionObject>& proposals) {
    if (!proposals.empty()) {
        std::sort(
            proposals.begin(),
            proposals.end(),
            [](const DetectionObject& a, const DetectionObject& b) { return a.conf > b.conf; }
        );
    }
}

cv::Mat InferEngine::draw_detections_on_image(
    const cv::Mat& image,
    const std::vector<DetectionObject>& detection_objects,
    const std::vector<std::string>& tag_names,
    const std::vector<std::string>& color_names
) const {
    cv::Mat debug_img = image.clone();

    std::vector<cv::Scalar> colors = ColorMapper::get_colors(color_names);

    const std::vector<cv::Scalar> kpt_colors = {
        cv::Scalar(255, 0, 0),
        cv::Scalar(0, 255, 0),
        cv::Scalar(0, 0, 255),
        cv::Scalar(255, 255, 0)
    };

    for (const auto& obj: detection_objects) {
        if (obj.kpts.size() < 4) {
            continue;
        }

        const std::array<cv::Point2f, 4> kpts = {
            obj.kpts[0].pt, obj.kpts[1].pt, obj.kpts[2].pt, obj.kpts[3].pt
        };

        for (size_t i = 0; i < kpts.size(); ++i) {
            cv::circle(debug_img, kpts[i], 5, kpt_colors[i], -1);
        }

        const cv::Scalar box_color = colors[obj.color];
        for (size_t j = 0; j < kpts.size(); ++j) {
            cv::line(debug_img, kpts[j], kpts[(j + 1) % 4], box_color, 2);
        }
        cv::line(debug_img, kpts[0], kpts[2], box_color, 1);
        cv::line(debug_img, kpts[1], kpts[3], box_color, 1);

        const std::string label =
            tag_names[obj.label] + " " + std::to_string(obj.conf).substr(0, 4);
        cv::putText(
            debug_img,
            label,
            kpts[0] - cv::Point2f(0, 15),
            cv::FONT_HERSHEY_SIMPLEX,
            0.7,
            cv::Scalar(255, 255, 255),
            2
        );
    }

    return debug_img;
}

std::vector<DetectionObject> InferEngine::generate_proposals_from_output(
    const float* output_data,
    int output_size,
    int num_color,
    int num_tag,
    int num_keypoints,
    int dim_keypoints,
    float conf_threshold,
    float keypoint_visibility_threshold,
    float scale_factor,
    float offset_x,
    float offset_y
) {
    std::vector<DetectionObject> proposals;
    if(mode_ == DetectionMode::ARMOR){
        const int detection_size = num_color + num_tag + num_keypoints * dim_keypoints;
        if (num_color != armor_model_contract::NUM_COLORS ||
            num_tag != armor_model_contract::NUM_CLASSES ||
            num_keypoints != armor_model_contract::NUM_KEYPOINTS ||
            dim_keypoints != armor_model_contract::KEYPOINT_DIMS ||
            detection_size != armor_model_contract::OUTPUT_COLUMNS ||
            output_size != armor_model_contract::OUTPUT_CANDIDATES * detection_size) {
            return proposals;
        }
        const int num_detections = output_size / detection_size;

        #pragma omp parallel for num_threads(4) schedule(guided)
        for (int i = 0; i < num_detections; ++i) {
            const int base_idx = i * detection_size;
            const float* row = &output_data[base_idx];

            const float* color_ptr = &row[0];
            const float* tag_ptr = &row[num_color];
            const float* max_color_it = std::max_element(color_ptr, color_ptr + num_color);
            const float* max_tag_it = std::max_element(tag_ptr, tag_ptr + num_tag);
            float confidence = (*max_color_it) * (*max_tag_it);

            if (confidence < conf_threshold)
                continue;

            int color = max_color_it - color_ptr;
            int label = max_tag_it - tag_ptr;

            std::vector<Keypoint> kpts;
            kpts.reserve(num_keypoints);
            const float* kpts_ptr = &row[num_color + num_tag];
            bool keypoints_visible = true;
            for (int k = 0; k < num_keypoints; ++k) {
                const float x = (kpts_ptr[dim_keypoints * k] - offset_x) / scale_factor;
                const float y = (kpts_ptr[dim_keypoints * k + 1] - offset_y) / scale_factor;
                const float kp_conf = kpts_ptr[dim_keypoints * k + 2];
                keypoints_visible = keypoints_visible &&
                    kp_conf >= keypoint_visibility_threshold;
                kpts.emplace_back(Keypoint {cv::Point2f(x, y), kp_conf});
            }
            if (!keypoints_visible)
                continue;

            float bbox[4] = {0.f, 0.f, 0.f, 0.f};
            if (!kpts.empty()) {
                float min_x = kpts[0].pt.x;
                float max_x = kpts[0].pt.x;
                float min_y = kpts[0].pt.y;
                float max_y = kpts[0].pt.y;
                for (const auto& kp : kpts) {
                    min_x = std::min(min_x, kp.pt.x);
                    max_x = std::max(max_x, kp.pt.x);
                    min_y = std::min(min_y, kp.pt.y);
                    max_y = std::max(max_y, kp.pt.y);
                }
                bbox[0] = (min_x + max_x) * 0.5f;
                bbox[1] = (min_y + max_y) * 0.5f;
                bbox[2] = std::max(0.0f, max_x - min_x);
                bbox[3] = std::max(0.0f, max_y - min_y);
            }

            DetectionObject obj;
            std::copy(bbox, bbox + 4, obj.bbox);
            obj.conf = confidence;
            obj.color = color;
            obj.label = label;
            obj.kpts = std::move(kpts);

            #pragma omp critical
            proposals.emplace_back(std::move(obj));
        }
    }

    return proposals;
}

ONNXRuntimeInferEngine::ONNXRuntimeInferEngine(
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
)
: NUM_COLOR(num_color),
  NUM_TAG(num_tag),
  NUM_KEYPOINTS(num_keypoints),
  DIM_KEYPOINTS(dim_keypoints),
  conf_threshold(conf_threshold),
  keypoint_visibility_threshold(keypoint_visibility_threshold),
  nms_threshold(nms_threshold),
  color_names(std::move(color_names)),
  tag_names(std::move(tag_names)),
  model_path(std::move(model_path))
{
    mode_ = mode;

    try {
        session_options_.SetIntraOpNumThreads(4);
        session_options_.SetGraphOptimizationLevel(
            GraphOptimizationLevel::ORT_ENABLE_ALL);

        session_ = std::make_unique<Ort::Session>(
            env_, this->model_path.c_str(), session_options_);

        Ort::AllocatorWithDefaultOptions allocator;
        {
            auto in_name = session_->GetInputNameAllocated(0, allocator);
            input_name_ = in_name.get();
        }
        {
            auto out_name = session_->GetOutputNameAllocated(0, allocator);
            output_name_ = out_name.get();
        }
        input_shape_ = session_->GetInputTypeInfo(0)
                           .GetTensorTypeAndShapeInfo().GetShape();

        if (input_shape_.size() == 4) {
            if (input_shape_[0] < 0) input_shape_[0] = armor_model_contract::INPUT_BATCH;
            if (input_shape_[1] < 0) input_shape_[1] = armor_model_contract::INPUT_CHANNELS;
            if (input_shape_[2] < 0) input_shape_[2] = armor_model_contract::INPUT_HEIGHT;
            if (input_shape_[3] < 0) input_shape_[3] = armor_model_contract::INPUT_WIDTH;

            input_image_height = static_cast<int>(input_shape_[2]);
            input_image_width  = static_cast<int>(input_shape_[3]);
        }

        contract_valid_ = true;
        std::cout << "[ONNXRuntimeInferEngine] loaded " << this->model_path
                  << "  input " << input_image_width << "x" << input_image_height
                  << "  (CPU onnxruntime " << ORT_API_VERSION << ")" << std::endl;
    } catch (const Ort::Exception& e) {
        std::cerr << "[ONNXRuntimeInferEngine] ORT error: " << e.what() << std::endl;
        contract_valid_ = false;
    } catch (const std::exception& e) {
        std::cerr << "[ONNXRuntimeInferEngine] " << e.what() << std::endl;
        contract_valid_ = false;
    }
}

void ONNXRuntimeInferEngine::set_input_image(const cv::Mat img) {
    image = img;
}

void ONNXRuntimeInferEngine::preprocess() {
    frame_valid_ = false;
    if (!contract_valid_) {
        detection_arr_.clear();
        return;
    }

    try {
        if (image.empty()) {
            throw std::runtime_error("input image is empty");
        }

        scale_factor_ = std::min(
            input_image_height / static_cast<float>(image.rows),
            input_image_width / static_cast<float>(image.cols));

        offset_x_ = -scale_factor_ * image.cols * 0.5f + input_image_width * 0.5f;
        offset_y_ = -scale_factor_ * image.rows * 0.5f + input_image_height * 0.5f;

        const int new_width = static_cast<int>(image.cols * scale_factor_);
        const int new_height = static_cast<int>(image.rows * scale_factor_);

        cv::Mat resized;
        cv::resize(image, resized, cv::Size(new_width, new_height), 0, 0, cv::INTER_LINEAR);

        if (preprocessed_image.size() != cv::Size(input_image_width, input_image_height)) {
            preprocessed_image.create(input_image_height, input_image_width, CV_8UC3);
        }
        preprocessed_image.setTo(cv::Scalar(128, 128, 128));

        const int x_offset = static_cast<int>(offset_x_);
        const int y_offset = static_cast<int>(offset_y_);
        const cv::Rect roi(x_offset, y_offset, new_width, new_height);
        resized.copyTo(preprocessed_image(roi));

        cv::Mat rgb;
        cv::cvtColor(preprocessed_image, rgb, cv::COLOR_BGR2RGB);
        cv::Mat float_mat;
        rgb.convertTo(float_mat, CV_32F, 1.0 / 255.0);

        const int chw_size = 1 * 3 * input_image_height * input_image_width;
        if (static_cast<int>(input_tensor_values_.size()) != chw_size) {
            input_tensor_values_.resize(chw_size);
        }

        const float* hwc = reinterpret_cast<const float*>(float_mat.data);
        const int area = input_image_height * input_image_width;
        for (int c = 0; c < 3; ++c) {
            for (int h = 0; h < input_image_height; ++h) {
                for (int w = 0; w < input_image_width; ++w) {
                    input_tensor_values_[c * area + h * input_image_width + w] =
                        hwc[(h * input_image_width + w) * 3 + c];
                }
            }
        }

        frame_valid_ = true;
    } catch (const std::exception& e) {
        std::cerr << "[ONNXRuntimeInferEngine] preprocess failed: " << e.what() << std::endl;
        detection_arr_.clear();
    }
}

void ONNXRuntimeInferEngine::infer() {
    if (!frame_valid_) {
        return;
    }
    try {

        Ort::Value input_tensor = Ort::Value::CreateTensor<float>(
            memory_info_,
            input_tensor_values_.data(),
            input_tensor_values_.size(),
            input_shape_.data(),
            input_shape_.size());

        const char* input_names_cstr[] = {input_name_.c_str()};
        const char* output_names_cstr[] = {output_name_.c_str()};

        auto output_tensors = session_->Run(
            Ort::RunOptions{nullptr},
            input_names_cstr,
            &input_tensor,
            1,
            output_names_cstr,
            1);

        auto& out_tensor = output_tensors.front();
        auto* out_data = out_tensor.GetTensorData<float>();
        auto out_info = out_tensor.GetTensorTypeAndShapeInfo();
        output_element_count_ = static_cast<int>(out_info.GetElementCount());
        output_values_.assign(out_data, out_data + output_element_count_);
    } catch (const Ort::Exception& e) {
        std::cerr << "[ONNXRuntimeInferEngine] infer failed: " << e.what() << std::endl;
        frame_valid_ = false;
        detection_arr_.clear();
    }
}

void ONNXRuntimeInferEngine::postprocess() {
    detection_arr_.clear();
    if (!frame_valid_ || output_values_.empty()) {
        return;
    }

    constexpr int expected_count =
        armor_model_contract::OUTPUT_CANDIDATES * armor_model_contract::OUTPUT_COLUMNS;
    if (output_element_count_ != expected_count) {
        if (!output_error_reported_) {
            std::cerr << "[ONNXRuntimeInferEngine] output does not match armor_model_contract"
                      << " (candidates=" << armor_model_contract::OUTPUT_CANDIDATES
                      << ", columns=" << armor_model_contract::OUTPUT_COLUMNS
                      << ", expected count=" << expected_count
                      << ", got count=" << output_element_count_ << ")" << std::endl;
            output_error_reported_ = true;
        }
        return;
    }

    std::vector<DetectionObject> proposals = generate_proposals_from_output(
        output_values_.data(), output_element_count_,
        NUM_COLOR, NUM_TAG, NUM_KEYPOINTS, DIM_KEYPOINTS,
        conf_threshold, keypoint_visibility_threshold,
        scale_factor_, offset_x_, offset_y_);

    if (proposals.empty()) {
        return;
    }

    sort_proposals_by_confidence(proposals);

    std::vector<int> picked;
    apply_nms(picked, proposals, nms_threshold);

    detection_arr_.reserve(picked.size());
    for (const int idx : picked) {
        detection_arr_.push_back(proposals[idx]);
    }
}

cv::Mat ONNXRuntimeInferEngine::draw_labeled_image() {
    if (image.empty()) {
        return cv::Mat();
    }
    return draw_detections_on_image(image, detection_arr_, tag_names, color_names);
}

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
) {
    return std::make_unique<ONNXRuntimeInferEngine>(
        std::move(model_path),
        num_color,
        num_tag,
        num_keypoints,
        dim_keypoints,
        conf_threshold,
        keypoint_visibility_threshold,
        nms_threshold,
        std::move(color_name),
        std::move(tag_name),
        mode
    );
}
