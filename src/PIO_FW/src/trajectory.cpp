#include "trajectory.h"

bool TrajectoryReader::open(const char* path){
  close();
  file.open(path, O_RDONLY);
  if (!file) return false;
  _ok = readHeader(hdr);
  current_index = 0;
  if (_ok) file.seek(32); // after header
  _path = path;
  return _ok;
}

bool TrajectoryReader::readHeader(TrajHeader& out){
  if (!file) return false;
  if (file.read((void*)&out, sizeof(out)) != sizeof(out)) return false;
  if (out.magic != 0x4C424D47UL) return false; // 'GMBL'
  if (out.version != 1) return false;
  if (!(out.axis_count==1 || out.axis_count==3)) return false;
  return true;
}

bool TrajectoryReader::readNextScalar(int axis_index, int32_t& out_deg_x1e6){
  if (!file || current_index >= hdr.total_samples) return false;

  if (hdr.axis_count == 1){
    int32_t v;
    if (file.read(&v, sizeof(v)) != sizeof(v)) return false;
    out_deg_x1e6 = v;
  } else {
    // legacy 3-in-1 frame: read 3 int32, pick component
    FramePos f{};
    if (file.read(&f, sizeof(f)) != sizeof(f)) return false;
    if (axis_index < 0 || axis_index > 2) return false;
    out_deg_x1e6 = f.axis[axis_index];
  }

  current_index++;
  return true;
}

bool TrajectoryReader::seekSample(uint64_t idx){
  if (!file || idx >= hdr.total_samples) return false;
  uint64_t stride = (hdr.axis_count == 1) ? sizeof(int32_t) : sizeof(FramePos);
  uint64_t offset = 32ULL + idx * stride;
  if (!file.seek(offset)) return false;
  current_index = idx;
  return true;
}

void TrajectoryReader::close(){
  if (file) file.close();
  _ok = false;
  current_index = 0;
  _path = nullptr;
}
