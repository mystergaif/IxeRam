// ════════════════════════════════════════════════════════════════════════
//  MEMORY INSPECTOR — TUI.cpp  (Part 1: helpers)
// ════════════════════════════════════════════════════════════════════════
#include "TUI.hpp"
#include "KittyGraphics.hpp"
#include "ftxui/component/component_options.hpp"
#include "ftxui/component/event.hpp"
#include "ftxui/dom/canvas.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/screen/color.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <thread>
using namespace ftxui;

// ODR for constexpr arrays
constexpr const char *TUI::VALUE_TYPE_NAMES[];
constexpr ValueType TUI::VALUE_TYPES[];
constexpr const char *TUI::SCAN_TYPE_NAMES[];
constexpr ScanType TUI::SCAN_TYPES[];

// ─── ctor / dtor ─────────────────────────────────────────────────────────
TUI::TUI(MemoryEngine &e, Scanner &s, const IxeRamConfig &cfg)
    : engine(e), scanner(s), config(cfg) {

  // Apply config to TUI fields
  ghidra_image_base      = cfg.ghidra_image_base;
  ct_path_input          = cfg.default_session_path;
  selected_value_type_idx = cfg.default_value_type;
  scanner.aligned_scan   = cfg.aligned_scan_default;
  log_max_lines          = cfg.log_max_lines;

  add_log("Memory Inspector ready. Attach a PID to begin.");
  add_log("Config loaded — theme:" + std::to_string((int)cfg.theme) +
          "  aligned:" + (cfg.aligned_scan_default ? "on" : "off") +
          "  session:" + cfg.default_session_path);

  if (engine.get_pid() != -1) {
    std::string comm_path =
        "/proc/" + std::to_string(engine.get_pid()) + "/comm";
    std::ifstream comm_file(comm_path);
    if (comm_file) {
      std::getline(comm_file, target_process_name);
    } else {
      target_process_name = "PID " + std::to_string(engine.get_pid());
    }
  }
}
TUI::~TUI() {}

// ─── Desktop Integration ───────────────────────────────────────────────
void TUI::copy_to_clipboard(const std::string &data) {
  // OSC 52 for terminal-level clipboard support (most modern terminals)
  std::cout << "\x1b]52;c;" << data << "\a" << std::flush;

  // External tool fallbacks for Linux
  std::string cmd = "echo -n '" + data + "' | xclip -selection clipboard 2>/dev/null || "
                    "echo -n '" + data + "' | wl-copy 2>/dev/null";
  if (system(cmd.c_str()) == -1) {
    add_log("! Clip engine failed");
  } else {
    add_log("✓ Copied: " + data);
  }
}

void TUI::show_ctx_at(int x, int y, uintptr_t addr, const std::string &val,
                      const std::string &mod, uintptr_t off) {
  context_menu_x = x;
  context_menu_y = y;
  context_menu_addr = addr;
  context_menu_val = val;
  context_menu_mod = mod;
  context_menu_offset = off;
  show_context_menu = true;
}

// ─── Logging ─────────────────────────────────────────────────────────────
void TUI::add_log(const std::string &msg) {
  std::lock_guard<std::mutex> lock(logs_mutex);
  logs.push_back("◈ " + msg);
  if ((int)logs.size() > log_max_lines)
    logs.erase(logs.begin());
}

// ─── read_as_double ──────────────────────────────────────────────────────
double TUI::read_as_double(uintptr_t addr) const {
  if (!addr)
    return 0.0;
  size_t sz = valueTypeSize(scanner.get_value_type());
  std::vector<uint8_t> b(sz, 0);
  if (!engine.read_memory(addr, b.data(), sz))
    return 0.0;
  switch (scanner.get_value_type()) {
  case ValueType::Int8: {
    int8_t v;
    memcpy(&v, b.data(), sz);
    return v;
  }
  case ValueType::Int16: {
    int16_t v;
    memcpy(&v, b.data(), sz);
    return v;
  }
  case ValueType::Int32: {
    int32_t v;
    memcpy(&v, b.data(), sz);
    return v;
  }
  case ValueType::Int64: {
    int64_t v;
    memcpy(&v, b.data(), sz);
    return (double)v;
  }
  case ValueType::UInt8: {
    uint8_t v;
    memcpy(&v, b.data(), sz);
    return v;
  }
  case ValueType::UInt16: {
    uint16_t v;
    memcpy(&v, b.data(), sz);
    return v;
  }
  case ValueType::UInt32: {
    uint32_t v;
    memcpy(&v, b.data(), sz);
    return v;
  }
  case ValueType::UInt64: {
    uint64_t v;
    memcpy(&v, b.data(), sz);
    return (double)v;
  }
  case ValueType::Float32: {
    float v;
    memcpy(&v, b.data(), sz);
    return v;
  }
  case ValueType::Float64: {
    double v;
    memcpy(&v, b.data(), sz);
    return v;
  }
  case ValueType::Bool: {
    uint8_t v;
    memcpy(&v, b.data(), sz);
    return v;
  }
  default:
    return 0.0;
  }
}

std::string TUI::addr_type_str(AddressType t) const {
  switch (t) {
  case AddressType::Code:
    return "C";
  case AddressType::Data:
    return "D";
  case AddressType::Heap:
    return "H";
  case AddressType::Stack:
    return "S";
  default:
    return "?";
  }
}

// ─── update_tracking_data ────────────────────────────────────────────────
void TUI::update_tracking_data() {
  if (scanner.is_scanning())
    return;
  auto &raw = scanner.get_results();
  if (raw.empty()) {
    categorized_results.clear();
    return;
  }

  auto regions = engine.update_maps();
  // Sort regions by start address to allow binary search
  std::sort(regions.begin(), regions.end(),
            [](const auto &a, const auto &b) { return a.start < b.start; });

  // Only re-categorize ALL if the result count changed or it's the first time
  static size_t last_raw_size = 0;
  bool full_update = (raw.size() != last_raw_size);
  last_raw_size = raw.size();

  // We'll prepare new data in local variables first to minimize lock time
  std::vector<CategorizedAddress> temp_results;
  std::map<uintptr_t, std::string> temp_values;
  std::map<uintptr_t, double> temp_doubles;

  if (full_update) {
    temp_results.reserve(raw.size());

    for (const auto &sr : raw) {
      AddressType atype = AddressType::Other;
      int score = 10;
      std::string mod_name = "[anon]";
      uintptr_t base = 0;
      uintptr_t f_off = 0;

      // Binary search for the region
      auto it = std::upper_bound(
          regions.begin(), regions.end(), sr.address,
          [](uintptr_t addr, const auto &reg) { return addr < reg.start; });

      if (it != regions.begin()) {
        const auto &reg = *(--it);
        if (sr.address >= reg.start && sr.address < reg.end) {
          base = reg.start;
          f_off = reg.file_offset;
          size_t sl = reg.pathname.find_last_of('/');
          mod_name = (sl != std::string::npos) ? reg.pathname.substr(sl + 1)
                                               : reg.pathname;
          if (mod_name.empty())
            mod_name = "[anon]";

          if (reg.permissions.find('x') != std::string::npos) {
            atype = AddressType::Code;
            score += 60;
          } else if (reg.pathname.find("[heap]") != std::string::npos) {
            atype = AddressType::Heap;
            score += 30;
          } else if (reg.pathname.find("[stack]") != std::string::npos) {
            atype = AddressType::Stack;
            score += 5;
          } else if (!reg.pathname.empty()) {
            atype = AddressType::Data;
            score += 40;
          }
        }
      }
      temp_results.push_back({sr.address, atype, score, mod_name, base, f_off});
    }

    // Sort by type only when results change
    std::sort(
        temp_results.begin(), temp_results.end(),
        [](const auto &a, const auto &b) { return (int)a.type < (int)b.type; });
  }

  {
    std::lock_guard<std::recursive_mutex> lock(ui_mutex);
    if (full_update) {
      categorized_results = std::move(temp_results);
    }

    if (selected_result_idx >= (int)categorized_results.size())
      selected_result_idx = 0;

    if (!categorized_results.empty() && main_tab == 0) {
      tracked_address = categorized_results[selected_result_idx].addr;
    }
  }

  if (tracked_address != 0) {
    double cur = read_as_double(tracked_address);
    {
      std::lock_guard<std::recursive_mutex> lock(ui_mutex);
      value_history.push_back((float)cur);
      if (value_history.size() > 120)
        value_history.erase(value_history.begin());
    }

    hex_dump.resize(512, 0);
    engine.read_memory(tracked_address, hex_dump.data(), 512);

    // Update disasm only if strictly needed or if on disasm tab
    if (show_disasm || main_tab == 5) {
      update_disasm();
    }
  }

  // Batch read values for the current visible window to avoid hundreds of
  // syscalls in UI This is a heuristic: we update the values of the few
  // addresses around selection
  {
    std::lock_guard<std::recursive_mutex> lock(ui_mutex);
    int start = std::max(0, selected_result_idx - 50);
    int end =
        std::min((int)categorized_results.size(), selected_result_idx + 50);

    // Clear old cache if raw size changed significantly to avoid map growth
    if (full_update && categorized_results.size() < 1000) {
      cached_address_values.clear();
      cached_address_doubles.clear();
    }

    for (int i = start; i < end; ++i) {
      uintptr_t a = categorized_results[i].addr;
      cached_address_doubles[a] = read_as_double(a);
      cached_address_values[a] = scanner.read_value_str(a);
    }
  }

  // Update Watchlist cached values (always do this for real-time)
  {
    std::lock_guard<std::recursive_mutex> lock(ui_mutex);
    for (auto &we : watchlist) {
      we.cached_val = scanner.read_value_str(we.addr);
    }
  }
}

// ─── update_memory_map ──────────────────────────────────────────────────
void TUI::update_memory_map() {
  std::vector<MapEntry> temp;
  auto regions = engine.update_maps();
  for (const auto &reg : regions) {
    MapEntry e;
    e.start = reg.start;
    e.end = reg.end;
    e.module_full = reg.pathname;

    // Wine/Proton support: paths can be Z:\home\... or have .exe/.dll
    std::string path = reg.pathname;
    size_t sl = path.find_last_of('/');
    e.module = (sl != std::string::npos) ? path.substr(sl + 1) : path;

    // If it's a wine preloader or similar, try to keep the original name
    if (e.module.empty())
      e.module = "[anonymous]";
    else if (e.module == "wine64-preloader" || e.module == "wine-preloader") {
      // Keep it, but maybe mark it
    }

    e.size_bytes = reg.end - reg.start;
    e.file_offset = reg.file_offset;
    e.is_stack = path.find("[stack]") != std::string::npos;
    e.is_heap = path.find("[heap]") != std::string::npos;
    e.is_code = reg.permissions.find('x') != std::string::npos;
    temp.push_back(e);
  }
  std::lock_guard<std::recursive_mutex> lock(ui_mutex);
  map_entries = std::move(temp);
}

// ─── update_disasm ──────────────────────────────────────────────────────
void TUI::update_disasm() {
  static csh handle = 0;
  static bool cs_init = false;
  if (!cs_init) {
    if (cs_open(CS_ARCH_X86, CS_MODE_64, &handle) != CS_ERR_OK)
      return;
    cs_init = true;
  }

  if (hex_dump.empty())
    return;

  cs_insn *insn;
  size_t count = cs_disasm(handle, hex_dump.data(), hex_dump.size(),
                           tracked_address, 0, &insn);

  std::lock_guard<std::recursive_mutex> lock(ui_mutex);
  disasm_lines.clear();
  if (count > 0) {
    for (size_t i = 0; i < count && i < 35; ++i) {
      char bytes_str[64] = {0};
      for (int j = 0; j < insn[i].size && j < 16; j++) {
        sprintf(bytes_str + strlen(bytes_str), "%02X ", insn[i].bytes[j]);
      }
      disasm_lines.push_back({insn[i].address, insn[i].mnemonic, insn[i].op_str,
                              bytes_str, insn[i].size});
    }
    cs_free(insn, count);
  } else {
    disasm_lines.push_back(
        {tracked_address, "???", "Invalid or unreadable memory", "", 1});
  }
}

// ─── freeze loop ─────────────────────────────────────────────────────────
void TUI::freezing_loop() {
  while (true) {
    auto copy = frozen_addresses;
    for (auto &[addr, fe] : copy)
      engine.write_memory(addr, fe.bytes.data(), fe.bytes.size());
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }
}

// ─── Heuristic naming ────────────────────────────────────────────────────
// Known x86-64 prologue patterns → predefined label
std::string TUI::match_known_prologue(const std::vector<uint8_t> &b) {
  if (b.size() < 4)
    return "";
  // push rbp; mov rbp, rsp  → 55 48 89 E5
  if (b[0] == 0x55 && b[1] == 0x48 && b[2] == 0x89 && b[3] == 0xE5)
    return "func_standard";
  // endbr64 → F3 0F 1E FA  (CET-enabled)
  if (b[0] == 0xF3 && b[1] == 0x0F && b[2] == 0x1E && b[3] == 0xFA)
    return "func_cet";
  // sub rsp, N → 48 83 EC
  if (b[0] == 0x48 && b[1] == 0x83 && b[2] == 0xEC)
    return "func_leaf";
  // xor eax,eax; ret → 31 C0 C3
  if (b[0] == 0x31 && b[1] == 0xC0 && b[2] == 0xC3)
    return "stub_return_zero";
  // nop sled
  if (b[0] == 0x90 && b[1] == 0x90)
    return "nop_sled";
  return "";
}

std::string TUI::guess_name(uintptr_t addr, const std::vector<uint8_t> &bytes,
                            const MapEntry *region) {
  std::string base_label = "sub";

  // 1. Prologue pattern
  std::string proto = match_known_prologue(bytes);
  if (!proto.empty())
    base_label = proto;

  // 2. Module context heuristics
  std::string mod = region ? region->module : "";
  if (mod.find("libc") != std::string::npos)
    base_label = "libc_" + base_label;
  else if (mod.find("libm") != std::string::npos)
    base_label = "libm_" + base_label;
  else if (mod.find("libpthread") != std::string::npos ||
           mod.find("libthread") != std::string::npos)
    base_label = "thread_" + base_label;
  else if (mod.find("libGL") != std::string::npos ||
           mod.find("libEGL") != std::string::npos)
    base_label = "gfx_" + base_label;
  else if (mod.find("libSDL") != std::string::npos)
    base_label = "sdl_" + base_label;
  else if (mod.find("libssl") != std::string::npos ||
           mod.find("libcrypto") != std::string::npos)
    base_label = "crypto_" + base_label;

  // 3. Scan for nearby ASCII strings (in first 128 bytes)
  std::string found_str;
  for (size_t i = 0; i + 4 < bytes.size(); ++i) {
    if (bytes[i] >= 0x20 && bytes[i] < 0x7F) {
      size_t len = 0;
      while (i + len < bytes.size() && bytes[i + len] >= 0x20 &&
             bytes[i + len] < 0x7F)
        ++len;
      if (len >= 4 && len <= 20) {
        found_str = std::string(bytes.begin() + i, bytes.begin() + i + len);
        // Sanitize: keep only alnum and _
        std::string clean;
        for (char c : found_str)
          if (isalnum(c) || c == '_')
            clean += c;
        if (clean.size() >= 3) {
          base_label = "ref_" + clean;
          break;
        }
      }
      i += len;
    }
  }

  // 4. Append hex offset
  std::ostringstream ss;
  ss << std::hex << std::uppercase << addr;
  return base_label + "_" + ss.str();
}

// ─── build_call_graph ───────────────────────────────────────────────────
void TUI::build_call_graph(uintptr_t root, int max_depth) {
  call_graph.clear();
  cg_index.clear();
  if (!root)
    return;

  auto regions = engine.update_maps();

  // Find region for an address
  auto find_region = [&](uintptr_t addr) -> const MapEntry * {
    for (const auto &e : map_entries)
      if (addr >= e.start && addr < e.end)
        return &e;
    return nullptr;
  };

  // BFS queue: (addr, depth, parent_addr)
  struct QItem {
    uint64_t addr;
    int depth;
  };
  std::vector<QItem> queue;
  std::set<uint64_t> visited;

  queue.push_back({root, 0});
  visited.insert(root);

  while (!queue.empty()) {
    auto [cur_addr, depth] = queue.front();
    queue.erase(queue.begin());

    // Read 256 bytes, disassemble
    std::vector<uint8_t> buf(256, 0);
    engine.read_memory(cur_addr, buf.data(), 256);

    const MapEntry *reg = find_region(cur_addr);

    CallNode node;
    node.addr = cur_addr;
    node.depth = depth;
    node.base_addr = reg ? reg->start : 0;
    node.offset = reg ? (cur_addr - reg->start) : cur_addr;
    node.module = reg ? reg->module : "???";
    node.is_external = (depth > 0 && reg && root > 0 && find_region(root) &&
                        find_region(root) != reg);
    node.name = guess_name(cur_addr, buf, reg);

    // Disassemble to find callees
    if (depth < max_depth) {
      csh handle;
      if (cs_open(CS_ARCH_X86, CS_MODE_64, &handle) == CS_ERR_OK) {
        cs_insn *insn;
        size_t count =
            cs_disasm(handle, buf.data(), buf.size(), cur_addr, 0, &insn);
        for (size_t i = 0; i < count; ++i) {
          std::string mn = insn[i].mnemonic;
          // Follow direct calls and conditional jumps
          if ((mn == "call" || mn == "jmp") && insn[i].op_str[0] == '0') {
            uint64_t target = 0;
            try {
              target = std::stoull(insn[i].op_str, nullptr, 16);
            } catch (...) {
            }
            if (target && !visited.count(target)) {
              visited.insert(target);
              node.callees.push_back(target);
              queue.push_back({target, depth + 1});
            }
          }
        }
        if (count > 0)
          cs_free(insn, count);
        cs_close(&handle);
      }
    }

    cg_index[cur_addr] = call_graph.size();
    call_graph.push_back(std::move(node));
  }
}

// ─── Ghidra export ───────────────────────────────────────────────────────
void TUI::export_ghidra_script(const std::string &path) {
  std::ofstream f(path);
  if (!f.is_open()) {
    add_log("✗ Cannot write " + path);
    return;
  }

  f << "# Ghidra Python Script — Generated by Memory Inspector\n";
  f << "# Run in Ghidra: Script Manager → Run Script\n";
  f << "# NOTE: Adjust IMAGE_BASE to match Ghidra's Image Base\n\n";
  f << "from ghidra.program.model.symbol import SourceType\n\n";

  // Determine image base from first module in map_entries
  uintptr_t runtime_base = 0;
  std::string main_module = "???";
  if (!map_entries.empty()) {
    // Find first executable region (likely the main binary)
    for (const auto &e : map_entries) {
      if (e.is_code && !e.is_heap && !e.is_stack && e.module != "[anonymous]") {
        runtime_base = e.start;
        main_module = e.module;
        break;
      }
    }
  }

  f << "RUNTIME_BASE = " << std::hex << "0x" << runtime_base << "\n";
  f << "# Main module detected: " << main_module << "\n\n";

  f << "def label(offset, name):\n";
  f << "    ghidra_base = currentProgram.getImageBase().getOffset()\n";
  f << "    addr = toAddr(ghidra_base + offset)\n";
  f << "    createLabel(addr, name, True, SourceType.USER_DEFINED)\n\n";

  f << "# ── Found scan results ──\n";
  for (const auto &res : categorized_results) {
    std::string val = scanner.read_value_str(res.addr);
    uint64_t offset = res.addr - res.base_addr;
    std::ostringstream ss;
    ss << std::hex << std::uppercase << offset;
    f << "label(0x" << ss.str() << ", \"var_" << ss.str() << "\")"
      << "  # module=" << res.module_name << " val=" << val
      << " type=" << valueTypeName(scanner.get_value_type()) << "\n";
  }

  f << "\n# ── Call graph functions ──\n";
  for (const auto &node : call_graph) {
    std::ostringstream ss;
    ss << std::hex << std::uppercase << node.offset;
    f << "label(0x" << ss.str() << ", \"" << node.name << "\")"
      << "  # depth=" << node.depth << " module=" << node.module << "\n";
  }

  f.close();
  add_log("✓ Ghidra script → " + path);
}

// ─── Cheat Table Save ────────────────────────────────────────────────────
// Format: simple JSON-like file for portability
bool TUI::save_cheat_table(const std::string &path) {
  std::ofstream f(path);
  if (!f) {
    add_log("✗ Cannot save cheat table to " + path);
    return false;
  }

  f << "{\n";
  f << "  \"pid\": " << engine.get_pid() << ",\n";
  f << "  \"ghidra_base\": " << ghidra_image_base << ",\n";
  f << "  \"value_type\": " << (int)scanner.get_value_type() << ",\n";

  for (size_t i = 0; i < watchlist.size(); ++i) {
    const auto &we = watchlist[i];
    std::string clean_desc = we.description;
    std::replace(clean_desc.begin(), clean_desc.end(), '"', '\'');
    f << "    {\"addr\": " << we.addr << ", \"type\": " << (int)we.type
      << ", \"frozen\": " << (we.frozen ? "true" : "false") << ", \"desc\": \""
      << clean_desc << "\"}";
    if (i + 1 < watchlist.size())
      f << ",";
    f << "\n";
  }
  f << "  ],\n";

  // Frozen addresses (addr + value bytes as hex)
  f << "  \"frozen\": [\n";
  size_t fi = 0;
  for (auto &[addr, fe] : frozen_addresses) {
    f << "    {\"addr\": " << addr << ", \"bytes\": \"";
    for (uint8_t b : fe.bytes) {
      char tmp[4];
      snprintf(tmp, sizeof(tmp), "%02X", b);
      f << tmp;
    }
    f << "\", \"val\": \"" << fe.display_val << "\"}";
    if (++fi < frozen_addresses.size())
      f << ",";
    f << "\n";
  }
  f << "  ],\n";

  // Pointer results (from scanner cache)
  f << "  \"pointers\": [\n";
  for (size_t i = 0; i < scanner.find_pointers_cache.size(); ++i) {
    const auto &p = scanner.find_pointers_cache[i];
    f << "    {\"module\": \"" << p.module_name << "\", \"base\": " << std::hex
      << p.base_module_addr << std::dec << ", \"offsets\": [";
    for (size_t j = 0; j < p.offsets.size(); ++j) {
      f << p.offsets[j];
      if (j + 1 < p.offsets.size())
        f << ", ";
    }
    f << "]}";
    if (i + 1 < scanner.find_pointers_cache.size())
      f << ",";
    f << "\n";
  }
  f << "  ]\n";
  f << "}\n";

  add_log("✓ Cheat Table saved → " + path);
  return true;
}

bool TUI::load_cheat_table(const std::string &path) {
  std::ifstream f(path);
  if (!f) {
    add_log("✗ Cannot load cheat table from " + path);
    return false;
  }

  // Simple line-by-line parser (no full JSON parser needed for our format)
  std::string content((std::istreambuf_iterator<char>(f)),
                      std::istreambuf_iterator<char>());
  f.close();

  watchlist.clear();
  frozen_addresses.clear();

  // Parse ghidra_base
  {
    auto pos = content.find("\"ghidra_base\": ");
    if (pos != std::string::npos) {
      pos += 15;
      try {
        ghidra_image_base = std::stoull(content.substr(pos));
      } catch (...) {
      }
    }
  }

  // Parse watchlist entries line by line (simple approach)
  {
    size_t start = content.find("\"watchlist\":");
    size_t end = content.find("\"frozen\":");
    if (start != std::string::npos && end != std::string::npos) {
      std::string wl_block = content.substr(start, end - start);
      std::istringstream ss(wl_block);
      std::string line;
      while (std::getline(ss, line)) {
        auto fa = line.find("\"addr\": ");
        auto ft = line.find("\"type\": ");
        auto fd = line.find("\"desc\": \"");
        if (fa == std::string::npos || ft == std::string::npos)
          continue;
        WatchEntry we;
        try {
          we.addr = std::stoull(line.substr(fa + 8));
          we.type = (ValueType)std::stoi(line.substr(ft + 8));
          if (fd != std::string::npos) {
            size_t ds = fd + 9;
            size_t de = line.find("\"", ds);
            if (de != std::string::npos)
              we.description = line.substr(ds, de - ds);
          }
          we.frozen = line.find("\"frozen\": true") != std::string::npos;
          we.cached_val = "?";
          watchlist.push_back(we);
        } catch (...) {
        }
      }
    }
  }

  add_log("✓ Cheat Table loaded ← " + path + " (" +
          std::to_string(watchlist.size()) + " watch entries)");
  return true;
}
