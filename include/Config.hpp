#pragma once

#include <cstdint>
#include <string>

// ─────────────────────────────────────────────────────────────────────────────
//  IxeRam Config  — persisted in ~/.config/ixeram/config.ini
//  Simple INI-style storage (no external dependencies).
// ─────────────────────────────────────────────────────────────────────────────

enum class ColorTheme {
  Dark   = 0,  // default dark (navy + blue accent)
  Neon   = 1,  // bright cyber-green on black
  Gruvbox = 2, // warm retro amber/green
  Light  = 3,  // light background
};

struct IxeRamConfig {
  // ── Meta ─────────────────────────────────────────────────────────────────
  bool first_run = true;          // false after first-run wizard completes

  // ── Appearance ───────────────────────────────────────────────────────────
  ColorTheme theme      = ColorTheme::Dark;
  bool       show_splash = true;  // show ASCII logo on launch
  bool       show_hex_addresses = true; // false = decimal

  // ── Scan defaults ────────────────────────────────────────────────────────
  bool aligned_scan_default = true;
  int  default_value_type   = 2;  // index into VALUE_TYPES (Int32)

  // ── Session ──────────────────────────────────────────────────────────────
  std::string default_session_path = "session.ixeram";

  // ── Ghidra ───────────────────────────────────────────────────────────────
  uintptr_t ghidra_image_base = 0x100000;

  // ── UI ───────────────────────────────────────────────────────────────────
  bool  show_tips    = true;   // show hint bar at bottom
  int   log_max_lines = 200;   // max log lines kept in memory
};

// ─────────────────────────────────────────────────────────────────────────────
class Config {
public:
  // Returns path to config file: ~/.config/ixeram/config.ini
  static std::string config_path();

  // Load config from disk (returns default config if file missing)
  static IxeRamConfig load();

  // Save config to disk (creates directories as needed)
  static bool save(const IxeRamConfig &cfg);

  // Convenience: check if this is a first run (config file missing)
  static bool is_first_run();
};
