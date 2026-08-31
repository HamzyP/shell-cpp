#include <iostream>
#include <string>
#include <set>
#include <cstdlib>
#include <sstream>
#include <filesystem>
#include <vector>
#include <readline/readline.h>
#include <readline/history.h>
#include <cstring>
#include <map>
#include <algorithm>

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

std::map<std::string, std::string> completions;


struct ParsedCommand {
  std::vector<std::string> args;
  bool redirect_stdout = false;
  bool redirect_stderr = false;
  bool redirect_stdapp = false;
  bool redirect_stderr_append = false;
  std::string redirect_file;
};

enum class ParserState {
  normal,
  singleq,
  doubleq,
};


std::vector<std::string> tokenizer(const std::string& input){
  ParserState state = ParserState::normal;

  std::vector<std::string> tokens;
  std::string temp;
  bool escaped = false;

  for(size_t i = 0; i < input.size(); i++){
    char c = input[i];

    switch (state){
      case ParserState::normal:
        //space
        if (escaped){
          temp += c;
          escaped = false;
        }

        else if (c == ' '){
          if (!temp.empty()){
            tokens.push_back(temp);
            temp = "";
          }
        }

        //escape - bs
        else if (c == '\\'){
          escaped = true;
        }
        //sq
        else if (c == '\''){
          state = ParserState::singleq;
        }
        // dq
        else if (c == '"'){
          state = ParserState::doubleq;
        }
        // >
        else if (c == '>') {
            bool append = (i + 1 < input.size() && input[i + 1] == '>');

            std::string op;

            // Was there an explicit fd before > ?
            if (temp == "1" || temp == "2") {
                op = temp;
                temp = "";
            }
            else {
                // Normal argument before the redirect
                if (!temp.empty()) {
                    tokens.push_back(temp);
                    temp = "";
                }
            }

            if (append) {
                op += ">>";
                i++; // we've consumed the second >
            }
            else {
                op += ">";
            }
            tokens.push_back(op);
        }
        else{
          temp += c;
          
        }
        break;
      
      case ParserState::singleq:
        if (c == '\''){
          state = ParserState::normal;
        }
        else{
          temp +=c;
        }
        break;

      case ParserState::doubleq:
        if (escaped){
          if (c == '\\' || c == '"'){
            temp+=c;
          }
          else{
            temp += '\\';
            temp += c;
          }
          escaped = false;
        }
        else if (c == '"' && !escaped){
          state = ParserState::normal;
        }
        else if (c == '\\'){
          escaped = true;
        }
        else{
          temp+=c;
        }
        break;
    }

    
  }
  if (!temp.empty()){
          tokens.push_back(temp);
    }
  return tokens;
}

ParsedCommand parse_input(const std::string& input){
  ParsedCommand result;
  std::vector<std::string> tokens = tokenizer(input);

  for (size_t i = 0; i<tokens.size(); i++){
    std::string token = tokens[i];

    if(token == ">" || token == "1>"){
      //stdout overwrite
      result.redirect_stdout = true;
      result.redirect_file = tokens[i + 1];
      i++;
    }
    else if (token == "2>") {
        // stderr overwrite
        result.redirect_stderr = true;
        result.redirect_file = tokens[i + 1];
        i++;
    }
    else if (token == "2>>"){
      result.redirect_stderr_append = true;
      result.redirect_file = tokens[i+1];
      i++;
    }
    else if (token == ">>" || token == "1>>") {
        // stdout append
        result.redirect_stdapp = true;
        result.redirect_file = tokens[i + 1];
        i++;
    }
    else {
        result.args.push_back(token);
    }
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

bool execute_command(ParsedCommand& parsed, const std::set<std::string>& shell_cmds, std::map<std::string, std::string>& completions){
      std::string command;
    // iss >> command;
    command = parsed.args[0];

    if(command == "exit"){
      return false;
    }
    else if (command == "complete"){
      if (parsed.args.size() >= 3 && parsed.args[1] == "-p"){
        if (completions.find(parsed.args[2]) != completions.end()){
          std::cout << "complete -C \'" << completions[parsed.args[2]] << "\' " << parsed.args[2]  << std::endl; //output the match
        } 
        else{
          std::cout << "complete: " << parsed.args[2] << ": no completion specification" << std::endl;
          }
      }
      else if (parsed.args.size() >= 4 && parsed.args[1] == "-C"){
        std::string path = parsed.args[2];
        std::string new_cmd = parsed.args[3];
        completions[new_cmd] = path;
      } 
      else if (parsed.args.size() >= 2 && parsed.args[1] == "-r"){
        std::string key = parsed.args[2];
        completions.erase(key);
      }
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
    else if(command == "jobs"){
      std::cout << "";
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
    std::cerr << command << ": command not found" << std::endl;// no external programs and no built ins.
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

int redirect_to_file(const std::string& file, int target_fd, bool append){
  #ifdef _WIN32
    int saved_fd = _dup(target_fd);

    int mode = _O_WRONLY | _O_CREAT;

    if (append) {
      mode |= _O_APPEND;
    } else{
      mode |= _O_TRUNC;
    }

    int file_fd = _open(
        file.c_str(),
        mode,
        _S_IREAD | _S_IWRITE
    );

    _dup2(file_fd, target_fd);
    _close(file_fd);

    #else
    int saved_fd = dup(target_fd);

    int mode = O_WRONLY | O_CREAT;

    if(append){
      mode |= O_APPEND;
    } else {
      mode |= O_TRUNC;
    }

    int file_fd = open(file.c_str(),
        mode,
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


char* duplicate_string(const char* str){
  #ifdef _WIN32
    return _strdup(str);
  #else
    return strdup(str);
  #endif
}

std::vector<std::string> path_commands(){
  std::vector<std::string> commands;

  //get path
  const char* path = std::getenv("PATH");

  if (path == nullptr){
    return commands;
  }

  //split path into dirs
  #ifdef _WIN32
    char delimiter = ';';
  #else
    char delimiter = ':';
  #endif

  std::istringstream ss(path);
  std::string directory;

  // loop through dirs
  while (std::getline(ss, directory, delimiter)){
    // directory is now 1 PATH directory
    if (!std::filesystem::exists(directory)){
        continue;
      }

    for (const auto& entry : std::filesystem::directory_iterator(directory)){
      //check each file in this PATH directory
      
      if (is_executable(entry.path())) {
        commands.push_back(entry.path().filename().string());
      }


    }
  }

  //add executable filesnames to commands

  return commands;
}

char* command_generator(const char* text, int state){
  static std::vector<std::string> commands;
  static size_t index;

  if (state == 0){
    index = 0;
    commands = {"echo", "exit"};

    std::vector<std::string> external = path_commands();
    commands.insert(commands.end(), external.begin(), external.end());
  }

  while (index < commands.size()){
    std::string command = commands[index++];

    if (command.compare(0, strlen(text), text) == 0) {
      return duplicate_string(command.c_str());
    }
  }
  return nullptr;
}

std::vector<std::string> run_completer(const std::string& path, const std::string& command, const std::string& current, const std::string& previous, const std::string& line, int point){
  
  std::string call =
    "COMP_LINE='" + line +
    "' COMP_POINT='" + std::to_string(point) +
    "' " + path +
    " '" + command +
    "' '" + current +
    "' '" + previous + "'";

  #ifdef _WIN32
  FILE* pipe = _popen(call.c_str(), "r");
  #else
  FILE* pipe = popen(call.c_str(), "r");
  #endif

  if (!pipe) return {};

  char buffer [1024];
  std::vector<std::string> result;
  
  while (fgets(buffer, sizeof(buffer), pipe) != nullptr){
    std::string candidate = buffer;

    if (!candidate.empty() && candidate.back() == '\n') {
        candidate.pop_back();
    }

    result.push_back(candidate);
  }

  #ifdef _WIN32
  _pclose(pipe);
  #else
  pclose(pipe);
  #endif

  return result;
}  
std::string return_lcp_completion (std::vector<std::string> wordlist){
  if (wordlist.empty()){
    return "";
  }
  int shortest_word_size = wordlist[0].size();
  std::string lcp = "";

  for ( std::string word : wordlist){
    if (word.size() < shortest_word_size){
      shortest_word_size = word.size();
    }
  }

  for (int i = 0; i < shortest_word_size; i++){
    char letter = wordlist[0][i];
    for (int j = 0; j < wordlist.size(); j++){
      if (wordlist[j][i] != letter){
        return lcp;
      } 
    
    }
    lcp += letter;
  }

  return lcp;
}



char** completion(const char* text, int start, int end){
  if(start != 0){
    std::string line = rl_line_buffer;
    std::vector<std::string> words = tokenizer(line);
    if(words.empty()){
      return nullptr;
    }

    std::string command = words[0];
    std::string current = text;
    std::string previous = "";

    static bool first_tab = false;

    if (current.empty()){
      if (words.size() >= 2) {
        previous = words.back();
      }
    }
    else if (words.size() >=  2){
      previous = words[words.size() - 2];
    }

    // size_t space = line.find(' ');
    // std::string command = line.substr(0, space);

    if (completions.find(command) != completions.end()){
      rl_attempted_completion_over = 1;
      std::vector<std::string> candidate = run_completer(completions[command], command, current, previous, line, rl_point);

      if (candidate.empty()){
        first_tab = false;
        rl_ding();
      }
      else if(candidate.size() == 1){
        first_tab = false;

        std::string completed = line.substr(0, start) + candidate[0] + " ";

        rl_replace_line(completed.c_str(), 0);  
      }
      else {
        std::string lcp = return_lcp_completion(candidate);

        if (lcp.size() > current.size()){
          first_tab = false;

          std::string completed = line.substr(0, start) + lcp;

          rl_replace_line(completed.c_str(), 0);
        }

        else{
          first_tab = !first_tab;

          if (first_tab){
            rl_ding();
          } 
          
          else{
            std::sort(candidate.begin(), candidate.end());
            
            rl_crlf();

            for (std::string c : candidate){
              std::cout << c << "  ";
            }
            std::cout << std::endl;

            rl_on_new_line();
            rl_redisplay();
          }
        }
      }

      rl_point = rl_end;
      return nullptr;
    }
  

    return nullptr;
  }
    rl_attempted_completion_over = 1;
  return rl_completion_matches(text, command_generator);
}




int main(){
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;
  std::set<std::string> shell_cmds = {
    "echo", "type", "exit", "pwd", "cd", "complete", "jobs" };

  rl_attempted_completion_function = completion;


  while (true){
    // std::cout << "$ ";

    // std::string input;
    // std::getline(std::cin, input);

    char* line = readline("$ ");

    if(line == nullptr){
      break;
    }

    std::string input = line;
    free(line);

    ParsedCommand parsed = parse_input(input);

    if (parsed.args.empty()){
      continue;
    }

    int saved_fd = -1;

    if(parsed.redirect_stderr_append){
      saved_fd = redirect_to_file(parsed.redirect_file, 2, true);
    }

    if(parsed.redirect_stdout){
      saved_fd = redirect_to_file(parsed.redirect_file, 1, false);
    }
       if (parsed.redirect_stdapp){
        saved_fd = redirect_to_file(parsed.redirect_file, 1, true);
       }

    if(parsed.redirect_stderr){
      saved_fd = redirect_to_file(parsed.redirect_file, 2, false);
    }

    bool keep_running = execute_command(parsed, shell_cmds, completions);

    if (parsed.redirect_stdout || parsed.redirect_stdapp){
      restore_fd(saved_fd, 1);
    }
    if (parsed.redirect_stderr || parsed.redirect_stderr_append){
      restore_fd(saved_fd, 2);
    }

    if (!keep_running) {
        break;
    }
  }
}

// int main(){
//   std::vector<std::string> abc = tokenizer(std::string("echo    hello world"));
//   for (std::string word : abc){
//     std::cout << word << std::endl;
//   }
//   return 1;
// }