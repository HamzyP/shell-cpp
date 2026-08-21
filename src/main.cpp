#include <iostream>
#include <string>
#include <set>

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
        std::cout << input.substr(5) << ": not found" << std::endl;
      }
    }

    else{
    std::cout << input << ": command not found" << std::endl;
    }
  }
}
