#include <iostream>

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>

#include <time.h>
#include <unistd.h>

#include <vector>
#include <string>
#include <string_view>

#include "Tokenizer.h"

// all the basic colours for a shell prompt
#define RED     "\033[1;31m"
#define GREEN	"\033[1;32m"
#define YELLOW  "\033[1;33m"
#define BLUE	"\033[1;34m"
#define WHITE	"\033[1;37m"
#define NC      "\033[0m"

using namespace std;

string getPrompt() {
    time_t timer;
    string timeString;
    timer = time(NULL);
    timeString = ctime(&timer);

    timeString = timeString.substr(4, 15);

    string user = getenv("USER");

    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        perror("getcwd error: ");
    }

    string cwdString = string(cwd);

    return timeString + " " + user + ":" + cwdString + "$";

}

vector<string> getChunks(string input) {
    vector<string> chunks = vector<string>();
    size_t pos = 0;
    string chunk;
    string delimiter = "&&";
    while ((pos = input.find(delimiter)) != string::npos) {
        chunk = input.substr(0, pos);
        chunks.push_back(chunk);
        input.erase(0, pos + delimiter.length());
    }
    chunks.push_back(input);

    return chunks;
}

int main () {

    vector<int> backPIDS = vector<int>();

    for (;;) {
        // need date/time, username, and absolute path to current dir
        cout << YELLOW << getPrompt() << NC << " ";
        
        // get user inputted command
        string input;
        getline(cin, input);

        if (input == "exit") {  // print exit message and break out of infinite loop
            cout << RED << "Now exiting shell..." << endl << "Goodbye" << NC << endl;
            break;
        }

        // split commands by && 
        vector<string> chunks = getChunks(input);
        
        for (long unsigned int i = 0; i < chunks.size(); i++) {

            input = chunks.at(i);

            // get tokenized commands from user input
            Tokenizer tknr(input);
            if (tknr.hasError()) {  // continue to next prompt if input had an error
                continue;
            }


            // check background processes
            vector<int> remainingPIDs = vector<int>();
            for (int pid : backPIDS) {
                int status;
                pid_t result = waitpid(pid, &status, WNOHANG);

                if (result != pid) {
                    remainingPIDs.push_back(pid);
                }
                else {
                    // cout << "Process Complete: " << pid << endl;
                }
            }
            backPIDS = remainingPIDs;
            
            
            // loop through each command in the pipe

            int numCmnds = tknr.commands.size();
            int savedStdin = dup(STDIN_FILENO);
            int savedStdout = dup(STDOUT_FILENO);

            // cout << "NumCmnds: " << numCmnds << endl;

            for (int current = 0; current < numCmnds; current++) {

                Command* currentCmnd = tknr.commands.at(current);

                // create pipe 
                int pipeFds[2];
                if (pipe(pipeFds) == -1) {
                    perror("Pipe creation failed");
                    exit(2);
                }

                // handle exit commnad
                if (currentCmnd->args.at(0) == "exit") {
                    cout << RED << "Now exiting shell..." << endl << "Goodbye" << NC << endl;
                    exit(0);
                }

                // handle cd command
                if (currentCmnd->args.at(0) == "cd") {
                    chdir((char*) currentCmnd->args.at(1).c_str());
                    continue;
                }

                // fork to create child
                pid_t pid = fork();
                if (pid < 0) {  // error check
                    perror("fork");
                    exit(2);
                }

                if (pid == 0) {  // child

                    // hook to pipe or terminal
                    
                    close(pipeFds[0]); // closing read end

                    if (current == numCmnds - 1) {
                        dup2(savedStdout, STDOUT_FILENO);
                    }
                    else {
                        dup2(pipeFds[1], STDOUT_FILENO);
                    }
                    close(pipeFds[1]); // closing duplicated fd

                    // create args array from command
                    int numArgs = currentCmnd->args.size();
                    char** args = new char*[numArgs + 1];

                    for (int i = 0; i < numArgs; i++) {
                        args[i] = (char*) currentCmnd->args.at(i).c_str();
                    }

                    args[numArgs] = nullptr;

                    
                    // check if file descriptors need to be changed

                    if (currentCmnd->in_file != "") {
                        int fd = open(currentCmnd->in_file.c_str(), O_RDONLY);

                        // cout << currentCmnd->in_file << endl;
                        if (fd < 0) {
                            perror("Failed to open input file");
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
                        cout << input << endl;
                        perror("execvp");
                        exit(2);
                    }

                } // end pid==0 block


                else {  // if parent, wait for child to finish

                    
                    close(pipeFds[1]); // closing write end of pipe
                    dup2(pipeFds[0], STDIN_FILENO);
                    close(pipeFds[0]); // closing duplicated read end

                    if (current != numCmnds - 1) { 
                        continue;
                    }

                    // restore original stdin
                    dup2(savedStdin, STDIN_FILENO);

                    if (currentCmnd->isBackground()) {
                        backPIDS.push_back(pid);
                    }
                    else {
                        int status = 0;
                        waitpid(pid, &status, 0);
                        if (status > 1) {  // exit if child didn't exec properly
                            exit(status);
                        }
                    }

                }

            } // end commands loop

        } // end chunks loops
    }
}
