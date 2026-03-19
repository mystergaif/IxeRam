// ═══════════════════════════════════════════════════════════════════════════
//  SetupWizard.cpp — IxeRam first-run interactive configuration tour
// ═══════════════════════════════════════════════════════════════════════════
#include "SetupWizard.hpp"

#include "Config.hpp"
#include "ftxui/component/component.hpp"
#include "ftxui/component/component_options.hpp"
#include "ftxui/component/event.hpp"
#include "ftxui/component/screen_interactive.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/screen/color.hpp"

#include <string>
#include <vector>

using namespace ftxui;

// ─────────────────────────────────────────────────────────────────────────────
//  Color palette (constant across wizard)
// ─────────────────────────────────────────────────────────────────────────────
static const auto W_BG      = Color::RGB(8,   8,  18);
static const auto W_FG      = Color::RGB(200, 200, 220);
static const auto W_ACCENT  = Color::RGB(80,  160, 255);
static const auto W_ACCENT2 = Color::RGB(140,  80, 255);
static const auto W_GREEN   = Color::RGB(50,  220, 120);
static const auto W_YELLOW  = Color::RGB(255, 220,  50);
static const auto W_RED     = Color::RGB(255,  70,  70);
static const auto W_DIM     = Color::RGB(90,   90, 110);
static const auto W_SEL_BG  = Color::RGB(25,  40,  80);
static const auto W_BORDER  = Color::RGB(40,  40,  70);
static const auto W_ORANGE  = Color::RGB(255, 160,  50);

// ─────────────────────────────────────────────────────────────────────────────
//  Helper: render a fancy wizard header
// ─────────────────────────────────────────────────────────────────────────────
static Element wizard_header(int step, int total, const std::string &title,
                             const std::string &subtitle) {
  // Step pips
  Elements pips;
  for (int i = 0; i < total; ++i) {
    if (i == step)
      pips.push_back(text(" ◆ ") | color(W_ACCENT) | bold);
    else if (i < step)
      pips.push_back(text(" ◇ ") | color(W_GREEN));
    else
      pips.push_back(text(" ◇ ") | color(W_DIM));
  }
  pips.push_back(filler());
  pips.push_back(text(" Step " + std::to_string(step + 1) + "/" +
                      std::to_string(total) + " ") |
                 color(W_DIM));

  return vbox({
    hbox({
      text("  ⬡ IxeRam") | color(W_ACCENT) | bold,
      text(" — Setup Wizard") | color(W_DIM),
      filler(),
    }),
    separator() | color(W_BORDER),
    hbox(std::move(pips)),
    separator() | color(W_BORDER),
    text("  " + title) | color(W_FG) | bold,
    text("  " + subtitle) | color(W_DIM),
    separator() | color(W_BORDER),
  });
}

// ─────────────────────────────────────────────────────────────────────────────
//  Helper: radio-style option list
// ─────────────────────────────────────────────────────────────────────────────
static Element option_list(const std::vector<std::string> &opts, int selected,
                           const std::vector<std::string> &descs = {}) {
  Elements rows;
  for (int i = 0; i < (int)opts.size(); ++i) {
    bool sel = i == selected;
    std::string bullet = sel ? "  ◆ " : "  ◇ ";
    Element row;
    if (!descs.empty() && i < (int)descs.size()) {
      row = hbox({
        text(bullet) | color(sel ? W_ACCENT : W_DIM),
        text(opts[i]) | color(sel ? W_FG : W_DIM) | bold,
        text("  — ") | color(W_DIM),
        text(descs[i]) | color(W_DIM),
      });
    } else {
      row = hbox({
        text(bullet) | color(sel ? W_ACCENT : W_DIM),
        text(opts[i]) | color(sel ? W_FG : W_DIM) | bold,
      });
    }
    if (sel)
      row = row | bgcolor(W_SEL_BG);
    rows.push_back(row);
  }
  return vbox(std::move(rows));
}

// ─────────────────────────────────────────────────────────────────────────────
//  Helper: toggle row
// ─────────────────────────────────────────────────────────────────────────────
static Element toggle_row(const std::string &label, const std::string &desc,
                          bool value, bool selected) {
  auto badge = value ? (text(" ON  ") | color(W_GREEN) | bold |
                        bgcolor(Color::RGB(0, 60, 30)))
                     : (text(" OFF ") | color(W_RED) | bold |
                        bgcolor(Color::RGB(60, 0, 0)));
  auto row = hbox({
    text(selected ? "  ▶ " : "    ") | color(W_ACCENT),
    text(label) | color(selected ? W_FG : W_DIM) | bold,
    filler(),
    badge,
    text("  "),
    text(desc) | color(W_DIM),
    text("  "),
  });
  if (selected)
    row = row | bgcolor(W_SEL_BG);
  return row;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Helper: footer / nav hint
// ─────────────────────────────────────────────────────────────────────────────
static Element nav_footer(bool has_prev, bool is_last) {
  return vbox({
    separator() | color(W_BORDER),
    hbox({
      text("  "),
      has_prev ? (text("[← Prev] ") | color(W_DIM)) : text(""), 
      text("[↑↓] Navigate") | color(W_DIM),
      text("  [Space/Enter] Select") | color(W_DIM),
      filler(),
      is_last ? (text("[ Enter ] Finish & Save ") | color(W_GREEN) | bold)
              : (text("[ Enter ] Next → ") | color(W_ACCENT) | bold),
      text("  [Q] Skip ") | color(W_DIM),
    }),
  });
}

// ─────────────────────────────────────────────────────────────────────────────
//  Preview panel — shows live theme preview colours
// ─────────────────────────────────────────────────────────────────────────────
static Element theme_preview(int theme_idx) {
  // Colors per theme: {bg, fg, accent, green, red}
  struct Pal { Color bg, fg, accent, green, red, yellow; };
  const Pal palettes[] = {
    { Color::RGB(8,8,18),     Color::RGB(200,200,220), Color::RGB(80,160,255),  Color::RGB(50,220,120),  Color::RGB(255,70,70),  Color::RGB(255,220,50)  }, // Dark
    { Color::RGB(0,0,0),      Color::RGB(20,255,80),   Color::RGB(0,255,180),   Color::RGB(0,200,60),    Color::RGB(255,50,80),  Color::RGB(255,220,0)   }, // Neon
    { Color::RGB(29,32,33),   Color::RGB(235,219,178), Color::RGB(131,165,152), Color::RGB(184,187,38),  Color::RGB(251,73,52),  Color::RGB(250,189,47)  }, // Gruvbox
    { Color::RGB(245,245,250),Color::RGB(40,40,60),    Color::RGB(30,100,200),  Color::RGB(0,140,70),    Color::RGB(200,0,0),    Color::RGB(180,120,0)   }, // Light
  };
  const char *names[] = {"Dark", "Neon", "Gruvbox", "Light"};
  const Pal &p = palettes[theme_idx];

  return vbox({
    text("  Preview: ") | color(W_DIM),
    hbox({
      text("  "),
      vbox({
        hbox({ text(" 0x00AFBEEF ") | color(p.accent) | bold,
               text("+0x1C0 ") | color(p.fg),
               text(" game.exe ") | color(p.yellow),
               text("│ ") | color(W_DIM),
               text("42069") | color(p.green) | bold }) | bgcolor(p.bg),
        hbox({ text(" 0x00B0DEAD ") | color(p.fg),
               text("+0x200 ") | color(p.fg),
               text(" heap    ") | color(p.green),
               text("│ ") | color(W_DIM),
               text("0     ") | color(p.red) }) | bgcolor(p.bg),
        hbox({ text(" [F2]Scan  [F4]Attach  [B]CallGraph  [Q]Quit ") | color(p.accent) }) | bgcolor(p.bg),
      }),
    }),
    text("  Theme: " + std::string(names[theme_idx])) | color(W_ACCENT) | bold,
  });
}

// ─────────────────────────────────────────────────────────────────────────────
//  SetupWizard::run()
// ─────────────────────────────────────────────────────────────────────────────
IxeRamConfig SetupWizard::run() {
  IxeRamConfig cfg{};  // Start with defaults
  cfg.first_run = false;

  auto screen = ScreenInteractive::Fullscreen();
  bool finished = false;
  bool skipped  = false;

  // ── Wizard state ─────────────────────────────────────────────────────────
  const int TOTAL_STEPS = 5;
  int step = 0;

  // Step 0: Welcome (no choices)
  // Step 1: Theme selection
  int  theme_sel = 0;
  // Step 2: Display options (toggles)
  int  disp_cursor = 0; // 0=show_splash, 1=show_hex_addr, 2=show_tips
  bool t_splash = true, t_hex = true, t_tips = true;
  // Step 3: Scan defaults
  int  scan_cursor = 0; // 0=aligned, 1=default_val_type (submenu)
  bool t_aligned = true;
  int  val_type_sel = 2; // Int32
  // Step 4: Session settings
  std::string session_path_input = "session.ixeram";
  std::string ghidra_base_input  = "0x100000";
  int  sess_cursor = 0;
  bool editing_session = false;
  bool editing_ghidra  = false;

  const std::vector<std::string> THEMES = {"Dark", "Neon", "Gruvbox", "Light"};
  const std::vector<std::string> THEME_DESC = {
    "navy background, blue accents",
    "pure black, glowing green/cyan",
    "retro warm amber on dark brown",
    "light background, professional look",
  };
  const std::vector<std::string> VAL_TYPES = {
    "Int8","Int16","Int32","Int64",
    "UInt8","UInt16","UInt32","UInt64",
    "Float32","Float64","Bool","AOB","String"
  };

  // FTXUI input components for text fields
  auto input_session  = Input(&session_path_input, "session.ixeram");
  auto input_ghidra   = Input(&ghidra_base_input,  "0x100000");

  // ── Main component ────────────────────────────────────────────────────────
  auto wizard = CatchEvent(
    Renderer([&] {
      Element content;

      switch (step) {
        // ── Step 0: Welcome ────────────────────────────────────────────────
        case 0: {
          content = vbox({
            wizard_header(0, TOTAL_STEPS,
              "Welcome to IxeRam!",
              "Let's configure the tool for your workflow. This takes about 30 seconds."),
            text(""),
            hbox({
              text("  "),
              vbox({
                text("  ⬡  IxeRam is a terminal-native memory scanner & debugger.") | color(W_FG) | bold,
                text(""),
                text("  ◆  Multi-threaded scanning — uses all CPU cores")             | color(W_ACCENT),
                text("  ◆  Live disassembler + assembler (Capstone + Keystone)")      | color(W_ACCENT),
                text("  ◆  Pointer scanner, Call graph, Structure dissector")         | color(W_ACCENT),
                text("  ◆  Ghidra integration, JSON export")                          | color(W_ACCENT),
                text("  ◆  Speedhack via LD_PRELOAD, Memory freeze")                  | color(W_ACCENT),
                text(""),
                text("  We'll now walk you through a few quick settings.") | color(W_DIM),
                text("  You can always change these later from the config file:") | color(W_DIM),
                text("  ~/.config/ixeram/config.ini") | color(W_YELLOW),
              }),
            }),
            filler(),
            nav_footer(false, false),
          });
          break;
        }

        // ── Step 1: Theme ──────────────────────────────────────────────────
        case 1: {
          content = vbox({
            wizard_header(1, TOTAL_STEPS,
              "🎨 Color Theme",
              "Choose a color palette for the interface."),
            text(""),
            option_list(THEMES, theme_sel, THEME_DESC),
            separator() | color(W_BORDER),
            theme_preview(theme_sel),
            filler(),
            nav_footer(true, false),
          });
          break;
        }

        // ── Step 2: Display options ────────────────────────────────────────
        case 2: {
          content = vbox({
            wizard_header(2, TOTAL_STEPS,
              "🖥️  Display Options",
              "Configure what is shown in the interface."),
            text(""),
            toggle_row("Show Splash Logo", "ASCII/Kitty logo on launch", t_splash, disp_cursor == 0),
            text(""),
            toggle_row("Hex Addresses",    "Show addresses as 0x… (vs decimal)",   t_hex,    disp_cursor == 1),
            text(""),
            toggle_row("Show Tips",        "Hint bar at the bottom of each tab",    t_tips,   disp_cursor == 2),
            filler(),
            nav_footer(true, false),
          });
          break;
        }

        // ── Step 3: Scan defaults ──────────────────────────────────────────
        case 3: {
          content = vbox({
            wizard_header(3, TOTAL_STEPS,
              "🔍 Scan Defaults",
              "Set default behavior for memory scanning."),
            text(""),
            toggle_row("Aligned Scan",
                       "Only scan aligned addresses (faster, fewer results)",
                       t_aligned, scan_cursor == 0),
            separator() | color(W_BORDER),
            hbox({
              text(scan_cursor == 1 ? "  ▶ " : "    ") | color(W_ACCENT),
              text("Default Value Type") | color(scan_cursor == 1 ? W_FG : W_DIM) | bold,
              text("  Current: ") | color(W_DIM),
              text(VAL_TYPES[val_type_sel]) | color(W_YELLOW) | bold,
            }) | (scan_cursor == 1 ? bgcolor(W_SEL_BG) : nothing),
            // Sub-selector for value type
            (scan_cursor == 1) ? vbox({
              separator() | color(W_BORDER),
              text("  Select default scan type:") | color(W_DIM),
              option_list(VAL_TYPES, val_type_sel),
            }) : text(""),
            filler(),
            nav_footer(true, false),
          });
          break;
        }

        // ── Step 4: Session ────────────────────────────────────────────────
        case 4: {
          content = vbox({
            wizard_header(4, TOTAL_STEPS,
              "💾 Session & Integration",
              "Set default paths and Ghidra base address."),
            text(""),
            hbox({
              text(sess_cursor == 0 ? "  ▶ " : "    ") | color(W_ACCENT),
              text("Default Session File  ") | color(sess_cursor == 0 ? W_FG : W_DIM) | bold,
            }) | (sess_cursor == 0 ? bgcolor(W_SEL_BG) : nothing),
            hbox({
              text("    "),
              input_session->Render() | color(W_YELLOW) | size(WIDTH, EQUAL, 40),
            }),
            text(""),
            hbox({
              text(sess_cursor == 1 ? "  ▶ " : "    ") | color(W_ACCENT),
              text("Ghidra ImageBase     ") | color(sess_cursor == 1 ? W_FG : W_DIM) | bold,
            }) | (sess_cursor == 1 ? bgcolor(W_SEL_BG) : nothing),
            hbox({
              text("    "),
              input_ghidra->Render() | color(W_YELLOW) | size(WIDTH, EQUAL, 40),
            }),
            text(""),
            text("  ℹ️  Ghidra base: address used to compute offsets in Ghidra script export.") | color(W_DIM),
            text("  ℹ️  Default: 0x100000 for standard x86-64 ELF binaries.") | color(W_DIM),
            filler(),
            nav_footer(true, true),
          });
          break;
        }
      }

      return content | border | color(W_BORDER) | bgcolor(W_BG);
    }),
    [&](Event ev) -> bool {
      if (step == 4 && (editing_session || editing_ghidra)) {
        // Let the active text input swallow keys
        if (sess_cursor == 0) return input_session->OnEvent(ev);
        if (sess_cursor == 1) return input_ghidra->OnEvent(ev);
      }

      if (ev == Event::Character('q') || ev == Event::Character('Q')) {
        skipped = true;
        screen.ExitLoopClosure()();
        return true;
      }

      if (ev == Event::Return || ev == Event::Character(' ')) {
        if (step < TOTAL_STEPS - 1) {
          // Advance step (collecting current state)
          step++;
        } else {
          // Finish
          finished = true;
          screen.ExitLoopClosure()();
        }
        return true;
      }

      // Backspace / Left arrows go back
      if (ev == Event::ArrowLeft || ev == Event::Backspace) {
        if (step > 0) { step--; }
        return true;
      }

      // Navigation per step
      switch (step) {
        case 1: // Theme
          if (ev == Event::ArrowDown) theme_sel = (theme_sel + 1) % (int)THEMES.size();
          else if (ev == Event::ArrowUp) theme_sel = (theme_sel - 1 + (int)THEMES.size()) % (int)THEMES.size();
          else if (ev == Event::Character('j')) theme_sel = (theme_sel + 1) % (int)THEMES.size();
          else if (ev == Event::Character('k')) theme_sel = (theme_sel - 1 + (int)THEMES.size()) % (int)THEMES.size();
          break;

        case 2: // Display toggles
          if (ev == Event::ArrowDown || ev == Event::Character('j')) disp_cursor = (disp_cursor + 1) % 3;
          else if (ev == Event::ArrowUp || ev == Event::Character('k')) disp_cursor = (disp_cursor - 1 + 3) % 3;
          else if (ev == Event::Character('s') || ev == Event::Character(' ')) {
            if (disp_cursor == 0) t_splash = !t_splash;
            else if (disp_cursor == 1) t_hex    = !t_hex;
            else if (disp_cursor == 2) t_tips   = !t_tips;
            return true;
          }
          break;

        case 3: // Scan
          if (ev == Event::ArrowDown || ev == Event::Character('j')) {
            if (scan_cursor == 1) val_type_sel = (val_type_sel + 1) % (int)VAL_TYPES.size();
            else scan_cursor = 1;
          } else if (ev == Event::ArrowUp || ev == Event::Character('k')) {
            if (scan_cursor == 1 && val_type_sel > 0) val_type_sel--;
            else if (scan_cursor == 1 && val_type_sel == 0) scan_cursor = 0;
            else scan_cursor = std::max(0, scan_cursor - 1);
          } else if (ev == Event::Character('s') || ev == Event::Character(' ')) {
            if (scan_cursor == 0) t_aligned = !t_aligned;
            return true;
          }
          break;

        case 4: // Session
          if (ev == Event::ArrowDown || ev == Event::Character('j')) {
            if (!editing_session && !editing_ghidra) sess_cursor = std::min(1, sess_cursor + 1);
          } else if (ev == Event::ArrowUp || ev == Event::Character('k')) {
            if (!editing_session && !editing_ghidra) sess_cursor = std::max(0, sess_cursor - 1);
          }
          // Tab toggles editing
          if (ev == Event::Tab) {
            if (sess_cursor == 0) { editing_session = !editing_session; editing_ghidra = false; }
            if (sess_cursor == 1) { editing_ghidra  = !editing_ghidra;  editing_session = false; }
            return true;
          }
          // Forward typing to inputs
          if (sess_cursor == 0) return input_session->OnEvent(ev);
          if (sess_cursor == 1) return input_ghidra->OnEvent(ev);
          break;
      }
      return true;
    });

  screen.Loop(wizard);

  // ── Apply collected settings → IxeRamConfig ──────────────────────────────
  if (!skipped) {
    cfg.first_run            = false;
    cfg.theme                = static_cast<ColorTheme>(theme_sel);
    cfg.show_splash          = t_splash;
    cfg.show_hex_addresses   = t_hex;
    cfg.show_tips            = t_tips;
    cfg.aligned_scan_default = t_aligned;
    cfg.default_value_type   = val_type_sel;
    cfg.default_session_path = session_path_input.empty() ? "session.ixeram" : session_path_input;

    // Parse ghidra base
    try {
      cfg.ghidra_image_base = std::stoull(ghidra_base_input, nullptr, 16);
    } catch (...) {
      try { cfg.ghidra_image_base = std::stoull(ghidra_base_input); }
      catch (...) { cfg.ghidra_image_base = 0x100000; }
    }
  } else {
    // Skipped — save defaults (so wizard won't run again)
    cfg = IxeRamConfig{};
    cfg.first_run = false;
  }

  Config::save(cfg);
  return cfg;
}
