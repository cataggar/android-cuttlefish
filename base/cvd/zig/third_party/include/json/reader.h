#pragma once

#include <iosfwd>
#include <memory>
#include <string>

#include "json/value.h"

namespace Json {

class Features {
 public:
  static Features all() { return Features(); }
  static Features strictMode() {
    Features f;
    f.allowComments_ = false;
    f.strictRoot_ = true;
    return f;
  }
  bool allowComments_ = true;
  bool strictRoot_ = false;
  bool allowDroppedNullPlaceholders_ = false;
  bool allowNumericKeys_ = false;
};

class Reader {
 public:
  Reader() = default;
  explicit Reader(const Features& features) : features_(features) {}

  bool parse(const std::string& document, Value& root,
             bool collectComments = true);
  bool parse(const char* beginDoc, const char* endDoc, Value& root,
             bool collectComments = true);
  bool parse(std::istream& is, Value& root, bool collectComments = true);

  std::string getFormatedErrorMessages() const { return errors_; }
  std::string getFormattedErrorMessages() const { return errors_; }
  bool good() const { return errors_.empty(); }

 private:
  Features features_;
  std::string errors_;
};

class CharReader {
 public:
  virtual ~CharReader() = default;
  virtual bool parse(char const* beginDoc, char const* endDoc, Value* root,
                     std::string* errs) = 0;

  class Factory {
   public:
    virtual ~Factory() = default;
    virtual CharReader* newCharReader() const = 0;
  };
};

class CharReaderBuilder : public CharReader::Factory {
 public:
  Value settings_;

  CharReaderBuilder();
  ~CharReaderBuilder() override;

  CharReader* newCharReader() const override;
  bool validate(Json::Value* invalid) const;
  Value& operator[](const std::string& key);

  static void setDefaults(Json::Value* settings);
  static void strictMode(Json::Value* settings);
};

bool parseFromStream(CharReader::Factory const& fact, std::istream& sin,
                     Value* root, std::string* errs);

}  // namespace Json
