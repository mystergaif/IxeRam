#pragma once

#include "MemoryEngine.hpp"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

// ─── Scan Mode ───────────────────────────────────────────────────────────────
enum class ScanType {
  ExactValue,  // == value
  NotEqual,    // != value
  BiggerThan,  // >  value
  SmallerThan, // <  value
  Between,     // min <= value <= max
  Increased,   // increased since last scan (any amount)
  IncreasedBy, // increased by exact amount
  Decreased,   // decreased since last scan (any amount)
  DecreasedBy, // decreased by exact amount
  Changed,     // any change
  Unchanged    // no change
};

// ─── Value Type ──────────────────────────────────────────────────────────────
enum class ValueType {
  Int8,
  Int16,
  Int32,
  Int64,
  UInt8,
  UInt16,
  UInt32,
  UInt64,
  Float32,
  Float64,
  Bool,
  AOB,
  String,
  String16
};

inline std::string valueTypeName(ValueType vt) {
  switch (vt) {
  case ValueType::Int8:
    return "Int8";
  case ValueType::Int16:
    return "Int16";
  case ValueType::Int32:
    return "Int32";
  case ValueType::Int64:
    return "Int64";
  case ValueType::UInt8:
    return "UInt8";
  case ValueType::UInt16:
    return "UInt16";
  case ValueType::UInt32:
    return "UInt32";
  case ValueType::UInt64:
    return "UInt64";
  case ValueType::Float32:
    return "Float32";
  case ValueType::Float64:
    return "Float64";
  case ValueType::Bool:
    return "Bool";
  case ValueType::AOB:
    return "AOB";
  case ValueType::String:
    return "String";
  case ValueType::String16:
    return "String16";
  default:
    return "???";
  }
}

inline size_t valueTypeSize(ValueType vt) {
  switch (vt) {
  case ValueType::Int8:
  case ValueType::UInt8:
  case ValueType::Bool:
    return 1;
  case ValueType::Int16:
  case ValueType::UInt16:
    return 2;
  case ValueType::Int32:
  case ValueType::UInt32:
  case ValueType::Float32:
    return 4;
  case ValueType::Int64:
  case ValueType::UInt64:
  case ValueType::Float64:
    return 8;
  case ValueType::String:
  case ValueType::String16:
    return 1; // Size is dynamic based on length
  default:
    return 4;
  }
}

// ─── Scan Result ─────────────────────────────────────────────────────────────
struct ScanResult {
  uintptr_t address;
  uint8_t prev_value[8]; // Enough for all primitive types
};

// ─── Value Record Entry
// ───────────────────────────────────────────────────────
struct ValueRecord {
  std::chrono::steady_clock::time_point timestamp;
  double value;
};

// ─── Scanner ─────────────────────────────────────────────────────────────────
class Scanner {
public:
  Scanner(MemoryEngine &engine);

  // Perform 1st scan
  void initial_scan(ValueType type, const std::string &value_str);

  // Perform unknown-initial-value scan (snapshot all writable RAM)
  void unknown_initial_scan(ValueType type);

  // Perform subsequent refinement scan
  // For Between: value_str = "min,max"
  void next_scan(ScanType scan_type, const std::string &value_str = "");

  // Save/load pointer paths to file
  bool save_ptr_results(const std::string &path) const;
  bool load_ptr_results(const std::string &path);

  // Record / playback
  std::vector<ValueRecord> value_recording;
  bool recording_active = false;

  // Array-of-Bytes
  void aob_scan(const std::string &pattern);

  const std::vector<ScanResult> &get_results() const { return results; }
  void clear_results() { results.clear(); }

  ValueType get_value_type() const { return current_value_type; }
  bool is_first_scan() const { return first_scan; }
  void reset_first_scan() { first_scan = true; }

  // Read current value at address as formatted string
  std::string read_value_str(uintptr_t address) const;

  // Write a value to address (string -> bytes according to type)
  bool write_value(uintptr_t address, const std::string &value_str,
                   ValueType type);

  // Aligned scanning (performance)
  bool aligned_scan = true;

private:
  MemoryEngine &engine;
  std::vector<ScanResult> results;
  ValueType current_value_type = ValueType::Int32;
  bool first_scan = true;

  std::vector<uint8_t> parse_value(const std::string &value_str,
                                   ValueType type) const;

  template <typename T> void initial_scan_typed(T target);

  template <typename T>
  void next_scan_typed(ScanType scan_type, T target, bool use_target);

  bool match_pattern(const std::vector<uint8_t> &data,
                     const std::vector<int> &pattern);

  // ─── Pointer Scanning ───────────────────────────────────────────────
public:
  struct PointerPath {
    uintptr_t base_module_addr;
    std::string module_name;
    std::vector<int64_t> offsets; // chain of offsets
    uintptr_t final_address;
  };
  std::vector<PointerPath> find_pointers(uintptr_t target_addr,
                                         int max_depth = 2,
                                         int max_offset = 1024);
  // Cache of last pointer scan results (used for save/load)
  mutable std::vector<PointerPath> find_pointers_cache;

  // ─── Parallel / Async Support ────────────────────────────────────────
  float get_progress() const { return progress.load(); }
  bool is_scanning() const { return scanning_active.load(); }
  void set_scanning(bool val) { scanning_active.store(val); }

private:
  std::atomic<float> progress{0.0f};
  std::atomic<bool> scanning_active{false};
};
