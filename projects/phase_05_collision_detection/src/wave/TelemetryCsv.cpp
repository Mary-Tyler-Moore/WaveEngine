#include "wave/TelemetryCsv.hpp"

#include <filesystem>
#include <sstream>

namespace wave {

namespace {

constexpr const char* kHeader =
    "t_ms,frame,dt_ms,fps_smooth,boat_x,boat_y,cam_x,cam_y,"
    "culling_enabled,radius_tiles,chunks_visible,chunks_loaded_total,"
    "chunks_load_started,chunks_load_finished,chunks_unload_started,chunks_unload_finished,"
    "load_queue_len,note\n";

std::string SanitizeNote(std::string_view note) {
    std::string sanitized(note);
    for (char& c : sanitized) {
        if (c == ',') {
            c = ';';
        }
    }
    return sanitized;
}

}  // namespace

TelemetryCsv::TelemetryCsv(const std::filesystem::path& path) {
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    const bool exists = std::filesystem::exists(path, ec);
    const auto size = exists ? std::filesystem::file_size(path, ec) : 0;

    file_.open(path, std::ios::out | std::ios::app);
    if (!file_.is_open()) {
        return;
    }
    if (size == 0) {
        file_ << kHeader;
    }
    lastFlush_ = std::chrono::steady_clock::now();
}

void TelemetryCsv::Sample(const TelemetrySample& row) {
    if (!file_.is_open()) {
        return;
    }
    WriteRow(row, "");
}

void TelemetryCsv::Event(const TelemetrySample& row, std::string_view note) {
    if (!file_.is_open()) {
        return;
    }
    WriteRow(row, note);
}

void TelemetryCsv::WriteRow(const TelemetrySample& row, std::string_view note) {
    const std::string sanitized = SanitizeNote(note);
    file_ << row.t_ms << ','
          << row.frame << ','
          << row.dt_ms << ','
          << row.fps_smooth << ','
          << row.boat_x << ','
          << row.boat_y << ','
          << row.cam_x << ','
          << row.cam_y << ','
          << row.culling_enabled << ','
          << row.radius_tiles << ','
          << row.chunks_visible << ','
          << row.chunks_loaded_total << ','
          << row.chunks_load_started << ','
          << row.chunks_load_finished << ','
          << row.chunks_unload_started << ','
          << row.chunks_unload_finished << ','
          << row.load_queue_len << ','
          << sanitized << '\n';
    MaybeFlush();
}

void TelemetryCsv::MaybeFlush() {
    const auto now = std::chrono::steady_clock::now();
    if (now - lastFlush_ >= std::chrono::seconds(1)) {
        file_.flush();
        lastFlush_ = now;
    }
}

}  // namespace wave
