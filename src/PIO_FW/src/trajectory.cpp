#include "trajectory.h"

static inline uint32_t stride_bytes_for(const TrajHeader& h) {
  // Per your format: axis_count==1 -> one scalar (int32), legacy 3-in-1 -> 12 bytes/frame
  return (h.axis_count == 1) ? (uint32_t)sizeof(int32_t) : (uint32_t)sizeof(FramePos);
}

bool TrajectoryReader::open(const char* path) {
  // Start clean
  close();

  if (!file.open(path, O_RDONLY)) {
    Serial.printf("[traj] open FAIL: %s\n", path);
    _ok = false;
    return false;
  }
  _path = path;

  // Parse header (leaves cursor at end of whatever fixed header we read)
  _ok = readHeader(hdr);
  if (!_ok) {
    Serial.printf("[traj] header parse FAIL: %s\n", path);
    file.close();
    return false;
  }

  // If readHeader() didn't set a concrete size, infer it now.
  // Strategy: compute expected header length = fileSize - (samples * stride).
  // If that equals 64 or 32 (or 28 for legacy), trust it and use that value.
  // Otherwise, fall back to "where we are now" (curPosition).
  uint32_t inferred = 0;
  const uint32_t stride = stride_bytes_for(hdr);
  const uint64_t fs = file.fileSize();
  const uint64_t data_len = (uint64_t)hdr.total_samples * (uint64_t)stride;
  if (fs >= data_len) {
    const uint64_t maybe = fs - data_len;
    if (maybe == 64)       inferred = 64;
    else if (maybe == 32)  inferred = 32;
    else if (maybe == 28)  inferred = 28; // legacy
  }

  const uint32_t cur = (uint32_t)file.curPosition();
  if (header_bytes_on_disk == 0) {
    header_bytes_on_disk = inferred ? inferred : cur; // prefer inference, else cursor
  } else {
    // If we already set it (from readHeader), but inference disagrees with a known valid size, prefer the known size.
    if ((header_bytes_on_disk != 28) && (header_bytes_on_disk != 32) && (header_bytes_on_disk != 64) && inferred) {
      header_bytes_on_disk = inferred;
    }
  }

  // Seek to the first sample explicitly (robust for variable header sizes).
  if (!file.seek(header_bytes_on_disk)) {
    Serial.printf("[traj] seek to data_start=%lu FAIL: %s\n",
                  static_cast<unsigned long>(header_bytes_on_disk), path);
    file.close();
    _ok = false;
    return false;
  }

  current_index = 0;

  // Summary line
  Serial.printf("[traj] %s | hdr=%luB axis=%u dt_us=%u samples=%llu scale=%u stride=%u fs=%llu\n",
                path,
                static_cast<unsigned long>(header_bytes_on_disk),
                static_cast<unsigned>(hdr.axis_count),
                static_cast<unsigned>(hdr.sample_dt_us),
                static_cast<unsigned long long>(hdr.total_samples),
                static_cast<unsigned>(hdr.angle_scale),
                (unsigned)stride,
                (unsigned long long)fs);

  return true;
}

bool TrajectoryReader::readHeader(TrajHeader& out) {
  if (!file) return false;

  // Try to read the canonical 32-byte header first
  uint8_t buf32[32];
  if (file.read(buf32, sizeof(buf32)) != (int)sizeof(buf32)) return false;

  auto parse32 = [&](const uint8_t* p)->bool {
    // The struct is packed and matches the on-disk layout (LE on Teensy).
    memcpy(&out, p, 32);
    if (out.magic != 0x4C424D47UL) return false;     // 'GMBL'
    if (out.version != 1) return false;
    if (!(out.axis_count == 1 || out.axis_count == 3)) return false;
    if (out.sample_dt_us == 0u) return false;
    if (out.total_samples == 0ull) return false;
    if (out.angle_scale == 0u) return false;
    return true;
  };

  if (parse32(buf32)) {
    header_bytes_on_disk = 32;   // data may still start later (e.g., padded 64), we'll fix in open()
    return true;
  }

  // Fall back: older 28-byte header (no flags/header_crc32 in file)
  file.seek(0);
  uint8_t buf28[28];
  if (file.read(buf28, sizeof(buf28)) != (int)sizeof(buf28)) return false;

  memset(&out, 0, sizeof(out));
  memcpy(&out, buf28, 28);
  // Validate minimal fields
  if (out.magic != 0x4C424D47UL) return false;
  if (out.version != 1) return false;
  if (!(out.axis_count == 1 || out.axis_count == 3)) return false;
  if (out.sample_dt_us == 0u) return false;
  if (out.total_samples == 0ull) return false;
  if (out.angle_scale == 0u) return false;

  header_bytes_on_disk = 28;
  return true;
}

bool TrajectoryReader::readNextScalar(int axis_index, int32_t& out_deg_x1e6) {
  if (!file || current_index >= hdr.total_samples) return false;

  if (hdr.axis_count == 1) {
    int32_t v;
    if (file.read(&v, sizeof(v)) != (int)sizeof(v)) return false;
    out_deg_x1e6 = v;
  } else {
    // legacy 3-in-1 frame: read 3 int32, pick component
    FramePos f{};
    if (file.read(&f, sizeof(f)) != (int)sizeof(f)) return false;
    if (axis_index < 0 || axis_index > 2) return false;
    out_deg_x1e6 = f.axis[axis_index];
  }

  current_index++;
  return true;
}

bool TrajectoryReader::seekSample(uint64_t idx) {
  if (!file || idx >= hdr.total_samples) return false;
  const uint32_t stride = stride_bytes_for(hdr);
  const uint64_t offset = (uint64_t)header_bytes_on_disk + idx * (uint64_t)stride;
  if (!file.seek(offset)) return false;
  current_index = idx;
  return true;
}

void TrajectoryReader::close() {
  if (file) file.close();
  _ok = false;
  current_index = 0;
  _path = nullptr;
  header_bytes_on_disk = 32;  // default expectation next time; may be overridden by readHeader/open
}
