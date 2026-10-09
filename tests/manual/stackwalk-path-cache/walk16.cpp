// Minimal client matching the reported reproducer. Unlike walk_many, this
// intentionally performs no per-stage instrumentation.
#include "frame.h"
#include "procstate.h"
#include "walker.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <set>
#include <string>
#include <vector>

using namespace Dyninst;
using namespace Dyninst::Stackwalker;

static long rss_kib() {
  std::ifstream input("/proc/self/status");
  std::string key;
  long value = 0;
  while(input >> key) {
    if(key == "VmRSS:") {
      input >> value;
      break;
    }
    input.ignore(4096, '\n');
  }
  return value;
}

int main(int argc, char** argv) {
  auto const started = std::chrono::steady_clock::now();
  std::vector<Walker*> walkers;
  std::set<std::string> libraries;
  size_t frames = 0;
  size_t named = 0;

  for(int argument = 1; argument < argc; ++argument) {
    Walker* walker = Walker::newWalker(std::atoi(argv[argument]));
    if(!walker) {
      std::fprintf(stderr, "newWalker(%s) failed\n", argv[argument]);
      return 1;
    }
    walkers.push_back(walker);

    std::vector<THR_ID> threads;
    walker->getAvailableThreads(threads);
    for(auto const thread : threads) {
      std::vector<Frame> stack;
      walker->walkStack(stack, thread);
      for(auto& frame : stack) {
        std::string name;
        std::string library;
        Offset offset = 0;
        void* symtab = nullptr;
        ++frames;
        if(frame.getName(name) && !name.empty()) ++named;
        if(frame.getLibOffset(library, offset, symtab)) libraries.insert(library);
      }
    }
  }

  auto const seconds = std::chrono::duration<double>(
      std::chrono::steady_clock::now() - started).count();
  std::printf("processes=%d frames=%zu named=%zu library_names=%zu "
              "time=%.3fs rss=%.1fMiB\n",
              argc - 1, frames, named, libraries.size(), seconds,
              rss_kib() / 1024.0);

  for(auto* walker : walkers) {
    dynamic_cast<ProcDebug*>(walker->getProcessState())->detach(false);
  }
}
