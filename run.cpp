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
  std::string real_output;
  bool passed = false;
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

struct Run_result {

  bool pass_compile = false;
  std::string compile_error;

  int total_tests = 0;
  int passed_tests = 0;
  std::vector<Test> tests_results;
  double execution_time = 0;
  int exit_code = 0;
};

Run_result run_student_code(const std::string &code,
                            const std::vector<Test> &tests) {
  Run_result result;

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
    result.pass_compile = false;
    result.compile_error = "ERROR: Cannot create file";
    return result;
  }
  file << code;
  file.close();

  if (!fs::exists(filename)) {
    result.pass_compile = false;
    result.compile_error = "ERROR: File was not created";
    return result;
  }

  if (fs::file_size(filename) == 0) {
    result.pass_compile = false;
    result.compile_error = "ERROR: File is empty";
    return result;
  }

  std::string compile_command = "docker run --rm "
                                "--memory=256m "
                                "--cpus=0.5 "
                                "--stop-timeout=5 "
                                "--network=none "
                                "--read-only "
                                "-v /tmp:/workspace "
                                "-w /workspace "
                                "silkeh/clang:latest clang++ " +
                                filename + " -o solution 2>&1";

  std::string compile_result = terminal(compile_command);
  std::string executable = "/tmp/solution";

  if (!fs::exists(executable)) {
    result.pass_compile = false;
    result.compile_error = compile_result;
    fs::remove(filename);
    return result;
  }
  result.pass_compile = true; 
  result.compile_error = "";
  std::string run_result = "docker run --rm "
                           "--memory=256m "
                           "--cpus=0.5 "
                           "--network=none "
                           "--read-only "
                           "--stop-timeout=5 "
                           "-v /tmp:/workspace "
                           "-w /workspace "
                           "silkeh/clang:latest ./solution";

  result.total_tests = tests.size();
  result.passed_tests = 0;
  std::vector<Test> test_results = tests;

  for (int i = 0; i < tests.size(); ++i) {

    std::string output = terminal(run_result, tests[i].input);

    test_results[i].real_output = output;
    if (output == tests[i].expected_output) {
      test_results[i].passed = true;
    } else {
      test_results[i].passed = false;
    }

    if (test_results[i].passed) {
      result.passed_tests++;
    }
  }

  result.tests_results = test_results;

  fs::remove(executable);
  fs::remove(filename);
  return result;
}

int main() {
  std::string students_kod = "#include <iostream>\nint main() { int a, b; "
                             "std::cin >> a >> b; std::cout << a + b; }";
  std::vector<Test> tests = {{"2 3", "5", "", false},
                             {"10 20", "30", "", false},
                             {"-5 5", "0", "", false},
                             {"100 200", "300", "", false}};

  Run_result result = run_student_code(students_kod, tests);

  if (!result.pass_compile) {
    std::cout << "COMPILATION ERROR:\n" << result.compile_error << std::endl;
    return 1;
  }

  std::cout << "Running " << result.total_tests << " tests...\n";
  for (int i = 0; i < result.tests_results.size(); ++i) {

    std::cout << "Test " << i + 1 << ": ";

    if (result.tests_results[i].passed) {
      std::cout << "PASSED\n";
    } else {
      std::cout << "FAILED\n";
      std::cout << "Expected: '" << result.tests_results[i].expected_output
                << "'\n";
      std::cout << "Got: '" << result.tests_results[i].real_output
                << "'\n";
    }
  }

  std::cout << "\n=== RESULTS ===\n";
  std::cout << "Passed: " << result.passed_tests << "/" << result.total_tests
            << " tests\n";

  if (result.passed_tests == result.total_tests) {
    std::cout << "ALL TESTS PASSED!\n";
  } else {
    std::cout << "Some tests failed.\n";
  }
}
