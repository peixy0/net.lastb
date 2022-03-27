#include "app.hpp"
#include <spdlog/spdlog.h>
#include <utmp.h>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <mutex>
#include <nlohmann/json.hpp>
#include <thread>
#include <vector>
#include "network.hpp"

namespace application {

AppLayer::AppLayer() {
  StartDaemon();
}

network::HttpResponse AppLayer::Process(const network::HttpRequest& req) {
  spdlog::debug("app received request {} {} {}", req.method, req.uri, req.version);
  for (const auto& [field, value] : req.headers) {
    spdlog::debug("app received header {}: {}", field, value);
  }
  if (req.uri == "/") {
    std::shared_lock l{mutex};
    return network::PlainTextHttpResponse{network::HttpStatus::OK, *content};
  }
  return network::PlainTextHttpResponse{network::HttpStatus::NotFound, ""};
}

void AppLayer::StartDaemon() {
  daemon = std::thread{[this] { DaemonTask(); }};
}

void AppLayer::DaemonTask() {
  while (true) {
    std::vector<utmp> entries;
    utmpname("/var/log/btmp");
    setutent();
    for (;;) {
      auto ent = getutent();
      if (not ent) {
        break;
      }
      entries.emplace_back(*ent);
    }
    endutent();

    std::string result;
    int n = 0;
    for (auto it = entries.rbegin(); it != entries.rend() and n < 50; it++) {
      time_t t = it->ut_tv.tv_sec;
      char timebuf[50];
      std::strftime(timebuf, sizeof timebuf, "%c %Z", std::gmtime(&t));
      result += timebuf;
      result += " ";
      result += it->ut_line;
      result += " ";
      result += it->ut_host;
      result += "\t";
      result += it->ut_user;
      result += "\n";
      n++;
    }

    auto s = std::make_shared<std::string>(result);
    {
      std::unique_lock l{mutex};
      content = s;
    }
    {
      using namespace std::literals::chrono_literals;
      std::this_thread::sleep_for(10min);
    }
  }
}

}  // namespace application
