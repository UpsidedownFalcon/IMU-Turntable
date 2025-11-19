#include <Arduino.h>
#include <SdFat.h>

#pragma pack(push, 1)
struct TrajHeader {
  uint32_t magic;       // 'GMBL' little-endian = 0x4C424D47
  uint8_t  version;     // 1
  uint8_t  axis_count;  // 1 (per-file) or 3 (legacy)
  uint16_t reserved;
  uint32_t sample_dt_us;
  uint64_t total_samples;
  uint32_t angle_scale; // e.g. 1'000'000 (deg * 1e6)
  uint32_t flags;       // bit0: positions present; bit1: velocities present (unused)
  uint32_t header_crc32;// optional (not validated currently)
};

struct FramePos { int32_t axis[3]; }; // legacy 3-in-1 frame
#pragma pack(pop)

static_assert(sizeof(TrajHeader) == 32, "TrajHeader must be 32 bytes");
static_assert(sizeof(FramePos)   == 12, "FramePos must be 12 bytes");

class TrajectoryReader {
public:
  bool open(const char* path);
  bool readHeader(TrajHeader& h);
  // Reads next frame's scalar position (deg * angle_scale). If axis_count==3, pick axis_index 0..2.
  bool readNextScalar(int axis_index, int32_t& out_deg_x1e6);
  bool seekSample(uint64_t idx);
  void close();

  uint64_t samples()   const { return hdr.total_samples; }
  uint32_t dt_us()     const { return hdr.sample_dt_us; }
  uint32_t scale()     const { return hdr.angle_scale; }
  uint8_t  axisCount() const { return hdr.axis_count; }
  bool ok()            const { return _ok; }

private:
  FsFile file;
  TrajHeader hdr{};
  uint64_t current_index = 0;
  bool _ok = false;
  const char* _path = nullptr;
  uint8_t header_bytes_on_disk = 32; 
};
