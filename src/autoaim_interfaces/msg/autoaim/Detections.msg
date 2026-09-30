std_msgs/Header header

int32 mode # -1:null, 0:armor（本工程只保留装甲板）
int32 label # 当mode=0时，表示目标车辆的ID。其余情况应为-1

ArmorDetection[] armor_detections