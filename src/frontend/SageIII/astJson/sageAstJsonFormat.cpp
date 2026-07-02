#include "sageAstJsonPrivate.h"

namespace Rose {
namespace AstJson {

const char *const kFormat = "rex-sage-ast-json";
const int kSchemaVersion = 27;

class JsonParser {
public:
  explicit JsonParser(const std::string &input) : input_(input) {}

  JsonValue parse() {
    JsonValue value = parseValue();
    skipWhitespace();
    if (pos_ != input_.size()) {
      fail("trailing data after JSON value");
    }
    return value;
  }

private:
  const std::string &input_;
  size_t pos_ = 0;

  [[noreturn]] void fail(const std::string &message) const {
    std::ostringstream out;
    out << "invalid AST JSON at byte " << pos_ << ": " << message;
    throw std::runtime_error(out.str());
  }

  void skipWhitespace() {
    while (pos_ < input_.size() &&
           std::isspace(static_cast<unsigned char>(input_[pos_]))) {
      ++pos_;
    }
  }

  bool consume(char expected) {
    skipWhitespace();
    if (pos_ < input_.size() && input_[pos_] == expected) {
      ++pos_;
      return true;
    }
    return false;
  }

  void require(char expected) {
    if (!consume(expected)) {
      std::string message = "expected '";
      message += expected;
      message += "'";
      fail(message);
    }
  }

  JsonValue parseValue() {
    skipWhitespace();
    if (pos_ >= input_.size()) {
      fail("unexpected end of input");
    }

    const char c = input_[pos_];
    if (c == '"') {
      return JsonValue::string(parseString());
    }
    if (c == '{') {
      return parseObject();
    }
    if (c == '[') {
      return parseArray();
    }
    if (startsWith("true")) {
      pos_ += 4;
      return JsonValue::boolean(true);
    }
    if (startsWith("false")) {
      pos_ += 5;
      return JsonValue::boolean(false);
    }
    if (startsWith("null")) {
      pos_ += 4;
      return JsonValue::null();
    }
    if (c == '-' || std::isdigit(static_cast<unsigned char>(c))) {
      return parseNumber();
    }
    fail("unexpected JSON value");
  }

  bool startsWith(const char *keyword) const {
    const size_t length = std::char_traits<char>::length(keyword);
    return input_.compare(pos_, length, keyword) == 0;
  }

  JsonValue parseObject() {
    require('{');
    std::map<std::string, JsonValue> object;
    skipWhitespace();
    if (consume('}')) {
      return JsonValue::objectValue(std::move(object));
    }
    while (true) {
      skipWhitespace();
      if (pos_ >= input_.size() || input_[pos_] != '"') {
        fail("expected object key");
      }
      std::string key = parseString();
      require(':');
      object.emplace(std::move(key), parseValue());
      if (consume('}')) {
        break;
      }
      require(',');
    }
    return JsonValue::objectValue(std::move(object));
  }

  JsonValue parseArray() {
    require('[');
    std::vector<JsonValue> array;
    skipWhitespace();
    if (consume(']')) {
      return JsonValue::arrayValue(std::move(array));
    }
    while (true) {
      array.push_back(parseValue());
      if (consume(']')) {
        break;
      }
      require(',');
    }
    return JsonValue::arrayValue(std::move(array));
  }

  JsonValue parseNumber() {
    const size_t begin = pos_;
    if (input_[pos_] == '-') {
      ++pos_;
    }
    while (pos_ < input_.size() &&
           std::isdigit(static_cast<unsigned char>(input_[pos_]))) {
      ++pos_;
    }
    if (pos_ < input_.size() && input_[pos_] == '.') {
      ++pos_;
      while (pos_ < input_.size() &&
             std::isdigit(static_cast<unsigned char>(input_[pos_]))) {
        ++pos_;
      }
    }
    if (pos_ < input_.size() && (input_[pos_] == 'e' || input_[pos_] == 'E')) {
      ++pos_;
      if (pos_ < input_.size() &&
          (input_[pos_] == '+' || input_[pos_] == '-')) {
        ++pos_;
      }
      while (pos_ < input_.size() &&
             std::isdigit(static_cast<unsigned char>(input_[pos_]))) {
        ++pos_;
      }
    }
    if (begin == pos_) {
      fail("malformed number");
    }
    return JsonValue::number(input_.substr(begin, pos_ - begin));
  }

  std::string parseString() {
    require('"');
    std::string result;
    while (pos_ < input_.size()) {
      char c = input_[pos_++];
      if (c == '"') {
        return result;
      }
      if (c != '\\') {
        result += c;
        continue;
      }
      if (pos_ >= input_.size()) {
        fail("unterminated escape sequence");
      }
      const char escaped = input_[pos_++];
      switch (escaped) {
      case '"':
      case '\\':
      case '/':
        result += escaped;
        break;
      case 'b':
        result += '\b';
        break;
      case 'f':
        result += '\f';
        break;
      case 'n':
        result += '\n';
        break;
      case 'r':
        result += '\r';
        break;
      case 't':
        result += '\t';
        break;
      case 'u':
        if (pos_ + 4 > input_.size()) {
          fail("short unicode escape");
        }
        // The writer only emits \\u00XX for control bytes. Preserve other
        // escapes as '?' because Sage source text in this path is
        // byte-oriented.
        if (input_[pos_] == '0' && input_[pos_ + 1] == '0') {
          const std::string hex = input_.substr(pos_ + 2, 2);
          result += static_cast<char>(std::stoi(hex, nullptr, 16));
        } else {
          result += '?';
        }
        pos_ += 4;
        break;
      default:
        fail("unsupported escape sequence");
      }
    }
    fail("unterminated string");
  }
};

JsonValue parseJson(const std::string &json) {
  return JsonParser(json).parse();
}

const NodeRecord &AstFileRecord::node(uint64_t id) const {
  auto found = index_by_id.find(id);
  if (found == index_by_id.end()) {
    throw std::runtime_error("AST JSON references unknown node id " +
                             std::to_string(id));
  }
  return nodes[found->second];
}

bool startsWith(const std::string &s, const std::string &prefix) {
  return s.rfind(prefix, 0) == 0;
}

std::string sanitizePathComponent(std::string name) {
  for (char &c : name) {
    unsigned char uc = static_cast<unsigned char>(c);
    if (!std::isalnum(uc) && c != '_' && c != '-' && c != '.') {
      c = '_';
    }
  }
  if (name.empty()) {
    return "ast";
  }
  return name;
}

uint64_t fnv1a64(const std::string &value) {
  uint64_t hash = 14695981039346656037ull;
  for (unsigned char c : value) {
    hash ^= c;
    hash *= 1099511628211ull;
  }
  return hash;
}

std::string hex64(uint64_t value) {
  std::ostringstream out;
  out << std::hex << std::setw(16) << std::setfill('0') << value;
  return out.str();
}

uint64_t processId() { return static_cast<uint64_t>(::getpid()); }

std::vector<std::string> commandLine(const SgSourceFile *file) {
  if (file == nullptr) {
    return {};
  }
  return file->get_originalCommandLineArgumentList();
}

bool argumentSelectsCheckpoint(const std::string &value,
                               Checkpoint checkpoint) {
  return value == "all" || value == checkpointName(checkpoint);
}

bool hasCheckpointArgument(const SgSourceFile *file, Checkpoint checkpoint) {
  const std::string prefix = "-rex:ast-json-checkpoint=";
  for (const std::string &arg : commandLine(file)) {
    if (startsWith(arg, prefix) &&
        argumentSelectsCheckpoint(arg.substr(prefix.size()), checkpoint)) {
      return true;
    }
  }

  const char *env = std::getenv("REX_AST_JSON_CHECKPOINT");
  return env != nullptr && argumentSelectsCheckpoint(env, checkpoint);
}

std::string commandLineValue(const SgSourceFile *file,
                             const std::string &prefix) {
  for (const std::string &arg : commandLine(file)) {
    if (startsWith(arg, prefix)) {
      return arg.substr(prefix.size());
    }
  }
  return "";
}

std::string defaultOutputDirectory(const SgSourceFile *file) {
  std::string from_arg = commandLineValue(file, "-rex:ast-json-dir=");
  if (!from_arg.empty()) {
    return from_arg;
  }

  const char *env = std::getenv("REX_AST_JSON_DIR");
  if (env != nullptr && env[0] != '\0') {
    return env;
  }

  return ".rex-ast-json";
}

std::string trim(const std::string &input) {
  size_t begin = 0;
  while (begin < input.size() &&
         std::isspace(static_cast<unsigned char>(input[begin]))) {
    ++begin;
  }
  size_t end = input.size();
  while (end > begin &&
         std::isspace(static_cast<unsigned char>(input[end - 1]))) {
    --end;
  }
  return input.substr(begin, end - begin);
}

std::string jsonString(const std::string &input) {
  std::ostringstream out;
  out << '"';
  for (unsigned char c : input) {
    switch (c) {
    case '"':
      out << "\\\"";
      break;
    case '\\':
      out << "\\\\";
      break;
    case '\b':
      out << "\\b";
      break;
    case '\f':
      out << "\\f";
      break;
    case '\n':
      out << "\\n";
      break;
    case '\r':
      out << "\\r";
      break;
    case '\t':
      out << "\\t";
      break;
    default:
      if (c < 0x20) {
        out << "\\u";
        const char *digits = "0123456789abcdef";
        out << '0' << '0' << digits[(c >> 4) & 0xf] << digits[c & 0xf];
      } else {
        out << static_cast<char>(c);
      }
      break;
    }
  }
  out << '"';
  return out.str();
}

void indent(std::ostream &out, int level) {
  for (int i = 0; i < level; ++i) {
    out << ' ';
  }
}

void writeStringField(std::ostream &out, int level, const char *name,
                      const std::string &value, bool comma) {
  indent(out, level);
  out << jsonString(name) << ": " << jsonString(value);
  if (comma) {
    out << ',';
  }
  out << '\n';
}

void writeIntegerField(std::ostream &out, int level, const char *name,
                       int64_t value, bool comma) {
  indent(out, level);
  out << jsonString(name) << ": " << value;
  if (comma) {
    out << ',';
  }
  out << '\n';
}

void writeBoolField(std::ostream &out, int level, const char *name, bool value,
                    bool comma) {
  indent(out, level);
  out << jsonString(name) << ": " << (value ? "true" : "false");
  if (comma) {
    out << ',';
  }
  out << '\n';
}

void requireFileIdMapping(int id, const std::string &filename,
                          const std::string &context) {
  if (id < 0) {
    throw std::runtime_error("AST JSON " + context +
                             " uses negative file id as a registered filename");
  }
  if (filename.empty()) {
    throw std::runtime_error("AST JSON " + context +
                             " has an empty registered filename");
  }

  std::map<int, std::string> &id_to_name = Sg_File_Info::get_fileidtoname_map();
  std::map<std::string, int> &name_to_id = Sg_File_Info::get_nametofileid_map();

  auto id_entry = id_to_name.find(id);
  if (id_entry != id_to_name.end() && id_entry->second != filename) {
    std::ostringstream message;
    message << "AST JSON " << context << " conflicts with existing file id "
            << id << ": existing " << id_entry->second << ", JSON " << filename;
    throw std::runtime_error(message.str());
  }

  auto name_entry = name_to_id.find(filename);
  if (name_entry != name_to_id.end() && name_entry->second != id) {
    std::ostringstream message;
    message << "AST JSON " << context
            << " conflicts with existing filename id for " << filename
            << ": existing " << name_entry->second << ", JSON " << id;
    throw std::runtime_error(message.str());
  }

  id_to_name[id] = filename;
  name_to_id[filename] = id;
}

std::string filenameForFileId(int id, const std::string &context) {
  const std::map<int, std::string> &id_to_name =
      Sg_File_Info::get_fileidtoname_map();
  auto found = id_to_name.find(id);
  if (found == id_to_name.end()) {
    std::ostringstream message;
    message << "AST JSON " << context << " references unregistered file id "
            << id;
    throw std::runtime_error(message.str());
  }
  return found->second;
}

std::string rawStringField(const std::string &name, const std::string &value) {
  return jsonString(name) + ": " + jsonString(value);
}

std::string rawIntegerField(const std::string &name, int64_t value) {
  return jsonString(name) + ": " + std::to_string(value);
}

std::string rawBoolField(const std::string &name, bool value) {
  return jsonString(name) + ": " + (value ? "true" : "false");
}

std::string rawBitVectorJson(const SgBitVector &bits) {
  std::ostringstream out;
  out << '[';
  for (size_t i = 0; i < bits.size(); ++i) {
    if (i != 0) {
      out << ", ";
    }
    out << (bits[i] ? "true" : "false");
  }
  out << ']';
  return out.str();
}

std::string rawStringListJson(const SgStringList &values) {
  std::ostringstream out;
  out << '[';
  for (size_t i = 0; i < values.size(); ++i) {
    if (i != 0) {
      out << ", ";
    }
    out << jsonString(values[i]);
  }
  out << ']';
  return out.str();
}

SgStringList stringListFromJson(const JsonValue &value,
                                const std::string &field_name) {
  if (value.kind != JsonValue::Kind::Array) {
    throw std::runtime_error("AST JSON " + field_name +
                             " field is not an array");
  }
  SgStringList result;
  result.reserve(value.array.size());
  for (const JsonValue &entry : value.array) {
    result.push_back(entry.asString());
  }
  return result;
}

void writeFileIdMapJson(std::ostream &out, int level, bool comma) {
  const std::map<int, std::string> &file_id_map =
      Sg_File_Info::get_fileidtoname_map();

  int expected_id = 0;
  for (const auto &entry : file_id_map) {
    if (entry.first != expected_id) {
      std::ostringstream message;
      message << "AST JSON cannot serialize sparse Sg_File_Info file id map: "
              << "expected id " << expected_id << ", found " << entry.first;
      throw std::runtime_error(message.str());
    }
    if (entry.second.empty()) {
      std::ostringstream message;
      message << "AST JSON cannot serialize empty filename for file id "
              << entry.first;
      throw std::runtime_error(message.str());
    }
    ++expected_id;
  }

  indent(out, level);
  out << jsonString("file_id_map") << ": [\n";
  for (auto entry = file_id_map.begin(); entry != file_id_map.end(); ++entry) {
    indent(out, level + 2);
    out << "{\n";
    writeIntegerField(out, level + 4, "id", entry->first);
    writeStringField(out, level + 4, "filename", entry->second, false);
    indent(out, level + 2);
    out << '}';
    if (std::next(entry) != file_id_map.end()) {
      out << ',';
    }
    out << '\n';
  }
  indent(out, level);
  out << ']';
  if (comma) {
    out << ',';
  }
  out << '\n';
}

void writeRawObject(std::ostream &out, int level,
                    const std::vector<std::string> &fields, bool comma);

} // namespace AstJson
} // namespace Rose
