#include "Config.hpp"
#include "MemoryEngine.hpp"
#include "Scanner.hpp"
#include "SetupWizard.hpp"
#include "TUI.hpp"
#include <iostream>
#include <unistd.h>

int main() {
  if (geteuid() != 0) {
    std::cerr << "!!! ERROR: This tool requires root privileges (sudo) to "
                 "access other processes memory !!!"
              << std::endl;
    return 1;
  }

  // ── First-run wizard / config load ────────────────────────────────────────
  IxeRamConfig cfg;
  if (Config::is_first_run()) {
    cfg = SetupWizard::run();
  } else {
    cfg = Config::load();
  }

  // ── Launch TUI ────────────────────────────────────────────────────────────
  MemoryEngine engine;
  Scanner scanner(engine);
  TUI tui(engine, scanner, cfg);

  tui.run();

  return 0;
}
