#include "MemoryEngine.hpp"
#include <fstream>
#include <signal.h>
#include <sstream>
#include <stdexcept>
#include <sys/uio.h>
#include <unistd.h>
#include <sys/ptrace.h>
#include <sys/user.h>
#include <sys/wait.h>
#include <cstddef>
#include <iostream>

MemoryEngine::MemoryEngine() : target_pid(-1) {}

MemoryEngine::~MemoryEngine() { detach(); }

bool MemoryEngine::attach(pid_t pid) {
  if (kill(pid, 0) == -1)
    return false;

  std::string mem_path = "/proc/" + std::to_string(pid) + "/mem";
  if (access(mem_path.c_str(), R_OK | W_OK) != 0) {
    throw std::runtime_error("Permission denied to /proc/" +
                             std::to_string(pid) +
                             "/mem! Enable ptrace_scope or run as root.");
  }

  target_pid = pid;
  update_maps();
  
  target_pid = pid;
  update_maps();
  
  // Just check if we CAN attach, then detach immediately
  if (ptrace(PTRACE_ATTACH, target_pid, nullptr, nullptr) == 0) {
      int status;
      waitpid(target_pid, &status, 0);
      ptrace(PTRACE_DETACH, target_pid, nullptr, nullptr);
      return true;
  }
  return false;
}

void MemoryEngine::detach() {
  if (target_pid != -1) {
      clear_breakpoints();
      ptrace(PTRACE_DETACH, target_pid, nullptr, nullptr);
  }
  target_pid = -1;
  regions.clear();
}

bool MemoryEngine::read_memory(uintptr_t address, void *buffer, size_t size) {
  if (target_pid == -1)
    return false;

  struct iovec local[1];
  struct iovec remote[1];

  local[0].iov_base = buffer;
  local[0].iov_len = size;
  remote[0].iov_base = (void *)address;
  remote[0].iov_len = size;

  ssize_t nread = process_vm_readv(target_pid, local, 1, remote, 1, 0);
  return nread == (ssize_t)size;
}
#ifndef IOV_MAX
#define IOV_MAX 1024
#endif

bool MemoryEngine::read_memory_batch(const std::vector<uintptr_t> &addresses,
                                     void *buffers, size_t item_size) {
  if (target_pid == -1 || addresses.empty())
    return false;

  size_t total = addresses.size();
  size_t processed = 0;

  while (processed < total) {
    size_t chunk_size = std::min(total - processed, (size_t)IOV_MAX);
    std::vector<struct iovec> locals(chunk_size);
    std::vector<struct iovec> remotes(chunk_size);

    for (size_t i = 0; i < chunk_size; ++i) {
      locals[i].iov_base = (uint8_t *)buffers + (processed + i) * item_size;
      locals[i].iov_len = item_size;
      remotes[i].iov_base = (void *)addresses[processed + i];
      remotes[i].iov_len = item_size;
    }

    ssize_t nread = process_vm_readv(target_pid, locals.data(), chunk_size,
                                     remotes.data(), chunk_size, 0);
    if (nread != (ssize_t)(chunk_size * item_size)) {
      // If full batch fails, try one by one for this batch to at least get what
      // we can
      for (size_t i = 0; i < chunk_size; ++i) {
        read_memory(addresses[processed + i],
                    (uint8_t *)buffers + (processed + i) * item_size,
                    item_size);
      }
    }
    processed += chunk_size;
  }
  return true;
}

bool MemoryEngine::write_memory(uintptr_t address, const void *buffer,
                                size_t size) {
  if (target_pid == -1)
    return false;

  struct iovec local[1];
  struct iovec remote[1];

  local[0].iov_base = const_cast<void *>(buffer);
  local[0].iov_len = size;
  remote[0].iov_base = (void *)address;
  remote[0].iov_len = size;

  ssize_t nwritten = process_vm_writev(target_pid, local, 1, remote, 1, 0);
  return nwritten == (ssize_t)size;
}

std::vector<MemoryRegion> MemoryEngine::update_maps() {
  regions.clear();
  if (target_pid == -1)
    return regions;

  std::string path = "/proc/" + std::to_string(target_pid) + "/maps";
  std::ifstream maps_file(path);
  std::string line;

  while (std::getline(maps_file, line)) {
    std::istringstream iss(line);
    std::string address_range, perms, offset, dev, inode, pathname;

    iss >> address_range >> perms >> offset >> dev >> inode;
    std::getline(iss, pathname); // Pathname might be empty or have spaces

    size_t dash_pos = address_range.find('-');
    if (dash_pos != std::string::npos) {
      MemoryRegion region;
      region.start =
          std::stoull(address_range.substr(0, dash_pos), nullptr, 16);
      region.end = std::stoull(address_range.substr(dash_pos + 1), nullptr, 16);
      region.permissions = perms;
      try {
        region.file_offset = std::stoull(offset, nullptr, 16);
      } catch (...) {
        region.file_offset = 0;
      }

      // Trim leading spaces from pathname
      size_t first = pathname.find_first_not_of(' ');
      if (first != std::string::npos) {
        region.pathname = pathname.substr(first);
      } else {
        region.pathname = "";
      }

      regions.push_back(region);
    }
  }
  return regions;
}

bool MemoryEngine::pause_process() {
  if (target_pid <= 0)
    return false;
  // If attached via ptrace, we should wait until the process stops actually
  if (kill(target_pid, SIGSTOP) == 0) {
    int status;
    waitpid(target_pid, &status, 0); // Synchronous wait
    process_paused = true;
    return true;
  }
  return false;
}

bool MemoryEngine::resume_process() {
  if (target_pid <= 0)
    return false;
    
  // If we are tracing (ptrace attached), we must use ptrace(PTRACE_CONT)
  // to continue execution properly after a SIGSTOP/SIGTRAP.
  if (ptrace(PTRACE_CONT, target_pid, nullptr, nullptr) == 0) {
      process_paused = false;
      return true;
  }
  
  if (kill(target_pid, SIGCONT) == 0) {
    process_paused = false;
    return true;
  }
  return false;
}

bool MemoryEngine::kill_process() {
  if (target_pid <= 0)
    return false;
  if (kill(target_pid, SIGKILL) == 0) {
    target_pid = -1;
    process_paused = false;
    return true;
  }
  return false;
}

// ─── Auto-attach by process name ─────────────────────────────────────────────
#include <chrono>
#include <dirent.h>
#include <thread>

pid_t MemoryEngine::find_pid_by_name(const std::string &name) {
  DIR *dir = opendir("/proc");
  if (!dir)
    return -1;

  struct dirent *ent;
  while ((ent = readdir(dir)) != nullptr) {
    if (ent->d_type != DT_DIR)
      continue;
    bool is_pid = true;
    for (char c : std::string(ent->d_name))
      if (!isdigit(c)) {
        is_pid = false;
        break;
      }
    if (!is_pid)
      continue;

    std::string comm_path = "/proc/" + std::string(ent->d_name) + "/comm";
    std::ifstream comm_file(comm_path);
    if (!comm_file)
      continue;

    std::string comm;
    std::getline(comm_file, comm);
    if (comm == name || name.find(comm) != std::string::npos ||
        comm.find(name) != std::string::npos) {
      pid_t pid = std::stoi(ent->d_name);
      closedir(dir);
      return pid;
    }
  }
  closedir(dir);
  return -1;
}

pid_t MemoryEngine::wait_for_process(const std::string &name, int max_ms) {
  auto start = std::chrono::steady_clock::now();
  while (true) {
    pid_t pid = find_pid_by_name(name);
    if (pid != -1)
      return pid;
    auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration_cast<std::chrono::milliseconds>(now - start)
            .count() >= max_ms)
      return -1;
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
  }
}

// ─── Software Breakpoints via ptrace ─────────────────────────────────────────
#ifndef offsetof
#define offsetof(TYPE, MEMBER) ((size_t) & ((TYPE *)0)->MEMBER)
#endif

#define DR_ADDR(slot) (offsetof(struct user, u_debugreg[0]) + (slot) * sizeof(long))
#define DR_CONTROL offsetof(struct user, u_debugreg[7])
#define DR_STATUS offsetof(struct user, u_debugreg[6])

bool MemoryEngine::set_breakpoint(uintptr_t address) {
  if (target_pid <= 0)
    return false;
  if (breakpoints.count(address))
    return true;

  errno = 0;
  long orig = ptrace(PTRACE_PEEKTEXT, target_pid, (void *)address, nullptr);
  if (errno != 0) {
    std::cerr << "[!] Breakpoint PEEK failed at 0x" << std::hex << address << " (errno " << std::dec << errno << "). Attached?\n";
    return false;
  }

  uint8_t orig_byte = (uint8_t)(orig & 0xFF);
  breakpoints[address] = orig_byte;

  long patched = (orig & ~0xFFL) | 0xCC;
  if (ptrace(PTRACE_POKETEXT, target_pid, (void *)address, (void *)patched) != 0) {
    return false;
  }
  
  breakpoints[address] = orig_byte;
  return true;
}

bool MemoryEngine::remove_breakpoint(uintptr_t address) {
  if (target_pid <= 0)
    return false;
  auto it = breakpoints.find(address);
  if (it == breakpoints.end())
    return false;

  errno = 0;
  long current = ptrace(PTRACE_PEEKTEXT, target_pid, (void *)address, nullptr);
  if (errno != 0)
    return false;

  long restored = (current & ~0xFFL) | it->second;
  if (ptrace(PTRACE_POKETEXT, target_pid, (void *)address, (void *)restored) !=
      0)
    return false;

  breakpoints.erase(it);
  return true;
}

bool MemoryEngine::wait_breakpoint(uintptr_t &hit_addr, int timeout_ms) {
  if (target_pid <= 0)
    return false;
    
  auto start = std::chrono::steady_clock::now();
  while (true) {
    int wstatus = 0;
    pid_t wpid = waitpid(target_pid, &wstatus, WNOHANG);
    if (wpid == target_pid) {
      if (WIFEXITED(wstatus)) {
          std::cerr << "[Tracer] Target process exited.\n";
          target_pid = -1;
          return false;
      }
      if (WIFSTOPPED(wstatus)) {
        int sig = WSTOPSIG(wstatus);
        if (sig == SIGTRAP) {
            struct user_regs_struct regs;
            if (ptrace(PTRACE_GETREGS, target_pid, nullptr, &regs) == 0) {
              hit_addr = (uintptr_t)(regs.rip - 1);
              
              // Only adjust RIP if it's one of OUR breakpoints
              if (breakpoints.count(hit_addr)) {
                  regs.rip = hit_addr;
                  ptrace(PTRACE_SETREGS, target_pid, nullptr, &regs);
              } else {
                  hit_addr = regs.rip; // Might be a hardware BP or actual int3 in code
              }

              AccessRecord ar;
              ar.rip = regs.rip;
              ar.rax = regs.rax;
              ar.rbx = regs.rbx;
              ar.rcx = regs.rcx;
              ar.rdx = regs.rdx;
              ar.is_write = false; 
              access_records.push_back(ar);
              
              process_paused = true;
              return true;
            }
        } else {
            // Forward other signals to the target process
            std::clog << "[Tracer] Received signal " << sig << ", forwarding...\n";
            ptrace(PTRACE_CONT, target_pid, nullptr, (void*)(long)sig);
        }
      }
    }
    
    auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration_cast<std::chrono::milliseconds>(now - start)
            .count() >= timeout_ms)
      return false;
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
}

void MemoryEngine::clear_breakpoints() {
  if (target_pid > 0) {
    for (auto const& [addr, orig] : breakpoints) {
      errno = 0;
      long current = ptrace(PTRACE_PEEKTEXT, target_pid, (void *)addr, nullptr);
      if (errno == 0) {
        long restored = (current & ~0xFFL) | orig;
        ptrace(PTRACE_POKETEXT, target_pid, (void *)addr, (void *)restored);
      }
    }
  }
  breakpoints.clear();
}

bool MemoryEngine::set_hw_breakpoint(int slot, uintptr_t address,
                                     HWBreakpointType type,
                                     HWBreakpointSize size) {
  if (target_pid <= 0 || slot < 0 || slot > 3)
    return false;

  // 1. Set the address register
  if (ptrace(PTRACE_POKEUSER, target_pid, DR_ADDR(slot), (void *)address) != 0)
    return false;

  // 2. Read current DR7
  long dr7 = ptrace(PTRACE_PEEKUSER, target_pid, DR_CONTROL, nullptr);

  // 3. Configure the slot in DR7
  // Bits: 0, 2, 4, 6 (L0-L3)
  // RW: 16-17, 20-21, 24-25, 28-29
  // LEN: 18-19, 22-23, 26-27, 30-31

  // Enable L(slot)
  dr7 |= (1L << (slot * 2));

  // Set RW and LEN
  int rw_offset = 16 + (slot * 4);
  int len_offset = 18 + (slot * 4);

  dr7 &= ~(3L << rw_offset);
  dr7 |= ((long)type << rw_offset);

  dr7 &= ~(3L << len_offset);
  dr7 |= ((long)size << len_offset);

  if (ptrace(PTRACE_POKEUSER, target_pid, DR_CONTROL, (void *)dr7) != 0)
    return false;

  hw_breakpoints[slot] = {address, type, size, true};
  return true;
}

bool MemoryEngine::clear_hw_breakpoint(int slot) {
  if (target_pid <= 0 || slot < 0 || slot > 3)
    return false;

  long dr7 = ptrace(PTRACE_PEEKUSER, target_pid, DR_CONTROL, nullptr);
  // Disable L(slot)
  dr7 &= ~(1L << (slot * 2));

  if (ptrace(PTRACE_POKEUSER, target_pid, DR_CONTROL, (void *)dr7) != 0)
    return false;

  hw_breakpoints[slot].active = false;
  return true;
}

std::vector<HWBreakpoint> MemoryEngine::get_hw_breakpoints() const {
  std::vector<HWBreakpoint> res;
  for (int i = 0; i < 4; ++i) {
    if (hw_breakpoints[i].active)
      res.push_back(hw_breakpoints[i]);
  }
  return res;
}

bool MemoryEngine::attach_ptrace() {
  if (target_pid <= 0) return false;
  if (ptrace(PTRACE_ATTACH, target_pid, nullptr, nullptr) != 0) {
      std::cerr << "[!] ptrace(PTRACE_ATTACH) failed (errno " << errno << ")\n";
      return false;
  }
  int status;
  waitpid(target_pid, &status, 0); // Synchronous stop
  return true;
}

bool MemoryEngine::detach_ptrace() {
  if (target_pid <= 0) return false;
  return ptrace(PTRACE_DETACH, target_pid, nullptr, nullptr) == 0;
}

bool MemoryEngine::step_over(uintptr_t breakpoint_addr) {
  if (target_pid <= 0) return false;
  
  bool was_bp = false;
  if (breakpoint_addr > 0 && breakpoints.count(breakpoint_addr)) {
      was_bp = true;
      remove_breakpoint(breakpoint_addr);
  }

  if (ptrace(PTRACE_SINGLESTEP, target_pid, nullptr, nullptr) != 0) {
      std::cerr << "[!] PTRACE_SINGLESTEP failed (errno " << errno << ")\n";
      return false;
  }
  
  int status;
  waitpid(target_pid, &status, 0); 
  
  if (was_bp) {
      set_breakpoint(breakpoint_addr);
  }
  return true;
}
