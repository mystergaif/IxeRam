#include "Config.hpp"

#include <cstdlib>
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <pwd.h>
#include <unistd.h>
#include <sys/types.h>

// ─────────────────────────────────────────────────────────────────────────────
//  Helpers
// ─────────────────────────────────────────────────────────────────────────────

static std::string get_home() {
  // 1. If running under sudo, try to get original user's home
  const char *su = std::getenv("SUDO_USER");
  if (su) {
    struct passwd *pw = getpwnam(su);
    if (pw && pw->pw_dir) return std::string(pw->pw_dir);
  }

  // 2. Otherwise try HOME env
  const char *h = std::getenv("HOME");
  if (h) return std::string(h);

  // 3. Last resort - fallback to current UID home
  struct passwd *pw = getpwuid(getuid());
  if (pw && pw->pw_dir) return std::string(pw->pw_dir);

  return "/root";
}

static void mkdir_p(const std::string &path) {
  // create directory (and potential parent config dir)
  ::mkdir(path.c_str(), 0755);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Config::config_path
// ─────────────────────────────────────────────────────────────────────────────
std::string Config::config_path() {
  return get_home() + "/.config/ixeram/config.ini";
}

// ─────────────────────────────────────────────────────────────────────────────
//  Config::is_first_run
// ─────────────────────────────────────────────────────────────────────────────
bool Config::is_first_run() {
  std::ifstream f(config_path());
  return !f.good();
}

// ─────────────────────────────────────────────────────────────────────────────
//  INI-style helpers
// ─────────────────────────────────────────────────────────────────────────────

static std::string trim(const std::string &s) {
  auto b = s.find_first_not_of(" \t\r\n");
  auto e = s.find_last_not_of(" \t\r\n");
  return (b == std::string::npos) ? "" : s.substr(b, e - b + 1);
}

static bool parse_bool(const std::string &v) {
  return v == "1" || v == "true" || v == "yes";
}

static std::string bool_str(bool v) { return v ? "1" : "0"; }

// ─────────────────────────────────────────────────────────────────────────────
//  Config::load
// ─────────────────────────────────────────────────────────────────────────────
IxeRamConfig Config::load() {
  IxeRamConfig cfg{};
  std::ifstream f(config_path());
  if (!f.good())
    return cfg; // first run — return defaults

  std::string line;
  while (std::getline(f, line)) {
    line = trim(line);
    if (line.empty() || line[0] == '#' || line[0] == '[')
      continue;
    auto eq = line.find('=');
    if (eq == std::string::npos)
      continue;
    std::string key = trim(line.substr(0, eq));
    std::string val = trim(line.substr(eq + 1));

    if (key == "first_run")             cfg.first_run             = parse_bool(val);
    else if (key == "theme")            cfg.theme                 = static_cast<ColorTheme>(std::stoi(val));
    else if (key == "show_splash")      cfg.show_splash           = parse_bool(val);
    else if (key == "show_hex_addresses") cfg.show_hex_addresses  = parse_bool(val);
    else if (key == "aligned_scan")     cfg.aligned_scan_default  = parse_bool(val);
    else if (key == "default_value_type") cfg.default_value_type  = std::stoi(val);
    else if (key == "session_path")     cfg.default_session_path  = val;
    else if (key == "ghidra_base")      cfg.ghidra_image_base     = std::stoull(val, nullptr, 16);
    else if (key == "show_tips")        cfg.show_tips             = parse_bool(val);
    else if (key == "log_max_lines")    cfg.log_max_lines         = std::stoi(val);
  }

  return cfg;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Config::save
// ─────────────────────────────────────────────────────────────────────────────
bool Config::save(const IxeRamConfig &cfg) {
  // Ensure directory exists
  std::string dir = get_home() + "/.config/ixeram";
  mkdir_p(get_home() + "/.config");
  mkdir_p(dir);

  std::ofstream f(config_path());
  if (!f.good())
    return false;

  f << "# IxeRam Configuration File\n";
  f << "# Generated automatically — edit at your own risk.\n";
  f << "\n[meta]\n";
  f << "first_run=" << bool_str(cfg.first_run) << "\n";
  f << "\n[appearance]\n";
  f << "theme="              << static_cast<int>(cfg.theme)      << "\n";
  f << "show_splash="        << bool_str(cfg.show_splash)        << "\n";
  f << "show_hex_addresses=" << bool_str(cfg.show_hex_addresses) << "\n";
  f << "show_tips="          << bool_str(cfg.show_tips)          << "\n";
  f << "\n[scan]\n";
  f << "aligned_scan="       << bool_str(cfg.aligned_scan_default) << "\n";
  f << "default_value_type=" << cfg.default_value_type           << "\n";
  f << "\n[session]\n";
  f << "session_path="       << cfg.default_session_path         << "\n";
  f << "\n[ghidra]\n";
  std::ostringstream oss;
  oss << std::hex << cfg.ghidra_image_base;
  f << "ghidra_base=0x"      << oss.str()                       << "\n";
  f << "\n[ui]\n";
  f << "log_max_lines="      << cfg.log_max_lines                << "\n";

  return true;
}
