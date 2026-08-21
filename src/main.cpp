#include <iostream>
#include <string>

int main() {
  // Flush after every std::cout / std:cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  std::string input;

  std::cout << "$ ";
  std::cin >> input;

  std::cout << input << ": command not found";
}
