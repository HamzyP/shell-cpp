#include <iostream>
#include <string>
#include <set>
#include <cstdlib>
#include <sstream>
#include <filesystem>
#include <vector>

#ifdef _WIN32
#include <process.h>
#include <io.h>
#include <fcntl.h>
#include <sys/stat.h>
#else
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#endif

struct ParsedCommand {
  std::vector<std::string> args;
  bool redirect_stdout = false;
  bool redirect_stderr = false;
  std::string redirect_file;
};

ParsedCommand parse_input(const std::string& input){
    ParsedCommand result;
    std::string temp;

    //add our own parser here
    // std::istringstream iss(input);
    bool in_single_quote = false;
    bool in_double_quote = false;
    bool escaped = false;


    for (char c : input){
      if (c == '>' && !in_single_quote && !in_double_quote){
        

        if(temp == "1"){
          temp = "";
          result.redirect_stdout = true;
        }
        else if (temp == "2"){
          temp = "";
          result.redirect_stderr = true;
        }
        else if (!temp.empty()){
          result.args.push_back(temp);
          result.redirect_stdout = true;
          temp = "";
        }

        continue;
      }
      if (result.redirect_stdout || result.redirect_stderr){
        if (c == ' ' && result.redirect_file.empty()){
          continue;
        }

        result.redirect_file += c;
        continue;
      }
      if (escaped){
        if (in_double_quote){
          if (c == '"' || c == '\\'){
            temp +=c;
          } 
          else {
            temp += '\\';
            temp += c;
          }
        }
        else{
          temp +=c;
        }
          escaped = false;
      }
      else if ( c == '\\' && !in_single_quote){
        escaped = true;
      }

      else if ( c == '\'' && !in_double_quote){
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
            result.args.push_back(temp);
            temp = "";
          }
        }
      }

      else{
        temp += c;
      }
      
    }
    if (!temp.empty()){
      result.args.push_back(temp);
    }


  return result;
}


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
void launch_program_windows(const std::string& command_path, std::vector<std::string>& args){
  //_spawnv() does not accept vector<string> so we do C-style strings 
  std::vector<const char*> argv;
  for (const std::string& arg : args) {
    argv.push_back(arg.c_str());
  }

  argv.push_back(nullptr); // so spawnv knows where the arguments stop.

  _spawnv(_P_WAIT, command_path.c_str(), argv.data()); //P_WAIT means wait until program finishes then return here to shell.
  
}

#else

void launch_program_linux(const std::string& command_path, std::vector<std::string>& args){
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

bool execute_command(ParsedCommand& parsed, const std::set<std::string>& shell_cmds){
      std::string command;
    // iss >> command;
    command = parsed.args[0];

    if(command == "exit"){
      return false;
    }

    else if(command == "echo"){
      for (size_t i = 1; i < parsed.args.size(); i++){
        std::cout << parsed.args[i];

        if (i < parsed.args.size() - 1){
          std::cout << ' ';
        }

      }

      std::cout << std::endl;
    }

    else if(command == "type"){
      std::string argument = parsed.args[1];
      handle_type(argument, shell_cmds);
    }

    else if(command == "pwd"){
      // call OS for the path and output
      std::cout << std::filesystem::current_path().string() << std::endl; // works on both linux and windows
    }
    else if(command == "cd"){
      //check if valid path
      std::string arg_path = parsed.args[1];

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
        std::cerr << "cd: " << arg_path <<": No such file or directory" << std::endl;
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
      launch_program_windows(command_path, parsed.args);
      #else
      launch_program_linux(command_path, parsed.args);
      #endif
    }
  }

  return true;

}

int redirect_to_file(const std::string& file, int target_fd){
  #ifdef _WIN32
    int saved_fd = _dup(target_fd);

    int file_fd = _open(
        file.c_str(),
        _O_WRONLY | _O_CREAT | _O_TRUNC,
        _S_IREAD | _S_IWRITE
    );

    _dup2(file_fd, target_fd);
    _close(file_fd);
    #else
    int saved_fd = dup(target_fd);

    int file_fd = open(file.c_str(),O_WRONLY | O_CREAT | O_TRUNC,
        0644); 
    
        dup2(file_fd, target_fd);
        close(file_fd);
    #endif

      return saved_fd;
}

void restore_fd(int saved_fd, int target_fd){
  std::cout.flush();

  #ifdef _WIN32
  _dup2(saved_fd, target_fd);
  _close(saved_fd);
  #else
  dup2(saved_fd, target_fd);
  close(saved_fd);
  #endif
}


int main(){
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;
  std::set<std::string> shell_cmds = {
    "echo", "type", "exit", "pwd", "cd" };

  while (true){
    std::cout << "$ ";

    std::string input;
    std::getline(std::cin, input);

    ParsedCommand parsed = parse_input(input);

    if (parsed.args.empty()){
      continue;
    }

    int saved_fd = -1;

    if(parsed.redirect_stdout){
      saved_fd = redirect_to_file(parsed.redirect_file, 1);
    }

    if(parsed.redirect_stderr){
      saved_fd = redirect_to_file(parsed.redirect_file, 2);
    }

    bool keep_running = execute_command(parsed, shell_cmds);

    if (parsed.redirect_stdout){
      restore_fd(saved_fd, 1);
    }
    if (parsed.redirect_stderr){
      restore_fd(saved_fd, 2);
    }

    if (!keep_running) {
        break;
    }
  }
}