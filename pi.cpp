#include <io.h>

#include <atomic>
#include <cassert>
#include <chrono>
#include <csignal>
#include <cstring>
#include <iostream>
#include <mutex>
#include <random>
#include <thread>
#include <vector>

#define TIME_MEASURE 1

namespace {
class Timer {
 public:
  void start() { start_ = clock::now(); }
  void end() { end_ = clock::now(); }

  template <typename T>
  long long getInterval() {
    return std::chrono::duration_cast<T>(end_ - start_).count();
  }

 private:
  using clock = std::chrono::high_resolution_clock;
  clock::time_point start_, end_;
};

long g_total_pts;
std::atomic<long> g_hit(0);
std::atomic<long long> g_last_trigger_time;
std::atomic<bool> g_first_time_trigger(true);
std::atomic<bool> g_stop(false);

void signalHandler(int signum) {
  auto current_trigger_time_tick =
      std::chrono::high_resolution_clock::now().time_since_epoch().count();
  if (!g_first_time_trigger) {
    if (current_trigger_time_tick - g_last_trigger_time <
        1000000000) {  // 1 second in nanoseconds
      g_stop = true;
      return;
    }
  } else {
    g_first_time_trigger = false;
  }
  g_last_trigger_time = current_trigger_time_tick;

  char str[20];
  int len = snprintf(str, sizeof(str), "pi = %f\n", 4.0 * g_hit / g_total_pts);
  write(1, str, len);

  std::signal(SIGINT, signalHandler);  // reinstall
}

void assignPoints(long points) {
  std::mt19937 rand_engine(std::random_device{}());
  long hit = 0;

  double x, y;
  for (long i = 0; i < points; i++) {
    if (g_stop) {
      return;
    }
    x = static_cast<double>(rand_engine()) / rand_engine.max();
    y = static_cast<double>(rand_engine()) / rand_engine.max();
    if ((x * x + y * y) < 1.0) {
      ++hit;
    }
  }
  g_hit += hit;
}
}  // namespace

int main(int argc, char* argv[]) {
  assert(argc == 3);
  g_total_pts = std::stol(argv[1]);
  int nthreads = std::stoi(argv[2]);

  if (g_total_pts < 1 || nthreads < 1) {
    printf("Arguments must be positive integers.\n");
    return EXIT_FAILURE;
  }

  std::signal(SIGINT, signalHandler);

#if TIME_MEASURE
  Timer timer;
  timer.start();
#endif

  std::vector<std::thread> workers;
  workers.reserve(nthreads);

  for (size_t i = 0; i < nthreads; ++i) {
    workers.emplace_back(assignPoints, g_total_pts / nthreads);
  }
  for (auto& worker : workers) {
    worker.join();
  }

#if TIME_MEASURE
  timer.end();
  printf("elapsed time = %lld ns\n",
         timer.getInterval<std::chrono::nanoseconds>());
#endif

  printf("pi = %f\n", 4.0 * g_hit / g_total_pts);
}