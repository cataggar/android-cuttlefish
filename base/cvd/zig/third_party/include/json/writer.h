#pragma once

#include <iosfwd>
#include <memory>
#include <string>

#include "json/value.h"

namespace Json {

class StreamWriter {
 protected:
  std::ostream* sout_ = nullptr;

 public:
  StreamWriter() = default;
  virtual ~StreamWriter() = default;
  virtual int write(Value const& root, std::ostream* sout) = 0;

  class Factory {
   public:
    virtual ~Factory() = default;
    virtual StreamWriter* newStreamWriter() const = 0;
  };
};

class StreamWriterBuilder : public StreamWriter::Factory {
 public:
  Value settings_;

  StreamWriterBuilder();
  ~StreamWriterBuilder() override;

  StreamWriter* newStreamWriter() const override;
  bool validate(Json::Value* invalid) const;
  Value& operator[](const std::string& key);

  static void setDefaults(Json::Value* settings);
};

std::string writeString(StreamWriter::Factory const& factory,
                        Value const& root);

class Writer {
 public:
  virtual ~Writer() = default;
  virtual std::string write(const Value& root) = 0;
};

class FastWriter : public Writer {
 public:
  FastWriter() = default;
  ~FastWriter() override = default;

  void enableYAMLCompatibility() {}
  void dropNullPlaceholders() {}
  void omitEndingLineFeed() { omitEndingLineFeed_ = true; }

  std::string write(const Value& root) override;

 private:
  bool omitEndingLineFeed_ = false;
};

class StyledWriter : public Writer {
 public:
  StyledWriter() = default;
  ~StyledWriter() override = default;

  std::string write(const Value& root) override;
};

std::string valueToString(Int value);
std::string valueToString(UInt value);
std::string valueToString(LargestInt value);
std::string valueToString(LargestUInt value);
std::string valueToString(double value);
std::string valueToString(bool value);
std::string valueToQuotedString(const char* value);

}  // namespace Json
