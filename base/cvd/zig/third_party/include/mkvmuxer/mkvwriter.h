#pragma once

#include <cstdio>

#include "mkvmuxer/mkvmuxertypes.h"

namespace mkvmuxer {

class IMkvWriter {
 public:
  virtual ~IMkvWriter() = default;
  virtual int32 Write(const void* buf, uint32 len) = 0;
  virtual int64 Position() const = 0;
  virtual int32 Position(int64 position) = 0;
  virtual bool Seekable() const = 0;
  virtual void ElementStartNotify(uint64 element_id, int64 position) = 0;
};

class MkvWriter : public IMkvWriter {
 public:
  MkvWriter();
  explicit MkvWriter(FILE* fp);
  ~MkvWriter() override;

  int32 Write(const void* buf, uint32 len) override;
  int64 Position() const override;
  int32 Position(int64 position) override;
  bool Seekable() const override;
  void ElementStartNotify(uint64 element_id, int64 position) override;

  bool Open(const char* filename);
  void Close();

 private:
  FILE* file_ = nullptr;
  bool writer_owns_file_ = false;
};

}  // namespace mkvmuxer
