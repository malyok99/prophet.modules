#include <iostream>
#include <chrono>
#include <nlohmann/json.hpp>
#include <fstream>
#include <iomanip>
#include <ctime>
#include "module_interface.hpp"

using json = nlohmann::json;

static std::chrono::steady_clock::time_point start_time;
static bool tracking = false;
std::vector<std::string> tracked_editors;

extern "C" {

  void module_init(const char* module_config_json) {
    tracked_editors.clear();

    auto cfg = json::parse(module_config_json);

    if (cfg.contains("additional") && cfg["additional"].is_array()) {
      for (auto& item : cfg["additional"]) {
        tracked_editors.push_back(item.get<std::string>());
      }
    }

    std::cout << "[codetrackd] initialized with editors: ";
    for (auto& e : tracked_editors) std::cout << e << " ";
    std::cout << "\n";
  }

  void module_run(const char* dependency_data_json) {
    auto data = json::parse(dependency_data_json);

    bool ide_is_open = false;

    if (data.contains("check_if_process_running")) {
      const auto& processes = data["check_if_process_running"];
      for (auto& editor : tracked_editors) {
        if (processes.contains(editor) && processes[editor].get<bool>()) {
          ide_is_open = true;
          break;
        }
      }
    }

    if (ide_is_open && !tracking) {
      start_time = std::chrono::steady_clock::now();
      tracking = true;
      std::cout << "[codetrackd] Code editor opened, tracking started\n";
    } 
    else if (!ide_is_open && tracking) {
      auto end_time = std::chrono::steady_clock::now();
      auto duration_seconds = std::chrono::duration_cast<std::chrono::seconds>(end_time - start_time).count();
      tracking = false;

      int hours = duration_seconds / 3600;
      int minutes = (duration_seconds % 3600) / 60;

      auto t = std::time(nullptr);
      auto tm = *std::localtime(&t);
      char date_buf[11]; // YYYY-MM-DD
      std::strftime(date_buf, sizeof(date_buf), "%Y-%m-%d", &tm);

      std::ifstream in("data.json");
      json j;
      if (in.is_open()) {
        in >> j;
        in.close();
      } else {
        j = {
          {"module", "codetrackd"},
          {"data_type", "time_spent"},
          {"records", json::array()}
        };
      }

      j["records"].push_back({
          {"date", date_buf},
          {"hours", hours},
          {"minutes", minutes}
          });

      std::ofstream out("data.json");
      out << std::setw(4) << j << std::endl;

      std::cout << "[codetrackd] Code editor closed, total time: "
        << hours << "h " << minutes << "m\n";
    }
  }

  void module_stop() {
    tracking = false;
    std::cout << "[codetrackd] stopped\n";
  }

  void module_cleanup() {
    std::cout << "[codetrackd] cleaned up\n";
  }

}
