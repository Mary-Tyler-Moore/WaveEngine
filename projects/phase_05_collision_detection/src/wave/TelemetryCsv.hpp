#pragma once

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

namespace wave {

struct TelemetrySample {
    int64_t t_ms = 0;
    int frame = 0;
    float dt_ms = 0.0f;
    float fps_smooth = 0.0f;
    float boat_x = 0.0f;
    float boat_y = 0.0f;
    float cam_x = 0.0f;
    float cam_y = 0.0f;
    int culling_enabled = 1;
    int radius_tiles = 0;
    int chunks_visible = 0;
    int chunks_loaded_total = 0;
    int chunks_load_started = 0;
    int chunks_load_finished = 0;
    int chunks_unload_started = 0;
    int chunks_unload_finished = 0;
    int load_queue_len = 0;
};

class TelemetryCsv {
  public:
    explicit TelemetryCsv(const std::filesystem::path& path);

    void Sample(const TelemetrySample& row);
    void Event(const TelemetrySample& row, std::string_view note);

  private:
    void WriteRow(const TelemetrySample& row, std::string_view note);
    void MaybeFlush();

    std::ofstream file_;
    std::chrono::steady_clock::time_point lastFlush_;
};

}  // namespace wave
