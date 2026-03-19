#pragma once

#include "Scanner.hpp"
#include "ftxui/component/component.hpp"
#include "ftxui/component/screen_interactive.hpp"
#include <capstone/capstone.h>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <vector>

class TUI {
public:
  TUI(MemoryEngine &engine, Scanner &scanner);
  ~TUI();
  void run();

private:
  MemoryEngine &engine;
  Scanner &scanner;
  ftxui::ScreenInteractive screen = ftxui::ScreenInteractive::Fullscreen();

  // ─── Main tab ────────────────────────────────────────────────────────
  // 0=Addresses  1=Memory Map  2=Call Graph 3=Watch 4=Ptr 5=Disasm
  // 6=Struct Dissector
  int main_tab = 0;

  // ─── UI State ────────────────────────────────────────────────────────
  std::string pid_input;
  std::string scan_value;
  std::string next_scan_value;
  std::string write_value_input;
  std::string goto_addr_input;         // G key: jump to address
  std::string speedhack_input = "1.0"; // Speedhack multiplier
  std::string struct_base_addr_input;  // Structure dissector base
  std::vector<std::string> logs;
  std::mutex logs_mutex;
  int selected_result_idx = 0;
  int selected_map_idx = 0;
  int selected_cg_idx = 0; // selected node in call graph list
  std::string target_process_name = "OFFLINE";
  std::recursive_mutex ui_mutex;

  // ─── Scan selection ─────────────────────────────────────────────────
  int selected_value_type_idx = 2; // Int32
  int selected_scan_type_idx = 0;  // ExactValue

  static constexpr const char *VALUE_TYPE_NAMES[] = {
      "Int8",   "Int16",   "Int32",   "Int64", "UInt8", "UInt16", "UInt32",
      "UInt64", "Float32", "Float64", "Bool",  "AOB",   "String", "String16"};
  static constexpr ValueType VALUE_TYPES[] = {
      ValueType::Int8,    ValueType::Int16,   ValueType::Int32,
      ValueType::Int64,   ValueType::UInt8,   ValueType::UInt16,
      ValueType::UInt32,  ValueType::UInt64,  ValueType::Float32,
      ValueType::Float64, ValueType::Bool,    ValueType::AOB,
      ValueType::String,  ValueType::String16};
  static constexpr const char *SCAN_TYPE_NAMES[] = {
      "Exact Value",  "Not Equal", "Bigger Than",  "Smaller Than",
      "Between",      "Increased", "Increased By", "Decreased",
      "Decreased By", "Changed",   "Unchanged"};
  static constexpr ScanType SCAN_TYPES[] = {
      ScanType::ExactValue,  ScanType::NotEqual,  ScanType::BiggerThan,
      ScanType::SmallerThan, ScanType::Between,   ScanType::Increased,
      ScanType::IncreasedBy, ScanType::Decreased, ScanType::DecreasedBy,
      ScanType::Changed,     ScanType::Unchanged};
  static constexpr int VALUE_TYPE_COUNT = 14;
  static constexpr int SCAN_TYPE_COUNT = 11;

  // ─── Real-time tracking ─────────────────────────────────────────────
  uintptr_t tracked_address = 0;
  std::vector<float> value_history;
  std::vector<uint8_t> hex_dump;
  std::map<uintptr_t, double> last_vals_for_color;
  std::map<uintptr_t, std::string> cached_address_values;
  std::map<uintptr_t, double> cached_address_doubles;

  struct FrozenEntry {
    std::vector<uint8_t> bytes;
    std::string display_val;
  };
  std::map<uintptr_t, FrozenEntry> frozen_addresses;

  // ─── Disassembler lines ─────────────────────────────────────────────
  struct DisasmLine {
    uint64_t addr;
    std::string mnem;
    std::string ops;
    std::string bytes_hex;
    size_t size;
  };
  bool show_disasm = false;
  std::vector<DisasmLine> disasm_lines;
  int selected_disasm_idx = 0;
  std::vector<uintptr_t> disasm_history;
  bool show_patch_modal = false;
  std::string patch_input;
  uintptr_t patch_addr = 0;

  // ─── Memory Map ────────────────────────────────────────────────────
  struct MapEntry {
    uintptr_t start;
    uintptr_t end;
    std::string perms;
    std::string module;      // short basename
    std::string module_full; // full path
    size_t size_bytes;
    bool is_stack;
    bool is_heap;
    bool is_code;              // has 'x'
    uintptr_t file_offset = 0; // from /proc/maps
  };
  std::vector<MapEntry> map_entries;
  void update_memory_map();

  // ─── Call Graph ─────────────────────────────────────────────────────
  struct CallNode {
    uint64_t addr;
    std::string name; // heuristic name
    std::string module;
    uintptr_t base_addr;
    uint64_t offset;
    std::vector<uint64_t> callees;
    int depth;
    bool is_external; // call crosses module boundary
  };
  std::vector<CallNode> call_graph;    // BFS-ordered node list
  std::map<uint64_t, size_t> cg_index; // addr → index in call_graph
  int cg_max_depth = 4;
  void build_call_graph(uintptr_t root, int max_depth);
  std::string guess_name(uintptr_t addr, const std::vector<uint8_t> &bytes,
                         const MapEntry *region);
  std::string match_known_prologue(const std::vector<uint8_t> &bytes);

  // ─── Address categorization ─────────────────────────────────────────
  enum class AddressType { Code, Data, Heap, Stack, Other };
  struct CategorizedAddress {
    uintptr_t addr;
    AddressType type;
    int suspicious_score;
    std::string module_name;
    uintptr_t base_addr;   // runtime start of the region
    uintptr_t file_offset; // file offset of the region start (from /proc/maps)
    // ghidra_addr = ghidra_image_base + file_offset + (addr - base_addr)
  };
  std::vector<CategorizedAddress> categorized_results;
  bool hide_suspicious_low = false;

  // ─── Watchlist ──────────────────────────────────────────────────────
  struct WatchEntry {
    uintptr_t addr;
    std::string description;
    ValueType type;
    bool frozen;
    std::string cached_val; // for UI display
    std::vector<uint8_t> frozen_val;
  };
  std::vector<WatchEntry> watchlist;
  int selected_watch_idx = 0;

  // ─── Pointer Scanning Results ───────────────────────────────────────
  std::vector<Scanner::PointerPath> ptr_results;
  int selected_ptr_idx = 0;

  // ─── Modals ─────────────────────────────────────────────────────────
  bool show_attach_modal = false;
  bool show_scan_modal = false;
  bool show_next_scan_modal = false;
  bool show_write_modal = false;
  bool show_help_modal = false;
  bool show_type_modal = false;
  bool show_scan_type_modal = false;
  bool show_goto_modal = false; // G: jump to address
  bool show_ghidra_base_modal = false;
  bool show_ptr_modal = false;
  bool show_watch_modal = false;
  bool show_struct_modal = false;
  bool show_speedhack_modal = false;
  bool show_kill_modal = false;
  bool show_autoattach_modal = false; // #10: auto-attach by name
  bool show_save_ct_modal = false;    // #2: cheat table save
  bool show_load_ct_modal = false;    // #2: cheat table load
  bool show_bp_list_modal = false;    // #7: breakpoint list
  bool show_record_modal = false;     // #8: recording controls
  bool show_filter_modal = false;      // Filter results modal

  std::string ghidra_base_input;
  std::string watch_desc_input;
  std::string autoattach_name_input; // #10: process name for auto-attach
  std::string ct_path_input = "session.ixeram"; // #2: cheat table file path
  std::string between_min_input;                // #3: Between scan min
  std::string between_max_input;                // #3: Between scan max

  // ─── Result Filtering ───────────────────────────────────────────────
  std::string filter_module_input;     // filter by module name (substring)
  bool filter_show_changed_only = false; // only show addresses whose value changed since first scan
  int  filter_module_sel_idx = 0;      // selected module in the filter list
  std::vector<std::string> filter_module_list; // discovered module names

  // ─── Record / Playback (#8) ─────────────────────────────────────────
  bool record_playing = false;
  size_t record_play_idx = 0;
  std::chrono::steady_clock::time_point record_start;

  // ─── Breakpoint Access Records (#7) ─────────────────────────────────
  std::vector<AccessRecord> bp_hits; // accumulated from engine.access_records
  std::mutex bp_mutex;
  int selected_bp_idx = 0;
  bool show_access_tab = false; // sub-view inside Disasm tab

  // ─── Helpers ────────────────────────────────────────────────────────
  void add_log(const std::string &message);
  void update_tracking_data();
  void update_disasm();
  void freezing_loop();
  std::string addr_type_str(AddressType t) const;
  double read_as_double(uintptr_t addr) const;

  // Cheat Table save/load
  bool save_cheat_table(const std::string &path);
  bool load_cheat_table(const std::string &path);

  // Ghidra export
  void export_ghidra_script(const std::string &path);
  uintptr_t ghidra_image_base = 0x100000; // Default for x64 ELF
};
