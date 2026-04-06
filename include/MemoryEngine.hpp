#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <sys/types.h>
#include <vector>

struct MemoryRegion {
  uintptr_t start;
  uintptr_t end;
  std::string permissions;
  std::string pathname;
  uintptr_t file_offset = 0; // offset in the backing file (from /proc/maps)

  bool is_writable() const {
    return permissions.find('w') != std::string::npos;
  }
  bool is_readable() const {
    return permissions.find('r') != std::string::npos;
  }
};

struct AccessRecord {
  uint64_t rip;                // instruction address that caused access
  uint64_t rax, rbx, rcx, rdx; // register context
  bool is_write;
};

enum class HWBreakpointType { Execute = 0, Write = 1, ReadWrite = 3 };
enum class HWBreakpointSize { Byte1 = 0, Byte2 = 1, Byte4 = 3, Byte8 = 2 };

struct HWBreakpoint {
  uintptr_t address;
  HWBreakpointType type;
  HWBreakpointSize size;
  bool active = false;
};

class MemoryEngine {
public:
  MemoryEngine();
  ~MemoryEngine();

  bool attach(pid_t pid);
  void detach();

  bool read_memory(uintptr_t address, void *buffer, size_t size);
  bool read_memory_batch(const std::vector<uintptr_t> &addresses, void *buffers,
                         size_t item_size);
  bool write_memory(uintptr_t address, const void *buffer, size_t size);

  std::vector<MemoryRegion> update_maps();
  pid_t get_pid() const { return target_pid; }

  bool pause_process();
  bool resume_process();
  bool kill_process();
  bool is_paused() const { return process_paused; }
  
  bool attach_ptrace();
  bool detach_ptrace();
  bool step_over(); // New helper

  // ─── Auto-attach by process name ──────────────────────────────────
  // Returns PID of first matching process, or -1 if not found
  static pid_t find_pid_by_name(const std::string &name);
  // Watch for process by name to appear, returns pid when found (blocking,
  // max_ms timeout)
  static pid_t wait_for_process(const std::string &name, int max_ms = 10000);

  // ─── Software Breakpoints (int3) via ptrace ────────────────────────
  // Set int3 breakpoint at address (saves original byte)
  bool set_breakpoint(uintptr_t address);
  // Remove breakpoint (restore original byte)
  bool remove_breakpoint(uintptr_t address);
  // Wait for any breakpoint to hit (blocking), populates last_access
  bool wait_breakpoint(uintptr_t &hit_addr, int timeout_ms = 5000);
  // Get last access records (filled by breakpoint hit)
  std::vector<AccessRecord> access_records;
  // Clear all breakpoints
  void clear_breakpoints();

  // ─── Hardware Breakpoints/Watchpoints (DR0-DR3) ──────────────────────
  // Set hardware breakpoint/watchpoint in specified slot (0-3)
  bool set_hw_breakpoint(int slot, uintptr_t address, HWBreakpointType type,
                         HWBreakpointSize size);
  // Clear hardware breakpoint in specified slot
  bool clear_hw_breakpoint(int slot);
  // Get active hardware breakpoints
  std::vector<HWBreakpoint> get_hw_breakpoints() const;

private:
  pid_t target_pid;
  bool process_paused = false;
  std::vector<MemoryRegion> regions;
  // breakpoint addr -> original byte
  std::map<uintptr_t, uint8_t> breakpoints;
  // Hardware breakpoint slots
  HWBreakpoint hw_breakpoints[4];
};
