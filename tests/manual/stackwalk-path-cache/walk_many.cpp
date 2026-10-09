// Manual diagnostic for measuring Stackwalker memory across many targets.
#include "frame.h"
#include "procstate.h"
#include "walker.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <malloc.h>
#include <set>
#include <string>
#include <vector>

using namespace Dyninst;
using namespace Dyninst::Stackwalker;

namespace {

struct Counts {
  size_t threads = 0;
  size_t frames = 0;
  size_t named = 0;
  size_t library_offsets = 0;
  size_t thread_failures = 0;
  size_t walk_failures = 0;
};

struct Memory {
  long rss_kib = 0;
  long anon_kib = 0;
  long file_kib = 0;
  long shmem_kib = 0;
  long private_dirty_kib = 0;
  long smaps_anon_kib = 0;
};

auto const started = std::chrono::steady_clock::now();

long value_from_line(std::string const& line, std::string const& key) {
  if(line.rfind(key, 0) != 0) return -1;
  return std::stol(line.substr(key.size()));
}

Memory read_memory() {
  Memory result;
  std::ifstream status("/proc/self/status");
  std::string line;
  while(std::getline(status, line)) {
    long value = 0;
    if((value = value_from_line(line, "VmRSS:")) >= 0) result.rss_kib = value;
    if((value = value_from_line(line, "RssAnon:")) >= 0) result.anon_kib = value;
    if((value = value_from_line(line, "RssFile:")) >= 0) result.file_kib = value;
    if((value = value_from_line(line, "RssShmem:")) >= 0) result.shmem_kib = value;
  }

  std::ifstream rollup("/proc/self/smaps_rollup");
  while(std::getline(rollup, line)) {
    long value = 0;
    if((value = value_from_line(line, "Private_Dirty:")) >= 0)
      result.private_dirty_kib = value;
    if((value = value_from_line(line, "Anonymous:")) >= 0)
      result.smaps_anon_kib = value;
  }
  return result;
}

std::string write_malloc_info(std::string const& stage, size_t index) {
  auto const* directory = std::getenv("MALLOC_INFO_DIR");
  if(!directory || !*directory) return {};
  auto path = std::string(directory) + "/malloc-" + stage + "-" +
              std::to_string(index) + ".xml";
  FILE* output = std::fopen(path.c_str(), "w");
  if(!output) {
    std::perror("fopen malloc_info");
    return {};
  }
  if(malloc_info(0, output) != 0) {
    std::perror("malloc_info");
    path.clear();
  }
  std::fclose(output);
  return path;
}

void record(char const* stage, size_t index, PID pid, Counts const& counts,
            size_t library_names) {
  auto const memory = read_memory();
  auto const xml = write_malloc_info(stage, index);
  auto const elapsed = std::chrono::duration<double>(
      std::chrono::steady_clock::now() - started).count();
  std::cout << stage << '\t' << index << '\t' << pid << '\t' << elapsed << '\t'
            << counts.threads << '\t' << counts.frames << '\t' << counts.named << '\t'
            << counts.library_offsets << '\t' << library_names << '\t'
            << counts.thread_failures << '\t' << counts.walk_failures << '\t'
            << memory.rss_kib << '\t' << memory.anon_kib << '\t' << memory.file_kib
            << '\t' << memory.shmem_kib << '\t' << memory.private_dirty_kib << '\t'
            << memory.smaps_anon_kib << '\t' << xml << '\n';
  std::cout.flush();
}

bool wants(std::string const& mode, char const* operation) {
  return mode == "both" || mode == operation;
}

}  // namespace

int main(int argc, char** argv) {
  if(argc < 3) {
    std::cerr << "usage: walk_many walk|name|liboffset|both PID...\n";
    return 2;
  }
  std::string const mode = argv[1];
  if(mode != "walk" && mode != "name" && mode != "liboffset" && mode != "both") {
    std::cerr << "invalid mode: " << mode << '\n';
    return 2;
  }

  std::cout << "stage\tindex\tpid\telapsed_s\tthreads\tframes\tnamed\t"
               "library_offsets\tlibrary_names\tthread_failures\twalk_failures\t"
               "rss_kib\tanon_kib\tfile_kib\tshmem_kib\tprivate_dirty_kib\t"
               "smaps_anon_kib\tmalloc_info\n";

  Counts counts;
  std::set<std::string> libraries;
  std::vector<Walker*> walkers;
  record("before_attach", 0, 0, counts, libraries.size());

  bool ok = true;
  for(int argument = 2; argument < argc; ++argument) {
    auto const index = static_cast<size_t>(argument - 1);
    auto const pid = static_cast<PID>(std::strtol(argv[argument], nullptr, 10));
    Walker* walker = Walker::newWalker(pid);
    if(!walker) {
      std::cerr << "newWalker(" << pid << ") failed\n";
      ok = false;
      break;
    }
    walkers.push_back(walker);
    record("after_attach", index, pid, counts, libraries.size());

    std::vector<THR_ID> threads;
    if(!walker->getAvailableThreads(threads)) {
      ++counts.thread_failures;
      ok = false;
      record("thread_list_failed", index, pid, counts, libraries.size());
      continue;
    }
    counts.threads += threads.size();

    std::vector<std::vector<Frame>> stacks;
    stacks.reserve(threads.size());
    for(auto const thread : threads) {
      stacks.emplace_back();
      if(!walker->walkStack(stacks.back(), thread)) {
        ++counts.walk_failures;
        ok = false;
      }
      counts.frames += stacks.back().size();
    }
    record("after_walk", index, pid, counts, libraries.size());

    if(wants(mode, "name")) {
      for(auto& stack : stacks) {
        for(auto& frame : stack) {
          std::string name;
          if(frame.getName(name) && !name.empty()) ++counts.named;
        }
      }
    }
    record("after_names", index, pid, counts, libraries.size());

    if(wants(mode, "liboffset")) {
      for(auto& stack : stacks) {
        for(auto& frame : stack) {
          std::string library;
          Offset offset = 0;
          void* symtab = nullptr;
          if(frame.getLibOffset(library, offset, symtab)) {
            ++counts.library_offsets;
            libraries.insert(library);
          }
        }
      }
    }
    record("after_libraries", index, pid, counts, libraries.size());
  }

  auto const measured = std::chrono::duration<double>(
      std::chrono::steady_clock::now() - started).count();
  auto const before_detach = read_memory();
  std::cout << "summary\tprocesses=" << walkers.size() << "\tframes=" << counts.frames
            << "\tnamed=" << counts.named << "\tlibrary_offsets="
            << counts.library_offsets << "\tlibrary_names=" << libraries.size()
            << "\ttime_s=" << measured << "\trss_kib=" << before_detach.rss_kib
            << "\n";

  for(auto* walker : walkers) {
    auto* process = dynamic_cast<ProcDebug*>(walker->getProcessState());
    if(!process || !process->detach(false)) {
      std::cerr << "detach failed\n";
      ok = false;
    }
  }
  record("after_detach", walkers.size(), 0, counts, libraries.size());

  for(auto* walker : walkers) delete walker;
  walkers.clear();
  record("after_delete", 0, 0, counts, libraries.size());

  for(auto const& library : libraries) std::cerr << "library=" << library << '\n';
  return ok ? 0 : 1;
}
