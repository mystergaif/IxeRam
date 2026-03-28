// ═══════════════════════════════════════════════════════════════════════
//  TUI::run() — main UI, appended to TUI.cpp
// ═══════════════════════════════════════════════════════════════════════
#include "Config.hpp"
#include "KittyGraphics.hpp"
#include "TUI.hpp"
#include "ftxui/component/component.hpp"
#include "ftxui/component/component_options.hpp"
#include "ftxui/component/event.hpp"
#include <csignal>
#include "ftxui/dom/canvas.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/screen/color.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <iomanip>
#include <keystone/keystone.h>
#include <set>
#include <sstream>
#include <sys/mman.h>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>
#include <unordered_map>
using namespace ftxui;

void TUI::run() {
  // ─── Theme Colors ──────────────────────────────────────────────────
  auto C_BG      = Color::RGB(8,   8,  14);
  auto C_FG      = Color::RGB(200, 200, 220);
  auto C_ACCENT  = Color::RGB(80,  160, 255);
  auto C_ACCENT2 = Color::RGB(140, 80, 255);
  auto C_GREEN   = Color::RGB(50,  230, 120);
  auto C_RED     = Color::RGB(255, 70,  70);
  auto C_ORANGE  = Color::RGB(255, 170, 50);
  auto C_CYAN    = Color::RGB(50,  220, 220);
  auto C_DIM     = Color::RGB(90,  90,  110);
  auto C_YELLOW  = Color::RGB(255, 220, 50);
  auto C_SEL_BG  = Color::RGB(30,  50,  90);

  if (config.theme == ColorTheme::Neon) {
    C_BG      = Color::RGB(0,   0,   0);
    C_FG      = Color::RGB(20,  255, 80);
    C_ACCENT  = Color::RGB(0,   255, 180);
    C_ACCENT2 = Color::RGB(0,   200, 60);
    C_RED     = Color::RGB(255, 50,  80);
    C_YELLOW  = Color::RGB(255, 220, 0);
    C_SEL_BG  = Color::RGB(30,  30,  30);
  } else if (config.theme == ColorTheme::Gruvbox) {
    C_BG      = Color::RGB(29,  32,  33);
    C_FG      = Color::RGB(235, 219, 178);
    C_ACCENT  = Color::RGB(131, 165, 152);
    C_ACCENT2 = Color::RGB(184, 187, 38);
    C_RED     = Color::RGB(251, 73,  52);
    C_YELLOW  = Color::RGB(250, 189, 47);
    C_SEL_BG  = Color::RGB(60,  56,  54);
  } else if (config.theme == ColorTheme::Light) {
    C_BG      = Color::RGB(245, 245, 250);
    C_FG      = Color::RGB(40,  40,  60);
    C_ACCENT  = Color::RGB(30,  100, 200);
    C_ACCENT2 = Color::RGB(0,   140, 70);
    C_RED     = Color::RGB(200, 0,   0);
    C_YELLOW  = Color::RGB(180, 120, 0);
    C_SEL_BG  = Color::RGB(200, 200, 220);
  }

  if (config.show_splash) {
    // Quick splash screen
    auto splash_screen = ScreenInteractive::TerminalOutput();
    auto splash_comp = Renderer([&] {
      return vbox({
        text(""),
        text("      ⬡ IxeRam Memory Scanner") | bold | color(C_ACCENT),
        text("      ━━━━━━━━━━━━━━━━━━━━━━━") | color(C_DIM),
        text("      Launching Terminal UI...") | color(C_FG),
        text(""),
      }) | borderDouble | color(C_ACCENT) | center;
    });
    std::thread t([&] {
        std::this_thread::sleep_for(std::chrono::milliseconds(800));
        splash_screen.ExitLoopClosure()();
    });
    t.detach();
    splash_screen.Loop(splash_comp);
  }

  // (using static TUI::hex_str)

  auto clickable = [&](const std::string &label, Color c) {
    return text(label) | color(c);
  };


  // ─── Scroll offsets for panels ───────────────────────────────────────
  int sidebar_scroll = 0; // sidebar vertical scroll offset
  int log_scroll = 0;     // log panel scroll offset (from bottom)
  int center_scroll = 0;  // extra scroll for center panel (struct/watch)

  // Helper: get terminal dims
  auto term_rows = [&]() { return screen.dimy(); };
  auto term_cols = [&]() { return screen.dimx(); };

  auto input_pid = Input(&pid_input, "PID...");
  auto input_scan = Input(&scan_value, "value...");
  auto input_next =
      Input(&next_scan_value, "value (blank for relative scans)...");
  auto input_write = Input(&write_value_input, "new value...");
  auto input_goto_a = Input(&goto_addr_input, "0x... or decimal...");
  auto input_ghidra_base = Input(&ghidra_base_input, "0x... or decimal...");
  std::string patch_hex_input;
  auto input_patch_hex = Input(&patch_hex_input, "e.g. 90 90");
  auto input_watch_desc = Input(&watch_desc_input, "Description...");
  auto input_struct_base = Input(&struct_base_addr_input, "0x...");
  auto input_speedhack = Input(&speedhack_input, "2.0 or 0.5...");
  auto input_autoattach = Input(&autoattach_name_input, "process name...");
  auto input_ct_path = Input(&ct_path_input, "session.ixeram");
  auto input_between_min = Input(&between_min_input, "min...");
  auto input_between_max = Input(&between_max_input, "max...");
  
  settings_theme_idx = static_cast<int>(config.theme);
  settings_log_max_lines_input = std::to_string(config.log_max_lines);
  settings_ghidra_base_input = hex_str(config.ghidra_image_base);

  auto input_log_max_s = Input(&settings_log_max_lines_input, "200");
  auto input_ghidra_base_s = Input(&settings_ghidra_base_input, "0x100000");

  std::vector<std::string> theme_names_list = {"Dark", "Neon", "Gruvbox", "Light"};
  auto theme_radio = Radiobox(&theme_names_list, &settings_theme_idx);
  auto check_splash = Checkbox(" Show ASCII Splash on Launch", &config.show_splash);
  auto check_hex = Checkbox(" Show Addresses as Hex", &config.show_hex_addresses);
  auto check_tips = Checkbox(" Show Hint Bar (Tips)", &config.show_tips);
  auto check_align = Checkbox(" Aligned Scan by Default", &config.aligned_scan_default);

  auto settings_container = Container::Vertical({
      theme_radio,
      check_splash,
      check_hex,
      check_tips,
      check_align,
      input_log_max_s,
      input_ghidra_base_s,
      Checkbox(" Show Sidebar (left)", &show_sidebar),
      Checkbox(" Show Right Panel (Hex/Graph)", &show_right_panel),
      Checkbox(" Show Log Output (bottom)", &show_log_panel),
  });

  auto btn_toggle_sidebar = Button(" [S] ", [&] { show_sidebar = !show_sidebar; }, ButtonOption::Simple());
  auto btn_toggle_right   = Button(" [R] ", [&] { show_right_panel = !show_right_panel; }, ButtonOption::Simple());
  auto btn_toggle_log     = Button(" [L] ", [&] { show_log_panel = !show_log_panel; }, ButtonOption::Simple());

  auto header_buttons = Container::Horizontal({
      btn_toggle_sidebar,
      btn_toggle_right,
      btn_toggle_log,
  });


  auto settings_tab = Renderer(settings_container, [&] {
      return vbox(Elements{
          text(" ◈ INTERACTIVE SETTINGS ") | bold | color(C_ACCENT2) | hcenter,
          separatorDouble(),
          hbox(Elements{
              vbox(Elements{
                  text(" APPEARANCE ") | bold | color(C_ACCENT),
                  separatorLight(),
                  text(" Color Theme:") | color(C_DIM),
                  theme_radio->Render() | borderLight | color(C_CYAN),
                  separatorLight(),
                  check_splash->Render(),
                  check_hex->Render(),
                  check_tips->Render(),
              }) | flex,
              separator(),
              vbox(Elements{
                  text(" SCAN & ENGINE ") | bold | color(C_ACCENT),
                  separatorLight(),
                  check_align->Render(),
                  separatorLight(),
                  hbox(Elements{text(" Max Log Lines: ") | color(C_DIM), input_log_max_s->Render() | flex}) | borderLight,
                  hbox(Elements{text(" Ghidra Base:  ") | color(C_DIM), input_ghidra_base_s->Render() | flex}) | borderLight,
                  separatorLight(),
                  text(" VISIBILITY SETTINGS ") | bold | color(C_CYAN),
                  Checkbox(" Show Sidebar (left)", &show_sidebar)->Render(),
                  Checkbox(" Show Right Panel (Hex/Graph)", &show_right_panel)->Render(),
                  Checkbox(" Show Log Output (bottom)", &show_log_panel)->Render(),
                  separatorLight(),
                  text(" (Changes apply immediately) ") | color(C_DIM) | dim,
              }) | flex,

          }),
          filler(),
          separatorDouble(),
          hbox(Elements{
              text(" [Enter/S] Save & Apply Settings ") | bold | color(C_GREEN),
              filler(),
              text(" Config: " + Config::config_path()) | color(C_DIM)
          })
      }) | borderDouble | color(C_ACCENT2);
  });





  auto do_set_ghidra_base_logic = [&] {
      try {
          uintptr_t base = ghidra_base_input.empty() ? 0 : (ghidra_base_input.find("0x") == 0 ? std::stoull(ghidra_base_input, nullptr, 16) : std::stoull(ghidra_base_input, nullptr, 10));
          ghidra_image_base = base;
          add_log("✓ Ghidra ImageBase set to " + hex_str(base));
      } catch(...) { add_log("✗ Invalid base"); }
      show_ghidra_base_modal = false;
      ghidra_base_input.clear();
  };

  auto do_attach_lambda = [&] {
      try { do_attach(std::stoi(pid_input)); } catch(...) { add_log("✗ Bad PID"); }
      show_attach_modal = false;
  };

  auto do_initial_scan_lambda = [&] {
      do_first_scan(scan_value);
      show_scan_modal = false;
  };

  auto do_next_scan_lambda = [&] {
      do_next_scan(next_scan_value);
      show_next_scan_modal = false;
      next_scan_value.clear();
  };

  auto do_unknown_scan_lambda = [&] {
      do_unknown_scan();
      show_scan_modal = false;
  };

  auto do_write_lambda = [&] {
      do_write_memory(tracked_address, write_value_input);
      show_write_modal = false;
      write_value_input.clear();
  };

  auto do_goto_lambda = [&] {
      try {
          uintptr_t addr = (goto_addr_input.find("0x") == 0) ? std::stoull(goto_addr_input, nullptr, 16) : std::stoull(goto_addr_input, nullptr, 10);
          do_goto_address(addr);
      } catch(...) { add_log("✗ Invalid address"); }
      show_goto_modal = false;
      goto_addr_input.clear();
  };

  auto do_add_watch_lambda = [&] {
      do_add_watch(tracked_address, watch_desc_input);
      show_watch_modal = false;
      watch_desc_input.clear();
  };

  auto do_ptr_scan_lambda = [&] {
      do_ptr_scan();
      show_ptr_modal = false;
  };

  auto do_autoattach_lambda = [&] {
      do_auto_attach(autoattach_name_input);
      show_autoattach_modal = false;
      autoattach_name_input.clear();
  };

  // ──────────────────────────────────────────────────────────────────
  // ADDRESS TAB  (with live module + changed-value filters)
  // ──────────────────────────────────────────────────────────────────
  auto address_tab = Renderer([&] {
    static uint32_t fc = 0; fc++;

    std::vector<CategorizedAddress> results_copy;
    std::unordered_map<uintptr_t, double> current_vals;
    std::unordered_map<uintptr_t, std::string> current_strs;
    std::unordered_map<uintptr_t, double> prev_vals_copy;
    size_t total_results = 0;
    size_t filtered_total = 0;

    int visible_count = std::max(5, term_rows() - 18);
    {
      std::lock_guard<std::recursive_mutex> lock(ui_mutex);
      if (categorized_results.empty())
        return vbox(
          text(" ◈ No scan results ") | color(C_DIM) | hcenter,
          text(" Press F2 to start a scan or") | color(C_DIM) | hcenter,
          text(" Ctrl+Alt+U for Unknown Initial Value scan ") | color(C_YELLOW) | hcenter
        );

      total_results = categorized_results.size();

      // Build filtered view
      std::vector<int> visible_indices;
      visible_indices.reserve(total_results);
      for (int i = 0; i < (int)total_results; ++i) {
        const auto& res = categorized_results[i];
        // Module filter
        if (!filter_module_input.empty()) {
          std::string mn_lower = res.module_name;
          std::string fi_lower = filter_module_input;
          for (auto& c : mn_lower) c = tolower(c);
          for (auto& c : fi_lower) c = tolower(c);
          if (mn_lower.find(fi_lower) == std::string::npos)
            continue;
        }
        // suspicious filter
        if (hide_suspicious_low && res.suspicious_score < 30)
          continue;
        // Changed-only filter: current != prev
        if (filter_show_changed_only) {
          if (cached_address_doubles.count(res.addr)) {
            double cur = cached_address_doubles.at(res.addr);
            if (last_vals_for_color.count(res.addr)) {
              double prev = last_vals_for_color.at(res.addr);
              if (cur == prev) continue;
            }
          }
        }
        visible_indices.push_back(i);
      }
      filtered_total = visible_indices.size();

      // Clamp selection
      if (selected_result_idx >= (int)filtered_total)
        selected_result_idx = std::max(0, (int)filtered_total - 1);

      int win_start = std::max(0, selected_result_idx - visible_count / 2);
      int win_end   = std::min((int)filtered_total, win_start + visible_count);

      // Build module list for filter dropdown
      {
        std::set<std::string> mods;
        for (const auto& r : categorized_results)
          if (!r.module_name.empty()) mods.insert(r.module_name);
        filter_module_list.assign(mods.begin(), mods.end());
      }

      for (int wi = win_start; wi < win_end; ++wi) {
        int gi = visible_indices[wi];
        const auto& res = categorized_results[gi];
        results_copy.push_back(res);
        current_vals[res.addr]  = cached_address_doubles[res.addr];
        current_strs[res.addr]  = cached_address_values[res.addr];
        if (last_vals_for_color.count(res.addr))
          prev_vals_copy[res.addr] = last_vals_for_color.at(res.addr);
      }
    }

    // ─── Build header with filter status ─────────────────────────────
    // active filter pill helpers
    auto pill = [&](const std::string& label, Color c) {
      return hbox({text(" "), text(label) | bold | color(c),
                   text(" ") | color(c)}) |
             bgcolor(Color::RGB(30, 30, 50));
    };

    Elements header_badges;
    header_badges.push_back(
        hbox({text(" Results: ") | color(C_DIM),
              text(std::to_string(filtered_total)) | color(C_ACCENT) | bold,
              text(" / ") | color(C_DIM),
              text(std::to_string(total_results)) | color(C_DIM),
              text("  ") | color(C_DIM)
             }));
    if (!filter_module_input.empty())
      header_badges.push_back(pill("MOD:" + filter_module_input, C_ORANGE));
    if (filter_show_changed_only)
      header_badges.push_back(pill("CHANGED ONLY", C_RED));
    if (hide_suspicious_low)
      header_badges.push_back(pill("SCORE>30", C_YELLOW));
    header_badges.push_back(filler());
    header_badges.push_back(text(" [F6]Filter ") | color(C_DIM));

    Elements rows;
    // Column header
    rows.push_back(
      hbox(std::move(header_badges)) |
        bgcolor(Color::RGB(16,16,28)));
    rows.push_back(
      hbox(
        text("  ")  | color(C_DIM) | size(WIDTH, EQUAL, 2),
        text("TYPE") | color(C_DIM) | bold | size(WIDTH, EQUAL, 4),
        text(" ADDRESS          ") | color(C_DIM) | bold | size(WIDTH, EQUAL, 18),
        text(" +OFFSET   ") | color(C_DIM) | bold | size(WIDTH, EQUAL, 12),
        text(" MODULE               ") | color(C_DIM) | bold | size(WIDTH, EQUAL, 22),
        text(" VALUE         ") | color(C_DIM) | bold
      ) | bgcolor(Color::RGB(20, 20, 36)));
    rows.push_back(separatorLight() | color(C_DIM));

    int win_start_disp = std::max(0, selected_result_idx - visible_count / 2);
    for (size_t ri = 0; ri < results_copy.size(); ++ri) {
      const auto& res = results_copy[ri];
      int global_filtered_idx = win_start_disp + (int)ri;

      std::string vs  = current_strs[res.addr];
      double dv       = current_vals[res.addr];
      bool changed    = false;
      Color cv = C_FG;
      if (prev_vals_copy.count(res.addr)) {
        double pv = prev_vals_copy.at(res.addr);
        if (dv > pv)       { cv = C_RED;    changed = true; }
        else if (dv < pv)  { cv = C_ACCENT; changed = true; }
        else               { changed = false; }
      }
      if (fc % 3 == 0)
        last_vals_for_color[res.addr] = dv;

      std::ostringstream sa, so;
      sa << "0x" << std::hex << std::uppercase
         << std::setw(12) << std::setfill('0') << res.addr;
      so << "+" << std::hex << std::uppercase << (res.addr - res.base_addr);
      std::string offset_str = so.str();

      bool frz = frozen_addresses.count(res.addr);
      Color cc = C_DIM;
      switch (res.type) {
      case AddressType::Code:  cc = C_RED;    break;
      case AddressType::Data:  cc = C_ORANGE; break;
      case AddressType::Heap:  cc = C_GREEN;  break;
      case AddressType::Stack: cc = C_ACCENT; break;
      default: break;
      }

      // Shorten module name for display
      std::string mod_disp = res.module_name;
      if (mod_disp.size() > 20) mod_disp = mod_disp.substr(0, 17) + "...";

      auto row_dec = hbox(
        text(frz ? "❄" : changed ? "●" : " ") |
          color(frz ? C_CYAN : changed ? cv : C_DIM),
        text(" "),
        text(addr_type_str(res.type)) | color(cc) | bold | size(WIDTH, EQUAL, 3),
        text(sa.str()) |
          color(res.suspicious_score > 50 ? C_FG : C_DIM) |
          size(WIDTH, EQUAL, 16),
        text(" ") | color(C_DIM),
        text(offset_str) | color(C_ACCENT2) | size(WIDTH, EQUAL, 11),
        text(" ") | color(C_DIM),
        text(mod_disp) | color(C_YELLOW) | size(WIDTH, EQUAL, 21),
        text(" │ ") | color(C_DIM),
        text(vs) | color(cv) | bold | size(WIDTH, EQUAL, 14)
      );

      if (global_filtered_idx == selected_result_idx)
        row_dec = row_dec | bgcolor(C_SEL_BG) | color(Color::White);

      auto row_comp = row_dec;
      rows.push_back(row_comp);
    }

    if (results_copy.empty()) {
      rows.push_back(
        text(" No results match the current filters. ") |
          color(C_DIM) | hcenter);
      rows.push_back(
        text(" Press [F6] to change filters. ") | color(C_YELLOW) | hcenter);
    }

    return vbox(std::move(rows)) | reflect(result_list_box);
  });



  // ──────────────────────────────────────────────────────────────────
  // MEMORY MAP TAB
  // ──────────────────────────────────────────────────────────────────
  auto memmap_tab = Renderer([&] {
    if (map_entries.empty())
      return text(" No map. Attach a PID. ") | color(C_DIM);
    Elements rows;
    rows.push_back(hbox(text(" START            ") | color(C_DIM) | bold,
                        text(" END              ") | color(C_DIM) | bold,
                        text(" SIZE      ") | color(C_DIM) | bold,
                        text(" PERMS ") | color(C_DIM) | bold,
                        text(" MODULE") | color(C_DIM) | bold) |
                   bgcolor(Color::RGB(20, 20, 35)));
    rows.push_back(separatorLight() | color(C_DIM));
    int visible_map = std::max(5, term_rows() - 14);
    int vs2 = 0, ve = 0;
    std::vector<MapEntry> map_copy;
    {
      std::lock_guard<std::recursive_mutex> lock(ui_mutex);
      if (map_entries.empty())
        return text(" No map records ") | center;
      vs2 = std::max(0, selected_map_idx - visible_map / 2);
      ve = std::min((int)map_entries.size(), vs2 + visible_map);
      for (int i = vs2; i < ve; ++i)
        map_copy.push_back(map_entries[i]);
    }
    for (int i = 0; i < (int)map_copy.size(); ++i) {
      int global_idx = vs2 + i;
      const auto &e = map_copy[i];
      std::ostringstream ss, se, sz;
      ss << "0x" << std::hex << std::uppercase << std::setw(14)
         << std::setfill('0') << e.start;
      se << "0x" << std::hex << std::uppercase << std::setw(14)
         << std::setfill('0') << e.end;
      size_t kb = e.size_bytes / 1024;
      if (kb > 1024)
        sz << (kb / 1024) << " MB";
      else
        sz << kb << " KB";
      Color mc = C_FG;
      if (e.is_code)
        mc = Color::RGB(255, 120, 120);
      else if (e.is_heap)
        mc = Color::RGB(100, 255, 150);
      else if (e.is_stack)
        mc = Color::RGB(100, 150, 255);
      else if (e.module != "[anonymous]")
        mc = C_YELLOW;
      auto row_dec =
          hbox(text(" " + ss.str() + " ") | color(C_DIM),
               text(" " + se.str() + " ") | color(C_DIM),
               text(" " + sz.str() + " ") | color(C_ACCENT) |
                   size(WIDTH, EQUAL, 10),
               text(" " + e.perms + " ") | color(e.is_code ? C_RED : C_DIM) |
                   size(WIDTH, EQUAL, 6),
               text(" " + e.module) | color(mc) | bold);

      if (global_idx == selected_map_idx)
        row_dec = row_dec | bgcolor(C_SEL_BG) | color(Color::White);

      auto row_comp = row_dec;
      rows.push_back(row_comp);
    }

    return vbox(std::move(rows));
  });

  // ──────────────────────────────────────────────────────────────────
  // CALL GRAPH TAB
  // ──────────────────────────────────────────────────────────────────
  auto callgraph_tab = Renderer([&] {
    if (call_graph.empty()) {
      Elements h;
      h.push_back(text(" No call graph data. ") | color(C_DIM));
      h.push_back(text(" Select an address in Addresses tab, then press B.") |
                  color(C_DIM));
      return vbox(std::move(h));
    }
    Elements rows;
    rows.push_back(
        hbox(text(" D") | color(C_DIM) | bold | size(WIDTH, EQUAL, 3),
             text(" ADDRESS       ") | color(C_DIM) | bold |
                 size(WIDTH, EQUAL, 16),
             text(" +OFFSET   ") | color(C_DIM) | bold | size(WIDTH, EQUAL, 12),
             text(" MODULE       ") | color(C_DIM) | bold |
                 size(WIDTH, EQUAL, 16),
             text(" HEURISTIC NAME") | color(C_DIM) | bold) |
        bgcolor(Color::RGB(20, 20, 35)));
    rows.push_back(separatorLight() | color(C_DIM));
    int visible_cg = std::max(5, term_rows() - 14);
    int vs2 = 0, ve = 0;
    std::vector<CallNode> cg_copy;
    {
      std::lock_guard<std::recursive_mutex> lock(ui_mutex);
      if (call_graph.empty())
        return text(" No CG records ") | center;
      vs2 = std::max(0, selected_cg_idx - visible_cg / 2);
      ve = std::min((int)call_graph.size(), vs2 + visible_cg);
      for (int i = vs2; i < ve; ++i)
        cg_copy.push_back(call_graph[i]);
    }
    for (int i = 0; i < (int)cg_copy.size(); ++i) {
      int global_idx = vs2 + i;
      const auto &n = cg_copy[i];
      std::string ind(n.depth * 2, ' ');
      std::ostringstream sa, so;
      sa << "0x" << std::hex << std::uppercase << n.addr;
      so << "+" << std::hex << std::uppercase << n.offset;
      Color nc = (n.depth == 0) ? C_YELLOW : n.is_external ? C_RED : C_GREEN;
      auto row =
          hbox(text(" " + std::to_string(n.depth)) | color(C_DIM) |
                   size(WIDTH, EQUAL, 3),
               text(" " + sa.str()) | color(C_CYAN) | size(WIDTH, EQUAL, 16),
               text(" " + so.str()) | color(C_ACCENT2) | size(WIDTH, EQUAL, 12),
               text(" " + n.module) | color(C_ORANGE) | size(WIDTH, EQUAL, 16),
               text(" " + ind + n.name) | color(nc) | bold);
      if (global_idx == selected_cg_idx)
        row = row | bgcolor(C_SEL_BG) | color(Color::White);
      rows.push_back(row);
    }
    rows.push_back(separatorLight() | color(C_DIM));
    rows.push_back(
        hbox(text(" Nodes: ") | color(C_DIM),
             text(std::to_string(call_graph.size())) | color(C_ACCENT) | bold,
             text("  [B]rebuild  [E]export to Ghidra") | color(C_DIM)));
    return vbox(std::move(rows));
  });

  // ──────────────────────────────────────────────────────────────────
  // WATCHLIST TAB
  // ──────────────────────────────────────────────────────────────────
  auto watch_tab = Renderer([&] {
    if (watchlist.empty())
      return text(" Watchlist empty. [A] to add current address. ") |
             color(C_DIM) | center;
    Elements rows;
    rows.push_back(hbox(text(" DESCRIPTION      ") | color(C_DIM) | bold,
                        text(" ADDRESS          ") | color(C_DIM) | bold,
                        text(" TYPE      ") | color(C_DIM) | bold,
                        text(" VALUE            ") | color(C_DIM) | bold) |
                   bgcolor(Color::RGB(20, 20, 35)));
    rows.push_back(separatorLight() | color(C_DIM));
    for (int i = 0; i < (int)watchlist.size(); ++i) {
      auto &we = watchlist[i];
      auto row = hbox(text(" " + we.description) | size(WIDTH, EQUAL, 18),
                      text(" " + hex_str(we.addr)) | color(C_CYAN) |
                          size(WIDTH, EQUAL, 18),
                      text(" " + valueTypeName(we.type)) | color(C_ACCENT2) |
                          size(WIDTH, EQUAL, 10),
                      text(" " + we.cached_val) | color(C_GREEN) | bold);
      if (i == selected_watch_idx)
        row = row | bgcolor(C_SEL_BG);
      rows.push_back(row);
    }
    return vbox(std::move(rows));
  });

  // ──────────────────────────────────────────────────────────────────
  // POINTER SCAN TAB
  // ──────────────────────────────────────────────────────────────────
  auto ptr_tab = Renderer([&] {
    if (ptr_results.empty())
      return text(" No pointer results. [P] to scan current address. ") |
             color(C_DIM) | center;
    Elements rows;
    rows.push_back(hbox(text(" MODULE           ") | color(C_DIM) | bold,
                        text(" BASE ADDR        ") | color(C_DIM) | bold,
                        text(" OFFSETS          ") | color(C_DIM) | bold) |
                   bgcolor(Color::RGB(20, 20, 35)));
    rows.push_back(separatorLight() | color(C_DIM));
    for (int i = 0; i < (int)ptr_results.size(); ++i) {
      auto &p = ptr_results[i];
      std::string off_str;
      for (auto o : p.offsets)
        off_str += (o >= 0 ? "+" : "") + std::to_string(o) + " ";
      auto row = hbox(
          text(" " + p.module_name) | color(C_ORANGE) | size(WIDTH, EQUAL, 18),
          text(" " + hex_str(p.base_module_addr)) | size(WIDTH, EQUAL, 18),
          text(" " + off_str) | color(C_YELLOW));
      if (i == selected_ptr_idx)
        row = row | bgcolor(C_SEL_BG);
      rows.push_back(row);
    }
    return vbox(std::move(rows));
  });

  // ──────────────────────────────────────────────────────────────────
  // DISASSEMBLER TAB  (#11: improved coloring)
  // ──────────────────────────────────────────────────────────────────
  // Helper: colorize an operand string token-by-token
  auto colorize_ops = [&](const std::string &ops) -> Element {
    // Split on commas and brackets, color registers, immediates, mem refs
    Elements parts;
    std::string cur;
    auto flush = [&]() {
      if (cur.empty())
        return;
      // Registers: start with r/e and 2-3 chars, or known names
      auto is_reg = [](const std::string &s) {
        static const char *regs[] = {
            "rax",  "rbx",  "rcx",  "rdx",    "rsi",  "rdi",  "rbp",  "rsp",
            "r8",   "r9",   "r10",  "r11",    "r12",  "r13",  "r14",  "r15",
            "eax",  "ebx",  "ecx",  "edx",    "esi",  "edi",  "ebp",  "esp",
            "al",   "bl",   "cl",   "dl",     "ah",   "bh",   "ch",   "dh",
            "ax",   "bx",   "cx",   "dx",     "si",   "di",   "bp",   "sp",
            "xmm0", "xmm1", "xmm2", "xmm3",   "xmm4", "xmm5", "xmm6", "xmm7",
            "ymm0", "ymm1", "rip",  "rflags", nullptr};
        std::string sl = s;
        for (auto &c : sl)
          c = tolower(c);
        for (int i = 0; regs[i]; ++i)
          if (sl == regs[i])
            return true;
        return false;
      };
      // Immediate/hex: starts with 0x or minus or digit
      bool is_imm =
          (!cur.empty() && (cur[0] == '0' || cur[0] == '-' || isdigit(cur[0])));
      // Memory reference: contains '['
      bool is_mem = (cur.find('[') != std::string::npos);
      Color oc = C_FG;
      if (is_mem)
        oc = Color::RGB(255, 200, 100);
      else if (is_imm)
        oc = Color::RGB(180, 130, 255);
      else if (is_reg(cur))
        oc = Color::RGB(100, 220, 255);
      parts.push_back(text(cur) | color(oc));
      cur.clear();
    };
    for (char c : ops) {
      if (c == ',' || c == ' ' || c == '[' || c == ']') {
        flush();
        Color sc = (c == '[' || c == ']') ? Color::RGB(255, 200, 100) : C_DIM;
        parts.push_back(text(std::string(1, c)) | color(sc));
      } else {
        cur += c;
      }
    }
    flush();
    if (parts.empty())
      return text("");
    return hbox(std::move(parts));
  };

  // Breakpoint hits tab (sub-view)
  auto bp_hits_view = Renderer([&] {
    std::lock_guard<std::mutex> lock(bp_mutex);
    Elements rows;
    rows.push_back(hbox(text(" RIP (hit addr)    ") | color(C_DIM) | bold |
                            size(WIDTH, EQUAL, 20),
                        text(" RAX              ") | color(C_DIM) | bold |
                            size(WIDTH, EQUAL, 20),
                        text(" RBX              ") | color(C_DIM) | bold |
                            size(WIDTH, EQUAL, 20),
                        text(" Write?") | color(C_DIM) | bold) |
                   bgcolor(Color::RGB(20, 20, 35)));
    rows.push_back(separatorLight() | color(C_DIM));
    if (bp_hits.empty()) {
      rows.push_back(
          text(" No breakpoint hits yet. [Z] set bp, [Ctrl+Z] wait") |
          color(C_DIM));
    }
    for (int i = 0; i < (int)bp_hits.size(); ++i) {
      const auto &ar = bp_hits[i];
      std::ostringstream sr, sa, sb;
      sr << "0x" << std::hex << std::uppercase << ar.rip;
      sa << "0x" << std::hex << std::uppercase << ar.rax;
      sb << "0x" << std::hex << std::uppercase << ar.rbx;
      auto row =
          hbox(text(" " + sr.str()) | color(C_CYAN) | size(WIDTH, EQUAL, 20),
               text(" " + sa.str()) | color(C_ACCENT) | size(WIDTH, EQUAL, 20),
               text(" " + sb.str()) | color(C_ACCENT2) | size(WIDTH, EQUAL, 20),
               text(ar.is_write ? " WRITE" : " READ") |
                   color(ar.is_write ? C_RED : C_GREEN));
      if (i == selected_bp_idx)
        row = row | bgcolor(C_SEL_BG);
      rows.push_back(row);
    }
    return vbox(std::move(rows));
  });

  auto disasm_tab_r = Renderer([&] {
    if (show_access_tab) {
      return bp_hits_view->Render();
    }
    if (disasm_lines.empty())
      return text(" ERR: No disassembly available here. ") | color(C_RED) |
             center;
    Elements rows;
    rows.push_back(hbox(text(" ADDRESS          ") | color(C_DIM) | bold,
                        text(" BYTES                 ") | color(C_DIM) | bold,
                        text(" MNEM     ") | color(C_DIM) | bold,
                        text(" OPERANDS") | color(C_DIM) | bold) |
                   bgcolor(Color::RGB(20, 20, 35)));
    rows.push_back(separatorLight() | color(C_DIM));

    int visible_disasm = std::max(5, term_rows() - 14);
    int start_idx = std::max(0, selected_disasm_idx - visible_disasm / 2);
    for (int i = start_idx;
         i < std::min((int)disasm_lines.size(), start_idx + visible_disasm);
         ++i) {
      auto &l = disasm_lines[i];
      Color mc = C_ORANGE;
      if (!l.mnem.empty()) {
        char c0 = l.mnem[0];
        if (c0 == 'j' && l.mnem != "jmp")
          mc = Color::RGB(255, 140, 50); // conditional jumps: orange
        else if (l.mnem == "jmp")
          mc = Color::RGB(255, 80, 80); // unconditional jump: red
        else if (l.mnem == "call")
          mc = Color::RGB(255, 60, 180); // call: pink-red
        else if (l.mnem == "mov" || l.mnem == "movsx" || l.mnem == "movzx" ||
                 l.mnem == "movaps" || l.mnem == "movdqa")
          mc = Color::RGB(80, 200, 255); // move: cyan
        else if (l.mnem == "lea")
          mc = Color::RGB(60, 230, 200); // lea: teal
        else if (l.mnem == "push" || l.mnem == "pop")
          mc = Color::RGB(160, 100, 255); // stack: purple
        else if (l.mnem == "ret" || l.mnem == "retn")
          mc = Color::RGB(80, 255, 100); // ret: green
        else if (l.mnem == "nop")
          mc = C_DIM;
        else if (l.mnem == "xor" || l.mnem == "and" || l.mnem == "or" ||
                 l.mnem == "not" || l.mnem == "test")
          mc = Color::RGB(255, 220, 80); // logical: yellow
        else if (l.mnem == "add" || l.mnem == "sub" || l.mnem == "imul" ||
                 l.mnem == "idiv" || l.mnem == "inc" || l.mnem == "dec")
          mc = Color::RGB(255, 160, 80); // arithmetic: orange-yellow
        else if (l.mnem == "cmp")
          mc = Color::RGB(200, 200, 80); // compare: dim yellow
      }
      std::ostringstream ss;
      ss << "0x" << std::hex << std::uppercase << l.addr;
      bool is_node = cg_index.count(l.addr) > 0;
      bool has_bp = (engine.get_pid() > 0); // could check bp map

      auto row = hbox(text(ss.str() + " ") | color(is_node ? C_YELLOW : C_DIM) |
                          size(WIDTH, EQUAL, 18),
                      text(l.bytes_hex) | color(C_DIM) | size(WIDTH, EQUAL, 23),
                      text(" "),
                      text(l.mnem) | color(mc) | bold | size(WIDTH, EQUAL, 8),
                      colorize_ops(l.ops));

      if (i == selected_disasm_idx)
        row = row | bgcolor(C_SEL_BG);
      rows.push_back(row);
    }
    // Status bar for disasm tab
    rows.push_back(separatorLight() | color(C_DIM));
    rows.push_back(hbox(
        text(" [SPACE]patch ") | color(C_DIM),
        text("[ENTER]jump ") | color(C_DIM), text("[BSPC]back ") | color(C_DIM),
        text("[Z] Set BP ") | color(C_YELLOW),
        text("[U] Access Log") | color(show_access_tab ? C_GREEN : C_DIM)));
    return vbox(std::move(rows));
  });

  // ──────────────────────────────────────────────────────────────────
  // STRUCTURE DISSECTOR TAB
  // ──────────────────────────────────────────────────────────────────
  auto struct_tab_r = Renderer(input_struct_base, [&] {
    uintptr_t base = 0;
    if (!struct_base_addr_input.empty()) {
      try {
        if (struct_base_addr_input.find("0x") == 0)
          base = std::stoull(struct_base_addr_input, nullptr, 16);
        else
          base = std::stoull(struct_base_addr_input, nullptr, 10);
      } catch (...) {
      }
    } else {
      base = tracked_address;
    }

    Elements rows;
    rows.push_back(hbox(text(" Base Addr: ") | color(C_DIM),
                        input_struct_base->Render() | size(WIDTH, EQUAL, 20)));
    rows.push_back(separatorLight() | color(C_DIM));
    rows.push_back(
        hbox(text(" OFFSET ") | bold | color(C_DIM) | size(WIDTH, EQUAL, 9),
             text(" HEX         ") | bold | color(C_DIM) |
                 size(WIDTH, EQUAL, 14),
             text(" INT32       ") | bold | color(C_DIM) |
                 size(WIDTH, EQUAL, 14),
             text(" FLOAT       ") | bold | color(C_DIM) |
                 size(WIDTH, EQUAL, 14),
             text(" STRING") | bold | color(C_DIM)) |
        bgcolor(Color::RGB(20, 20, 35)));
    rows.push_back(separatorLight() | color(C_DIM));

    if (base) {
      std::vector<uint8_t> buf(128, 0);
      engine.read_memory(base, buf.data(), 128);
      for (int i = 0; i < 128; i += 4) {
        std::ostringstream os, hs, is, fs;
        os << "+" << std::hex << std::uppercase << std::setfill('0')
           << std::setw(2) << i;
        hs << std::hex << std::uppercase << std::setfill('0') << std::setw(2)
           << (int)buf[i] << " " << std::setw(2) << (int)buf[i + 1] << " "
           << std::setw(2) << (int)buf[i + 2] << " " << std::setw(2)
           << (int)buf[i + 3];
        int32_t iv;
        std::memcpy(&iv, &buf[i], 4);
        is << iv;
        float fv;
        std::memcpy(&fv, &buf[i], 4);
        if (std::abs(fv) > 1e-10 && std::abs(fv) < 1e10)
          fs << std::fixed << std::setprecision(3) << fv;
        else
          fs << "---";

        std::string strv;
        for (int j = 0; j < 4; ++j) {
          char c = buf[i + j];
          if (c >= 0x20 && c < 0x7F)
            strv += c;
          else
            strv += ".";
        }

        rows.push_back(hbox(
            text(" " + os.str()) | color(C_ACCENT2) | size(WIDTH, EQUAL, 9),
            text(" " + hs.str()) | color(C_CYAN) | size(WIDTH, EQUAL, 14),
            text(" " + is.str()) | color(C_YELLOW) | size(WIDTH, EQUAL, 14),
            text(" " + fs.str()) | color(C_GREEN) | size(WIDTH, EQUAL, 14),
            text(" " + strv) | color(C_FG)));
      }
    } else {
      rows.push_back(text(" Provide base address to dissect ") | color(C_DIM) |
                     center);
    }
    return vbox(std::move(rows));
  });

  // ──────────────────────────────────────────────────────────────────
  // HEX EDITOR (READ/WRITE) TAB
  // ──────────────────────────────────────────────────────────────────
  auto hex_editor_tab_r = Renderer([&] {
    if (hex_editor_base == 0) hex_editor_base = tracked_address;
    if (hex_editor_buf.size() < (size_t)hex_editor_rows * 16) {
        hex_editor_buf.resize(hex_editor_rows * 16, 0);
    }
    engine.read_memory(hex_editor_base, hex_editor_buf.data(), hex_editor_buf.size());

    Elements rows;
    rows.push_back(hbox({
        text("  ") | size(WIDTH, EQUAL, 10),
        text(" 00 01 02 03 04 05 06 07  08 09 0A 0B 0C 0D 0E 0F ") | color(C_DIM) | bold,
        text("  ASCII           ") | color(C_DIM) | bold
    }));
    rows.push_back(separatorLight() | color(C_DIM));

    for (int r = 0; r < hex_editor_rows; r++) {
        Elements hex_cols;
        Elements asc_cols;
        std::ostringstream addr_s;
        addr_s << " " << std::hex << std::uppercase << std::setfill('0') << std::setw(8) << (hex_editor_base + r * 16) << " ";
        hex_cols.push_back(text(addr_s.str()) | color(C_DIM));

        for (int c = 0; c < 16; c++) {
            int idx = r * 16 + c;
            uint8_t b = hex_editor_buf[idx];
            std::ostringstream bs;
            bs << std::hex << std::uppercase << std::setfill('0') << std::setw(2) << (int)b;
            
            bool is_cursor = (idx == hex_editor_cursor_idx);
            auto hex_cell = text(bs.str());
            if (is_cursor) {
                hex_cell = hex_cell | bold | bgcolor(C_ACCENT) | color(Color::Black);
            } else if (b == 0) hex_cell = hex_cell | color(C_DIM);
            else if (b >= 0x20 && b < 0x7F) hex_cell = hex_cell | color(C_GREEN);

            hex_cols.push_back(hex_cell);
            hex_cols.push_back(text(c == 7 ? "  " : " "));

            char ch = (b >= 0x20 && b < 0x7f) ? (char)b : '.';
            auto asc_cell = text(std::string(1, ch));
            if (is_cursor) asc_cell = asc_cell | bold | color(C_ACCENT);
            else asc_cell = asc_cell | color(ch == '.' ? C_DIM : C_CYAN);
            asc_cols.push_back(asc_cell);
        }
        rows.push_back(hbox(hbox(std::move(hex_cols)), text(" │ ") | color(C_DIM), hbox(std::move(asc_cols))));
    }

    rows.push_back(separatorLight() | color(C_DIM));
    rows.push_back(hbox({
        text(" [ARROWS]Navigate  [0-9A-F]Edit Byte  [PgUp/Dn]Scroll  [G]Goto-Editor  [Z]Set HW Breakpoint ") | color(C_DIM)
    }));

    return vbox(std::move(rows));
  });

  // ──────────────────────────────────────────────────────────────────
  // HARDWARE BREAKPOINT MODAL
  // ──────────────────────────────────────────────────────────────────
  auto hw_bp_modal_r = Renderer([&] {
    Elements items;
    items.push_back(text(" ⬤ HARDWARE BREAKPOINTS / WATCHPOINTS ") | bold | color(C_RED) | hcenter);
    items.push_back(separatorDouble());
    
    for (int i = 0; i < 4; i++) {
        auto& slot = hw_ui_slots[i];
        std::ostringstream sa; sa << "0x" << std::hex << std::uppercase << slot.addr;

        auto row = hbox({
            text(" SLOT " + std::to_string(i) + ": ") | color(C_DIM),
            text(slot.active ? " ENABLED " : " DISABLED ") | color(slot.active ? C_GREEN : C_DIM) | bold,
            text(" Addr: ") | color(C_DIM),
            text(sa.str()) | color(C_CYAN) | size(WIDTH, EQUAL, 14),
            text(" Type: ") | color(C_DIM),
            text(slot.type_idx == 0 ? "Exec " : (slot.type_idx == 1 ? "Write" : "R/W  ")) | color(C_ORANGE),
            text(" Size: ") | color(C_DIM),
            text(std::to_string(slot.size_idx == 0 ? 1 : (slot.size_idx == 1 ? 2 : (slot.size_idx == 3 ? 4 : 8))) + "b") | color(C_YELLOW)
        });
        items.push_back(row | borderLight);
    }
    
    items.push_back(separatorLight());
    items.push_back(text(" [0-3] Clear Slot   [H] Set NEW at Cursor addr   [Esc] Close ") | color(C_DIM) | hcenter);
    
    return vbox(std::move(items)) | size(WIDTH, EQUAL, 70) | borderDouble | bgcolor(Color::RGB(10, 10, 20)) | center;
  });

  // ──────────────────────────────────────────────────────────────────
  // HEX / DISASM + GRAPH
  // ──────────────────────────────────────────────────────────────────
  auto hex_view = Renderer([&] {
    if (show_disasm) {
      Elements lines;
      for (auto &l : disasm_lines) {
        Color mc = C_ORANGE;
        if (!l.mnem.empty()) {
          char c0 = l.mnem[0];
          if (c0 == 'j' || l.mnem == "call")
            mc = C_RED;
          else if (l.mnem == "mov" || l.mnem == "lea")
            mc = C_CYAN;
          else if (l.mnem == "push" || l.mnem == "pop")
            mc = C_ACCENT2;
          else if (l.mnem == "ret")
            mc = C_GREEN;
          else if (l.mnem == "nop")
            mc = C_DIM;
        }
        std::ostringstream ss;
        ss << "0x" << std::hex << std::uppercase << l.addr;
        bool is_node = cg_index.count(l.addr) > 0;
        lines.push_back(hbox(
            text(ss.str()) | color(is_node ? C_YELLOW : C_DIM) |
                size(WIDTH, EQUAL, 16),
            text("  "), text(l.mnem) | color(mc) | bold | size(WIDTH, EQUAL, 8),
            text(l.ops) | color(is_node ? C_YELLOW : C_FG)));
      }
      return vbox(std::move(lines));
    }
    Elements rows;
    for (int i = 0; i < 8; ++i) {
      Elements cols;
      std::ostringstream as;
      as << "0x" << std::hex << std::uppercase << std::setw(6)
         << std::setfill('0') << (tracked_address + i * 16);
      cols.push_back(text(as.str() + "  ") | color(C_DIM));
      for (int j = 0; j < 16; ++j) {
        int idx = i * 16 + j;
        if (idx < (int)hex_dump.size()) {
          std::ostringstream bs;
          bs << std::setw(2) << std::setfill('0') << std::hex << std::uppercase
             << (int)hex_dump[idx];
          Color bc = C_FG;
          if (hex_dump[idx] == 0)
            bc = C_DIM;
          else if (hex_dump[idx] == 0xFF)
            bc = C_RED;
          else if (hex_dump[idx] >= 0x20 && hex_dump[idx] < 0x7F)
            bc = C_GREEN;
          cols.push_back(text(bs.str() + " ") | color(bc));
        }
      }
      cols.push_back(text("│") | color(C_DIM));
      for (int j = 0; j < 16; ++j) {
        int idx = i * 16 + j;
        if (idx < (int)hex_dump.size()) {
          char ch = (hex_dump[idx] >= 0x20 && hex_dump[idx] < 0x7F)
                        ? (char)hex_dump[idx]
                        : '.';
          cols.push_back(text(std::string(1, ch)) |
                         color(ch == '.' ? C_DIM : C_CYAN));
        }
      }
      rows.push_back(hbox(std::move(cols)));
    }
    return vbox(std::move(rows));
  });

  auto graph_view = Renderer([&] {
    return canvas([&](Canvas &c) {
             if (value_history.size() < 2)
               return;
             float mn =
                 *std::min_element(value_history.begin(), value_history.end());
             float mx =
                 *std::max_element(value_history.begin(), value_history.end());
             float rng = (mx - mn > 0) ? (mx - mn) : 1.0f;
             int H = 38;
             for (size_t i = 1; i < value_history.size(); ++i) {
               int y1 = H - (int)((value_history[i - 1] - mn) / rng * H);
               int y2 = H - (int)((value_history[i] - mn) / rng * H);
               c.DrawBlockLine((int)(i - 1) * 2, y1, (int)i * 2, y2, C_GREEN);
             }
             std::ostringstream smx, smn;
             smx << (int)mx;
             smn << (int)mn;
             c.DrawText(0, 0, smx.str());
             c.DrawText(0, H, smn.str());
           }) |
           color(C_GREEN);
  });

  // Log panel height (rows visible in log box ~6 lines)
  static constexpr int LOG_PANEL_ROWS = 6;
  auto log_view = Renderer([&]() -> Element {
    std::vector<std::string> log_copy;
    {
      std::lock_guard<std::mutex> lock(logs_mutex);
      log_copy = logs;
    }
    Elements l;
    int total_logs = (int)log_copy.size();
    // log_scroll: 0 = bottom (latest), positive = scrolled up
    int clamped_scroll =
        std::min(log_scroll, std::max(0, total_logs - LOG_PANEL_ROWS));
    int end_idx = total_logs - clamped_scroll;
    int start_idx = std::max(0, end_idx - LOG_PANEL_ROWS);
    for (int i = start_idx; i < end_idx; ++i)
      l.push_back(text(log_copy[i]) | color(C_DIM));
    if (clamped_scroll > 0)
      l.push_back(text(" ↑ +" + std::to_string(clamped_scroll) +
                       " more above (scroll up)") |
                  color(C_ACCENT2) | dim);
    return vbox(std::move(l)) | reflect(log_box);
  });

  // ──────────────────────────────────────────────────────────────────
  // SIDEBAR
  // ──────────────────────────────────────────────────────────────────
  auto sidebar_view = Renderer([&] {
    bool hp = (engine.get_pid() != -1);
    auto mk_tab = [&](int idx, const std::string &label) {
      bool sel = (main_tab == idx);
      return text(" " + label + " ") | (sel ? bold : dim) |
             color(sel ? Color::White : C_DIM) |
             (sel ? bgcolor(C_SEL_BG) : bgcolor(Color::Default));
    };
    static uint32_t name_check_count = 0;
    if (hp && target_process_name == "OFFLINE" &&
        (name_check_count++ % 100 == 0)) {
      std::string comm_path =
          "/proc/" + std::to_string(engine.get_pid()) + "/comm";
      std::ifstream comm_file(comm_path);
      if (comm_file)
        std::getline(comm_file, target_process_name);
    }

    // --- FIXED PART (Always visible) ---
    auto fixed_part = vbox(
        {hbox({text(" ◉ ") | color(hp ? C_GREEN : C_RED),
               text(hp ? std::to_string(engine.get_pid()) : "OFFLINE") |
                   color(hp ? C_GREEN : C_RED) | bold,
               filler(),
               text(main_tab == 0   ? "[ADR]"
                    : main_tab == 1 ? "[MAP]"
                    : main_tab == 2 ? "[CG]"
                    : main_tab == 3 ? "[WCH]"
                    : main_tab == 4 ? "[PTR]"
                    : main_tab == 5 ? "[ASM]"
                    : main_tab == 6 ? "[STR]"
                    : main_tab == 7 ? "[SET]"
                                    : "[HEX]") |
                   color(C_ACCENT2) | bold}) |
             borderDouble | color(C_ACCENT),
         hbox({mk_tab(0, "A"), mk_tab(1, "M"), mk_tab(2, "C"), mk_tab(3, "W"),
               mk_tab(4, "P"), mk_tab(5, "D"), mk_tab(6, "S"), mk_tab(7, "⚙"), mk_tab(8, "H")}) |
             hcenter | borderLight});

    Elements sb;
    sb.push_back(
        vbox(
            text(" SCAN ") | bold | color(C_ACCENT2) | hcenter,
            text(" Results: " + std::to_string(scanner.get_results().size())) |
                color(C_ACCENT),
            hbox(text(" Type: ") | color(C_DIM),
                 text(VALUE_TYPE_NAMES[selected_value_type_idx]) |
                     color(C_CYAN) | bold),
            hbox(text(" Mode: ") | color(C_DIM),
                 text(SCAN_TYPE_NAMES[selected_scan_type_idx]) | color(C_CYAN)),
            hbox(text(" Align: ") | color(C_DIM),
                 text(scanner.aligned_scan ? "ON (4-byte)" : "OFF (byte)") |
                     color(scanner.aligned_scan ? C_GREEN : C_ORANGE)),
            separatorLight() | color(C_DIM),
            scanner.is_scanning()
                ? vbox(
                      text(" Scanning... ") | color(C_YELLOW) | blink,
                      gauge(scanner.get_progress()) | color(C_ACCENT) |
                          borderLight | size(HEIGHT, EQUAL, 3),
                      text(" " +
                           std::to_string((int)(scanner.get_progress() * 100)) +
                           "%") |
                          hcenter | color(C_DIM))
                : text(" Idle") | color(C_DIM),
            separatorLight() | color(C_DIM),
            // Active filter indicators
            !filter_module_input.empty()
              ? hbox({text(" MOD:") | color(C_DIM), text(filter_module_input) | color(C_ORANGE) | bold})
              : text(" Module: All") | color(C_DIM),
            filter_show_changed_only
              ? text(" ● Changed Only") | color(C_RED)
              : text(" Changed: Show All") | color(C_DIM),
            text(" [T]type [Y]mode [L]align [F6]filter") | color(C_DIM)) |
        borderLight);
    sb.push_back(vbox(text(" ACTIONS ") | bold | color(C_ACCENT) | hcenter,
                      clickable(" F2 First Scan", C_FG),
                      clickable(" Ctrl+Alt+U Unknown Scan", C_YELLOW),
                      clickable(" F7 Next Scan", C_FG),
                      clickable(" F8 Clear Results", C_FG),
                      clickable(" F6 Filter Results", C_CYAN),
                      clickable(" X  Export JSON", C_GREEN),
                      clickable(" W  Write Value", C_ORANGE),
                      clickable(" G  Go-to Addr", C_YELLOW),
                      clickable(" B  Build CallGraph", C_GREEN),
                      clickable(" P  Pointer Scan", C_YELLOW),
                      clickable(" A  Add to Watch", C_ACCENT),
                      clickable(" E  Ghidra Export", C_ACCENT2),
                      clickable(" N  Auto-Attach", C_ACCENT),
                      clickable(" R  Rec/Stop Rec",
                                scanner.recording_active ? C_RED : C_DIM),
                      clickable(" V  Playback Rec", C_DIM),
                      clickable(" Z  Set Breakpoint", C_YELLOW),
                      separatorLight() | color(C_DIM),
                      clickable(" Ctrl+S Save Table", C_YELLOW),
                      clickable(" Ctrl+O Load Table", C_YELLOW),
                      clickable(" Ctrl+P Save PtrMap", C_YELLOW),
                      separatorLight() | color(C_DIM),
                      clickable(" F10 Speedhack", C_YELLOW),
                      clickable(" F11 Pause/Resume",
                                engine.is_paused() ? C_RED : C_ACCENT),
                      clickable(" F12 Kill Process", C_RED),
                      clickable(" F5 Freeze Value", C_CYAN),
                      clickable(" F3 Hex/Asm Toggle", C_DIM),
                      clickable(" F4 Attach PID", C_DIM),
                      clickable(" F1 Help Menu", C_DIM),
                      clickable(" Q  Quit Tool", C_RED)) |
                 borderLight | reflect(actions_box));

    // ── Config info panel ───────────────────────────────────────────────
    if (config.show_tips) {
      const char *theme_names[] = {"Dark", "Neon", "Gruvbox", "Light"};
      int tidx = std::clamp((int)config.theme, 0, 3);
      sb.push_back(
          vbox(
              text(" ⚙ CONFIG ") | bold | color(C_DIM) | hcenter,
              hbox(text(" Theme: ") | color(C_DIM),
                   text(theme_names[tidx]) | color(C_ACCENT) | bold),
              hbox(text(" Align: ") | color(C_DIM),
                   text(config.aligned_scan_default ? "ON" : "OFF") |
                       color(config.aligned_scan_default ? C_GREEN : C_ORANGE)),
              text(" Ctrl+Alt+W: reset config") | color(C_DIM)) |
          borderLight);
    }

    if (tracked_address && !categorized_results.empty()) {
      CategorizedAddress ri = {};
      if (selected_result_idx < (int)categorized_results.size())
        ri = categorized_results[selected_result_idx];
      std::ostringstream sa, so;
      sa << "0x" << std::hex << std::uppercase << tracked_address;
      so << "0x" << std::hex << std::uppercase
         << (tracked_address - ri.base_addr);
      bool frz = frozen_addresses.count(tracked_address);

      bool is_mappable =
          (ri.module_name != "[stack]" && ri.module_name != "[heap]" &&
           ri.module_name != "[anonymous]" && !ri.module_name.empty());

      Elements info;
      info.push_back(text(" SELECTED ") | bold | color(C_ACCENT2) | hcenter);
      info.push_back(text(" " + sa.str()) | color(C_CYAN) | hcenter);
      info.push_back(
          hbox(text(" Mod: ") | color(C_DIM),
               text(ri.module_name) | color(is_mappable ? C_GREEN : C_RED)));
      info.push_back(hbox(text(" Off: ") | color(C_DIM),
                          text(so.str()) | color(C_ACCENT)));
      info.push_back(hbox(text(" Typ: ") | color(C_DIM),
                          text(valueTypeName(scanner.get_value_type())) |
                              color(C_ORANGE)));
      info.push_back(hbox(text(" Val: ") | color(C_DIM),
                          text(scanner.read_value_str(tracked_address)) |
                              color(C_GREEN) | bold));
      info.push_back(
          hbox(text(" Frz: ") | color(C_DIM),
               text(frz ? "❄YES" : "NO") | color(frz ? C_CYAN : C_DIM)));
      info.push_back(separatorLight() | color(C_DIM));

      if (is_mappable) {
        uintptr_t ghidra_addr = ghidra_image_base + ri.file_offset +
                                (tracked_address - ri.base_addr);
        std::ostringstream sga;
        sga << "0x" << std::hex << std::uppercase << ghidra_addr;
        std::string ghidra_str = sga.str();

        {
          std::ofstream gf("/tmp/ghidra_addr.txt");
          gf << ghidra_str << "\n";
          gf << "# offset=" << so.str() << " module=" << ri.module_name << "\n";
          gf << "# base=0x" << std::hex << ghidra_image_base << "\n";
        }

        info.push_back(separatorLight() | color(C_DIM));
        info.push_back(text(" ► GHIDRA ADDR ") | bold |
                       bgcolor(Color::RGB(40, 30, 0)) | color(C_YELLOW) |
                       hcenter);
        info.push_back(text(" " + ghidra_str) | color(C_YELLOW) | bold |
                       hcenter);
        info.push_back(separatorLight() | color(C_DIM));
        info.push_back(hbox(text(" off: ") | color(C_DIM),
                            text(so.str()) | color(C_ORANGE) | bold));
        info.push_back(
            hbox(text(" base:") | color(C_DIM),
                 text("0x" + hex_str(ghidra_image_base)) | color(C_DIM)));
        info.push_back(text(" → cat /tmp/ghidra_addr.txt") | color(C_DIM));
      } else {
        info.push_back(separatorLight() | color(C_DIM));
        info.push_back(text(" ⚠ DYNAMIC REGION ") | color(C_RED) | bold |
                       hcenter);
        info.push_back(text(" Stack/Heap/Anon ") | dim | hcenter);
        info.push_back(text(" No static addr ") | dim | hcenter);
      }
      sb.push_back(vbox(std::move(info)) | borderLight);
    }
    sb.push_back(hbox(text(" F9 ") | bold | color(C_ACCENT),
                      text(" Set Ghidra Base") | dim));
    if (sidebar_scroll > 0)
      sb.push_back(text(" ↑ scrolled (wheel to navigate) ") | color(C_DIM) |
                   dim);
    sb.push_back(filler());

    // Apply sidebar scroll
    int skip = std::min(sidebar_scroll, std::max(0, (int)sb.size() - 2));
    Elements sb_visible(sb.begin() + skip, sb.end());

    return vbox(std::move(sb_visible));
  });

  // ──────────────────────────────────────────────────────────────────
  // MAIN LAYOUT
  // ──────────────────────────────────────────────────────────────────
  auto main_layout = Renderer([&]() -> Element {
    bool hp = engine.get_pid() != -1;

    auto mk_tab_btn = [&](int idx, const std::string &label) {
      bool sel = (main_tab == idx);
      auto content = text(sel ? " ● " + label : "   " + label);
      return content | (sel ? bold : dim) |
             color(sel ? Color::White : Color::RGB(150, 150, 160)) |
             (sel ? bgcolor(Color::RGB(40, 45, 70)) : bgcolor(Color::Default)) |
             borderLight | color(sel ? C_ACCENT : C_DIM);
    };

    auto header = vbox(
        hbox(
             text(" ❖  ") | color(C_ACCENT) | bold,
             text("IxeRam") | bold | color(Color::White),
             filler(),
             hbox(
                 text(" ✦ PROCESS: ") | color(C_DIM),
                 text(hp ? target_process_name : "OFFLINE") |
                     color(hp ? C_GREEN : C_RED) | bold,
                 hp ? (Element)(text(" [" + std::to_string(engine.get_pid()) + "]") | color(C_DIM)) : (Element)filler()
             ) | borderLight | color(C_DIM),
             filler(),
             text(std::to_string(scanner.get_results().size()) + " results ") | color(C_ACCENT2) | bold,
             hbox(
                   btn_toggle_sidebar->Render() | color(show_sidebar ? C_GREEN : C_DIM),
                   btn_toggle_right->Render() | color(show_right_panel ? C_ACCENT : C_DIM),
                   btn_toggle_log->Render() | color(show_log_panel ? C_YELLOW : C_DIM)
             ) | borderLight
        ) | bgcolor(Color::RGB(10, 10, 20)) |
               borderLight,

        hbox(text(" ⚡ TABS: ") | bold | color(C_ACCENT2),
                mk_tab_btn(0, "ADR"), mk_tab_btn(1, "MAP"), mk_tab_btn(2, "CG "),
                mk_tab_btn(3, "WCH"), mk_tab_btn(4, "PTR"), mk_tab_btn(5, "ASM"),
                mk_tab_btn(6, "STR"), mk_tab_btn(7, "SET"), mk_tab_btn(8, "HEX"), filler(), text(" [TAB] CYCLE ") | dim
        ) | bgcolor(Color::RGB(15, 15, 25)) | borderLight);

    Element center;
    if (main_tab == 0) {
      center =
          vbox(window(hbox(text(" ◈ ADDRESSES "), filler(),
                           text(hide_suspicious_low ? "[FILTERED]" : "[ALL]") |
                               color(C_DIM)),
                      address_tab->Render()) |
                   flex,
               window(hbox(text(" ◈ WATCHLIST (Saved) "), filler(),
                           text(std::to_string(watchlist.size()) + " items") |
                               color(C_DIM)),
                      watch_tab->Render()) |
                    flex,
                show_log_panel ? window(text(" ◈ LOG "), log_view->Render()) |
                    size(HEIGHT, EQUAL, 6) : filler() | size(HEIGHT, EQUAL, 0)
               ) |
          flex;
    } else if (main_tab == 1) {
      center = vbox(window(hbox(text(" ◈ MEMORY MAP "), filler(),
                                text(std::to_string(map_entries.size()) +
                                     " regions") |
                                    color(C_DIM)),
                           memmap_tab->Render()) |
                        flex,
                    show_log_panel ? window(text(" ◈ LOG "), log_view->Render()) |
                        size(HEIGHT, EQUAL, 8) : filler() | size(HEIGHT, EQUAL, 0)) |
               flex;
    } else if (main_tab == 2) {
      center = vbox(window(hbox(text(" ◈ CALL GRAPH "), filler(),
                                text("[B]build [E]export") | color(C_DIM)),
                           callgraph_tab->Render()) |
                        flex,
                    show_log_panel ? window(text(" ◈ LOG "), log_view->Render()) |
                        size(HEIGHT, EQUAL, 8) : filler() | size(HEIGHT, EQUAL, 0)) |
               flex;
    } else if (main_tab == 3) {
      center =
          vbox(window(hbox(text(" ◈ WATCHLIST "), filler(),
                           text(std::to_string(watchlist.size()) + " items") |
                               color(C_DIM)),
                      watch_tab->Render()) |
                   flex,
               show_log_panel ? window(text(" ◈ LOG "), log_view->Render()) |
                   size(HEIGHT, EQUAL, 8) : filler() | size(HEIGHT, EQUAL, 0)) |
          flex;
    } else if (main_tab == 4) {
      center =
          vbox(window(hbox(text(" ◈ POINTER SCAN "), filler(),
                           text(std::to_string(ptr_results.size()) + " paths") |
                               color(C_DIM)),
                      ptr_tab->Render()) |
                   flex,
               show_log_panel ? window(text(" ◈ LOG "), log_view->Render()) |
                   size(HEIGHT, EQUAL, 8) : filler() | size(HEIGHT, EQUAL, 0)) |
          flex;
    } else if (main_tab == 5) {
      center =
          vbox(window(hbox(text(" ◈ DISASSEMBLER / MEMORY VIEWER "), filler(),
                           text("[SPACE]patch [ENTER]jump [BACKSPACE]back") |
                               color(C_DIM)),
                      disasm_tab_r->Render()) |
                   flex,
               show_log_panel ? window(text(" ◈ LOG "), log_view->Render()) |
                   size(HEIGHT, EQUAL, 8) : filler() | size(HEIGHT, EQUAL, 0)) |
          flex;
    } else if (main_tab == 6) {
      center = vbox(window(hbox(text(" ◈ STRUCTURE DISSECTOR "), filler()),
                           struct_tab_r->Render()) |
                        flex,
                    show_log_panel ? window(text(" ◈ LOG "), log_view->Render()) |
                        size(HEIGHT, EQUAL, 8) : filler() | size(HEIGHT, EQUAL, 0)) |
               flex;
    } else if (main_tab == 8) {
        center = vbox(window(hbox(text(" ◈ HEX EDITOR "), filler(), text(hex_str(hex_editor_base)) | color(C_DIM)),
                           hex_editor_tab_r->Render()) | flex,
                     show_log_panel ? window(text(" ◈ LOG "), log_view->Render()) |
                         size(HEIGHT, EQUAL, 8) : filler() | size(HEIGHT, EQUAL, 0)) |
               flex;
    } else {
      center = vbox(window(hbox(text(" ◈ SETTINGS "), filler()),
                           settings_tab->Render()) |
                         flex,
                     show_log_panel ? window(text(" ◈ LOG "), log_view->Render()) |
                         size(HEIGHT, EQUAL, 8) : filler() | size(HEIGHT, EQUAL, 0)) |
               flex;
    }

    Elements main_hbox;
    if (show_sidebar) {
        main_hbox.push_back(sidebar_view->Render() | size(WIDTH, EQUAL, 32));
        main_hbox.push_back(separator() | color(C_DIM));
    }
    main_hbox.push_back(center);
    if (show_right_panel) {
        main_hbox.push_back(separator() | color(C_DIM));
        main_hbox.push_back(vbox(Elements{
                       window(text(show_disasm ? " ◈ DISASM " : " ◈ HEX "),
                                 hex_view->Render()) |
                           flex,
                       window(text(" ◈ GRAPH "), graph_view->Render()) |
                           size(HEIGHT, EQUAL, 14)
                       }) | flex);
    }


    auto footer =
        hbox(text(" F2:Scan") | color(C_GREEN), text("|") | color(C_DIM),
             text("F7:Next") | color(C_ACCENT), text("|") | color(C_DIM),
             text("F8:Clr") | color(C_RED), text("|") | color(C_DIM),
             text("W:Write") | color(C_ORANGE), text("|") | color(C_DIM),
             text("G:Goto") | color(C_YELLOW), text("|") | color(C_DIM),
             text("B:CG") | color(C_GREEN), text("|") | color(C_DIM),
             text("X:JSON") | color(C_GREEN), text("|") | color(C_DIM),
             text("E:Ghidra") | color(C_ACCENT2), text("|") | color(C_DIM),
             text("F5:Frz") | color(C_CYAN), text("|") | color(C_DIM),
             text("N:AttachName") | color(C_ACCENT), text("|") | color(C_DIM),
             scanner.recording_active ? (text("⏺ REC") | color(C_RED) | blink)
                                      : (text("R:Rec") | color(C_DIM)),
             text("|") | color(C_DIM), text("Ctrl+S:Save") | color(C_YELLOW),
             text("|") | color(C_DIM), text("Tab:Tab") | color(C_DIM),
             text("|") | color(C_DIM), text("Q:Quit") | color(C_RED), filler(),
             text(" IxeRam — Internium Entertainment ") | color(C_DIM)) |
        bgcolor(Color::RGB(12, 12, 22));

    return vbox(header,

                hbox(std::move(main_hbox)) | flex,
                footer) |
           bgcolor(C_BG) | color(C_FG);

  });

  // ──────────────────────────────────────────────────────────────────
  // MODALS
  // ──────────────────────────────────────────────────────────────────
  auto patch_modal_r = Renderer(input_patch_hex, [&] {
    std::ostringstream ss;
    ss << "0x" << std::hex << std::uppercase << patch_addr;
    return vbox(text(" ◈ PATCH INSTRUCTION ") | bold | color(C_RED) | hcenter,
                separatorDouble(), text(" Addr: " + ss.str()) | color(C_CYAN),
                text(" Enter hex bytes (e.g. 90 90): ") | color(C_DIM),
                input_patch_hex->Render() | borderLight |
                    size(WIDTH, EQUAL, 34),
                text(" [Enter] Apply  [Esc] Cancel ") | dim | hcenter) |
           size(WIDTH, EQUAL, 38) | borderDouble |
           bgcolor(Color::RGB(20, 10, 10)) | center;
  });

  auto context_menu_r = Renderer([&] {
    auto item = [&](const std::string &label, Color c) {
      return text(" " + label + " ") | color(c);
    };

    Elements items;
    items.push_back(text(" ◈ CONTEXT ") | bold | color(C_ACCENT2) | hcenter);
    items.push_back(separatorLight());
    items.push_back(item("Copy Address", C_FG));
    items.push_back(item("Copy Value", C_FG));
    items.push_back(item("Copy Offset", C_FG));
    items.push_back(item("Copy Module", C_FG));
    items.push_back(separatorLight() | color(C_DIM));
    items.push_back(item("Add to Watchlist", C_ACCENT));
    items.push_back(item("Patch Memory", C_ORANGE));
    items.push_back(separatorLight() | color(C_DIM));
    items.push_back(item(" [ Close Menu ] ", C_RED));

    return vbox(std::move(items)) | borderDouble |
           bgcolor(Color::RGB(15, 15, 30)) | size(WIDTH, EQUAL, 22);
  });

  auto help_modal = Renderer([&] {
    auto row = [&](const std::string &k, const std::string &d,
                   Color c = Color::White) {
      return hbox(text(" " + k + " ") | color(c) | bold |
                      size(WIDTH, EQUAL, 14),
                  text(d));
    };
    Elements h;
    h.push_back(text(" ⬡ HELP — IxeRam ") | bold | color(C_ACCENT) | hcenter);
    h.push_back(separatorDouble());
    h.push_back(row("Tab",
                    "Switch Tabs (Addr, Map, CG, Watch, Ptr, Disasm, Struct, Settings)",
                    C_CYAN));

    h.push_back(row("F3", "Toggle Hex / Disassembler view", C_CYAN));
    h.push_back(row("F4", "Attach to Process PID", C_FG));
    h.push_back(row("F5", "Freeze / Unfreeze selected address", C_CYAN));
    h.push_back(row("F6", "Toggle filtering of low addresses", C_FG));
    h.push_back(row("F10", "Speedhack multiplier settings", C_YELLOW));
    h.push_back(row("F11", "Pause / Resume process (SIGSTOP/CONT)", C_ACCENT));
    h.push_back(row("F12 / K", "Kill process (with confirmation)", C_RED));
    h.push_back(row("Q", "Quit tool", C_RED));
    h.push_back(separatorLight());
    h.push_back(text(" ── Config & Session ──────────────────────────────") | color(C_DIM));
    h.push_back(row("Ctrl+S", "Save cheat table (session file)", C_YELLOW));
    h.push_back(row("Ctrl+O", "Load cheat table (session file)", C_YELLOW));
    h.push_back(row("Ctrl+Alt+W","Reset config → wizard on next launch", C_ORANGE));
    h.push_back(text(" Config: ~/.config/ixeram/config.ini") | color(C_DIM));
    h.push_back(separatorLight());
    h.push_back(
        text(" GHIDRA: Copy Offset -> Ghidra G (Go To) -> imageBase+offset") |
        color(C_YELLOW));
    h.push_back(text(" 'E' exports a Python script for Ghidra Script Manager") |
                color(C_DIM));
    h.push_back(text(" Developed by: mystergaif (IxeRam project)") |
                color(C_CYAN));
    h.push_back(separatorDouble());
    h.push_back(text(" ESC to close ") | hcenter | color(C_DIM));
    return vbox(std::move(h)) | size(WIDTH, EQUAL, 68) | borderDouble |
           bgcolor(Color::RGB(10, 10, 18)) | center;
  });


  auto attach_modal = Renderer(input_pid, [&] {
    return vbox(text(" ◈ ATTACH PID ") | bold | color(C_ACCENT) | hcenter,
                separatorLight(),
                hbox(text(" PID: ") | color(C_DIM), input_pid->Render() | flex),
                separatorLight(),
                text(" [Enter]attach  [ESC]cancel ") | color(C_DIM) | hcenter) |
           size(WIDTH, EQUAL, 38) | borderDouble |
           bgcolor(Color::RGB(10, 10, 20)) | center;
  });

  auto scan_modal_r = Renderer(input_scan, [&] {
    return vbox(
               text(" ◈ FIRST SCAN ") | bold | color(C_GREEN) | hcenter,
               hbox(text(" Type: ") | color(C_DIM),
                    text(VALUE_TYPE_NAMES[selected_value_type_idx]) |
                        color(C_ACCENT2) | bold),
               hbox(text(" Val:  ") | color(C_DIM),
                    input_scan->Render() | flex),
               separatorLight(),
               vbox({
                   hbox({text(" [ENTER] ") | color(C_GREEN), text("Exact Search")}) | hcenter,
                   hbox({text(" [U] ") | color(C_YELLOW), text("Unknown Initial Value")}) | hcenter,
               }),
               separatorLight(),
               text(
                   " (change type from main menu)    [ESC]cancel ") |
                   color(C_DIM) | hcenter) |
           size(WIDTH, EQUAL, 50) | borderDouble |
           bgcolor(Color::RGB(10, 12, 18)) | center;
  });

  auto next_modal_r = Renderer(input_next, [&] {
    bool nv = (selected_scan_type_idx <= 3 || selected_scan_type_idx == 5 ||
               selected_scan_type_idx == 7);
    Elements e;
    e.push_back(text(" ◈ NEXT SCAN ") | bold | color(C_ACCENT) | hcenter);
    e.push_back(hbox(text(" Mode: ") | color(C_DIM),
                     text(SCAN_TYPE_NAMES[selected_scan_type_idx]) |
                         color(C_ACCENT2) | bold));
    e.push_back(
        hbox(text(" Type: ") | color(C_DIM),
             text(VALUE_TYPE_NAMES[selected_value_type_idx]) | color(C_CYAN)));
    e.push_back(separatorLight());
    if (nv)
      e.push_back(
          hbox(text(" Val:  ") | color(C_DIM), input_next->Render() | flex));
    else
      e.push_back(text(" (no value needed) ") | color(C_DIM));
    e.push_back(separatorLight());
    e.push_back(
        text(" (change mode from main menu)  [Enter]scan  [ESC]cancel ") |
        color(C_DIM) | hcenter);
    return vbox(std::move(e)) | size(WIDTH, EQUAL, 52) | borderDouble |
           bgcolor(Color::RGB(10, 12, 18)) | center;
  });

  auto write_modal_r = Renderer(input_write, [&] {
    std::ostringstream sa;
    sa << "0x" << std::hex << std::uppercase << tracked_address;
    return vbox(text(" ◈ WRITE VALUE ") | bold | color(C_ORANGE) | hcenter,
                hbox(text(" Addr: ") | color(C_DIM),
                     text(sa.str()) | color(C_CYAN) | bold),
                hbox(text(" Type: ") | color(C_DIM),
                     text(VALUE_TYPE_NAMES[selected_value_type_idx]) |
                         color(C_ACCENT2)),
                hbox(text(" Curr: ") | color(C_DIM),
                     text(scanner.read_value_str(tracked_address)) |
                         color(C_GREEN) | bold),
                separatorLight(),
                hbox(text(" New:  ") | color(C_DIM),
                     input_write->Render() | flex),
                separatorLight(),
                text(" [Enter]write  [ESC]cancel ") | color(C_DIM) | hcenter) |
           size(WIDTH, EQUAL, 46) | borderDouble |
           bgcolor(Color::RGB(14, 10, 10)) | center;
  });

  auto goto_modal_r = Renderer(input_goto_a, [&] {
    return vbox(text(" ◈ GO TO ADDRESS ") | bold | color(C_YELLOW) | hcenter,
                separatorLight(),
                hbox(text(" Addr: ") | color(C_DIM),
                     input_goto_a->Render() | flex),
                separatorLight(),
                text(" hex (0x...) or decimal ") | color(C_DIM) | hcenter,
                text(" [Enter]jump  [ESC]cancel ") | color(C_DIM) | hcenter) |
           size(WIDTH, EQUAL, 48) | borderDouble |
           bgcolor(Color::RGB(14, 14, 10)) | center;
  });

  auto type_modal_r = Renderer([&] {
    Elements it;
    for (int i = 0; i < VALUE_TYPE_COUNT; ++i) {
      auto r =
          hbox(text("  " + std::string(VALUE_TYPE_NAMES[i]) + "  ") | bold |
                   color(i == selected_value_type_idx ? Color::White : C_FG),
               text("(" + std::to_string((int)valueTypeSize(VALUE_TYPES[i])) +
                    "B)") |
                   color(C_DIM));
      it.push_back(i == selected_value_type_idx ? r | bgcolor(C_SEL_BG) : r);
    }
    return vbox(text(" ◈ VALUE TYPE ") | bold | color(C_ACCENT2) | hcenter,
                separatorLight(), vbox(std::move(it)), separatorLight(),
                text(" ↑/↓ [Enter]pick [ESC]close ") | color(C_DIM) | hcenter) |
           reflect(type_box) | size(WIDTH, EQUAL, 30) | borderDouble |
           bgcolor(Color::RGB(10, 10, 18)) | center;
  });

  auto stype_modal_r = Renderer([&] {
    Elements it;
    for (int i = 0; i < SCAN_TYPE_COUNT; ++i) {
      auto r = text("  " + std::string(SCAN_TYPE_NAMES[i]) + "  ") | bold;
      it.push_back(i == selected_scan_type_idx
                       ? r | bgcolor(C_SEL_BG) | color(Color::White)
                       : r | color(C_FG));
    }
    return vbox(text(" ◈ SCAN MODE ") | bold | color(C_ACCENT2) | hcenter,
                separatorLight(), vbox(std::move(it)), separatorLight(),
                text(" ↑/↓ [Enter]pick [ESC]close ") | color(C_DIM) | hcenter) |
           reflect(stype_box) | size(WIDTH, EQUAL, 28) | borderDouble |
           bgcolor(Color::RGB(10, 10, 18)) | center;
  });

  auto ghidra_base_modal_r = Renderer(input_ghidra_base, [&] {
    return vbox(text(" ◈ SET GHIDRA IMAGE BASE ") | bold | color(C_ACCENT) |
                    hcenter,
                separatorLight(),
                hbox(text(" Base: ") | color(C_DIM),
                     input_ghidra_base->Render() | flex),
                separatorLight(),
                text(" current: 0x" + hex_str(ghidra_image_base)) |
                    color(C_DIM) | hcenter,
                text(" [Enter]set  [ESC]cancel ") | color(C_DIM) | hcenter) |
           size(WIDTH, EQUAL, 48) | borderDouble |
           bgcolor(Color::RGB(10, 10, 20)) | center;
  });

  auto watch_modal_r = Renderer(input_watch_desc, [&] {
    return vbox(text(" ◈ ADD TO WATCHLIST ") | bold | color(C_ACCENT) | hcenter,
                separatorLight(),
                hbox(text(" Addr: ") | color(C_DIM),
                     text(hex_str(tracked_address)) | color(C_CYAN)),
                hbox(text(" Desc: ") | color(C_DIM),
                     input_watch_desc->Render() | flex),
                separatorLight(),
                text(" [Enter]add  [ESC]cancel ") | color(C_DIM) | hcenter) |
           size(WIDTH, EQUAL, 46) | borderDouble |
           bgcolor(Color::RGB(10, 15, 20)) | center;
  });

  // ─── Filter Modal ────────────────────────────────────────────────────
  auto input_filter_mod = Input(&filter_module_input, "e.g. libgame or [heap]...");
  auto filter_modal_r = Renderer(input_filter_mod, [&] {
    // Module list panel
    Elements mod_items;
    mod_items.push_back(
      text("  (All modules)") |
        (filter_module_input.empty() ? bgcolor(C_SEL_BG) | color(Color::White) : color(C_DIM)));
    for (int i = 0; i < (int)filter_module_list.size(); ++i) {
      const auto& m = filter_module_list[i];
      std::string mn = m;
      if (mn.size() > 36) mn = mn.substr(0, 33) + "...";
      bool sel = (m == filter_module_input);
      auto row = hbox({text("  "), text(mn)}) |
                 (sel ? bgcolor(C_SEL_BG) | color(Color::White) : color(C_FG));
      mod_items.push_back(row);
    }

    return vbox(
             text(" ◈ FILTER RESULTS ") | bold | color(C_CYAN) | hcenter,
             separatorLight() | color(C_DIM),
             // Module search
             vbox(
               text(" Module Filter:") | color(C_DIM) | bold,
               hbox(text(" ▶ ") | color(C_ACCENT),
                    input_filter_mod->Render() | flex),
               text(" (type to search, or use ↑/↓ to pick from list)") | color(C_DIM)
             ),
             separatorLight() | color(C_DIM),
             // Module list
             text(" Known Modules:") | color(C_DIM) | bold,
             vbox(std::move(mod_items)) |
               size(HEIGHT, LESS_THAN, 12) | frame,
             separatorLight() | color(C_DIM),
             // Changed-only toggle
             hbox({
               text(" Changed-Only: ") | color(C_DIM),
               filter_show_changed_only
                 ? text("[●  ON ]") | bold | color(C_RED)
                 : text("[○ OFF ]") | color(C_DIM),
               text("  [C] toggle") | color(C_DIM)
             }),
             separatorLight() | color(C_DIM),
             hbox({
               text(" [ENTER] apply ") | color(C_GREEN),
               text(" [DEL] clear filter ") | color(C_ORANGE),
               text(" [ESC] cancel ") | color(C_DIM)
             }) | hcenter
           ) |
           size(WIDTH, EQUAL, 52) | borderDouble |
           bgcolor(Color::RGB(8, 12, 22)) | center;
  });

  auto speedhack_modal_r = Renderer(input_speedhack, [&] {
    return vbox(text(" ◈ SPEEDHACK ") | bold | color(C_YELLOW) | hcenter,
                separatorLight(),
                hbox(text(" Speed: ") | color(C_DIM),
                     input_speedhack->Render() | flex),
                separatorLight(),
                text(" e.g. 0.5 (slow) or 2.0 (fast) ") | color(C_DIM) |
                    hcenter,
                text(" [Enter]apply  [ESC]cancel ") | color(C_DIM) | hcenter) |
           size(WIDTH, EQUAL, 40) | borderDouble |
           bgcolor(Color::RGB(20, 20, 10)) | center;
  });

  auto root = Renderer([&] {
    std::lock_guard<std::recursive_mutex> lock(ui_mutex);
    Element base = main_layout->Render();
    if (show_type_modal)
      base = dbox(base, type_modal_r->Render() | center);
    if (show_scan_type_modal)
      base = dbox(base, stype_modal_r->Render() | center);
    if (show_help_modal)
      base = dbox(base, help_modal->Render() | center);
    if (show_attach_modal)
      base = dbox(base, attach_modal->Render() | center);
    if (show_scan_modal)
      base = dbox(base, scan_modal_r->Render() | center);
    if (show_next_scan_modal)
      base = dbox(base, next_modal_r->Render() | center);
    if (show_write_modal)
      base = dbox(base, write_modal_r->Render() | center);
    if (show_goto_modal)
      base = dbox(base, goto_modal_r->Render() | center);
    if (show_ghidra_base_modal)
      base = dbox(base, ghidra_base_modal_r->Render() | center);
    if (show_watch_modal)
      base = dbox(base, watch_modal_r->Render() | center);
    if (show_patch_modal)
      base = dbox(base, patch_modal_r->Render() | center);
    if (show_speedhack_modal)
      base = dbox(base, speedhack_modal_r->Render() | center);
    if (show_kill_modal) {
      auto modal =
          vbox({text("⚠️  KILL PROCESS confirmation") | bold | color(C_RED) |
                    hcenter,
                separator(),
                text("Are you sure you want to KILL PID " +
                     std::to_string(engine.get_pid()) + "?") |
                    hcenter,
                text("This action is irreversible.") | hcenter | color(C_DIM),
                separator(),
                hbox({text(" [ENTER] Confirm kill ") | color(C_RED) | bold,
                      filler(), text(" [ESC] Cancel ") | color(C_DIM)})}) |
          borderRounded | color(C_FG) | size(WIDTH, GREATER_THAN, 40);
      base = dbox(base, modal | center);
    }
    // Auto-attach modal (#10)
    if (show_autoattach_modal) {
      auto modal =
          vbox(text(" ◈ AUTO-ATTACH BY NAME ") | bold | color(C_ACCENT) |
                   hcenter,
               separatorLight(),
               text(" Enter process name to wait for:") | color(C_DIM),
               hbox(text(" Name: ") | color(C_DIM),
                    input_autoattach->Render() | flex),
               separatorLight(),
               text(" Waits up to 15 sec for process to appear ") |
                    color(C_DIM) | hcenter,
               text(" [Enter]start [ESC]cancel ") | color(C_DIM) | hcenter) |
          size(WIDTH, EQUAL, 50) | borderDouble |
          bgcolor(Color::RGB(10, 10, 22)) | center;
      base = dbox(base, modal);
    }
    // Cheat Table save modal (#2)
    if (show_save_ct_modal) {
      auto modal =
          vbox(text(" ◈ SAVE CHEAT TABLE ") | bold | color(C_YELLOW) | hcenter,
               separatorLight(),
               hbox(text(" Path: ") | color(C_DIM),
                    input_ct_path->Render() | flex),
               separatorLight(),
               text(" Saves watchlist, frozen addrs, pointer map ") |
                   color(C_DIM) | hcenter,
               text(" [Enter]save [ESC]cancel ") | color(C_DIM) | hcenter) |
          size(WIDTH, EQUAL, 50) | borderDouble |
          bgcolor(Color::RGB(20, 20, 5)) | center;
      base = dbox(base, modal);
    }
    // Cheat Table load modal (#2)
    if (show_load_ct_modal) {
      auto modal =
          vbox(text(" ◈ LOAD CHEAT TABLE ") | bold | color(C_YELLOW) | hcenter,
               separatorLight(),
               hbox(text(" Path: ") | color(C_DIM),
                    input_ct_path->Render() | flex),
               separatorLight(),
               text(" Loads watchlist, frozen addrs, pointer map ") |
                   color(C_DIM) | hcenter,
               text(" [Enter]load [ESC]cancel ") | color(C_DIM) | hcenter) |
          size(WIDTH, EQUAL, 50) | borderDouble |
          bgcolor(Color::RGB(20, 20, 5)) | center;
      base = dbox(base, modal);
    }
    // Record modal (#8)
    if (show_record_modal) {
      size_t nsamp = scanner.value_recording.size();
      auto modal =
          vbox(text(" ◈ RECORD / PLAYBACK ") | bold | color(C_RED) | hcenter,
               separatorLight(),
               hbox(text(" Addr: ") | color(C_DIM),
                    text(hex_str(tracked_address)) | color(C_CYAN) | bold),
               hbox(text(" Samples: ") | color(C_DIM),
                    text(std::to_string(nsamp)) | color(C_GREEN) | bold),
               hbox(text(" Status: ") | color(C_DIM),
                    scanner.recording_active
                        ? (text("⏺ RECORDING") | color(C_RED) | blink)
                    : record_playing ? (text("▶ PLAYING") | color(C_GREEN))
                                     : (text("⏹ IDLE") | color(C_DIM))),
               separatorLight(),
               text(" [R] Start/Stop Record  [V] Start Playback ") |
                    color(C_DIM) | hcenter,
               text(" [ESC] Close ") | color(C_DIM) | hcenter) |
          size(WIDTH, EQUAL, 48) | borderDouble |
          bgcolor(Color::RGB(22, 8, 8)) | center;
      base = dbox(base, modal);
    }
    // Filter modal
    if (show_filter_modal)
      base = dbox(base, filter_modal_r->Render() | center);

    // Hardware Breakpoint Modal
    if (show_hw_bp_modal)
      base = dbox(base, hw_bp_modal_r->Render() | center);

    // Header buttons event catch
    base = dbox(base, header_buttons->Render() | size(HEIGHT, EQUAL, 0) | size(WIDTH, EQUAL, 0));

    if (show_context_menu) {
      int x = std::max(0, std::min(term_cols() - 25, context_menu_x));
      int y = std::max(0, std::min(term_rows() - 15, context_menu_y));

      Elements v_rows;
      v_rows.push_back(filler() | size(HEIGHT, EQUAL, y));

      Elements h_cols;
      h_cols.push_back(filler() | size(WIDTH, EQUAL, x));
      h_cols.push_back(context_menu_r->Render());
      h_cols.push_back(filler());

      v_rows.push_back(hbox(std::move(h_cols)));
      v_rows.push_back(filler());

      Elements d_layers;
      d_layers.push_back(base);
      d_layers.push_back(vbox(std::move(v_rows)));
      base = dbox(std::move(d_layers));
    }
    return base;
  });

  // ──────────────────────────────────────────────────────────────────
  // EVENT HANDLER
  // ──────────────────────────────────────────────────────────────────
  auto component = CatchEvent(root, [&](Event ev) -> bool {
    std::lock_guard<std::recursive_mutex> lock(ui_mutex);
    if (ev.is_mouse()) {
      auto &m = ev.mouse();
      bool is_left_click =
          (m.button == Mouse::Left && m.motion == Mouse::Pressed);
      bool is_right_click =
          (m.button == Mouse::Right && m.motion == Mouse::Pressed);

      if (is_left_click)
        show_context_menu = false;

      // ─── Result List Interaction ───────────────────────────────────────
      if (main_tab == 0 && result_list_box.Contain(m.x, m.y) &&
          (is_left_click || is_right_click)) {
        int row = m.y - result_list_box.y_min - 3;
        if (row >= 0 && !categorized_results.empty()) {
          int visible = std::max(5, term_rows() - 18);
          int win_start = std::max(0, selected_result_idx - visible / 2);
          int clicked = win_start + row;
          if (clicked >= 0 && clicked < (int)categorized_results.size()) {
            selected_result_idx = clicked;
            tracked_address = categorized_results[clicked].addr;
            if (is_right_click) {
              std::string val = "N/A";
              if (cached_address_values.count(tracked_address))
                val = cached_address_values[tracked_address];
              show_ctx_at(m.x, m.y, tracked_address, val,
                          categorized_results[clicked].module_name,
                          tracked_address -
                              categorized_results[clicked].base_addr);
            }
            return true;
          }
        }
      }

      // ─── Memory Map Interaction ────────────────────────────────────────
      if (main_tab == 1 && map_list_box.Contain(m.x, m.y) &&
          (is_left_click || is_right_click)) {
        int row = m.y - map_list_box.y_min - 2;
        if (row >= 0 && !map_entries.empty()) {
          int visible = std::max(5, term_rows() - 14);
          int win_start = std::max(0, selected_map_idx - visible / 2);
          int clicked = win_start + row;
          if (clicked >= 0 && clicked < (int)map_entries.size()) {
            selected_map_idx = clicked;
            tracked_address = map_entries[clicked].start;
            if (is_right_click) {
              show_ctx_at(m.x, m.y, tracked_address, "0x" + hex_str(tracked_address),
                          map_entries[clicked].module, 0);
            }
            return true;
          }
        }
      }

      // ─── Sidebar Actions ───────────────────────────────────────────────
      if (actions_box.Contain(m.x, m.y) && is_left_click) {
        int row = m.y - actions_box.y_min - 1 + sidebar_scroll;
        if (row == 0) screen.PostEvent(Event::F2);
        else if (row == 1) screen.PostEvent(Event::Character('\x15')); // Ctrl+U
        else if (row == 2) screen.PostEvent(Event::F7);
        else if (row == 3) screen.PostEvent(Event::F8);
        else if (row == 4) screen.PostEvent(Event::F6);
        else if (row == 5) screen.PostEvent(Event::Character('x'));
        else if (row == 6) screen.PostEvent(Event::Character('w'));
        else if (row == 7) screen.PostEvent(Event::Character('g'));
        else if (row == 8) screen.PostEvent(Event::Character('b'));
        else if (row == 9) screen.PostEvent(Event::Character('p'));
        else if (row == 10) screen.PostEvent(Event::Character('a'));
        else if (row == 11) screen.PostEvent(Event::Character('e'));
        else if (row == 12) screen.PostEvent(Event::Character('n'));
        else if (row == 13) screen.PostEvent(Event::Character('r'));
        else if (row == 14) screen.PostEvent(Event::Character('v'));
        else if (row == 15) screen.PostEvent(Event::Character('z'));
        else if (row == 17) screen.PostEvent(Event::Character('\x13')); // Ctrl+S
        else if (row == 18) screen.PostEvent(Event::Character('\x0f')); // Ctrl+O
        else if (row == 19) screen.PostEvent(Event::Character('\x10')); // Ctrl+P
        else if (row == 21) screen.PostEvent(Event::F10);
        else if (row == 22) screen.PostEvent(Event::F11);
        else if (row == 23) screen.PostEvent(Event::F12);
        else if (row == 24) screen.PostEvent(Event::F5);
        else if (row == 25) screen.PostEvent(Event::F3);
        else if (row == 26) screen.PostEvent(Event::F4);
        else if (row == 27) screen.PostEvent(Event::F1);
        else if (row == 28) screen.PostEvent(Event::Character('q'));
        return true;
      }

      // ─── Tabs Interaction ──────────────────────────────────────────────
      if (tabs_box.Contain(m.x, m.y) && is_left_click) {
        int tab_x = m.x - tabs_box.x_min;
        int tidx = tab_x / 4;
        if (tidx >= 0 && tidx < 8) {
          main_tab = tidx;
          return true;
        }
      }

      // Panel Toggles in header (top right area)
      if (is_left_click && m.y <= 2 && m.x > term_cols() - 18) {
          if (m.x > term_cols() - 6) show_log_panel = !show_log_panel;
          else if (m.x > term_cols() - 12) show_right_panel = !show_right_panel;
          else if (m.x > term_cols() - 18) show_sidebar = !show_sidebar;
          return true;
      }


      // ─── Modal Context Menu click ──────────────────────────────────────
      if (show_context_menu && is_left_click) {
        if (m.x >= context_menu_x && m.x < context_menu_x + 22 &&
            m.y >= context_menu_y && m.y < context_menu_y + 11) {
          int row = m.y - context_menu_y - 2;
          if (row == 0)
            copy_to_clipboard(hex_str(context_menu_addr));
          else if (row == 1)
            copy_to_clipboard(context_menu_val);
          else if (row == 2)
            copy_to_clipboard(hex_str(context_menu_offset));
          else if (row == 3)
            copy_to_clipboard(context_menu_mod);
          else if (row == 5)
            show_watch_modal = true;
          else if (row == 6) {
            patch_addr = context_menu_addr;
            show_patch_modal = true;
          } else if (row == 8)
            show_context_menu = false;

          show_context_menu = false;
          return true;
        }
      }

      bool is_wheel_up = (m.button == Mouse::WheelUp);
      bool is_wheel_down = (m.button == Mouse::WheelDown);
      if (!is_wheel_up && !is_wheel_down) {
        return false;
      }

      int sidebar_width = 32;
      int right_panel_w = term_cols() / 4; // approx
      // Determine which panel the mouse is over by X coordinate
      bool over_sidebar = (m.x < sidebar_width);
      // Right panel: roughly last ~right_panel_w columns (after center)
      // Log panel: bottom ~10 rows (y > term_rows() - 11)
      bool over_log = (m.y > term_rows() - 11);
      bool over_right =
          (!over_sidebar && !over_log && m.x > term_cols() - right_panel_w - 2);
      bool over_center = (!over_sidebar && !over_log && !over_right);

      if (over_sidebar) {
        // Sidebar scroll
        if (is_wheel_up)
          sidebar_scroll = std::max(0, sidebar_scroll - 1);
        if (is_wheel_down)
          sidebar_scroll++;
        return true;
      }
      if (over_log) {
        // Log scroll (0=bottom, positive=up)
        if (is_wheel_up)
          log_scroll = std::min(log_scroll + 3, (int)logs.size());
        if (is_wheel_down)
          log_scroll = std::max(0, log_scroll - 3);
        return true;
      }
      if (over_center || over_right) {
        int delta = is_wheel_up ? -1 : 1;
        if (main_tab == 0) {
          selected_result_idx =
              std::max(0, std::min((int)categorized_results.size() - 1,
                                   selected_result_idx + delta));
        } else if (main_tab == 1) {
          selected_map_idx = std::max(0, std::min((int)map_entries.size() - 1,
                                                  selected_map_idx + delta));
        } else if (main_tab == 2) {
          selected_cg_idx = std::max(
              0, std::min((int)call_graph.size() - 1, selected_cg_idx + delta));
        } else if (main_tab == 3) {
          selected_watch_idx =
              std::max(0, std::min((int)watchlist.size() - 1,
                                   selected_watch_idx + delta));
        } else if (main_tab == 4) {
          selected_ptr_idx = std::max(0, std::min((int)ptr_results.size() - 1,
                                                  selected_ptr_idx + delta));
        } else if (main_tab == 5) {
          selected_disasm_idx =
              std::max(0, std::min((int)disasm_lines.size() - 1,
                                   selected_disasm_idx + delta));
        } else if (main_tab == 6 || main_tab == 7) {
          center_scroll = std::max(0, center_scroll + delta);
        }

        return true;
      }
      return false;
    }
    // ─────────────────────────────────────────────────────────────────
    if (show_type_modal) {
      if (ev == Event::Escape) {
        show_type_modal = false;
        return true;
      }
      if (ev == Event::ArrowDown) {
        if (selected_value_type_idx < VALUE_TYPE_COUNT - 1)
          selected_value_type_idx++;
        return true;
      }
      if (ev == Event::ArrowUp) {
        if (selected_value_type_idx > 0)
          selected_value_type_idx--;
        return true;
      }
      if (ev == Event::Return) {
        add_log("Type → " +
                std::string(VALUE_TYPE_NAMES[selected_value_type_idx]));
        show_type_modal = false;
        return true;
      }
      return true;
    }
    if (show_scan_type_modal) {
      if (ev == Event::Escape) {
        show_scan_type_modal = false;
        return true;
      }
      if (ev == Event::ArrowDown) {
        if (selected_scan_type_idx < SCAN_TYPE_COUNT - 1)
          selected_scan_type_idx++;
        return true;
      }
      if (ev == Event::ArrowUp) {
        if (selected_scan_type_idx > 0)
          selected_scan_type_idx--;
        return true;
      }
      if (ev == Event::Return) {
        add_log("Mode → " +
                std::string(SCAN_TYPE_NAMES[selected_scan_type_idx]));
        show_scan_type_modal = false;
        return true;
      }
      return true;
    }
    if (show_help_modal) {
      if (ev == Event::Escape || ev == Event::F1) {
        show_help_modal = false;
      }
      return true;
    }
    if (show_attach_modal) {
      if (ev == Event::Return) {
        do_attach_lambda();
        return true;
      }
      if (ev == Event::Escape) {
        show_attach_modal = false;
        return true;
      }
      return input_pid->OnEvent(ev);
    }
    if (show_scan_modal) {
      if (ev == Event::Return) {
        do_initial_scan_lambda();
        return true;
      }
      if (ev == Event::Character('u') || ev == Event::Character('U')) {
        do_unknown_scan_lambda();
        return true;
      }
      if (ev == Event::Escape) {
        show_scan_modal = false;
        return true;
      }
      return input_scan->OnEvent(ev);
    }
    if (show_next_scan_modal) {
      if (ev == Event::Return) {
        do_next_scan_lambda();
        return true;
      }
      if (ev == Event::Escape) {
        show_next_scan_modal = false;
        return true;
      }
      return input_next->OnEvent(ev);
    }
    if (show_write_modal) {
      if (ev == Event::Return) {
        do_write_lambda();
        return true;
      }
      if (ev == Event::Escape) {
        show_write_modal = false;
        write_value_input.clear();
        return true;
      }
      return input_write->OnEvent(ev);
    }
    if (show_goto_modal) {
      if (ev == Event::Return) {
        do_goto_lambda();
        return true;
      }
      if (ev == Event::Escape) {
        show_goto_modal = false;
        goto_addr_input.clear();
        return true;
      }
      return input_goto_a->OnEvent(ev);
    }
    if (show_ghidra_base_modal) {
      if (ev == Event::Return) {
        do_set_ghidra_base_logic();
        return true;
      }
      if (ev == Event::Escape) {
        show_ghidra_base_modal = false;
        ghidra_base_input.clear();
        return true;
      }
      return input_ghidra_base->OnEvent(ev);
    }
    if (show_watch_modal) {
      if (ev == Event::Return) {
        do_add_watch_lambda();
        return true;
      }
      if (ev == Event::Escape) {
        show_watch_modal = false;
        return true;
      }
      return input_watch_desc->OnEvent(ev);
    }
    if (show_patch_modal) {
      if (ev == Event::Return) {
        if (!patch_hex_input.empty()) {
          std::vector<uint8_t> bytes;
          bool is_hex = true;
          std::stringstream ss(patch_hex_input);
          std::string bs;
          while (ss >> bs) {
            try {
              if (bs.find_first_not_of("0123456789abcdefABCDEF") !=
                  std::string::npos) {
                is_hex = false;
                break;
              }
              bytes.push_back((uint8_t)std::stoul(bs, nullptr, 16));
            } catch (...) {
              is_hex = false;
              break;
            }
          }

          if (!is_hex) {
            bytes.clear();
            ks_engine *ks;
            ks_err err = ks_open(KS_ARCH_X86, KS_MODE_64, &ks);
            if (err == KS_ERR_OK) {
              unsigned char *encode;
              size_t size;
              size_t count;
              if (ks_asm(ks, patch_hex_input.c_str(), patch_addr, &encode,
                         &size, &count) == KS_ERR_OK) {
                for (size_t i = 0; i < size; i++)
                  bytes.push_back(encode[i]);
                ks_free(encode);
              } else {
                add_log("✗ Assembler error: " +
                        std::string(ks_strerror(ks_errno(ks))));
              }
              ks_close(ks);
            }
          }

          if (!bytes.empty()) {
            engine.write_memory(patch_addr, bytes.data(), bytes.size());
            add_log("✓ Patched " + std::to_string(bytes.size()) + " bytes at " +
                    hex_str(patch_addr));
          }
        }
        show_patch_modal = false;
        return true;
      }
      if (ev == Event::Escape) {
        show_patch_modal = false;
        return true;
      }
      return input_patch_hex->OnEvent(ev);
    }

    if (show_speedhack_modal) {
      if (ev == Event::Return) {
        try {
          double spd = std::stod(speedhack_input);
          std::string shm_name =
              "/speedhack_" + std::to_string(engine.get_pid());
          int shm_fd = shm_open(shm_name.c_str(), O_CREAT | O_RDWR, 0666);
          if (shm_fd != -1) {
            ftruncate(shm_fd, sizeof(double));
            void *ptr = mmap(NULL, sizeof(double), PROT_READ | PROT_WRITE,
                             MAP_SHARED, shm_fd, 0);
            if (ptr != MAP_FAILED) {
              memcpy(ptr, &spd, sizeof(double));
              munmap(ptr, sizeof(double));
            }
            close(shm_fd);
            std::string shm_full_path =
                "/dev/shm/speedhack_" + std::to_string(engine.get_pid());
            chmod(shm_full_path.c_str(), 0666);
            add_log("✓ Speedhack set to " + speedhack_input + "x");
            add_log(
                "  (Run app with: LD_PRELOAD=./build/libspeedhack.so ./app)");
          } else {
            add_log("✗ Speedhack error: SHM open failed");
          }
        } catch (...) {
          add_log("✗ Invalid speed");
        }
        show_speedhack_modal = false;
        return true;
      }
      if (ev == Event::Escape) {
        show_speedhack_modal = false;
        return true;
      }
      return input_speedhack->OnEvent(ev);
    }

    if (show_kill_modal) {
      if (ev == Event::Return) {
        if (engine.kill_process()) {
          add_log("💀 Process Killed (SIGKILL)");
        } else {
          add_log("✗ Failed to kill process");
        }
        show_kill_modal = false;
        return true;
      }
      if (ev == Event::Escape) {
        show_kill_modal = false;
        return true;
      }
      return true;
    }

    if (ev == Event::Character('k') || ev == Event::Character('K') ||
        ev == Event::F12) {
      if (engine.get_pid() > 0) {
        show_kill_modal = true;
      } else {
        add_log("✗ No process attached");
      }
      return true;
    }

    if (show_autoattach_modal) {
      if (ev == Event::Return) {
        do_autoattach_lambda();
        return true;
      }
      if (ev == Event::Escape) {
        show_autoattach_modal = false;
        return true;
      }
      return input_autoattach->OnEvent(ev);
    }
    if (show_save_ct_modal) {
      if (ev == Event::Return) {
        save_cheat_table(ct_path_input);
        show_save_ct_modal = false;
        return true;
      }
      if (ev == Event::Escape) {
        show_save_ct_modal = false;
        return true;
      }
      return input_ct_path->OnEvent(ev);
    }
    if (show_load_ct_modal) {
      if (ev == Event::Return) {
        load_cheat_table(ct_path_input);
        show_load_ct_modal = false;
        return true;
      }
      if (ev == Event::Escape) {
        show_load_ct_modal = false;
        return true;
      }
      return input_ct_path->OnEvent(ev);
    }
    if (show_record_modal) {
      if (ev == Event::Character('r') || ev == Event::Character('R')) {
        do_record_toggle();
        return true;
      }
      if (ev == Event::Character('v') || ev == Event::Character('V')) {
        do_start_playback();
        return true;
      }
      if (ev == Event::Escape) {
        show_record_modal = false;
        return true;
      }
      return true;
    }
    // Filter modal event handling
    if (show_filter_modal) {
      if (ev == Event::Escape) {
        show_filter_modal = false;
        return true;
      }
      if (ev == Event::Return) {
        show_filter_modal = false;
        add_log("Filter: module=" +
                (filter_module_input.empty() ? "(all)" : filter_module_input) +
                (filter_show_changed_only ? " | changed-only" : ""));
        return true;
      }
      // DEL clears module filter
      if (ev == Event::Delete) {
        filter_module_input.clear();
        filter_module_sel_idx = 0;
        return true;
      }
      // C toggles changed-only
      if (ev == Event::Character('c') || ev == Event::Character('C')) {
        filter_show_changed_only = !filter_show_changed_only;
        return true;
      }
      // Arrow up/down to navigate module list
      if (ev == Event::ArrowDown) {
        if (filter_module_sel_idx < (int)filter_module_list.size()) {
          filter_module_sel_idx++;
          if (filter_module_sel_idx == 0)
            filter_module_input.clear();
          else if (filter_module_sel_idx <= (int)filter_module_list.size())
            filter_module_input = filter_module_list[filter_module_sel_idx - 1];
        }
        return true;
      }
      if (ev == Event::ArrowUp) {
        if (filter_module_sel_idx > 0) {
          filter_module_sel_idx--;
          if (filter_module_sel_idx == 0)
            filter_module_input.clear();
          else
            filter_module_input = filter_module_list[filter_module_sel_idx - 1];
        }
        return true;
      }
      return input_filter_mod->OnEvent(ev);
    }

    if (show_hw_bp_modal) {
      if (ev == Event::Escape) {
        show_hw_bp_modal = false;
        return true;
      }
      if (ev.is_character()) {
        char c = ev.character()[0];
        if (c >= '0' && c <= '3') {
          int slot = c - '0';
          if (engine.clear_hw_breakpoint(slot)) {
            hw_ui_slots[slot].active = false;
            add_log("✓ Cleared HW Slot " + std::to_string(slot));
          }
          return true;
        }
        if (c == 'h' || c == 'H') {
           // Find first free slot
           int slot = -1;
           for(int i=0; i<4; i++) if(!hw_ui_slots[i].active) { slot=i; break; }
           if (slot != -1 && tracked_address) {
             if (engine.set_hw_breakpoint(slot, tracked_address, HWBreakpointType::ReadWrite, HWBreakpointSize::Byte4)) {
               hw_ui_slots[slot] = {true, tracked_address, 3, 3};
               add_log("⬤ HW Watchpoint (R/W 4b) set at " + hex_str(tracked_address));
             }
           } else add_log("✗ No free HW slots or no address");
           return true;
        }
      }
      return true;
    }
    if (ev == Event::Character('q') || ev == Event::Character('Q')) {
      engine.detach();
      screen.ExitLoopClosure()();
      return true;
    }
    // Ctrl+Alt+W — reset config (re-runs wizard on next launch)
    if (ev == Event::Special("\x1b\x17")) { // ESC + Ctrl+W
      // Delete config file so wizard reappears on next run
      std::remove(Config::config_path().c_str());
      add_log("✓ Config reset — wizard will run on next launch");
      add_log("  Restart IxeRam to configure again.");
      return true;
    }
    if (ev == Event::F1) {
      show_help_modal = true;
      return true;
    }
    if (ev == Event::F2) {
      scan_value.clear();
      show_scan_modal = true;
      return true;
    }
    // Ctrl+Alt+U (often esc + ctrl+u in terminals)
    if (ev == Event::Special("\x1b\x15")) {
      do_unknown_scan();
      return true;
    }
    if (ev == Event::F3) {
      show_disasm = !show_disasm;
      add_log(show_disasm ? "→Disasm" : "→Hex");
      return true;
    }
    if (ev == Event::F4) {
      pid_input.clear();
      show_attach_modal = true;
      return true;
    }
    if (ev == Event::F5) {
      do_toggle_freeze(tracked_address);
      return true;
    }
    if (ev == Event::F6) {
      // Open filter modal (build module list first)
      {
        std::set<std::string> mods;
        std::lock_guard<std::recursive_mutex> lock(ui_mutex);
        for (const auto& r : categorized_results)
          if (!r.module_name.empty()) mods.insert(r.module_name);
        filter_module_list.assign(mods.begin(), mods.end());
      }
      filter_module_sel_idx = 0;
      show_filter_modal = true;
      return true;
    }
    if (ev == Event::F7) {
      next_scan_value.clear();
      show_next_scan_modal = true;
      return true;
    }
    if (ev == Event::F8) {
      do_clear_results();
      return true;
    }
    if (ev == Event::F9) {
      ghidra_base_input = hex_str(ghidra_image_base);
      show_ghidra_base_modal = true;
      return true;
    }
    if (ev == Event::F10) {
      show_speedhack_modal = true;
      return true;
    }
    if (ev == Event::F11) {
      do_pause_resume();
      return true;
    }
    // Moved to modal section above
    if (ev == Event::Character('w') || ev == Event::Character('W')) {
      if (tracked_address) {
        write_value_input.clear();
        show_write_modal = true;
      } else
        add_log("✗ No address");
      return true;
    }
    if (ev == Event::Character('g') || ev == Event::Character('G')) {
      goto_addr_input.clear();
      show_goto_modal = true;
      return true;
    }
    if (ev == Event::Character('t') || ev == Event::Character('T')) {
      show_type_modal = true;
      return true;
    }
    if (ev == Event::Character('y') || ev == Event::Character('Y')) {
      show_scan_type_modal = true;
      return true;
    }
    if (ev == Event::Character('b') || ev == Event::Character('B')) {
      do_build_cg(tracked_address);
      return true;
    }
    if (ev == Event::Character('e') || ev == Event::Character('E')) {
      do_ghidra_export("/tmp/ghidra_" + std::to_string(engine.get_pid()) + ".py");
      return true;
    }
    if (ev == Event::Character('x') || ev == Event::Character('X')) {
      do_export_results_json("ixeram_results.json");
      return true;
    }
    if (ev == Event::Character('a') || ev == Event::Character('A')) {
      if (tracked_address) {
        watch_desc_input.clear();
        show_watch_modal = true;
      }
      return true;
    }
    if (ev == Event::Character('p') || ev == Event::Character('P')) {
      do_ptr_scan();
      return true;
    }
    if (ev == Event::Character('l') || ev == Event::Character('L')) {
      scanner.aligned_scan = !scanner.aligned_scan;
      add_log(
          "Aligned Scan: " + std::string(scanner.aligned_scan ? "ON" : "OFF") +
          (scanner.aligned_scan ? " (Faster, 4-byte aligned)"
                                : " (Slow, byte-by-byte)"));
      return true;
    }
    if (ev == Event::Tab || ev == Event::Character('\t')) {
      main_tab = (main_tab + 1) % 9;
      return true;
    }

    // Panel Toggles Alt+1/2/3
    if (ev == Event::Special("\x1b" "1")) { show_sidebar = !show_sidebar; return true; }
    if (ev == Event::Special("\x1b" "2")) { show_right_panel = !show_right_panel; return true; }
    if (ev == Event::Special("\x1b" "3")) { show_log_panel = !show_log_panel; return true; }

    
    // Settings Tab specific logic
    if (main_tab == 7) {
      // (existing settings logic)
      if (ev == Event::Return || ev == Event::Character('s') || ev == Event::Character('S')) {
          config.theme = static_cast<ColorTheme>(settings_theme_idx);
          try {
              config.log_max_lines = std::stoi(settings_log_max_lines_input);
          } catch(...) {}
          try {
            if (settings_ghidra_base_input.size() > 2 && settings_ghidra_base_input[0] == '0' &&
                (settings_ghidra_base_input[1] == 'x' || settings_ghidra_base_input[1] == 'X'))
              config.ghidra_image_base = std::stoull(settings_ghidra_base_input, nullptr, 16);
            else
              config.ghidra_image_base = std::stoull(settings_ghidra_base_input, nullptr, 10);
            ghidra_image_base = config.ghidra_image_base;
          } catch(...) {}

          if (Config::save(config)) {
              add_log("✓ Settings Saved Successfully!");
              add_log("  Theme will apply on next restart.");
          } else {
              add_log("✗ Failed to save settings to " + Config::config_path());
          }
          return true;
      }
      return settings_container->OnEvent(ev);
    }

    if (main_tab == 8) {
        if (ev == Event::ArrowDown) {
            hex_editor_cursor_idx = (hex_editor_cursor_idx + 16) % (hex_editor_rows * 16);
            return true;
        }
        if (ev == Event::ArrowUp) {
            hex_editor_cursor_idx = (hex_editor_cursor_idx - 16 + (hex_editor_rows * 16)) % (hex_editor_rows * 16);
            return true;
        }
        if (ev == Event::ArrowRight) {
            hex_editor_cursor_idx = (hex_editor_cursor_idx + 1) % (hex_editor_rows * 16);
            return true;
        }
        if (ev == Event::ArrowLeft) {
            hex_editor_cursor_idx = (hex_editor_cursor_idx - 1 + (hex_editor_rows * 16)) % (hex_editor_rows * 16);
            return true;
        }
        if (ev == Event::PageDown) { hex_editor_base += 16 * hex_editor_rows; return true; }
        if (ev == Event::PageUp) { hex_editor_base -= 16 * hex_editor_rows; return true; }
        if (ev == Event::Character('g') || ev == Event::Character('G')) {
            show_goto_modal = true;
            return true;
        }
        if (ev == Event::Character('z') || ev == Event::Character('Z')) {
            show_hw_bp_modal = true;
            return true;
        }
        
        // Hex Editing
        if (ev.is_character()) {
            std::string c = ev.character();
            if (isxdigit(c[0])) {
                int val = isdigit(c[0]) ? (c[0] - '0') : (tolower(c[0]) - 'a' + 10);
                uintptr_t addr = hex_editor_base + hex_editor_cursor_idx;
                uint8_t b;
                engine.read_memory(addr, &b, 1);
                if (!hex_editor_edit_nibble) {
                    b = (uint8_t)((val << 4) | (b & 0x0F));
                    hex_editor_edit_nibble = true;
                } else {
                    b = (uint8_t)((b & 0xF0) | val);
                    hex_editor_edit_nibble = false;
                    hex_editor_cursor_idx = (hex_editor_cursor_idx + 1) % (hex_editor_rows * 16);
                }
                do_hex_edit(addr, b);
                return true;
            }
        }
    }



    if (ev == Event::Character('n') || ev == Event::Character('N')) {
      autoattach_name_input.clear();
      show_autoattach_modal = true;
      return true;
    }
    if (ev == Event::Character('r') || ev == Event::Character('R')) {
      do_record_toggle();
      return true;
    }
    if (ev == Event::Character('v') || ev == Event::Character('V')) {
      if (!record_playing)
        do_start_playback();
      else {
        record_playing = false;
        add_log("⏹ Playback stopped");
      }
      return true;
    }
    // Ctrl+S: Save cheat table
    if (ev == Event::Special({19})) { // Ctrl+S = ASCII 19
      show_save_ct_modal = true;
      return true;
    }
    // Ctrl+O: Load cheat table
    if (ev == Event::Special({15})) { // Ctrl+O = ASCII 15
      show_load_ct_modal = true;
      return true;
    }
    // Ctrl+P: Save pointer map
    if (ev == Event::Special({16})) { // Ctrl+P = ASCII 16
      std::string ppath =
          "pointers_" + std::to_string(engine.get_pid()) + ".ptr";
      if (scanner.save_ptr_results(ppath))
        add_log("✓ Pointer map saved → " + ppath);
      else
        add_log("✗ Failed to save pointer map");
      return true;
    }
    // Ctrl+B: Wait for breakpoint (non-blocking)
    if (ev == Event::Special({2})) { // Ctrl+B = ASCII 2
      do_wait_bp();
      return true;
    }
    // Disassembler specific keys
    if (main_tab == 5 && !disasm_lines.empty()) {
      if (ev == Event::Character(' ')) {
        patch_addr = disasm_lines[selected_disasm_idx].addr;
        patch_hex_input.clear();
        show_patch_modal = true;
        return true;
      }
      if (ev == Event::Return) {
        auto ops = disasm_lines[selected_disasm_idx].ops;
        if (ops.find("0x") == 0) {
          try {
            uintptr_t target = std::stoull(ops, nullptr, 16);
            disasm_history.push_back(tracked_address);
            tracked_address = target;
            selected_disasm_idx = 0;
            add_log("Followed jump to " + hex_str(target));
          } catch (...) {
          }
        }
        return true;
      }
      if (ev == Event::Backspace) {
        if (!disasm_history.empty()) {
          tracked_address = disasm_history.back();
          disasm_history.pop_back();
          selected_disasm_idx = 0;
          add_log("Returned to " + hex_str(tracked_address));
        }
        return true;
      }
      // #7: Set breakpoint
      if (ev == Event::Character('z') || ev == Event::Character('Z')) {
        do_set_bp(tracked_address);
        return true;
      }
      // Toggle access log tab
      if (ev == Event::Character('u') || ev == Event::Character('U')) {
        show_access_tab = !show_access_tab;
        add_log(show_access_tab ? "→ Access Log" : "→ Disasm");
        return true;
      }
    }

    if (ev == Event::ArrowDown) {
      if (main_tab == 0) {
        if (selected_result_idx < (int)categorized_results.size() - 1)
          selected_result_idx++;
      } else if (main_tab == 1) {
        if (selected_map_idx < (int)map_entries.size() - 1)
          selected_map_idx++;
      } else if (main_tab == 2) {
        if (selected_cg_idx < (int)call_graph.size() - 1)
          selected_cg_idx++;
      } else if (main_tab == 3) {
        if (selected_watch_idx < (int)watchlist.size() - 1)
          selected_watch_idx++;
      } else if (main_tab == 4) {
        if (selected_ptr_idx < (int)ptr_results.size() - 1)
          selected_ptr_idx++;
      } else if (main_tab == 5) {
        if (selected_disasm_idx < (int)disasm_lines.size() - 1)
          selected_disasm_idx++;
        else if (selected_disasm_idx > 0 && tracked_address > 0 &&
                 !disasm_lines.empty()) {
          tracked_address += disasm_lines[0].size;
        }
      }
      return true;
    }
    if (ev == Event::ArrowUp) {
      if (main_tab == 0) {
        if (selected_result_idx > 0)
          selected_result_idx--;
      } else if (main_tab == 1) {
        if (selected_map_idx > 0)
          selected_map_idx--;
      } else if (main_tab == 2) {
        if (selected_cg_idx > 0)
          selected_cg_idx--;
      } else if (main_tab == 3) {
        if (selected_watch_idx > 0)
          selected_watch_idx--;
      } else if (main_tab == 4) {
        if (selected_ptr_idx > 0)
          selected_ptr_idx--;
      } else if (main_tab == 5) {
        if (selected_disasm_idx > 0)
          selected_disasm_idx--;
        else if (selected_disasm_idx == 0 && tracked_address > 0) {
          // Slide window back by a heuristic amount or by checking previous
          // instructions Subtracting 4 is a safe compromise for x64 to find
          // *some* valid boundary
          tracked_address = (tracked_address > 8) ? tracked_address - 8 : 0;
        }
      }
      return true;
    }
    return false;
  });

  std::thread fz(&TUI::freezing_loop, this);
  fz.detach();
  std::thread upd([&] {
    while (true) {
      update_tracking_data();
      if (main_tab == 1)
        update_memory_map();

      // #8: Sample value for recording
      if (scanner.recording_active && tracked_address) {
        double cur = read_as_double(tracked_address);
        scanner.value_recording.push_back(
            {std::chrono::steady_clock::now(), cur});
        // Limit to 10 minutes at 150ms = 4000 samples
        if (scanner.value_recording.size() > 4000)
          scanner.value_recording.erase(scanner.value_recording.begin());
      }

      screen.PostEvent(Event::Custom);
      std::this_thread::sleep_for(std::chrono::milliseconds(150));
    }
  });
  upd.detach();

  KittyGraphics::render_logo_placeholder();
  screen.Loop(component);
}

// ─────────────────────────────────────────────────────────────────────
// COMMAND API IMPLEMENTATION
// ─────────────────────────────────────────────────────────────────────

void TUI::do_attach(int pid) {
  try {
    if (engine.attach(pid)) {
      add_log("✓ Attached to PID: " + std::to_string(pid));
      update_memory_map();
    } else {
      add_log("✗ Failed to attach to PID: " + std::to_string(pid));
    }
  } catch (const std::exception &e) {
    add_log("✗ Error: " + std::string(e.what()));
  }
}

void TUI::do_auto_attach(const std::string &name) {
  if (name.empty())
    return;
  add_log("⌚ Waiting for process: " + name + "...");
  std::thread([&, name] {
    pid_t pid = MemoryEngine::wait_for_process(name, 15000);
    if (pid != -1) {
      if (engine.attach(pid)) {
        std::lock_guard<std::recursive_mutex> lock(ui_mutex);
        target_process_name = name;
        add_log("✓ Auto-attached to '" + name +
                "' PID=" + std::to_string(pid));
        update_memory_map();
      } else
        add_log("✗ Auto-attach failed for PID " + std::to_string(pid));
    } else {
      add_log("✗ Process '" + name + "' not found (timeout)");
    }
    screen.PostEvent(ftxui::Event::Custom);
  }).detach();
}

void TUI::do_first_scan(const std::string &val) {
  if (scanner.is_scanning())
    return;
  scanner.set_scanning(true);
  add_log("⚡ Background Scan Started...");
  int vt_idx = selected_value_type_idx;
  std::thread([&, vt_idx, val] {
    scanner.initial_scan(VALUE_TYPES[vt_idx], val);
    add_log("✓ Scan [" + std::string(VALUE_TYPE_NAMES[vt_idx]) + "] → " +
            std::to_string(scanner.get_results().size()) + " results");
    screen.PostEvent(ftxui::Event::Custom);
  }).detach();
}

void TUI::do_next_scan(const std::string &val) {
  if (scanner.is_scanning())
    return;
  scanner.set_scanning(true);
  add_log("⚡ Background Refinement Started...");
  int st_idx = selected_scan_type_idx;
  std::thread([&, st_idx, val] {
    scanner.next_scan(SCAN_TYPES[st_idx], val);
    add_log("✓ Next [" + std::string(SCAN_TYPE_NAMES[st_idx]) + "] → " +
            std::to_string(scanner.get_results().size()) + " results");
    screen.PostEvent(ftxui::Event::Custom);
  }).detach();
}

void TUI::do_unknown_scan() {
  if (scanner.is_scanning())
    return;
  scanner.set_scanning(true);
  add_log("⚡ Background Unknown Scan Started...");
  int vt_idx = selected_value_type_idx;
  std::thread([&, vt_idx] {
    scanner.unknown_initial_scan(VALUE_TYPES[vt_idx]);
    add_log("✓ Unknown Scan [" + std::string(VALUE_TYPE_NAMES[vt_idx]) + "] → " +
            std::to_string(scanner.get_results().size()) + " results");
    screen.PostEvent(ftxui::Event::Custom);
  }).detach();
}

void TUI::do_clear_results() {
  std::lock_guard<std::recursive_mutex> lock(ui_mutex);
  scanner.clear_results();
  categorized_results.clear();
  value_history.clear();
  last_vals_for_color.clear();
  frozen_addresses.clear();
  selected_result_idx = 0;
  tracked_address = 0;
  add_log("✓ Results cleared");
}

void TUI::do_write_memory(uintptr_t addr, const std::string &val) {
  if (addr) {
    if (scanner.write_value(addr, val, VALUE_TYPES[selected_value_type_idx]))
      add_log("✓ Wrote " + val + " → " + hex_str(addr));
    else
      add_log("✗ Write failed");
  }
}

void TUI::do_goto_address(uintptr_t addr) {
  tracked_address = addr;
  hex_dump.resize(128, 0);
  engine.read_memory(addr, hex_dump.data(), 128);
  if (show_disasm)
    update_disasm();
  add_log("→ Jumped to " + hex_str(addr));
}

void TUI::do_toggle_freeze(uintptr_t addr) {
  if (!addr) return;
  std::lock_guard<std::recursive_mutex> lock(ui_mutex);
  if (frozen_addresses.count(addr)) {
    frozen_addresses.erase(addr);
    add_log("✓ Unfrozen " + hex_str(addr));
  } else {
    size_t sz = valueTypeSize(scanner.get_value_type());
    FrozenEntry fe;
    fe.bytes.resize(sz, 0);
    if (engine.read_memory(addr, fe.bytes.data(), sz)) {
      fe.display_val = scanner.read_value_str(addr);
      frozen_addresses[addr] = fe;
      add_log("❄ Frozen " + hex_str(addr) + " = " + fe.display_val);
    } else {
      add_log("✗ Freeze failed: cannot read " + hex_str(addr));
    }
  }
}

void TUI::do_add_watch(uintptr_t addr, const std::string &desc) {
  if (addr) {
    WatchEntry we;
    we.addr = addr;
    we.description = desc.empty() ? "No description" : desc;
    we.type = scanner.get_value_type();
    we.frozen = false;
    watchlist.push_back(std::move(we));
    add_log("✓ Added to Watchlist: " + hex_str(addr));
  }
}

void TUI::do_ptr_scan() {
  if (tracked_address) {
    add_log("Pointer Scan for " + hex_str(tracked_address) + "...");
    std::thread([&, addr = tracked_address] {
        auto res = scanner.find_pointers(addr, 2, 1024);
        std::lock_guard<std::recursive_mutex> lock(ui_mutex);
        ptr_results = res;
        scanner.find_pointers_cache = res;
        add_log("✓ Found " + std::to_string(ptr_results.size()) + " pointer paths");
        main_tab = 4;
        screen.PostEvent(ftxui::Event::Custom);
    }).detach();
  } else {
    add_log("✗ No address selected");
  }
}

void TUI::do_build_cg(uintptr_t addr) {
    if (!addr) {
        add_log("✗ No address for CG");
        return;
    }
    add_log("Building CG from " + hex_str(addr) + "...");
    update_memory_map();
    build_call_graph(addr, cg_max_depth);
    add_log("✓ CG: " + std::to_string(call_graph.size()) + " nodes");
    main_tab = 2;
}

void TUI::do_set_bp(uintptr_t addr) {
  if (!addr) return;
  if (engine.set_breakpoint(addr)) {
    add_log("⬤ Breakpoint set at " + hex_str(addr));
  } else {
    add_log("✗ Failed to set breakpoint (need attach)");
  }
}

void TUI::do_wait_bp() {
  add_log("⌚ Waiting for breakpoint hit...");
  std::thread([&] {
    uintptr_t hit = 0;
    if (engine.wait_breakpoint(hit, 5000)) {
      std::lock_guard<std::mutex> lock(bp_mutex);
      bp_hits.insert(bp_hits.end(), engine.access_records.begin(),
                     engine.access_records.end());
      engine.access_records.clear();
      add_log("⬤ Breakpoint hit at " + hex_str(hit));
      tracked_address = hit;
    } else {
      add_log("✗ No breakpoint hit (timeout)");
    }
    screen.PostEvent(ftxui::Event::Custom);
  }).detach();
}

void TUI::do_record_toggle() {
  if (!scanner.recording_active) {
    scanner.value_recording.clear();
    scanner.recording_active = true;
    record_start = std::chrono::steady_clock::now();
    add_log("⏺ Recording started for " + hex_str(tracked_address));
  } else {
    scanner.recording_active = false;
    add_log("⏹ Recording stopped. " + std::to_string(scanner.value_recording.size()) + " samples");
  }
}

void TUI::do_start_playback() {
  if (scanner.value_recording.empty()) {
    add_log("✗ No recording to play back");
    return;
  }
  record_playing = true;
  record_play_idx = 0;
  add_log("▶ Playing back " + std::to_string(scanner.value_recording.size()) + " samples");
  std::thread([&] {
    while (record_playing && record_play_idx < scanner.value_recording.size()) {
      double v = scanner.value_recording[record_play_idx].value;
      if (tracked_address) {
        float fv = (float)v;
        engine.write_memory(tracked_address, &fv, sizeof(fv));
      }
      ++record_play_idx;
      std::this_thread::sleep_for(std::chrono::milliseconds(16));
      screen.PostEvent(ftxui::Event::Custom);
    }
    record_playing = false;
    add_log("⏹ Playback finished");
    screen.PostEvent(ftxui::Event::Custom);
  }).detach();
}

void TUI::do_hex_edit(uintptr_t addr, uint8_t val) {
    if (engine.write_memory(addr, &val, 1)) {
        // refresh buffer if it's the tracked address region
        if (addr >= tracked_address && addr < tracked_address + 128) {
            engine.read_memory(tracked_address, hex_dump.data(), 128);
        }
    }
}

void TUI::do_set_hw_bp(int slot, uintptr_t addr, HWBreakpointType type, HWBreakpointSize size) {
    if (engine.set_hw_breakpoint(slot, addr, type, size)) {
        hw_ui_slots[slot].active = true;
        hw_ui_slots[slot].addr = addr;
        // map type/size to indices for UI if needed
        add_log("✓ HW BP " + std::to_string(slot) + " set at " + hex_str(addr));
    } else {
        add_log("✗ Failed to set HW BP");
    }
}

void TUI::do_clear_hw_bp(int slot) {
    if (engine.clear_hw_breakpoint(slot)) {
        hw_ui_slots[slot].active = false;
        add_log("✓ HW BP " + std::to_string(slot) + " cleared");
    }
}

void TUI::do_pause_resume() {
  if (engine.get_pid() == -1) {
    add_log("✗ No process attached");
    return;
  }
  if (engine.is_paused()) {
    if (engine.resume_process())
      add_log("▶ Process Resumed");
    else
      add_log("✗ Failed to resume");
  } else {
    if (engine.pause_process())
      add_log("⏸ Process Paused (Frozen)");
    else
      add_log("✗ Failed to pause");
  }
}

void TUI::do_save_ct(const std::string &path) {
    if (save_cheat_table(path)) add_log("💾 Saved session to " + path);
    else add_log("✗ Save failed");
}

void TUI::do_load_ct(const std::string &path) {
    if (load_cheat_table(path)) add_log("📂 Loaded session from " + path);
    else add_log("✗ Load failed");
}

void TUI::do_export_results_json(const std::string &path) {
    std::ofstream out(path);
    if (!out) {
        add_log("✗ Cannot export to " + path);
        return;
    }
    out << "[\n";
    for (size_t i = 0; i < categorized_results.size(); ++i) {
      auto &r = categorized_results[i];
      out << "  {\n"
          << "    \"address\": \"" << hex_str(r.addr) << "\",\n"
          << "    \"module\": \"" << r.module_name << "\",\n"
          << "    \"offset\": \"" << hex_str(r.addr - r.base_addr) << "\"\n"
          << "  }";
      if (i < categorized_results.size() - 1) out << ",";
      out << "\n";
    }
    out << "]\n";
    add_log("✓ Exported " + std::to_string(categorized_results.size()) + " results to " + path);
}

void TUI::do_ghidra_export(const std::string &path) {
    export_ghidra_script(path);
}

void TUI::do_kill_process() {
    if (engine.get_pid() != -1) {
        add_log("☠ Killing process " + std::to_string(engine.get_pid()));
        kill(engine.get_pid(), SIGKILL);
        engine.detach();
    }
}
