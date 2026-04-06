#include "Config.hpp"
#include "MemoryEngine.hpp"
#include "Scanner.hpp"
#include "SetupWizard.hpp"
#include "TUI.hpp"
#include "MainWindow.hpp"
#include <QApplication>
#include <iostream>
#include <string>
#include <unistd.h>
#include <vector>
#include <cstring>

void relaunch_as_root(int argc, char* argv[]) {
    // Check if pkexec exists in PATH
    if (system("which pkexec > /dev/null 2>&1") != 0) {
        std::cerr << "!!! ERROR: This tool requires root privileges.\n"
                  << "Please run it with 'sudo' or install 'pkexec'." << std::endl;
        exit(1);
    }

    std::vector<char*> args;
    args.push_back((char*)"pkexec");
    args.push_back((char*)"env");

    // List of environment variables to preserve for GUI/Wayland
    const char* env_vars[] = {
        "DISPLAY",
        "XAUTHORITY",
        "WAYLAND_DISPLAY",
        "XDG_RUNTIME_DIR"
    };

    for (const char* var : env_vars) {
        char* val = getenv(var);
        if (val) {
            std::string env_entry = std::string(var) + "=" + val;
            args.push_back(strdup(env_entry.c_str()));
        }
    }

    // Get full path to current executable
    char path[1024];
    ssize_t len = readlink("/proc/self/exe", path, sizeof(path) - 1);
    if (len != -1) {
        path[len] = '\0';
        args.push_back(strdup(path));
    } else {
        args.push_back(argv[0]);
    }

    for (int i = 1; i < argc; ++i) {
        args.push_back(argv[i]);
    }
    args.push_back(nullptr);

    std::cout << "Attempting to relaunch as root using pkexec with env preservation..." << std::endl;
    execvp("pkexec", args.data());
    
    perror("execvp pkexec failed");
    exit(1);
}

int main(int argc, char* argv[]) {
    // 1. Check for --gui flag early to handle display permissions if needed
    bool use_gui = false;
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--gui") {
            use_gui = true;
            break;
        }
    }

    // 2. Privilege check
    if (geteuid() != 0) {
        relaunch_as_root(argc, argv);
    }

    // 3. Core initialization
    IxeRamConfig cfg;
    // Note: SetupWizard might need to be adapted for GUI if we want it there
    if (Config::is_first_run() && !use_gui) {
        cfg = SetupWizard::run();
    } else {
        cfg = Config::load();
    }

    MemoryEngine engine;
    Scanner scanner(engine);

    if (use_gui) {
        // 4. Launch GUI
        // In Qt, QApplication must be initialized before any GUI objects
        QApplication app(argc, argv);
        MainWindow window;
        window.show();
        return app.exec();
    } else {
        // 5. Launch TUI
        TUI tui(engine, scanner, cfg);
        tui.run();
    }

    return 0;
}
