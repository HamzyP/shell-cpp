# C++ Shell

A **cross-platform command shell built from scratch in C++**, implementing process execution, parsing, pipelines, job control, completion, persistent history and parameter expansion.

Supports both **Linux/POSIX and Windows**, with platform-specific process, pipe and file-descriptor handling.

## Highlights

* External program execution using `fork()`, `execv()` and `waitpid()` on Linux
* Windows execution using `_spawnv`, `_pipe` and Win32 process APIs
* Multi-stage pipelines, including shell built-ins
* Background jobs with process tracking and automatic reaping
* Custom tokenizer supporting quoting, escaping and stdout/stderr redirection
* Built-ins including `cd`, `pwd`, `echo`, `type`, `jobs`, `history`, `declare` and `complete`
* Command and programmable tab completion using GNU Readline
* Persistent command history with `HISTFILE`
* Shell variables with `$VAR` and `${VAR}` parameter expansion

```bash
$ declare NAME=world
$ echo hello_$NAME
hello_world

$ cat file.txt | head -n 5 | grep hello

$ sleep 10 &
[1] 12345
```

## Build & Run

Built with CMake. The project currently targets C++23, although the implementation primarily relies on C++17-era language and standard library features.

On Unix-like environments:

```bash
./your_program.sh
```

The main implementation is in `src/main.cpp`.

---

Built while completing the CodeCrafters Build Your Own Shell challenge.
