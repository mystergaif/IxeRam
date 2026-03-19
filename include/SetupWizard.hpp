#pragma once

#include "Config.hpp"

// ─────────────────────────────────────────────────────────────────────────────
//  SetupWizard — first-run interactive configuration tour.
//
//  Call SetupWizard::run() before launching TUI when Config::is_first_run().
//  Returns the fully populated IxeRamConfig to use for this session.
// ─────────────────────────────────────────────────────────────────────────────

class SetupWizard {
public:
  // Runs the full interactive wizard (blocking), saves config, returns it.
  static IxeRamConfig run();
};
