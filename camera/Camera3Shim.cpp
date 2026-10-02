/*
 * Copyright (C) 2026 The LineageOS Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "Camera3Shim"
#define LOG_NDEBUG 0

#include <cutils/log.h>
#include <cutils/properties.h>
#include <dlfcn.h>
#include <sys/mman.h>
#include <hardware/hardware.h>
#include <hardware/camera.h>
#include <hardware/camera_common.h>
#include <hardware/camera3.h>
#include <camera/CameraMetadata.h>
#include <camera/CameraParameters.h>
#include <system/camera_metadata.h>
#include <utils/Mutex.h>
#include <utils/String8.h>
#include <ui/GraphicBufferMapper.h>
#include <ui/GraphicBuffer.h>
#include <ui/Rect.h>
#include <cstddef>

#include <vector>
#include <memory>
#include <atomic>
#include <thread>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <inttypes.h>
#include <poll.h>

using namespace android;

static Mutex gShimLock;
static camera_module_t *gVendorModule = nullptr;

#define MAX_CAMERAS 2
static camera_metadata_t *gStaticCharacteristics[MAX_CAMERAS] = {nullptr, nullptr};
static android::CameraMetadata gTemplates[MAX_CAMERAS][CAMERA3_TEMPLATE_COUNT];

#include <cutils/native_handle.h>

struct msm_priv_handle_t : public native_handle {
    int fd;
    int fd_metadata;
    int magic;
    int flags;
    unsigned int size;
    unsigned int offset;
    int bufferType;
    uint64_t base __attribute__((aligned(8)));
    unsigned int offset_metadata;
    uint64_t gpuaddr __attribute__((aligned(8)));
    int format;
    int width;
    int height;
};

static size_t get_jpeg_buffer_size(uint32_t width, uint32_t height, int camera_id) {
    uint32_t max_w = (camera_id == 0) ? 4160 : 2592;
    uint32_t max_h = (camera_id == 0) ? 3120 : 1944;
    float scaleFactor = ((float)(width * height)) / ((float)(max_w * max_h));
    size_t maxJpegBufferSize = 6 * 1024 * 1024;
    size_t kMinJpegBufferSize = 256 * 1024;
    return (size_t)(scaleFactor * (maxJpegBufferSize - kMinJpegBufferSize) + kMinJpegBufferSize);
}

static int check_vendor_module() {
    if (gVendorModule) {
        return 0;
    }

    void *handle = dlopen("/vendor/lib/hw/camera.vendor.msm8916.so", RTLD_NOW);
    if (!handle) {
        handle = dlopen("/vendor/lib/hw/camera.qcom_orig.so", RTLD_NOW);
    }
    if (handle) {
        const hw_module_t *hmi = (const hw_module_t *)dlsym(handle, HAL_MODULE_INFO_SYM_AS_STR);
        if (hmi) {
            gVendorModule = (camera_module_t *)hmi;
            ALOGI("Camera3Shim: Successfully loaded vendor module: %s (%s)", hmi->name, hmi->author);
            return 0;
        }
    }

    ALOGE("Camera3Shim: Failed to dlopen camera vendor HAL: %s", dlerror());
    return -ENODEV;
}

static camera_metadata_t *build_static_characteristics(int camera_id, int facing, int orientation) {
    if (gStaticCharacteristics[camera_id] != nullptr) {
        return gStaticCharacteristics[camera_id];
    }

    ALOGI("Camera3Shim: Building HAL3 static characteristics for camera %d (facing=%d, orientation=%d)",
          camera_id, facing, orientation);

    android::CameraMetadata meta;

    int32_t max_w = (camera_id == 0) ? 4160 : 2592;
    int32_t max_h = (camera_id == 0) ? 3120 : 1944;

    // Capabilities
    uint8_t capabilities[] = {
        ANDROID_REQUEST_AVAILABLE_CAPABILITIES_BACKWARD_COMPATIBLE
    };
    meta.update(ANDROID_REQUEST_AVAILABLE_CAPABILITIES, capabilities, sizeof(capabilities));

    // Supported Hardware Level
    uint8_t hardware_level = ANDROID_INFO_SUPPORTED_HARDWARE_LEVEL_LEGACY;
    meta.update(ANDROID_INFO_SUPPORTED_HARDWARE_LEVEL, &hardware_level, 1);

    // Lens facing & orientation
    uint8_t lens_facing = (facing == CAMERA_FACING_BACK) ?
                          ANDROID_LENS_FACING_BACK : ANDROID_LENS_FACING_FRONT;
    meta.update(ANDROID_LENS_FACING, &lens_facing, 1);

    int32_t sensor_orientation = orientation;
    meta.update(ANDROID_SENSOR_ORIENTATION, &sensor_orientation, 1);

    // Sensor sizes
    int32_t pixel_array_size[] = {max_w, max_h};
    meta.update(ANDROID_SENSOR_INFO_PIXEL_ARRAY_SIZE, pixel_array_size, 2);

    int32_t active_array_size[] = {0, 0, max_w, max_h};
    meta.update(ANDROID_SENSOR_INFO_ACTIVE_ARRAY_SIZE, active_array_size, 4);

    float sensor_physical_size[] = {
        (camera_id == 0) ? 4.71f : 3.63f,
        (camera_id == 0) ? 3.49f : 2.72f
    };
    meta.update(ANDROID_SENSOR_INFO_PHYSICAL_SIZE, sensor_physical_size, 2);

    uint8_t timestamp_source = ANDROID_SENSOR_INFO_TIMESTAMP_SOURCE_UNKNOWN;
    meta.update(ANDROID_SENSOR_INFO_TIMESTAMP_SOURCE, &timestamp_source, 1);

    int32_t sensitivity_range[] = {100, 1600};
    meta.update(ANDROID_SENSOR_INFO_SENSITIVITY_RANGE, sensitivity_range, 2);

    int32_t max_analog_sensitivity = 800;
    meta.update(ANDROID_SENSOR_MAX_ANALOG_SENSITIVITY, &max_analog_sensitivity, 1);

    int64_t exposure_time_range[] = {100000LL, 1000000000LL};
    meta.update(ANDROID_SENSOR_INFO_EXPOSURE_TIME_RANGE, exposure_time_range, 2);

    int64_t max_frame_duration = 1000000000LL;
    meta.update(ANDROID_SENSOR_INFO_MAX_FRAME_DURATION, &max_frame_duration, 1);

    // Stream configurations: format, width, height, direction
    std::vector<int32_t> stream_configs;
    std::vector<int64_t> min_frame_durations;
    std::vector<int64_t> stall_durations;

    struct Resolution { int32_t w; int32_t h; };
    std::vector<Resolution> preview_res = {
        {1920, 1080}, {1280, 960}, {1280, 720}, {960, 720}, {960, 540},
        {864, 480}, {768, 432}, {720, 480}, {640, 480}, {320, 240}, {176, 144}
    };
    std::vector<Resolution> jpeg_res;
    if (camera_id == 0) {
        jpeg_res = {
            {4160, 3120}, {4160, 2340}, {3840, 2160}, {3264, 2448}, {3264, 1836},
            {2560, 1920}, {2560, 1440}, {1920, 1080}, {1440, 1080}, {1280, 960},
            {1280, 720}, {640, 480}, {352, 288}, {320, 240}
        };
    } else {
        jpeg_res = {
            {2592, 1944}, {2560, 1920}, {2048, 1536}, {1920, 1080},
            {1280, 960}, {1280, 720}, {640, 480}, {320, 240}
        };
    }

    int32_t out_dir = ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS_OUTPUT;
    int64_t duration_30fps = 33333333LL; // 30 fps

    // JPEG (BLOB)
    for (const auto& r : jpeg_res) {
        stream_configs.push_back(HAL_PIXEL_FORMAT_BLOB);
        stream_configs.push_back(r.w);
        stream_configs.push_back(r.h);
        stream_configs.push_back(out_dir);

        min_frame_durations.push_back(HAL_PIXEL_FORMAT_BLOB);
        min_frame_durations.push_back(r.w);
        min_frame_durations.push_back(r.h);
        min_frame_durations.push_back(duration_30fps);

        stall_durations.push_back(HAL_PIXEL_FORMAT_BLOB);
        stall_durations.push_back(r.w);
        stall_durations.push_back(r.h);
        stall_durations.push_back(duration_30fps);
    }

    // IMPLEMENTATION_DEFINED (Preview)
    for (const auto& r : preview_res) {
        stream_configs.push_back(HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED);
        stream_configs.push_back(r.w);
        stream_configs.push_back(r.h);
        stream_configs.push_back(out_dir);

        min_frame_durations.push_back(HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED);
        min_frame_durations.push_back(r.w);
        min_frame_durations.push_back(r.h);
        min_frame_durations.push_back(duration_30fps);

        stall_durations.push_back(HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED);
        stall_durations.push_back(r.w);
        stall_durations.push_back(r.h);
        stall_durations.push_back(0); // non-stalling
    }

    // YCBCR_420_888
    for (const auto& r : preview_res) {
        stream_configs.push_back(HAL_PIXEL_FORMAT_YCBCR_420_888);
        stream_configs.push_back(r.w);
        stream_configs.push_back(r.h);
        stream_configs.push_back(out_dir);

        min_frame_durations.push_back(HAL_PIXEL_FORMAT_YCBCR_420_888);
        min_frame_durations.push_back(r.w);
        min_frame_durations.push_back(r.h);
        min_frame_durations.push_back(duration_30fps);

        stall_durations.push_back(HAL_PIXEL_FORMAT_YCBCR_420_888);
        stall_durations.push_back(r.w);
        stall_durations.push_back(r.h);
        stall_durations.push_back(0);
    }

    // YCrCb_420_SP (NV21)
    for (const auto& r : preview_res) {
        stream_configs.push_back(HAL_PIXEL_FORMAT_YCrCb_420_SP);
        stream_configs.push_back(r.w);
        stream_configs.push_back(r.h);
        stream_configs.push_back(out_dir);

        min_frame_durations.push_back(HAL_PIXEL_FORMAT_YCrCb_420_SP);
        min_frame_durations.push_back(r.w);
        min_frame_durations.push_back(r.h);
        min_frame_durations.push_back(duration_30fps);

        stall_durations.push_back(HAL_PIXEL_FORMAT_YCrCb_420_SP);
        stall_durations.push_back(r.w);
        stall_durations.push_back(r.h);
        stall_durations.push_back(0);
    }

    meta.update(ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS, stream_configs.data(), stream_configs.size());
    meta.update(ANDROID_SCALER_AVAILABLE_MIN_FRAME_DURATIONS, min_frame_durations.data(), min_frame_durations.size());
    meta.update(ANDROID_SCALER_AVAILABLE_STALL_DURATIONS, stall_durations.data(), stall_durations.size());

    uint8_t cropping_type = ANDROID_SCALER_CROPPING_TYPE_CENTER_ONLY;
    meta.update(ANDROID_SCALER_CROPPING_TYPE, &cropping_type, 1);

    float max_digital_zoom = 4.0f;
    meta.update(ANDROID_SCALER_AVAILABLE_MAX_DIGITAL_ZOOM, &max_digital_zoom, 1);

    uint8_t rotate_crop_modes[] = { ANDROID_SCALER_ROTATE_AND_CROP_NONE };
    meta.update(ANDROID_SCALER_AVAILABLE_ROTATE_AND_CROP_MODES, rotate_crop_modes, sizeof(rotate_crop_modes));

    // Request settings
    int32_t max_output_streams[] = {0, 3, 1}; // raw=0, non-stalling=3, stalling=1
    meta.update(ANDROID_REQUEST_MAX_NUM_OUTPUT_STREAMS, max_output_streams, 3);

    int32_t max_input_streams = 0;
    meta.update(ANDROID_REQUEST_MAX_NUM_INPUT_STREAMS, &max_input_streams, 1);

    uint8_t pipeline_max_depth = 4;
    meta.update(ANDROID_REQUEST_PIPELINE_MAX_DEPTH, &pipeline_max_depth, 1);

    int32_t partial_result_count = 1;
    meta.update(ANDROID_REQUEST_PARTIAL_RESULT_COUNT, &partial_result_count, 1);

    int32_t sync_max_latency = ANDROID_SYNC_MAX_LATENCY_UNKNOWN;
    meta.update(ANDROID_SYNC_MAX_LATENCY, &sync_max_latency, 1);

    // AE / AF / AWB / Effects
    uint8_t ae_modes[] = {
        ANDROID_CONTROL_AE_MODE_OFF,
        ANDROID_CONTROL_AE_MODE_ON,
        ANDROID_CONTROL_AE_MODE_ON_AUTO_FLASH,
        ANDROID_CONTROL_AE_MODE_ON_ALWAYS_FLASH
    };
    if (facing == CAMERA_FACING_BACK) {
        meta.update(ANDROID_CONTROL_AE_AVAILABLE_MODES, ae_modes, 4);
    } else {
        meta.update(ANDROID_CONTROL_AE_AVAILABLE_MODES, ae_modes, 2);
    }

    int32_t fps_ranges[] = {15, 30, 30, 30};
    meta.update(ANDROID_CONTROL_AE_AVAILABLE_TARGET_FPS_RANGES, fps_ranges, 4);

    int32_t compensation_range[] = {-2, 2};
    meta.update(ANDROID_CONTROL_AE_COMPENSATION_RANGE, compensation_range, 2);

    camera_metadata_rational_t compensation_step = {1, 1};
    meta.update(ANDROID_CONTROL_AE_COMPENSATION_STEP, &compensation_step, 1);

    uint8_t ae_lock = ANDROID_CONTROL_AE_LOCK_AVAILABLE_TRUE;
    meta.update(ANDROID_CONTROL_AE_LOCK_AVAILABLE, &ae_lock, 1);

    uint8_t antibanding_modes[] = {
        ANDROID_CONTROL_AE_ANTIBANDING_MODE_OFF,
        ANDROID_CONTROL_AE_ANTIBANDING_MODE_50HZ,
        ANDROID_CONTROL_AE_ANTIBANDING_MODE_60HZ,
        ANDROID_CONTROL_AE_ANTIBANDING_MODE_AUTO
    };
    meta.update(ANDROID_CONTROL_AE_AVAILABLE_ANTIBANDING_MODES, antibanding_modes, sizeof(antibanding_modes));

    uint8_t af_modes[] = {
        ANDROID_CONTROL_AF_MODE_OFF,
        ANDROID_CONTROL_AF_MODE_AUTO,
        ANDROID_CONTROL_AF_MODE_MACRO,
        ANDROID_CONTROL_AF_MODE_CONTINUOUS_PICTURE,
        ANDROID_CONTROL_AF_MODE_CONTINUOUS_VIDEO
    };
    meta.update(ANDROID_CONTROL_AF_AVAILABLE_MODES, af_modes, sizeof(af_modes));

    uint8_t awb_modes[] = {
        ANDROID_CONTROL_AWB_MODE_OFF,
        ANDROID_CONTROL_AWB_MODE_AUTO,
        ANDROID_CONTROL_AWB_MODE_INCANDESCENT,
        ANDROID_CONTROL_AWB_MODE_FLUORESCENT,
        ANDROID_CONTROL_AWB_MODE_WARM_FLUORESCENT,
        ANDROID_CONTROL_AWB_MODE_DAYLIGHT,
        ANDROID_CONTROL_AWB_MODE_CLOUDY_DAYLIGHT,
        ANDROID_CONTROL_AWB_MODE_TWILIGHT,
        ANDROID_CONTROL_AWB_MODE_SHADE
    };
    meta.update(ANDROID_CONTROL_AWB_AVAILABLE_MODES, awb_modes, sizeof(awb_modes));

    uint8_t awb_lock = ANDROID_CONTROL_AWB_LOCK_AVAILABLE_TRUE;
    meta.update(ANDROID_CONTROL_AWB_LOCK_AVAILABLE, &awb_lock, 1);

    uint8_t control_modes[] = {
        ANDROID_CONTROL_MODE_OFF,
        ANDROID_CONTROL_MODE_AUTO,
        ANDROID_CONTROL_MODE_USE_SCENE_MODE
    };
    meta.update(ANDROID_CONTROL_AVAILABLE_MODES, control_modes, sizeof(control_modes));

    uint8_t effects[] = {
        ANDROID_CONTROL_EFFECT_MODE_OFF,
        ANDROID_CONTROL_EFFECT_MODE_MONO,
        ANDROID_CONTROL_EFFECT_MODE_NEGATIVE,
        ANDROID_CONTROL_EFFECT_MODE_SOLARIZE,
        ANDROID_CONTROL_EFFECT_MODE_SEPIA,
        ANDROID_CONTROL_EFFECT_MODE_POSTERIZE,
        ANDROID_CONTROL_EFFECT_MODE_WHITEBOARD,
        ANDROID_CONTROL_EFFECT_MODE_BLACKBOARD,
        ANDROID_CONTROL_EFFECT_MODE_AQUA
    };
    meta.update(ANDROID_CONTROL_AVAILABLE_EFFECTS, effects, sizeof(effects));

    uint8_t scene_modes[] = {
        ANDROID_CONTROL_SCENE_MODE_DISABLED,
        ANDROID_CONTROL_SCENE_MODE_FACE_PRIORITY,
        ANDROID_CONTROL_SCENE_MODE_ACTION,
        ANDROID_CONTROL_SCENE_MODE_NIGHT,
        ANDROID_CONTROL_SCENE_MODE_PORTRAIT,
        ANDROID_CONTROL_SCENE_MODE_LANDSCAPE
    };
    meta.update(ANDROID_CONTROL_AVAILABLE_SCENE_MODES, scene_modes, sizeof(scene_modes));

    int32_t max_regions[] = {1, 0, 1}; // AE, AWB, AF
    meta.update(ANDROID_CONTROL_MAX_REGIONS, max_regions, 3);

    uint8_t vstab_modes[] = { ANDROID_CONTROL_VIDEO_STABILIZATION_MODE_OFF };
    meta.update(ANDROID_CONTROL_AVAILABLE_VIDEO_STABILIZATION_MODES, vstab_modes, 1);

    // Flash
    uint8_t flash_available = (facing == CAMERA_FACING_BACK) ? 1 : 0;
    meta.update(ANDROID_FLASH_INFO_AVAILABLE, &flash_available, 1);

    // Lens
    float apertures[] = {2.0f};
    meta.update(ANDROID_LENS_INFO_AVAILABLE_APERTURES, apertures, 1);

    float filter_densities[] = {0.0f};
    meta.update(ANDROID_LENS_INFO_AVAILABLE_FILTER_DENSITIES, filter_densities, 1);

    float focal_lengths[] = {3.5f};
    meta.update(ANDROID_LENS_INFO_AVAILABLE_FOCAL_LENGTHS, focal_lengths, 1);

    uint8_t ois[] = {ANDROID_LENS_OPTICAL_STABILIZATION_MODE_OFF};
    meta.update(ANDROID_LENS_INFO_AVAILABLE_OPTICAL_STABILIZATION, ois, 1);

    float hyperfocal_distance = 0.0f;
    meta.update(ANDROID_LENS_INFO_HYPERFOCAL_DISTANCE, &hyperfocal_distance, 1);

    float min_focus_distance = 0.1f;
    meta.update(ANDROID_LENS_INFO_MINIMUM_FOCUS_DISTANCE, &min_focus_distance, 1);

    // JPEG
    int32_t thumbnail_sizes[] = {0, 0, 160, 120, 320, 240, 512, 288};
    meta.update(ANDROID_JPEG_AVAILABLE_THUMBNAIL_SIZES, thumbnail_sizes, sizeof(thumbnail_sizes)/sizeof(int32_t));

    int32_t jpeg_max_size = 6 * 1024 * 1024;
    meta.update(ANDROID_JPEG_MAX_SIZE, &jpeg_max_size, 1);

    // Noise reduction & Edge & Shading & Hotpixel
    uint8_t nr_modes[] = {
        ANDROID_NOISE_REDUCTION_MODE_OFF,
        ANDROID_NOISE_REDUCTION_MODE_FAST,
        ANDROID_NOISE_REDUCTION_MODE_HIGH_QUALITY
    };
    meta.update(ANDROID_NOISE_REDUCTION_AVAILABLE_NOISE_REDUCTION_MODES, nr_modes, sizeof(nr_modes));

    uint8_t edge_modes[] = {
        ANDROID_EDGE_MODE_OFF,
        ANDROID_EDGE_MODE_FAST,
        ANDROID_EDGE_MODE_HIGH_QUALITY
    };
    meta.update(ANDROID_EDGE_AVAILABLE_EDGE_MODES, edge_modes, sizeof(edge_modes));

    uint8_t shading_modes[] = {
        ANDROID_SHADING_MODE_OFF,
        ANDROID_SHADING_MODE_FAST,
        ANDROID_SHADING_MODE_HIGH_QUALITY
    };
    meta.update(ANDROID_SHADING_AVAILABLE_MODES, shading_modes, sizeof(shading_modes));

    uint8_t hotpixel_modes[] = {
        ANDROID_HOT_PIXEL_MODE_OFF,
        ANDROID_HOT_PIXEL_MODE_FAST,
        ANDROID_HOT_PIXEL_MODE_HIGH_QUALITY
    };
    meta.update(ANDROID_HOT_PIXEL_AVAILABLE_HOT_PIXEL_MODES, hotpixel_modes, sizeof(hotpixel_modes));

    uint8_t aberration_modes[] = {
        ANDROID_COLOR_CORRECTION_ABERRATION_MODE_OFF,
        ANDROID_COLOR_CORRECTION_ABERRATION_MODE_FAST,
        ANDROID_COLOR_CORRECTION_ABERRATION_MODE_HIGH_QUALITY
    };
    meta.update(ANDROID_COLOR_CORRECTION_AVAILABLE_ABERRATION_MODES, aberration_modes, sizeof(aberration_modes));

    // Face detection
    uint8_t face_detect_modes[] = {ANDROID_STATISTICS_FACE_DETECT_MODE_OFF};
    meta.update(ANDROID_STATISTICS_INFO_AVAILABLE_FACE_DETECT_MODES, face_detect_modes, 1);

    int32_t max_face_count = 0;
    meta.update(ANDROID_STATISTICS_INFO_MAX_FACE_COUNT, &max_face_count, 1);

    // Request & Result keys
    std::vector<int32_t> request_keys = {
        ANDROID_COLOR_CORRECTION_ABERRATION_MODE,
        ANDROID_CONTROL_AE_ANTIBANDING_MODE,
        ANDROID_CONTROL_AE_EXPOSURE_COMPENSATION,
        ANDROID_CONTROL_AE_LOCK,
        ANDROID_CONTROL_AE_MODE,
        ANDROID_CONTROL_AE_TARGET_FPS_RANGE,
        ANDROID_CONTROL_AF_MODE,
        ANDROID_CONTROL_AF_TRIGGER,
        ANDROID_CONTROL_AWB_LOCK,
        ANDROID_CONTROL_AWB_MODE,
        ANDROID_CONTROL_CAPTURE_INTENT,
        ANDROID_CONTROL_EFFECT_MODE,
        ANDROID_CONTROL_MODE,
        ANDROID_CONTROL_SCENE_MODE,
        ANDROID_FLASH_MODE,
        ANDROID_JPEG_QUALITY,
        ANDROID_JPEG_THUMBNAIL_QUALITY,
        ANDROID_JPEG_THUMBNAIL_SIZE,
        ANDROID_SCALER_CROP_REGION,
    };
    meta.update(ANDROID_REQUEST_AVAILABLE_REQUEST_KEYS, request_keys.data(), request_keys.size());

    std::vector<int32_t> result_keys = request_keys;
    result_keys.push_back(ANDROID_SENSOR_TIMESTAMP);
    result_keys.push_back(ANDROID_CONTROL_AF_STATE);
    result_keys.push_back(ANDROID_CONTROL_AE_STATE);
    result_keys.push_back(ANDROID_CONTROL_AWB_STATE);
    result_keys.push_back(ANDROID_FLASH_STATE);
    result_keys.push_back(ANDROID_LENS_STATE);
    result_keys.push_back(ANDROID_REQUEST_PIPELINE_DEPTH);
    result_keys.push_back(ANDROID_SENSOR_EXPOSURE_TIME);
    result_keys.push_back(ANDROID_SENSOR_SENSITIVITY);
    result_keys.push_back(ANDROID_LENS_APERTURE);
    result_keys.push_back(ANDROID_LENS_FOCAL_LENGTH);
    meta.update(ANDROID_REQUEST_AVAILABLE_RESULT_KEYS, result_keys.data(), result_keys.size());

    // Characteristics keys: Automatically extract all keys currently present in meta
    const camera_metadata_t *raw_meta = meta.getAndLock();
    size_t count = get_camera_metadata_entry_count(raw_meta);
    std::vector<int32_t> char_keys;
    char_keys.reserve(count + 1);
    for (size_t i = 0; i < count; ++i) {
        camera_metadata_ro_entry_t entry;
        if (get_camera_metadata_ro_entry(raw_meta, i, &entry) == 0) {
            char_keys.push_back(entry.tag);
        }
    }
    char_keys.push_back(ANDROID_REQUEST_AVAILABLE_CHARACTERISTICS_KEYS);
    meta.unlock(raw_meta);

    meta.update(ANDROID_REQUEST_AVAILABLE_CHARACTERISTICS_KEYS, char_keys.data(), char_keys.size());

    // Lock and cache the final buffer
    const camera_metadata_t *final_meta = meta.getAndLock();
    gStaticCharacteristics[camera_id] = clone_camera_metadata(final_meta);
    meta.unlock(final_meta);

    ALOGI("Camera3Shim: Successfully built static characteristics for camera %d with %zu keys!",
          camera_id, char_keys.size());

    return gStaticCharacteristics[camera_id];
}

static void init_default_templates(int camera_id) {
    int32_t max_w = (camera_id == 0) ? 4160 : 2592;
    int32_t max_h = (camera_id == 0) ? 3120 : 1944;

    for (int t = 1; t < CAMERA3_TEMPLATE_COUNT; ++t) {
        android::CameraMetadata meta;

        uint8_t intent = (uint8_t)t;
        meta.update(ANDROID_CONTROL_CAPTURE_INTENT, &intent, 1);

        uint8_t control_mode = ANDROID_CONTROL_MODE_AUTO;
        meta.update(ANDROID_CONTROL_MODE, &control_mode, 1);

        uint8_t ae_mode = ANDROID_CONTROL_AE_MODE_ON;
        meta.update(ANDROID_CONTROL_AE_MODE, &ae_mode, 1);

        uint8_t af_mode = ANDROID_CONTROL_AF_MODE_AUTO;
        meta.update(ANDROID_CONTROL_AF_MODE, &af_mode, 1);

        uint8_t awb_mode = ANDROID_CONTROL_AWB_MODE_AUTO;
        meta.update(ANDROID_CONTROL_AWB_MODE, &awb_mode, 1);

        uint8_t flash_mode = ANDROID_FLASH_MODE_OFF;
        meta.update(ANDROID_FLASH_MODE, &flash_mode, 1);

        int32_t crop_region[] = {0, 0, max_w, max_h};
        meta.update(ANDROID_SCALER_CROP_REGION, crop_region, 4);

        int32_t fps_range[] = {15, 30};
        meta.update(ANDROID_CONTROL_AE_TARGET_FPS_RANGE, fps_range, 2);

        uint8_t jpeg_quality = 90;
        meta.update(ANDROID_JPEG_QUALITY, &jpeg_quality, 1);

        uint8_t thumbnail_quality = 85;
        meta.update(ANDROID_JPEG_THUMBNAIL_QUALITY, &thumbnail_quality, 1);

        int32_t thumbnail_size[] = {320, 240};
        meta.update(ANDROID_JPEG_THUMBNAIL_SIZE, thumbnail_size, 2);

        float focal_length = 3.5f;
        meta.update(ANDROID_LENS_FOCAL_LENGTH, &focal_length, 1);

        uint8_t enable_zsl = ANDROID_CONTROL_ENABLE_ZSL_FALSE;
        meta.update(ANDROID_CONTROL_ENABLE_ZSL, &enable_zsl, 1);

        gTemplates[camera_id][t] = meta;
    }
}

/*******************************************************************
 * camera3_device_ops implementation
 *******************************************************************/
#define SHIM_MAX_PREVIEW_BUFS 16

struct ShimRequest {
    uint32_t frame_number;
    std::vector<camera3_stream_buffer_t> output_buffers;
    bool is_jpeg;
    int64_t shutter_timestamp;
};

struct shim_camera3_device {
    camera3_device_t base;
    int camera_id;
    const camera3_callback_ops_t *callbacks;

    camera_device_t *vendor_dev;
    preview_stream_ops_t preview_window;

    camera3_stream_t *preview_stream;
    camera3_stream_t *jpeg_stream;

    Mutex request_lock;
    std::vector<ShimRequest> pending_requests;
    bool preview_running;
    std::atomic<bool> is_restarting{false};
    std::atomic<bool> is_taking_picture{false};
    std::atomic<bool> is_closing{false};
    std::thread restart_thread;

    Mutex shutter_lock;
    uint32_t last_shutter_frame{0};
    bool has_shutter_frame{false};

    std::string current_flash_mode{CameraParameters::FLASH_MODE_OFF};
    uint8_t last_ae_mode{ANDROID_CONTROL_AE_MODE_ON};
    uint8_t last_flash_mode{ANDROID_FLASH_MODE_OFF};

    int pw_buf_count;
    int pw_width;
    int pw_height;
    int pw_format;
    sp<GraphicBuffer> pw_buffers[SHIM_MAX_PREVIEW_BUFS];
    buffer_handle_t pw_handles[SHIM_MAX_PREVIEW_BUFS];
    bool pw_buf_dequeued[SHIM_MAX_PREVIEW_BUFS];
    std::vector<uint8_t> last_preview_frame;
    size_t last_preview_size;
    int last_preview_w{0};
    int last_preview_h{0};
    int last_preview_stride{0};
    int last_preview_scanlines{0};
};

static inline shim_camera3_device* get_shim_dev(struct preview_stream_ops* window) {
    return (shim_camera3_device *)((char *)window - offsetof(shim_camera3_device, preview_window));
}

static void dummy_release_memory(struct camera_memory *mem) {
    if (mem) {
        if (mem->handle) {
            munmap(mem->data, mem->size);
        } else if (mem->data) {
            free(mem->data);
        }
        delete mem;
    }
}

static camera_memory_t* shim_get_memory(int fd, size_t buf_size, unsigned int num_bufs, void *user) {
    (void)user;
    camera_memory_t *mem = new camera_memory_t();
    mem->size = buf_size * num_bufs;
    mem->release = dummy_release_memory;
    if (fd >= 0) {
        mem->data = mmap(NULL, mem->size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        mem->handle = (void*)1;
    } else {
        mem->data = malloc(mem->size);
        mem->handle = nullptr;
    }
    return mem;
}

static std::atomic<int64_t> gLastTimestamp(0);
static int64_t get_next_monotonic_timestamp() {
    int64_t now = systemTime(SYSTEM_TIME_BOOTTIME);
    int64_t last = gLastTimestamp.load();
    while (true) {
        int64_t next = (now > last) ? now : (last + 33333333LL);
        if (gLastTimestamp.compare_exchange_weak(last, next)) {
            return next;
        }
    }
}

static void populate_result_metadata(shim_camera3_device *sdev, android::CameraMetadata &meta, int64_t timestamp, bool is_jpeg = false) {
    meta.update(ANDROID_SENSOR_TIMESTAMP, &timestamp, 1);

    uint8_t control_mode = ANDROID_CONTROL_MODE_AUTO;
    meta.update(ANDROID_CONTROL_MODE, &control_mode, 1);

    uint8_t af_mode = ANDROID_CONTROL_AF_MODE_AUTO;
    meta.update(ANDROID_CONTROL_AF_MODE, &af_mode, 1);

    uint8_t af_state = ANDROID_CONTROL_AF_STATE_FOCUSED_LOCKED;
    meta.update(ANDROID_CONTROL_AF_STATE, &af_state, 1);

    uint8_t af_trigger = ANDROID_CONTROL_AF_TRIGGER_IDLE;
    meta.update(ANDROID_CONTROL_AF_TRIGGER, &af_trigger, 1);

    uint8_t ae_mode = sdev ? sdev->last_ae_mode : ANDROID_CONTROL_AE_MODE_ON;
    meta.update(ANDROID_CONTROL_AE_MODE, &ae_mode, 1);

    uint8_t ae_state = ANDROID_CONTROL_AE_STATE_CONVERGED;
    if (sdev && (sdev->last_ae_mode == ANDROID_CONTROL_AE_MODE_ON_AUTO_FLASH ||
                 sdev->last_ae_mode == ANDROID_CONTROL_AE_MODE_ON_ALWAYS_FLASH)) {
        if (sdev->current_flash_mode != CameraParameters::FLASH_MODE_OFF) {
            ae_state = ANDROID_CONTROL_AE_STATE_FLASH_REQUIRED;
        }
    }
    meta.update(ANDROID_CONTROL_AE_STATE, &ae_state, 1);

    uint8_t awb_mode = ANDROID_CONTROL_AWB_MODE_AUTO;
    meta.update(ANDROID_CONTROL_AWB_MODE, &awb_mode, 1);

    uint8_t awb_state = ANDROID_CONTROL_AWB_STATE_CONVERGED;
    meta.update(ANDROID_CONTROL_AWB_STATE, &awb_state, 1);

    uint8_t flash_state = (sdev && sdev->current_flash_mode != CameraParameters::FLASH_MODE_OFF && is_jpeg)
                              ? ANDROID_FLASH_STATE_FIRED
                              : ANDROID_FLASH_STATE_READY;
    meta.update(ANDROID_FLASH_STATE, &flash_state, 1);

    uint8_t flash_mode = sdev ? sdev->last_flash_mode : ANDROID_FLASH_MODE_OFF;
    meta.update(ANDROID_FLASH_MODE, &flash_mode, 1);

    int64_t exposure_time = 33333333L;
    meta.update(ANDROID_SENSOR_EXPOSURE_TIME, &exposure_time, 1);

    int32_t sensitivity = 100;
    meta.update(ANDROID_SENSOR_SENSITIVITY, &sensitivity, 1);

    float aperture = 2.0f;
    meta.update(ANDROID_LENS_APERTURE, &aperture, 1);

    float focal_length = 3.5f;
    meta.update(ANDROID_LENS_FOCAL_LENGTH, &focal_length, 1);

    uint8_t lens_state = ANDROID_LENS_STATE_STATIONARY;
    meta.update(ANDROID_LENS_STATE, &lens_state, 1);

    uint8_t pipeline_depth = 1;
    meta.update(ANDROID_REQUEST_PIPELINE_DEPTH, &pipeline_depth, 1);
}

static void shim_notify_shutter(shim_camera3_device *sdev, uint32_t frame_number, int64_t timestamp) {
    if (!sdev || !sdev->callbacks) return;

    {
        Mutex::Autolock l(sdev->shutter_lock);
        if (sdev->has_shutter_frame && frame_number <= sdev->last_shutter_frame) {
            ALOGE("Camera3Shim: CRITICAL: Out-of-order shutter prevented! Frame %u <= last %u",
                  frame_number, sdev->last_shutter_frame);
            return;
        }
        sdev->last_shutter_frame = frame_number;
        sdev->has_shutter_frame = true;
    }

    camera3_notify_msg_t notify_msg;
    memset(&notify_msg, 0, sizeof(notify_msg));
    notify_msg.type = CAMERA3_MSG_SHUTTER;
    notify_msg.message.shutter.frame_number = frame_number;
    notify_msg.message.shutter.timestamp = timestamp;
    sdev->callbacks->notify(sdev->callbacks, &notify_msg);
}

static void copy_yuv420sp_frame(void *dst, int dst_w, int dst_h, int dst_stride, int dst_scanlines,
                                const void *src, int src_w, int src_h, int src_stride, int src_scanlines) {
    if (!dst || !src || dst_w <= 0 || dst_h <= 0 || src_w <= 0 || src_h <= 0) {
        return;
    }
    if (dst_stride < dst_w) dst_stride = dst_w;
    if (dst_scanlines < dst_h) dst_scanlines = dst_h;
    if (src_stride < src_w) src_stride = src_w;
    if (src_scanlines < src_h) src_scanlines = src_h;

    const uint8_t *src_y = (const uint8_t *)src;
    uint8_t *dst_y = (uint8_t *)dst;
    const uint8_t *src_uv = src_y + (size_t)src_stride * src_scanlines;
    uint8_t *dst_uv = dst_y + (size_t)dst_stride * dst_scanlines;

    if (dst_w == src_w && dst_h == src_h && dst_stride == src_stride && dst_scanlines == src_scanlines) {
        size_t total_size = (size_t)dst_stride * dst_scanlines + (size_t)dst_stride * (dst_h / 2);
        memcpy(dst, src, total_size);
        return;
    }

    if (dst_w == src_w && dst_h == src_h) {
        for (int y = 0; y < dst_h; ++y) {
            memcpy(dst_y + y * dst_stride, src_y + y * src_stride, dst_w);
        }
        int uv_h = dst_h / 2;
        for (int y = 0; y < uv_h; ++y) {
            memcpy(dst_uv + y * dst_stride, src_uv + y * src_stride, dst_w);
        }
        return;
    }

    // Resampling Y plane
    for (int dy = 0; dy < dst_h; ++dy) {
        int sy = (dy * src_h) / dst_h;
        const uint8_t *s_row = src_y + sy * src_stride;
        uint8_t *d_row = dst_y + dy * dst_stride;
        for (int dx = 0; dx < dst_w; ++dx) {
            int sx = (dx * src_w) / dst_w;
            d_row[dx] = s_row[sx];
        }
    }

    // Resampling UV plane (NV21: 2 interleaved bytes per UV sample)
    int dst_uv_h = dst_h / 2;
    int src_uv_h = src_h / 2;
    int dst_uv_w = dst_w / 2;
    int src_uv_w = src_w / 2;

    for (int dy = 0; dy < dst_uv_h; ++dy) {
        int sy = (dy * src_uv_h) / dst_uv_h;
        const uint8_t *s_uv_row = src_uv + sy * src_stride;
        uint8_t *d_uv_row = dst_uv + dy * dst_stride;
        for (int dx = 0; dx < dst_uv_w; ++dx) {
            int sx = (dx * src_uv_w) / dst_uv_w;
            d_uv_row[dx * 2]     = s_uv_row[sx * 2];
            d_uv_row[dx * 2 + 1] = s_uv_row[sx * 2 + 1];
        }
    }
}

static void shim_complete_preview_request(shim_camera3_device *sdev, ShimRequest &req) {
    int64_t ts = get_next_monotonic_timestamp();
    shim_notify_shutter(sdev, req.frame_number, ts);

    bool has_data = false;
    for (size_t i = 0; i < req.output_buffers.size(); ++i) {
        camera3_stream_t *stream = req.output_buffers[i].stream;
        if (stream && stream != sdev->jpeg_stream) {
            buffer_handle_t *dst_hdl = req.output_buffers[i].buffer;
            if (dst_hdl && *dst_hdl) {
                const msm_priv_handle_t *dst_hnd = (const msm_priv_handle_t *)(*dst_hdl);
                int dst_stride = (dst_hnd && dst_hnd->width > 0) ? dst_hnd->width : (int)stream->width;
                int dst_scanlines = (dst_hnd && dst_hnd->height > 0) ? dst_hnd->height : (int)stream->height;
                void *dst_addr = nullptr;
                if (GraphicBufferMapper::get().lock(*dst_hdl, GRALLOC_USAGE_SW_WRITE_OFTEN,
                        Rect(stream->width, stream->height), &dst_addr) == 0 && dst_addr) {
                    Mutex::Autolock l(sdev->request_lock);
                    if (sdev->last_preview_size > 0 && !sdev->last_preview_frame.empty() &&
                        sdev->last_preview_w > 0 && sdev->last_preview_h > 0) {
                        copy_yuv420sp_frame(dst_addr, stream->width, stream->height, dst_stride, dst_scanlines,
                                            sdev->last_preview_frame.data(),
                                            sdev->last_preview_w, sdev->last_preview_h,
                                            sdev->last_preview_stride, sdev->last_preview_scanlines);
                        has_data = true;
                    }
                    GraphicBufferMapper::get().unlock(*dst_hdl);
                }
            }
        }
        req.output_buffers[i].status = has_data ? CAMERA3_BUFFER_STATUS_OK : CAMERA3_BUFFER_STATUS_ERROR;
        req.output_buffers[i].release_fence = -1;
    }

    android::CameraMetadata result_meta;
    populate_result_metadata(sdev, result_meta, ts, req.is_jpeg);

    camera3_capture_result_t result;
    memset(&result, 0, sizeof(result));
    result.frame_number = req.frame_number;
    result.result = result_meta.getAndLock();
    result.num_output_buffers = req.output_buffers.size();
    result.output_buffers = req.output_buffers.data();
    result.partial_result = 1;

    sdev->callbacks->process_capture_result(sdev->callbacks, &result);
    ALOGV("Camera3Shim: Completed cached preview request frame %u (has_data=%d)", req.frame_number, has_data);
}

static void shim_drain_pending_preview_requests(shim_camera3_device *sdev) {
    std::vector<ShimRequest> to_complete;
    {
        Mutex::Autolock l(sdev->request_lock);
        for (auto it = sdev->pending_requests.begin(); it != sdev->pending_requests.end(); ) {
            if (!it->is_jpeg) {
                to_complete.push_back(*it);
                it = sdev->pending_requests.erase(it);
            } else {
                ++it;
            }
        }
    }
    for (auto &r : to_complete) {
        shim_complete_preview_request(sdev, r);
    }
}

static int shim_pw_dequeue_buffer(struct preview_stream_ops* window,
                                  buffer_handle_t** buffer, int* stride) {
    shim_camera3_device *sdev = get_shim_dev(window);
    Mutex::Autolock l(sdev->request_lock);

    int count = sdev->pw_buf_count > 0 ? sdev->pw_buf_count : 8;
    for (int i = 0; i < count; ++i) {
        if (!sdev->pw_buf_dequeued[i] && sdev->pw_buffers[i] != nullptr) {
            sdev->pw_buf_dequeued[i] = true;
            *buffer = &sdev->pw_handles[i];
            if (stride) {
                *stride = sdev->pw_buffers[i]->getStride();
            }
            ALOGV("Camera3Shim: pw_dequeue_buffer index %d handle %p", i, sdev->pw_handles[i]);
            return 0;
        }
    }
    ALOGW("Camera3Shim: pw_dequeue_buffer: No free buffers in pool (count=%d), auto-recovering!", count);
    for (int i = 0; i < count; ++i) {
        sdev->pw_buf_dequeued[i] = false;
    }
    sdev->pw_buf_dequeued[0] = true;
    *buffer = &sdev->pw_handles[0];
    if (stride && sdev->pw_buffers[0] != nullptr) {
        *stride = sdev->pw_buffers[0]->getStride();
    }
    return 0;
}

static int shim_pw_enqueue_buffer(struct preview_stream_ops* window,
                                  buffer_handle_t* buffer) {
    shim_camera3_device *sdev = get_shim_dev(window);

    int buf_idx = -1;
    bool found_req = false;
    ShimRequest req;

    {
        Mutex::Autolock l(sdev->request_lock);

        int count = sdev->pw_buf_count > 0 ? sdev->pw_buf_count : 8;
        for (int i = 0; i < count; ++i) {
            if (sdev->pw_handles[i] == *buffer || sdev->pw_handles[i] == (buffer_handle_t)buffer) {
                buf_idx = i;
                break;
            }
        }

        if (buf_idx >= 0) {
            sdev->pw_buf_dequeued[buf_idx] = false;
        }

        if (!sdev->is_taking_picture && !sdev->is_restarting && !sdev->pending_requests.empty()) {
            for (auto it = sdev->pending_requests.begin(); it != sdev->pending_requests.end(); ) {
                if (sdev->has_shutter_frame && it->frame_number <= sdev->last_shutter_frame) {
                    ALOGW("Camera3Shim: pw_enqueue_buffer: dropping stale request frame %u <= last_shutter %u",
                          it->frame_number, sdev->last_shutter_frame);
                    it = sdev->pending_requests.erase(it);
                    continue;
                }
                if (!it->is_jpeg) {
                    req = *it;
                    sdev->pending_requests.erase(it);
                    found_req = true;
                    break;
                } else {
                    ++it;
                }
            }
        }
    }

    void *src_addr = nullptr;
    bool src_locked = false;
    int src_stride = sdev->pw_width;
    int src_scanlines = sdev->pw_height;

    if (buf_idx >= 0 && sdev->pw_buffers[buf_idx].get()) {
        const msm_priv_handle_t *src_hnd = (const msm_priv_handle_t *)sdev->pw_handles[buf_idx];
        if (src_hnd && src_hnd->width > 0) src_stride = src_hnd->width;
        if (src_hnd && src_hnd->height > 0) src_scanlines = src_hnd->height;
        size_t total_buf_size = (size_t)src_stride * src_scanlines + (size_t)src_stride * (sdev->pw_height / 2);

        if (sdev->pw_buffers[buf_idx]->lock(GRALLOC_USAGE_SW_READ_OFTEN, &src_addr) == 0 && src_addr) {
            src_locked = true;
            // Update last preview frame cache
            Mutex::Autolock l(sdev->request_lock);
            if (sdev->last_preview_frame.size() < total_buf_size) {
                sdev->last_preview_frame.resize(total_buf_size);
            }
            memcpy(sdev->last_preview_frame.data(), src_addr, total_buf_size);
            sdev->last_preview_size = total_buf_size;
            sdev->last_preview_w = sdev->pw_width;
            sdev->last_preview_h = sdev->pw_height;
            sdev->last_preview_stride = src_stride;
            sdev->last_preview_scanlines = src_scanlines;
        }
    }

    if (found_req) {
        for (size_t i = 0; i < req.output_buffers.size(); ++i) {
            camera3_stream_t *stream = req.output_buffers[i].stream;
            if (stream && stream != sdev->jpeg_stream) {
                buffer_handle_t *dst_hdl = req.output_buffers[i].buffer;
                if (dst_hdl && *dst_hdl) {
                    const msm_priv_handle_t *dst_hnd = (const msm_priv_handle_t *)(*dst_hdl);
                    int dst_stride = (dst_hnd && dst_hnd->width > 0) ? dst_hnd->width : (int)stream->width;
                    int dst_scanlines = (dst_hnd && dst_hnd->height > 0) ? dst_hnd->height : (int)stream->height;
                    void *dst_addr = nullptr;
                    if (GraphicBufferMapper::get().lock(*dst_hdl, GRALLOC_USAGE_SW_WRITE_OFTEN,
                            Rect(stream->width, stream->height), &dst_addr) == 0 && dst_addr) {
                        if (src_locked && src_addr) {
                            copy_yuv420sp_frame(dst_addr, stream->width, stream->height, dst_stride, dst_scanlines,
                                                src_addr, sdev->pw_width, sdev->pw_height, src_stride, src_scanlines);
                        }
                        GraphicBufferMapper::get().unlock(*dst_hdl);
                    }
                }
            }
        }

        int64_t timestamp = get_next_monotonic_timestamp();
        shim_notify_shutter(sdev, req.frame_number, timestamp);

        android::CameraMetadata result_meta;
        populate_result_metadata(sdev, result_meta, timestamp, req.is_jpeg);

        for (size_t j = 0; j < req.output_buffers.size(); ++j) {
            req.output_buffers[j].status = src_locked ? CAMERA3_BUFFER_STATUS_OK : CAMERA3_BUFFER_STATUS_ERROR;
            req.output_buffers[j].release_fence = -1;
        }

        camera3_capture_result_t result;
        memset(&result, 0, sizeof(result));
        result.frame_number = req.frame_number;
        result.result = result_meta.getAndLock();
        result.num_output_buffers = req.output_buffers.size();
        result.output_buffers = req.output_buffers.data();
        result.partial_result = 1;

        sdev->callbacks->process_capture_result(sdev->callbacks, &result);
        ALOGV("Camera3Shim: pw_enqueue_buffer: handle=%p, buf_idx=%d, frame=%u, ts=%" PRId64,
              buffer ? *buffer : nullptr, buf_idx, req.frame_number, timestamp);
    }

    if (src_locked) {
        sdev->pw_buffers[buf_idx]->unlock();
    }

    return 0;
}

static int shim_pw_cancel_buffer(struct preview_stream_ops* window,
                                 buffer_handle_t* buffer) {
    shim_camera3_device *sdev = get_shim_dev(window);
    Mutex::Autolock l(sdev->request_lock);
    int count = sdev->pw_buf_count > 0 ? sdev->pw_buf_count : 8;
    for (int i = 0; i < count; ++i) {
        if (sdev->pw_handles[i] == *buffer || sdev->pw_handles[i] == (buffer_handle_t)buffer) {
            sdev->pw_buf_dequeued[i] = false;
            break;
        }
    }
    return 0;
}

static int shim_pw_set_buffer_count(struct preview_stream_ops* window, int count) {
    shim_camera3_device *sdev = get_shim_dev(window);
    Mutex::Autolock l(sdev->request_lock);
    int actual = count + 4;
    if (actual > SHIM_MAX_PREVIEW_BUFS) actual = SHIM_MAX_PREVIEW_BUFS;
    sdev->pw_buf_count = actual;
    ALOGI("Camera3Shim: pw_set_buffer_count: requested %d, set pool size to %d", count, actual);
    return 0;
}

static int shim_pw_set_buffers_geometry(struct preview_stream_ops* window,
                                       int width, int height, int format) {
    shim_camera3_device *sdev = get_shim_dev(window);
    Mutex::Autolock l(sdev->request_lock);
    sdev->pw_width = width;
    sdev->pw_height = height;
    sdev->pw_format = format;
    ALOGI("Camera3Shim: pw_set_buffers_geometry: %dx%d format=0x%x", width, height, format);

    int count = sdev->pw_buf_count > 0 ? sdev->pw_buf_count : 8;
    for (int i = 0; i < count; ++i) {
        sdev->pw_buffers[i] = new GraphicBuffer(
            width, height, HAL_PIXEL_FORMAT_YCrCb_420_SP, 1,
            GRALLOC_USAGE_HW_CAMERA_WRITE | GRALLOC_USAGE_SW_READ_OFTEN | GRALLOC_USAGE_SW_WRITE_OFTEN,
            "Camera3Shim");
        sdev->pw_handles[i] = sdev->pw_buffers[i]->handle;
        sdev->pw_buf_dequeued[i] = false;
        ALOGI("Camera3Shim: Allocated pool buffer %d: %p (handle=%p)",
              i, sdev->pw_buffers[i].get(), sdev->pw_handles[i]);
    }
    return 0;
}

static int shim_pw_set_crop(struct preview_stream_ops* window,
                           int left, int top, int right, int bottom) {
    (void)window;
    (void)left;
    (void)top;
    (void)right;
    (void)bottom;
    return 0;
}

static int shim_pw_set_usage(struct preview_stream_ops* window, int usage) {
    (void)window;
    (void)usage;
    return 0;
}

static int shim_pw_set_swap_interval(struct preview_stream_ops* window, int interval) {
    (void)window;
    (void)interval;
    return 0;
}

static int shim_pw_get_min_undequeued_buffer_count(const struct preview_stream_ops *window,
                                                  int *count) {
    (void)window;
    if (count) *count = 1;
    return 0;
}

static int shim_pw_lock_buffer(struct preview_stream_ops* window,
                              buffer_handle_t* buffer) {
    (void)window;
    (void)buffer;
    return 0;
}

static void shim_data_cb_timestamp(nsecs_t timestamp, int32_t msg_type,
                                   const camera_memory_t *data, unsigned int index,
                                   void *user);

static void shim_notify_cb(int32_t msg_type, int32_t ext1, int32_t ext2, void *user) {
    ALOGV("Camera3Shim: notify_cb msg=0x%x ext1=%d ext2=%d user=%p", msg_type, ext1, ext2, user);
    shim_camera3_device *sdev = (shim_camera3_device *)user;
    if (!sdev) return;
    if (msg_type == CAMERA_MSG_ERROR) {
        ALOGE("Camera3Shim: notify_cb received CAMERA_MSG_ERROR ext1=%d ext2=%d", ext1, ext2);
        if (sdev->is_taking_picture) {
            sdev->is_taking_picture = false;
            sdev->is_restarting = true;
        }
    }
}

static void shim_data_cb(int32_t msg_type, const camera_memory_t *data, unsigned int index,
                         camera_frame_metadata_t *metadata, void *user) {
    (void)index;
    (void)metadata;
    shim_camera3_device *sdev = (shim_camera3_device *)user;
    if (!sdev || !data) return;

    ALOGV("Camera3Shim: data_cb msg=0x%x size=%zu", msg_type, data->size);

    if (msg_type == CAMERA_MSG_PREVIEW_FRAME) {
        Mutex::Autolock l(sdev->request_lock);
        if (sdev->last_preview_frame.size() < data->size) {
            sdev->last_preview_frame.resize(data->size);
        }
        memcpy(sdev->last_preview_frame.data(), data->data, data->size);
        sdev->last_preview_size = data->size;
        return;
    }

    if (msg_type == CAMERA_MSG_COMPRESSED_IMAGE) {
        ALOGI("Camera3Shim: Received JPEG compressed image of size %zu!", data->size);

        ShimRequest req;
        bool found_req = false;
        {
            Mutex::Autolock l(sdev->request_lock);
            for (auto it = sdev->pending_requests.begin(); it != sdev->pending_requests.end(); ++it) {
                if (it->is_jpeg) {
                    req = *it;
                    sdev->pending_requests.erase(it);
                    found_req = true;
                    break;
                }
            }
        }

        if (found_req) {
            for (size_t i = 0; i < req.output_buffers.size(); ++i) {
                if (req.output_buffers[i].stream == sdev->jpeg_stream) {
                    buffer_handle_t *buf = req.output_buffers[i].buffer;
                    if (buf && *buf) {
                        const msm_priv_handle_t *hnd = (const msm_priv_handle_t *)(*buf);
                        size_t blob_buf_size = 0;
                        if (hnd && hnd->size > 0 && hnd->size <= 16 * 1024 * 1024) {
                            blob_buf_size = hnd->size;
                        } else {
                            blob_buf_size = get_jpeg_buffer_size(sdev->jpeg_stream->width, sdev->jpeg_stream->height, sdev->camera_id);
                        }
                        if (hnd) {
                            ALOGI("Camera3Shim: JPEG buf handle=%p: size=%u width=%d height=%d format=0x%x",
                                  hnd, hnd->size, hnd->width, hnd->height, hnd->format);
                        }

                        int lock_w = (int)blob_buf_size;
                        int lock_h = 1;

                        void *vaddr = nullptr;
                        if (GraphicBufferMapper::get().lock(*buf, GRALLOC_USAGE_SW_WRITE_OFTEN,
                                Rect(lock_w, lock_h), &vaddr) == 0 && vaddr) {
                            memcpy(vaddr, data->data, data->size);

                            auto write_blob_header = [&](size_t buffer_end_offset, const char *source_desc) {
                                if (buffer_end_offset >= sizeof(camera3_jpeg_blob_t) &&
                                    buffer_end_offset <= blob_buf_size &&
                                    buffer_end_offset > data->size + sizeof(camera3_jpeg_blob_t)) {
                                    size_t offset = buffer_end_offset - sizeof(camera3_jpeg_blob_t);
                                    camera3_jpeg_blob_t *blob = (camera3_jpeg_blob_t *)((uint8_t *)vaddr + offset);
                                    blob->jpeg_blob_id = CAMERA3_JPEG_BLOB_ID;
                                    blob->jpeg_size = data->size;
                                    ALOGI("Camera3Shim: Wrote CAMERA3_JPEG_BLOB header (%s) at offset %zu (end=%zu, jpeg_size=%u)",
                                          source_desc, offset, buffer_end_offset, blob->jpeg_size);
                                }
                            };

                            write_blob_header(blob_buf_size, "blob_buf_size");
                            if (hnd && hnd->width > 0) {
                                write_blob_header((size_t)hnd->width, "hnd->width");
                            }

                            size_t calc = get_jpeg_buffer_size(sdev->jpeg_stream->width, sdev->jpeg_stream->height, sdev->camera_id);
                            write_blob_header(calc, "calc_raw");
                            write_blob_header((calc + 3) & ~3, "calc_align4");
                            write_blob_header((calc + 15) & ~15, "calc_align16");
                            write_blob_header((calc + 31) & ~31, "calc_align32");
                            write_blob_header((calc + 63) & ~63, "calc_align64");

                            GraphicBufferMapper::get().unlock(*buf);
                        } else {
                            ALOGE("Camera3Shim: Failed to lock JPEG output buffer!");
                        }
                    }
                } else if (req.output_buffers[i].stream != sdev->jpeg_stream) {
                    camera3_stream_t *stream = req.output_buffers[i].stream;
                    buffer_handle_t *dst_hdl = req.output_buffers[i].buffer;
                    if (dst_hdl && *dst_hdl && stream) {
                        const msm_priv_handle_t *dst_hnd = (const msm_priv_handle_t *)(*dst_hdl);
                        int dst_stride = (dst_hnd && dst_hnd->width > 0) ? dst_hnd->width : (int)stream->width;
                        int dst_scanlines = (dst_hnd && dst_hnd->height > 0) ? dst_hnd->height : (int)stream->height;
                        void *dst_addr = nullptr;
                        if (GraphicBufferMapper::get().lock(*dst_hdl, GRALLOC_USAGE_SW_WRITE_OFTEN,
                                Rect(stream->width, stream->height), &dst_addr) == 0 && dst_addr) {
                            bool copied = false;
                            Mutex::Autolock l(sdev->request_lock);
                            if (sdev->last_preview_size > 0 && !sdev->last_preview_frame.empty() &&
                                sdev->last_preview_w > 0 && sdev->last_preview_h > 0) {
                                copy_yuv420sp_frame(dst_addr, stream->width, stream->height, dst_stride, dst_scanlines,
                                                    sdev->last_preview_frame.data(),
                                                    sdev->last_preview_w, sdev->last_preview_h,
                                                    sdev->last_preview_stride, sdev->last_preview_scanlines);
                                copied = true;
                            }
                            GraphicBufferMapper::get().unlock(*dst_hdl);
                            ALOGI("Camera3Shim: Handled non-JPEG buffer in snapshot request (copied=%d)", copied);
                        }
                    }
                }
            }

            int64_t timestamp = req.shutter_timestamp > 0 ? req.shutter_timestamp : get_next_monotonic_timestamp();
            android::CameraMetadata result_meta;
            populate_result_metadata(sdev, result_meta, timestamp, req.is_jpeg);

            for (size_t j = 0; j < req.output_buffers.size(); ++j) {
                req.output_buffers[j].status = CAMERA3_BUFFER_STATUS_OK;
                req.output_buffers[j].release_fence = -1;
            }

            camera3_capture_result_t result;
            memset(&result, 0, sizeof(result));
            result.frame_number = req.frame_number;
            result.result = result_meta.getAndLock();
            result.num_output_buffers = req.output_buffers.size();
            result.output_buffers = req.output_buffers.data();
            result.partial_result = 1;

            sdev->callbacks->process_capture_result(sdev->callbacks, &result);
            ALOGI("Camera3Shim: Delivered JPEG result for frame %u with ts %" PRId64, req.frame_number, timestamp);

            // Drain preview requests that arrived during snapshot using cached preview frame
            shim_drain_pending_preview_requests(sdev);
        }

        sdev->is_restarting = true;
        sdev->is_taking_picture = false;

        // In HAL1, take_picture alters sensor mode and halts preview.
        // Cleanly reopen vendor device in a separate thread so mm-camera completely resets its
        // ISP pipeline, freeing the snapshot burst streams and restarting preview cleanly.
        if (sdev->vendor_dev) {
            sdev->preview_running = false;
            if (sdev->restart_thread.joinable()) {
                sdev->restart_thread.join();
            }
            sdev->restart_thread = std::thread([sdev]() {
                ALOGI("Camera3Shim: Post-snapshot: cleanly reopening vendor camera device...");

                // Step 1: Stop and close current vendor camera device
                {
                    Mutex::Autolock l(gShimLock);
                    if (sdev->is_closing) return;
                    if (sdev->vendor_dev) {
                        if (sdev->vendor_dev->ops->stop_preview) {
                            sdev->vendor_dev->ops->stop_preview(sdev->vendor_dev);
                        }
                        if (sdev->vendor_dev->ops->cancel_picture) {
                            sdev->vendor_dev->ops->cancel_picture(sdev->vendor_dev);
                        }
                        if (sdev->vendor_dev->ops->disable_msg_type) {
                            sdev->vendor_dev->ops->disable_msg_type(sdev->vendor_dev, CAMERA_MSG_ALL_MSGS);
                        }
                        if (sdev->vendor_dev->ops->set_preview_window) {
                            sdev->vendor_dev->ops->set_preview_window(sdev->vendor_dev, nullptr);
                        }
                        sdev->vendor_dev->common.close((hw_device_t *)sdev->vendor_dev);
                        sdev->vendor_dev = nullptr;
                    }
                }

                // Step 2: Brief pause (100ms) to allow mm-qcamera-daemon to cleanly release ISP session and ports
                usleep(100000);

                // Step 3: Reopen vendor device
                {
                    Mutex::Autolock l(gShimLock);
                    if (sdev->is_closing) return;

                    char id_str[16];
                    snprintf(id_str, sizeof(id_str), "%d", sdev->camera_id);
                    hw_device_t *vendor_hw = nullptr;
                    int rc = gVendorModule->common.methods->open((const hw_module_t *)gVendorModule,
                                                                 id_str, &vendor_hw);
                    if (rc != 0 || !vendor_hw) {
                        ALOGE("Camera3Shim: Post-snapshot: Failed to reopen vendor camera %s: %d", id_str, rc);
                        sdev->is_restarting = false;
                        return;
                    }
                    camera_device_t *vendor_cam = (camera_device_t *)vendor_hw;
                    sdev->vendor_dev = vendor_cam;

                    vendor_cam->ops->set_callbacks(vendor_cam,
                                                   shim_notify_cb,
                                                   shim_data_cb,
                                                   shim_data_cb_timestamp,
                                                   shim_get_memory,
                                                   sdev);
                    vendor_cam->ops->enable_msg_type(vendor_cam,
                                                     CAMERA_MSG_ERROR |
                                                     CAMERA_MSG_SHUTTER |
                                                     CAMERA_MSG_COMPRESSED_IMAGE);

                    {
                        Mutex::Autolock rl(sdev->request_lock);
                        for (int b = 0; b < SHIM_MAX_PREVIEW_BUFS; ++b) {
                            sdev->pw_buf_dequeued[b] = false;
                        }
                    }

                    vendor_cam->ops->set_preview_window(vendor_cam, &sdev->preview_window);

                    if (sdev->preview_stream && vendor_cam->ops->get_parameters && vendor_cam->ops->set_parameters) {
                        char *raw_params = vendor_cam->ops->get_parameters(vendor_cam);
                        if (raw_params) {
                            CameraParameters params;
                            params.unflatten(String8(raw_params));
                            vendor_cam->ops->put_parameters(vendor_cam, raw_params);

                            params.setPreviewSize(sdev->preview_stream->width, sdev->preview_stream->height);
                            params.setPreviewFormat("yuv420sp");
                            params.set("zsl", "off");
                            params.set("zsl-mode", "off");
                            params.set("zsl-enable", "0");
                            params.set("mot-zsl-mode", "0");
                            if (sdev->camera_id == 0) {
                                params.set(CameraParameters::KEY_FLASH_MODE, sdev->current_flash_mode.c_str());
                            }
                            vendor_cam->ops->set_parameters(vendor_cam, params.flatten().c_str());
                        }
                    }

                    ALOGI("Camera3Shim: Post-snapshot: Starting vendor preview on reopened device...");
                    int rc_sp = vendor_cam->ops->start_preview(vendor_cam);
                    ALOGI("Camera3Shim: Post-snapshot start_preview returned %d", rc_sp);
                    sdev->preview_running = (rc_sp == 0);
                    sdev->is_restarting = false;
                }
            });
        }
    }
}

static void shim_data_cb_timestamp(nsecs_t timestamp, int32_t msg_type,
                                   const camera_memory_t *data, unsigned int index,
                                   void *user) {
    (void)timestamp;
    (void)msg_type;
    (void)data;
    (void)index;
    (void)user;
}

static int shim_initialize(const struct camera3_device *dev,
                           const camera3_callback_ops_t *callback_ops) {
    ALOGI("Camera3Shim: %s(dev=%p, callback_ops=%p)", __FUNCTION__, dev, callback_ops);
    shim_camera3_device *sdev = (shim_camera3_device *)dev;
    sdev->callbacks = callback_ops;
    return 0;
}

static int shim_configure_streams(const struct camera3_device *dev,
                                  camera3_stream_configuration_t *stream_list) {
    ALOGI("Camera3Shim: %s(num_streams=%u, op_mode=%u)",
          __FUNCTION__, stream_list->num_streams, stream_list->operation_mode);
    shim_camera3_device *sdev = (shim_camera3_device *)dev;

    sdev->jpeg_stream = nullptr;
    sdev->preview_stream = nullptr;

    for (size_t i = 0; i < stream_list->num_streams; ++i) {
        camera3_stream_t *s = stream_list->streams[i];
        ALOGI("Camera3Shim: Stream %zu: %ux%u format=0x%x type=%d usage=0x%x",
              i, s->width, s->height, s->format, s->stream_type, s->usage);
        if (s->format == HAL_PIXEL_FORMAT_BLOB) {
            sdev->jpeg_stream = s;
            // Keep JPEG buffering minimal: large 13MP BLOBs + slow ion
            // otherwise ImageReader queue setup fails (ENODEV on
            // setMaxDequeuedBufferCount and session configuration abort).
            s->max_buffers = 2;
        } else {
            if (!sdev->preview_stream || (s->width * s->height > sdev->preview_stream->width * sdev->preview_stream->height)) {
                sdev->preview_stream = s;
            }
            s->max_buffers = 8;
        }
        if (s->format == HAL_PIXEL_FORMAT_BLOB) {
            s->usage |= GRALLOC_USAGE_SW_READ_OFTEN | GRALLOC_USAGE_SW_WRITE_OFTEN;
        } else {
            s->usage |= GRALLOC_USAGE_HW_CAMERA_WRITE | GRALLOC_USAGE_SW_READ_OFTEN | GRALLOC_USAGE_SW_WRITE_OFTEN;
        }
    }

    if (sdev->vendor_dev && sdev->preview_stream) {
        if (sdev->preview_running) {
            ALOGI("Camera3Shim: Stopping preview before reconfiguring streams...");
            sdev->vendor_dev->ops->stop_preview(sdev->vendor_dev);
            sdev->preview_running = false;
        }

        char *raw_params = sdev->vendor_dev->ops->get_parameters(sdev->vendor_dev);
        if (raw_params) {
            CameraParameters params;
            params.unflatten(String8(raw_params));
            sdev->vendor_dev->ops->put_parameters(sdev->vendor_dev, raw_params);

            const char *supp_prev = params.get(CameraParameters::KEY_SUPPORTED_PREVIEW_SIZES);
            const char *supp_video = params.get(CameraParameters::KEY_SUPPORTED_VIDEO_SIZES);
            ALOGI("Camera3Shim: supported preview sizes: %s", supp_prev ? supp_prev : "(null)");
            ALOGI("Camera3Shim: supported video sizes: %s", supp_video ? supp_video : "(null)");

            char req_sz[32];
            snprintf(req_sz, sizeof(req_sz), "%ux%u", sdev->preview_stream->width, sdev->preview_stream->height);
            if (supp_prev && strstr(supp_prev, req_sz) != nullptr) {
                params.setPreviewSize(sdev->preview_stream->width, sdev->preview_stream->height);
            } else {
                float aspect = (float)sdev->preview_stream->width / (float)sdev->preview_stream->height;
                if (aspect > 1.5f && supp_prev && strstr(supp_prev, "1280x720") != nullptr) {
                    params.setPreviewSize(1280, 720);
                } else if (supp_prev && strstr(supp_prev, "1280x960") != nullptr) {
                    params.setPreviewSize(1280, 960);
                } else {
                    params.setPreviewSize(640, 480);
                }
                int cur_w = 0, cur_h = 0;
                params.getPreviewSize(&cur_w, &cur_h);
                ALOGW("Camera3Shim: Preview size %s not supported by vendor, falling back to %dx%d",
                      req_sz, cur_w, cur_h);
            }
            params.setPreviewFormat("yuv420sp");
            params.set("zsl", "off");
            params.set("zsl-mode", "off");
            params.set("zsl-enable", "0");
            params.set("mot-zsl-mode", "0");
            if (sdev->camera_id == 0) {
                params.set(CameraParameters::KEY_FLASH_MODE, sdev->current_flash_mode.c_str());
            }
            int rc = sdev->vendor_dev->ops->set_parameters(sdev->vendor_dev, params.flatten().c_str());
            ALOGI("Camera3Shim: configure_streams set_parameters rc=%d", rc);
        }

        ALOGI("Camera3Shim: Starting vendor preview...");
        int rc_sp = sdev->vendor_dev->ops->start_preview(sdev->vendor_dev);
        ALOGI("Camera3Shim: Vendor start_preview returned %d", rc_sp);
        sdev->preview_running = (rc_sp == 0);
    }

    return 0;
}

static const camera_metadata_t* shim_construct_default_request_settings(
        const struct camera3_device *dev, int type) {
    shim_camera3_device *sdev = (shim_camera3_device *)dev;
    int id = sdev->camera_id;

    ALOGI("Camera3Shim: %s(type=%d, cam=%d)", __FUNCTION__, type, id);

    if (type < 1 || type >= CAMERA3_TEMPLATE_COUNT) {
        ALOGE("Camera3Shim: Invalid template type %d", type);
        return nullptr;
    }

    return gTemplates[id][type].getAndLock();
}

static int shim_process_capture_request(const struct camera3_device *dev,
                                        camera3_capture_request_t *request) {
    shim_camera3_device *sdev = (shim_camera3_device *)dev;
    ALOGV("Camera3Shim: %s(frame=%u, num_output_buffers=%u)",
          __FUNCTION__, request->frame_number, request->num_output_buffers);

    if (!sdev->callbacks) {
        ALOGE("Camera3Shim: Callbacks not initialized!");
        return -EINVAL;
    }

    if (request->settings) {
        camera_metadata_ro_entry entry;
        bool flash_changed = false;

        if (find_camera_metadata_ro_entry(request->settings, ANDROID_CONTROL_AE_MODE, &entry) == 0 && entry.count > 0) {
            if (sdev->last_ae_mode != entry.data.u8[0]) {
                sdev->last_ae_mode = entry.data.u8[0];
                flash_changed = true;
            }
        }

        if (find_camera_metadata_ro_entry(request->settings, ANDROID_FLASH_MODE, &entry) == 0 && entry.count > 0) {
            if (sdev->last_flash_mode != entry.data.u8[0]) {
                sdev->last_flash_mode = entry.data.u8[0];
                flash_changed = true;
            }
        }

        if (flash_changed && sdev->camera_id == 0) {
            std::string new_flash_mode = CameraParameters::FLASH_MODE_OFF;
            if (sdev->last_ae_mode == ANDROID_CONTROL_AE_MODE_ON_ALWAYS_FLASH) {
                new_flash_mode = CameraParameters::FLASH_MODE_ON;
            } else if (sdev->last_ae_mode == ANDROID_CONTROL_AE_MODE_ON_AUTO_FLASH) {
                new_flash_mode = CameraParameters::FLASH_MODE_AUTO;
            } else if (sdev->last_flash_mode == ANDROID_FLASH_MODE_TORCH) {
                new_flash_mode = CameraParameters::FLASH_MODE_TORCH;
            } else if (sdev->last_flash_mode == ANDROID_FLASH_MODE_SINGLE) {
                new_flash_mode = CameraParameters::FLASH_MODE_ON;
            } else {
                new_flash_mode = CameraParameters::FLASH_MODE_OFF;
            }

            if (new_flash_mode != sdev->current_flash_mode) {
                ALOGI("Camera3Shim: Flash mode changed: %s -> %s (ae_mode=%u, flash_mode=%u)",
                      sdev->current_flash_mode.c_str(), new_flash_mode.c_str(),
                      sdev->last_ae_mode, sdev->last_flash_mode);
                sdev->current_flash_mode = new_flash_mode;

                if (sdev->preview_running && sdev->vendor_dev && !sdev->is_taking_picture) {
                    char *raw_params = sdev->vendor_dev->ops->get_parameters(sdev->vendor_dev);
                    if (raw_params) {
                        CameraParameters params;
                        params.unflatten(String8(raw_params));
                        sdev->vendor_dev->ops->put_parameters(sdev->vendor_dev, raw_params);
                        params.set(CameraParameters::KEY_FLASH_MODE, sdev->current_flash_mode.c_str());
                        sdev->vendor_dev->ops->set_parameters(sdev->vendor_dev, params.flatten().c_str());
                    }
                }
            }
        }
    }

    ShimRequest req;
    req.frame_number = request->frame_number;
    req.is_jpeg = false;
    req.shutter_timestamp = 0;
    for (size_t i = 0; i < request->num_output_buffers; ++i) {
        camera3_stream_buffer_t buf = request->output_buffers[i];
        if (buf.acquire_fence >= 0) {
            struct pollfd pfd;
            pfd.fd = buf.acquire_fence;
            pfd.events = POLLIN;
            poll(&pfd, 1, 100);

            // In HIDL passthrough mode, HidlCamera3Device creates an acquire fence native_handle_t
            // and does not close it (assuming the HAL owns it). CameraDeviceSession then dups it
            // into buf.acquire_fence, leaking the original handle FD. Detect and close the original.
            for (int offset = 1; offset <= 2; ++offset) {
                int orig_fd = buf.acquire_fence - offset;
                if (orig_fd >= 0) {
                    char link[64], target[64] = {0};
                    snprintf(link, sizeof(link), "/proc/self/fd/%d", orig_fd);
                    ssize_t len = readlink(link, target, sizeof(target) - 1);
                    if (len > 0 && strstr(target, "sync_fence") != nullptr) {
                        close(orig_fd);
                    }
                }
            }
            close(buf.acquire_fence);
            buf.acquire_fence = -1;
        }
        req.output_buffers.push_back(buf);
        if (buf.stream == sdev->jpeg_stream) {
            req.is_jpeg = true;
        }
    }
    if (request->input_buffer && request->input_buffer->acquire_fence >= 0) {
        close(request->input_buffer->acquire_fence);
        request->input_buffer->acquire_fence = -1;
    }
    // shutter_timestamp for JPEG will be assigned monotonically after draining older preview requests

    if (!req.is_jpeg && sdev->is_taking_picture) {
        // While snapshot is exposing, queue preview requests so they complete AFTER the snapshot frame result
        Mutex::Autolock l(sdev->request_lock);
        sdev->pending_requests.push_back(req);
        return 0;
    }

    if (!req.is_jpeg && sdev->is_restarting) {
        // While vendor camera is reopening after snapshot, satisfy preview requests
        // in ascending order using cached frame data.
        shim_drain_pending_preview_requests(sdev);
        shim_complete_preview_request(sdev, req);
        return 0;
    }

    if (req.is_jpeg && sdev->vendor_dev) {
        sdev->is_taking_picture = true;
        sdev->preview_running = false;
        ALOGI("Camera3Shim: Request contains JPEG stream, triggering take_picture for frame %u", req.frame_number);

        // First drain all earlier preview requests (< req.frame_number)
        shim_drain_pending_preview_requests(sdev);

        // Push JPEG request into pending_requests
        {
            Mutex::Autolock l(sdev->request_lock);
            sdev->pending_requests.push_back(req);
        }

        // Notify SHUTTER immediately so Camera framework/app unblocks exposureLatch
        req.shutter_timestamp = get_next_monotonic_timestamp();
        {
            Mutex::Autolock l(sdev->request_lock);
            for (auto &r : sdev->pending_requests) {
                if (r.frame_number == req.frame_number) {
                    r.shutter_timestamp = req.shutter_timestamp;
                    break;
                }
            }
        }
        shim_notify_shutter(sdev, req.frame_number, req.shutter_timestamp);
        ALOGI("Camera3Shim: Notified SHUTTER for snapshot frame %u with ts %" PRId64,
              req.frame_number, req.shutter_timestamp);

        if (sdev->jpeg_stream) {
            char *raw_params = sdev->vendor_dev->ops->get_parameters(sdev->vendor_dev);
            if (raw_params) {
                CameraParameters params;
                params.unflatten(String8(raw_params));
                sdev->vendor_dev->ops->put_parameters(sdev->vendor_dev, raw_params);
                params.setPictureSize(sdev->jpeg_stream->width, sdev->jpeg_stream->height);
                params.setPictureFormat("jpeg");
                params.set("zsl", "off");
                params.set("zsl-mode", "off");
                params.set("zsl-enable", "0");
                params.set("mot-zsl-mode", "0");
                if (sdev->camera_id == 0) {
                    params.set(CameraParameters::KEY_FLASH_MODE, sdev->current_flash_mode.c_str());
                }
                int rc = sdev->vendor_dev->ops->set_parameters(sdev->vendor_dev, params.flatten().c_str());
                ALOGI("Camera3Shim: setPictureSize to %ux%u, flash_mode=%s rc=%d",
                      sdev->jpeg_stream->width, sdev->jpeg_stream->height, sdev->current_flash_mode.c_str(), rc);
            }
        }
        sdev->vendor_dev->ops->enable_msg_type(sdev->vendor_dev, CAMERA_MSG_COMPRESSED_IMAGE);
        int rc = sdev->vendor_dev->ops->take_picture(sdev->vendor_dev);
        ALOGI("Camera3Shim: take_picture returned %d", rc);
        return 0;
    }

    {
        Mutex::Autolock l(sdev->request_lock);
        sdev->pending_requests.push_back(req);
    }

    return 0;
}

static void shim_dump(const struct camera3_device *dev, int fd) {
    (void)dev;
    dprintf(fd, "Camera3Shim: Active device\n");
}

static int shim_flush(const struct camera3_device *dev) {
    ALOGI("Camera3Shim: %s()", __FUNCTION__);
    shim_camera3_device *sdev = (shim_camera3_device *)dev;
    if (!sdev || !sdev->callbacks) return 0;

    std::vector<ShimRequest> pending;
    {
        Mutex::Autolock l(sdev->request_lock);
        pending = std::move(sdev->pending_requests);
        sdev->pending_requests.clear();
    }

    ALOGI("Camera3Shim: %s: Flushing %zu pending requests", __FUNCTION__, pending.size());

    for (auto &req : pending) {
        camera3_notify_msg_t notify_msg;
        memset(&notify_msg, 0, sizeof(notify_msg));
        notify_msg.type = CAMERA3_MSG_ERROR;
        notify_msg.message.error.frame_number = req.frame_number;
        notify_msg.message.error.error_code = CAMERA3_MSG_ERROR_REQUEST;
        notify_msg.message.error.error_stream = nullptr;
        sdev->callbacks->notify(sdev->callbacks, &notify_msg);

        for (size_t j = 0; j < req.output_buffers.size(); ++j) {
            req.output_buffers[j].status = CAMERA3_BUFFER_STATUS_ERROR;
            req.output_buffers[j].release_fence = -1;
        }
        camera3_capture_result_t result;
        memset(&result, 0, sizeof(result));
        result.frame_number = req.frame_number;
        result.result = nullptr;
        result.num_output_buffers = req.output_buffers.size();
        result.output_buffers = req.output_buffers.data();
        result.partial_result = 1;
        sdev->callbacks->process_capture_result(sdev->callbacks, &result);
    }
    return 0;
}

static camera3_device_ops_t sCamera3Ops = {
    .initialize = shim_initialize,
    .configure_streams = shim_configure_streams,
    .register_stream_buffers = nullptr,
    .construct_default_request_settings = shim_construct_default_request_settings,
    .process_capture_request = shim_process_capture_request,
    .get_metadata_vendor_tag_ops = nullptr,
    .dump = shim_dump,
    .flush = shim_flush,
    .reserved = {0},
};

static int shim_camera_device_close(struct hw_device_t *device) {
    ALOGI("Camera3Shim: Closing camera device %p", device);
    shim_camera3_device *sdev = (shim_camera3_device *)device;
    if (sdev) {
        sdev->is_closing = true;
        if (sdev->restart_thread.joinable()) {
            sdev->restart_thread.join();
        }
        shim_flush((const struct camera3_device *)sdev);
        Mutex::Autolock l(gShimLock);
        if (sdev->vendor_dev) {
            if (sdev->preview_running) {
                sdev->vendor_dev->ops->stop_preview(sdev->vendor_dev);
                sdev->preview_running = false;
            }
            if (sdev->vendor_dev->ops->cancel_picture) {
                sdev->vendor_dev->ops->cancel_picture(sdev->vendor_dev);
            }
            if (sdev->vendor_dev->ops->disable_msg_type) {
                sdev->vendor_dev->ops->disable_msg_type(sdev->vendor_dev, CAMERA_MSG_ALL_MSGS);
            }
            if (sdev->vendor_dev->ops->set_preview_window) {
                sdev->vendor_dev->ops->set_preview_window(sdev->vendor_dev, nullptr);
            }
            sdev->vendor_dev->common.close((hw_device_t *)sdev->vendor_dev);
            sdev->vendor_dev = nullptr;
            usleep(100000); // 100ms pause to allow mm-qcamera-daemon to cleanly release sensor/VFE channels
        }
        for (int i = 0; i < SHIM_MAX_PREVIEW_BUFS; ++i) {
            sdev->pw_buffers[i] = nullptr;
        }
        delete sdev;
    }
    return 0;
}

/*******************************************************************
 * camera_module_t implementation
 *******************************************************************/
static int shim_camera_device_open(const hw_module_t *module, const char *name,
                                   hw_device_t **device) {
    ALOGI("Camera3Shim: %s(name=%s)", __FUNCTION__, name);

    if (!name || !device) {
        return -EINVAL;
    }

    int id = atoi(name);
    if (id < 0 || id >= MAX_CAMERAS) {
        ALOGE("Camera3Shim: Invalid camera ID: %s", name);
        return -EINVAL;
    }

    Mutex::Autolock l(gShimLock);

    init_default_templates(id);

    shim_camera3_device *dev = new shim_camera3_device();
    dev->callbacks = nullptr;
    dev->vendor_dev = nullptr;
    dev->preview_stream = nullptr;
    dev->jpeg_stream = nullptr;
    dev->preview_running = false;
    dev->pw_buf_count = 0;
    dev->pw_width = 0;
    dev->pw_height = 0;
    dev->pw_format = 0;
    dev->last_preview_size = 0;
    dev->last_preview_w = 0;
    dev->last_preview_h = 0;
    dev->last_preview_stride = 0;
    dev->last_preview_scanlines = 0;
    for (int i = 0; i < SHIM_MAX_PREVIEW_BUFS; ++i) {
        dev->pw_buffers[i] = nullptr;
        dev->pw_handles[i] = nullptr;
        dev->pw_buf_dequeued[i] = false;
    }

    dev->base.common.tag = HARDWARE_DEVICE_TAG;
    dev->base.common.version = CAMERA_DEVICE_API_VERSION_3_2;
    dev->base.common.module = (hw_module_t *)module;
    dev->base.common.close = shim_camera_device_close;
    dev->base.ops = &sCamera3Ops;
    dev->camera_id = id;

    dev->preview_window.dequeue_buffer = shim_pw_dequeue_buffer;
    dev->preview_window.enqueue_buffer = shim_pw_enqueue_buffer;
    dev->preview_window.cancel_buffer = shim_pw_cancel_buffer;
    dev->preview_window.set_buffer_count = shim_pw_set_buffer_count;
    dev->preview_window.set_buffers_geometry = shim_pw_set_buffers_geometry;
    dev->preview_window.set_crop = shim_pw_set_crop;
    dev->preview_window.set_usage = shim_pw_set_usage;
    dev->preview_window.set_swap_interval = shim_pw_set_swap_interval;
    dev->preview_window.get_min_undequeued_buffer_count = shim_pw_get_min_undequeued_buffer_count;
    dev->preview_window.lock_buffer = shim_pw_lock_buffer;

    if (check_vendor_module() == 0 && gVendorModule) {
        camera_device_t *vendor_cam = nullptr;
        int rv = -1;
        for (int retry = 0; retry < 15; ++retry) {
            rv = gVendorModule->common.methods->open(
                    (const hw_module_t *)gVendorModule, name,
                    (hw_device_t **)&vendor_cam);
            if (rv == 0 && vendor_cam) {
                break;
            }
            ALOGW("Camera3Shim: Vendor open %s failed with %d, retrying (%d/15) in 50ms...", name, rv, retry + 1);
            usleep(50000);
        }
        if (rv == 0 && vendor_cam) {
            dev->vendor_dev = vendor_cam;
            ALOGI("Camera3Shim: Opened vendor HAL1 camera %s (%p)", name, vendor_cam);
            if (vendor_cam->ops->set_callbacks) {
                vendor_cam->ops->set_callbacks(vendor_cam,
                                               shim_notify_cb,
                                               shim_data_cb,
                                               shim_data_cb_timestamp,
                                               shim_get_memory,
                                               dev);
            }
            if (vendor_cam->ops->set_preview_window) {
                vendor_cam->ops->set_preview_window(vendor_cam, &dev->preview_window);
            }
            if (vendor_cam->ops->get_parameters) {
                char *p = vendor_cam->ops->get_parameters(vendor_cam);
                if (p) {
                    ALOGI("Camera3Shim: vendor camera %s raw params: %s", name, p);
                    vendor_cam->ops->put_parameters(vendor_cam, p);
                }
            }
        } else {
            ALOGE("Camera3Shim: Failed to open vendor camera %s: %d", name, rv);
            delete dev;
            return (rv != 0) ? rv : -EIO;
        }
    }

    *device = &dev->base.common;
    ALOGI("Camera3Shim: Opened camera %d successfully as HAL 3.2", id);
    return 0;
}

static int shim_get_number_of_cameras(void) {
    if (check_vendor_module() == 0 && gVendorModule->get_number_of_cameras) {
        int n = gVendorModule->get_number_of_cameras();
        ALOGI("Camera3Shim: Vendor reports %d cameras", n);
        return n;
    }
    return 2;
}

static int shim_get_camera_info(int camera_id, struct camera_info *info) {
    ALOGI("Camera3Shim: %s(id=%d)", __FUNCTION__, camera_id);

    if (camera_id < 0 || camera_id >= MAX_CAMERAS || !info) {
        return -EINVAL;
    }

    int facing = (camera_id == 0) ? CAMERA_FACING_BACK : CAMERA_FACING_FRONT;
    int orientation = (camera_id == 0) ? 90 : 270;

    if (check_vendor_module() == 0 && gVendorModule->get_camera_info) {
        struct camera_info vinfo;
        if (gVendorModule->get_camera_info(camera_id, &vinfo) == 0) {
            facing = vinfo.facing;
            orientation = vinfo.orientation;
        }
    }

    info->facing = facing;
    info->orientation = orientation;
    info->device_version = CAMERA_DEVICE_API_VERSION_3_2;
    info->resource_cost = 50;
    info->conflicting_devices = nullptr;
    info->conflicting_devices_length = 0;
    info->static_camera_characteristics = build_static_characteristics(camera_id, facing, orientation);

    return 0;
}

static const camera_module_callbacks_t *gModuleCallbacks = nullptr;

static int shim_set_callbacks(const camera_module_callbacks_t *callbacks) {
    ALOGI("Camera3Shim: set_callbacks(%p)", callbacks);
    gModuleCallbacks = callbacks;
    return 0;
}

static int shim_set_torch_mode(const char *camera_id, bool enabled) {
    ALOGI("Camera3Shim: set_torch_mode(cam=%s, enabled=%d)", camera_id, enabled);
    if (camera_id == nullptr)
        return -EINVAL;
    // Back camera only on osprey
    if (atoi(camera_id) != 0) {
        ALOGE("Camera3Shim: no flashlight on camera %s", camera_id);
        return -ENOSYS;
    }
    if (check_vendor_module() == 0 && gVendorModule->set_torch_mode) {
        int rc = gVendorModule->set_torch_mode(camera_id, enabled);
        if (rc == 0)
            return 0;
        ALOGW("Camera3Shim: vendor set_torch_mode failed (%d), sysfs fallback", rc);
    }
    // Sysfs fallback: sky81296 torch LED
    static const char *kTorchPaths[] = {
        "/sys/class/leds/torch-light0/brightness",
        "/sys/devices/soc.0/1b0c000.qcom,cci/qcom,sky81296.132/leds/torch-light0/brightness",
        nullptr
    };
    int fd = -1;
    for (int i = 0; kTorchPaths[i] != nullptr; i++) {
        fd = open(kTorchPaths[i], O_WRONLY | O_CLOEXEC);
        if (fd >= 0) break;
    }
    if (fd < 0) {
        ALOGE("Camera3Shim: failed to open torch-light0 brightness (errno=%d: %s)", errno, strerror(errno));
        return -EIO; // NEVER return -ENOSYS as CameraService throws unhandled IllegalArgumentException
    }
    const char *val = enabled ? "255" : "0";
    ssize_t len = write(fd, val, strlen(val));
    close(fd);
    if (len < 0) {
        ALOGE("Camera3Shim: failed to write torch brightness (errno=%d: %s)", errno, strerror(errno));
        return -EIO;
    }
    if (gModuleCallbacks && gModuleCallbacks->torch_mode_status_change)
        gModuleCallbacks->torch_mode_status_change(gModuleCallbacks, camera_id,
                enabled ? TORCH_MODE_STATUS_AVAILABLE_ON : TORCH_MODE_STATUS_AVAILABLE_OFF);
    return 0;
}

static struct hw_module_methods_t shim_module_methods = {
    .open = shim_camera_device_open
};

camera_module_t HAL_MODULE_INFO_SYM = {
    .common = {
        .tag = HARDWARE_MODULE_TAG,
        .module_api_version = CAMERA_MODULE_API_VERSION_2_4,
        .hal_api_version = HARDWARE_HAL_API_VERSION,
        .id = CAMERA_HARDWARE_MODULE_ID,
        .name = "msm8916 Camera3 Shim",
        .author = "Antigravity & Osprey Team",
        .methods = &shim_module_methods,
        .dso = nullptr,
        .reserved = {0},
    },
    .get_number_of_cameras = shim_get_number_of_cameras,
    .get_camera_info = shim_get_camera_info,
    .set_callbacks = shim_set_callbacks,
    .get_vendor_tag_ops = nullptr,
    .open_legacy = nullptr,
    .set_torch_mode = shim_set_torch_mode,
    .init = nullptr,
    .reserved = {0},
};
