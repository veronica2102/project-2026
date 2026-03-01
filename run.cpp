#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <string>

namespace fs = std::filesystem;

std::string terminal(std::string command) {
  char buffer[128];
  std::string result = "";
  FILE *pipe = popen(command.c_str(), "r");

  if (!pipe) {
    return "ERROR: Failed to run command";
  }

  while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
    result += buffer;
  }

  int status = pclose(pipe);
  if (status != 0) {
    result += "\nCommand exited with status: " + std::to_string(status);
  }

  return result;
}

int main() {
  std::string students_kod =
      "#include <iostream>\nint main() { std::cout << \"Hello world\"; }";

  auto now = std::chrono::system_clock::now();
  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                now.time_since_epoch())
                .count();

  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<> dis(1000, 9999);
  int random_num = dis(gen);

  std::string filename = "/tmp/solution_" + std::to_string(ms) + "_" +
                         std::to_string(random_num) + ".cpp";

  std::ofstream file(filename);
  if (!file) {
    std::cerr << "ERROR: Cannot create file" << std::endl;
    return 1;
  }
  file << students_kod;
  file.close();

  if (!fs::exists(filename)) {
    std::cerr << "ERROR: File was not created" << std::endl;
    return 1;
  }

  if (fs::file_size(filename) == 0) {
    std::cerr << "ERROR: File is empty" << std::endl;
    return 1;
  }

  std::string command = "docker run --rm "
                        "-v /tmp:/workspace "
                        "-w /workspace "
                        "silkeh/clang:latest bash -c \"clang++ " +
                        filename + " -o solution && ./solution\"";

  std::string result = terminal(command);
  std::cout << result;

  fs::remove(filename);
}
