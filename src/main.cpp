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
void launch_program_windows(const std::string& command_path, std::vector<std::string>& input_v){
  //_spawnv() does not accept vector<string> so we do C-style strings 
  std::vector<const char*> argv;
  for (const std::string& arg : input_v) {
    argv.push_back(arg.c_str());
  }

  argv.push_back(nullptr); // so spawnv knows where the arguments stop.

  _spawnv(_P_WAIT, command_path.c_str(), argv.data()); //P_WAIT means wait until program finishes then return here to shell.
  
}

#else

void launch_program_linux(const std::string& command_path, std::vector<std::string>& input_v){
  //execv does not accept vector<string>, so C-style string
  std::vector<char*> argv;

  for (std::string& arg : input_v){
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
    std::vector<std::string> input_v;
    std::string temp;
    std::cout << "$ ";
    std::getline(std::cin, input);

    //add our own parser here
    // std::istringstream iss(input);
    bool in_single_quote = false;
    bool in_double_quote = false;


    for (char c : input){

      if ( c == '\'' && !in_double_quote){
        in_single_quote = !in_single_quote;
      }
      else if (c == '"' && !in_single_quote){
        in_double_quote = !in_double_quote;
      }

      else if (c == ' '){
        if (in_single_quote || in_double_quote){
          temp += c;
        }

        else{
          if (!temp.empty()){
            input_v.push_back(temp);
            temp = "";
          }
        }
      }

      else{
        temp += c;
      }
      
    }
    if (!temp.empty()){
      input_v.push_back(temp);
    }

    std::string command;
    // iss >> command;
    command = input_v[0];

    if(command == "exit"){
      break;
    }

    else if(command == "echo"){
      for (size_t i = 1; i < input_v.size(); i++){
        std::cout << input_v[i];

        if (i < input_v.size() - 1){
          std::cout << ' ';
        }

      }

      std::cout << std::endl;
    }

    else if(command == "type"){
      std::string argument = input_v[1];
      handle_type(argument, shell_cmds);
    }

    else if(command == "pwd"){
      // call OS for the path and output
      std::cout << std::filesystem::current_path().string() << std::endl; // works on both linux and windows
    }
    else if(command == "cd"){
      //check if valid path
      std::string arg_path = input_v[1];

      if (arg_path == "~"){
        #ifdef _WIN32
        arg_path = std::getenv("USERPROFILE");
        #else
        arg_path = std::getenv("HOME");

        #endif
      }


      if (std::filesystem::exists(arg_path) && std::filesystem::is_directory(arg_path)){ //valid path and is dir
        std::filesystem::current_path(arg_path); //change the dir (overload func)
      }
      else{
        std::cout << "cd: " << arg_path <<": No such file or directory" << std::endl;
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
      launch_program_windows(command_path, input_v);
      #else
      launch_program_linux(command_path, input_v);
      #endif
    }
  }

  }
}


