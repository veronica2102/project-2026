#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <string>
#include <vector>

namespace fs = std::filesystem;

struct Test {
  std::string input;
  std::string expected_output;
  bool passed;
};

std::string error(int status) {
  if (status == 0)
    return "Успешно :)";
  if (status > 128) {
    int signal = status - 128;
    switch (signal) {
    case 1:
      return "Hangup";
    case 2:
      return "Interrupt";
    case 3:
      return "Quit";
    case 4:
      return "Illegal instruction";
    case 5:
      return "Trace trap";
    case 6:
      return "Abort";
    case 8:
      return "Floating point exception";
    case 9:
      return "Killed";
    case 11:
      return "Segmentation fault";
    case 13:
      return "Broken pipe";
    case 15:
      return "Terminated";
    default:
      return "Сигнал " + std::to_string(signal);
    }
  }

  switch (status) {
  case 1:
    return "General error";
  case 2:
    return "No such file or directory";
  case 126:
    return "Command cannot execute";
  case 127:
    return "Command not found";
  case 128:
    return "Invalid exit argument";
  default:
    return "Error code " + std::to_string(status);
  }
}

std::string terminal(std::string command, std::string input = "") {
  char buffer[128];
  std::string result = "";
  std::string full_command;
  if (!input.empty()) {
    full_command = "echo '" + input + "' | " + command;
  } else {
    full_command = command;
  }
  FILE *pipe = popen(full_command.c_str(), "r");

  if (!pipe) {
    return "ERROR: Failed to run command";
  }

  while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
    result += buffer;
  }

  int status = pclose(pipe);
  if (status != 0) {
    result += "\n" + error(status);
  }

  return result;
}

int main() {
  std::string students_kod = "#include <iostream>\nint main() { int a, b; "
                             "std::cin >> a >> b; std::cout << a + b; }";
  std::vector<Test> tests = {{"2 3", "5", false},
                             {"10 20", "30", false},
                             {"-5 5", "0", false},
                             {"100 200", "300", false}};
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

  std::string compine_command = "docker run --rm "
                                "--memory=256m "
                                "--cpus=0.5 "
                                "--stop-timeout=5 "
                                "--network=none "
                                "--read-only "
                                "-v /tmp:/workspace "
                                "-w /workspace "
                                "silkeh/clang:latest clang++ " +
                                filename + " -o solution 2>&1";

  std::string compine_result = terminal(compine_command);
  std::string executable = "/tmp/solution";
  if (!fs::exists(executable)) {
    std::cout << "COMPILATION ERROR:\n" << compine_result << std::endl;
    fs::remove(filename);
    return 1;
  }
  std::cout << "Running " << tests.size() << " tests...\n";
  std::string run_result = "docker run --rm "
                           "--memory=256m "
                           "--cpus=0.5 "
                           "--network=none "
                           "--read-only "
                           "--stop-timeout=5 "
                           "-v /tmp:/workspace "
                           "-w /workspace "
                           "silkeh/clang:latest ./solution";
  int passed_tests = 0;
  for (int i = 0; i < tests.size(); ++i) {
    std::cout << "Test " << i + 1 << ": ";
    std::string output = terminal(run_result, tests[i].input);
    if (output == tests[i].expected_output) {
      tests[i].passed = true;
      passed_tests++;
      std::cout << "PASSED\n";
    } else {
      tests[i].passed = false;
      std::cout << "FAILED\n";
      std::cout << "Expected: '" << tests[i].expected_output << "'\n";
      std::cout << "Got:      '" << output << "'\n";
    }
  }
  std::cout << "\n=== RESULTS ===\n";
  std::cout << "Passed: " << passed_tests << "/" << tests.size() << " tests\n";

  if (passed_tests == tests.size()) {
    std::cout << "ALL TESTS PASSED!\n";
  } else {
    std::cout << "Some tests failed.\n";
  }

  fs::remove(executable);
  fs::remove(filename);
}
