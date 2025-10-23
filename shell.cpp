#include <iostream>

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>

#include <vector>
#include <string>

#include "Tokenizer.h"

// all the basic colours for a shell prompt
#define RED     "\033[1;31m"
#define GREEN	"\033[1;32m"
#define YELLOW  "\033[1;33m"
#define BLUE	"\033[1;34m"
#define WHITE	"\033[1;37m"
#define NC      "\033[0m"

using namespace std;

int main () {
    for (;;) {
        // need date/time, username, and absolute path to current dir
        cout << YELLOW << "Shell$" << NC << " ";
        
        // get user inputted command
        string input;
        getline(cin, input);

        if (input == "exit") {  // print exit message and break out of infinite loop
            cout << RED << "Now exiting shell..." << endl << "Goodbye" << NC << endl;
            break;
        }

        // get tokenized commands from user input
        Tokenizer tknr(input);
        if (tknr.hasError()) {  // continue to next prompt if input had an error
            continue;
        }

        // // print out every command token-by-token on individual lines
        // // prints to cerr to avoid influencing autograder
        // for (auto cmd : tknr.commands) {
        //     for (auto str : cmd->args) {
        //         cerr << "|" << str << "| ";
        //     }
        //     if (cmd->hasInput()) {
        //         cerr << "in< " << cmd->in_file << " ";
        //     }
        //     if (cmd->hasOutput()) {
        //         cerr << "out> " << cmd->out_file << " ";
        //     }
        //     cerr << endl;
        // }

        // fork to create child
        pid_t pid = fork();
        if (pid < 0) {  // error check
            perror("fork");
            exit(2);
        }

        if (pid == 0) {  // if child, exec to run command
            // run single commands with no arguments
            // char* args[] = {(char*) tknr.commands.at(0)->args.at(0).c_str(), nullptr};

            // create args array from command
            Command* currentCmnd = tknr.commands.at(0);
            int numArgs = currentCmnd->args.size();
            char** args = new char*[numArgs + 1];

            for (int i = 0; i < numArgs; i++) {
                args[i] = (char*) currentCmnd->args.at(i).c_str();
            }

            args[numArgs] = nullptr;

            
            // check if file descriptors need to be changed

            if (currentCmnd->in_file != "") {
                int fd = open(currentCmnd->in_file.c_str(), O_RDONLY, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);

                if (fd < 0) {
                    perror("Failed to open input file:");
                }

                dup2(fd, 0);
                close(fd);
            }
            if (currentCmnd->out_file != "") {
                int fd = open(currentCmnd->out_file.c_str(), O_CREAT|O_WRONLY|O_TRUNC, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
                
                if (fd < 0) {
                    perror("Failed to open output file:");
                }
                
                dup2(fd, 1);
                close(fd);
            }

            if (execvp(args[0], args) < 0) {  // error check
                perror("execvp");
                exit(2);
            }

        } // end pid==0 block

        
        else {  // if parent, wait for child to finish
            int status = 0;
            waitpid(pid, &status, 0);
            if (status > 1) {  // exit if child didn't exec properly
                exit(status);
            }
        }
    }
}
