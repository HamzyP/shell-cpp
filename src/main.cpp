#include <iostream>
#include <string>
#include <set>
#include <cstdlib>
#include <sstream>
#include <filesystem>
#include <vector>

#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#include <sys/wait.h>
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
        
        #ifdef _WIN32
        std::filesystem::path candidate = std::filesystem::path(directory) / (command + ".exe");
        #else
        std::filesystem::path candidate = std::filesystem::path(directory) / command;
        #endif

        if (std::filesystem::exists(candidate)){ // candidate exists
          if (is_executable(candidate)){//candidate is executable
            return candidate.string();
          }

        }
      }
      return ""; //found nothing
}


void handle_type(const std::string& argument, const std::set<std::string>& shell_cmds){
      if (shell_cmds.count(argument)){
        std::cout << argument << " is a shell builtin" << std::endl;
      } else {
        std::string command_path = find_command(argument);
        if (command_path != ""){
          std::cout << argument << " is " << command_path << std::endl;
        }else{
        std::cout << argument << ": not found" << std::endl;
        }
      }
}

#ifdef _WIN32
void launch_program_windows(const std::string& command_path, const std::string& command, std::istringstream& iss){
  //split arguments
  std::vector<std::string> args;

  args.push_back(command);
  
  std::string argument;
  while (iss >> argument) {
    args.push_back(argument);
  }

  //_spawnv() does not accept vector<string> so we do C-style strings 
  std::vector<const char*> argv;
  for (const std::string& arg : args) {
    argv.push_back(arg.c_str());
  }

  argv.push_back(nullptr); // so spawnv knows where the arguments stop.

  _spawnv(_P_WAIT, command_path.c_str(), argv.data()); //P_WAIT means wait until program finishes then return here to shell.
  
}

#else

void launch_program_linux(const std::string& command_path, const std::string& command, std::istringstream& iss){
  //split arguments
  std::vector<std::string> args;
  args.push_back(command); //argv[0] is program name

  std::string argument;

  while (iss >> argument){
    args.push_back(argument);
  }

  //execv does not accept vector<string>, so C-style string
  std::vector<char*> argv;

  for (std::string& arg : args){
    argv.push_back(arg.data());
  }

  argv.push_back(nullptr); //so execv knows where the arguments stop

  //fork() makes a copy of our shell process
  pid_t pid = fork();
  if (pid == 0) {
    //this is child process

    //replace child process with external program
    execv(command_path.c_str(), argv.data());

    //if we reach here, execv failed
    perror("execv");
    _exit(1);
  }
  else if (pid >0){
    //this must be original shell process
    // wait until external finishes
    waitpid(pid, nullptr, 0);
}
}
#endif

int main() {
  // Flush after every std::cout / std:cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  std::string input;
  std::set<std::string> shell_cmds = {"echo", "type", "exit", "pwd", "cd"};

  while(true){

    std::cout << "$ ";
    std::getline(std::cin, input);
    std::istringstream iss(input);
    std::string command;
    iss >> command;

    if(command == "exit"){
      break;
    }

    else if(command == "echo"){
      std::cout << input.substr(5) << std::endl;
    }

    else if(command == "type"){
      std::string argument;
      iss >> argument; //only one word as an argument
      handle_type(argument, shell_cmds);
    }

    else if(command == "pwd"){
      // call OS for the path and output
      std::cout << std::filesystem::current_path().string() << std::endl; // works on both linux and windows
    }
    else if(command == "cd"){
      //check if valid path
      std::string arg_path;
      iss >> arg_path; //only one word as an argument

      if (std::filesystem::exists(arg_path) && std::filesystem::is_directory(arg_path)){ //valid path and is dir
        std::filesystem::current_path(arg_path); //change the dir (overload func)
      }
      else{
        std::cout << "cd: " << arg_path <<": No such file or directory";
      }
      // if not valid print error message and dont change dir
    }

    else{
      std::string command_path = find_command(command);

      if (command_path == ""){
    std::cout << command << ": command not found" << std::endl;// no external programs and no built ins.
    }
    else{
      #ifdef _WIN32
      launch_program_windows(command_path, command, iss);
      #else
      launch_program_linux(command_path, command, iss);
      #endif
    }
  }

  }
}


