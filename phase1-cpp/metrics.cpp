#include <iostream>
#include <fstream>
#include <string>
#include <thread>
#include <chrono>

int main() {
    std::ifstream file;

    // Read CPU stats from /proc/stat
    file.open("/proc/stat");
    if (!file.is_open()) {
        std::cerr << "Failed to open /proc/stat" << std::endl;
        return 1;
    }

    std::string label;
    long long user, nice, system, idle, iowait, irq, softirq, steal;
    file >> label >> user >> nice >> system >> idle >> iowait >> irq >> softirq >> steal;
    file.close();

    long long total1 = user + nice + system + idle + iowait + irq + softirq + steal;
    long long idle1 = idle + iowait;

    // Sleep for a short period to get a second reading
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // Read CPU stats again
    file.clear();
    file.open("/proc/stat");
    if (!file.is_open()) {
        std::cerr << "Failed to open /proc/stat" << std::endl;
        return 1;
    }

    file >> label >> user >> nice >> system >> idle >> iowait >> irq >> softirq >> steal;
    file.close();

    long long total2 = user + nice + system + idle + iowait + irq + softirq + steal;
    long long idle2 = idle + iowait;

    // Calculate CPU usage percentage
    long long delta_total = total2 - total1;
    long long delta_idle = idle2 - idle1;
    double cpu_percent = 100.0 * (delta_total - delta_idle)/delta_total;

    // Read memory stats from /proc/meminfo
    file.clear();
    file.open("/proc/meminfo");
    if (!file.is_open()) {
        std::cerr << "Failed to open /proc/meminfo" << std::endl;
        return 1;
    }

    std::string key;
    long long mem_total_kb = 0, mem_available_kb = 0;
    while (file >> key) {
        if (key == "MemTotal:") {
            file >> mem_total_kb;
        } else if (key == "MemAvailable:") {
            file >> mem_available_kb;
        }

        if (mem_total_kb && mem_available_kb) {
            break;
        }
    }
    file.close();

    // Calculate memory usage percentage
    double mem_percent = 100.0 * (mem_total_kb - mem_available_kb) / mem_total_kb;
    long long mem_used_kb = mem_total_kb - mem_available_kb;

    std::cout << "{\"cpu_percent\":" << cpu_percent
              << ",\"mem_percent\":" << mem_percent
              << ",\"mem_used_kb\":" << mem_used_kb
              << "}";

    return 0;
}
