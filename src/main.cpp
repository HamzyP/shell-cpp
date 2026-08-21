#include <iostream>
#include <string>
#include <set>
#include <cstdlib>
#include <sstream>
#include <filesystem>

#ifdef _WIN32
#else
#include <unistd.h>
#endif

bool is_executable(const std::filesystem::path& path){
  #ifdef _WIN32
  return std::filesystem::is_regular_file(path);
  #else //for linux
  return access(path.c_str(), X_OK) == 0;
  #endif
}

std::string find_command(const std::string& command){
      #ifdef _WIN32
      char delimiter = ';';
      #else
      char delimiter = ':';
      #endif

      std::string directory; 

      const char* path = std::getenv("PATH"); // get the all paths as a huge string
      std::istringstream ss(path);
      while(std::getline (ss, directory, delimiter)){ // loop through all possible file paths using delimiter
        // std::cout << directory << std::endl ; // output each file path
        std::string candidate = directory + "/" + command;
        if (std::filesystem::exists(candidate)){ // candidate exists
          if (is_executable(candidate)){//candidate is executable
            return candidate;
          }

        }
      }
      return ""; //found nothing
}

int main() {
  // Flush after every std::cout / std:cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  std::string input;
  std::set<std::string> shell_cmds = {"echo", "type", "exit"};

  while(true){

    std::cout << "$ ";
    std::getline(std::cin, input);

    if(input == "exit"){
      break;
    }

    else if(input.substr(0,5) == "echo "){
      std::cout << input.substr(5) << std::endl;
    }
    else if(input.substr(0,5) == "type "){
      if (shell_cmds.count(input.substr(5))){
        std::cout << input.substr(5) << " is a shell builtin" << std::endl;
      } else {
        std::string command_path = find_command(input.substr(5));
        if (command_path != ""){
          std::cout << input.substr(5) << " is " << command_path << std::endl;
        }else{
        std::cout << input.substr(5) << ": not found" << std::endl;
        }
      }
    }

    else{
    std::cout << input << ": command not found" << std::endl;
    }
  }
}


