#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>

typedef struct {
    char *name;
    char *value;
} EnvVar;

typedef struct {
    char *name;
    int returnValue;
    struct tm time;
} Command;

char* getANSIColorCode(const char* color) {
    if (strcmp(color, "red") == 0){
        return "\033[31m";
    }
    else if (strcmp(color, "green") == 0){
        return "\033[32m";
    }
    else if (strcmp(color, "blue") == 0){
        return "\033[34m";
    }
    else{
        return NULL;
    }
}

int main(int argc, char *argv[]) {
    char *prompt = "cshell$ ";
    char *lineptr = NULL;
    char* exitColor = NULL;

    size_t n = 0;
    int exit_requested = 0;
    int script_mode = 0;

    EnvVar envVars[100];
    int envVarCount = 0;

    Command commandHistory[100];
    int commandCount = 0;

    char* themeColor = NULL;

    // for script mode
    FILE *scriptFile = NULL;
    if (argc == 2) {
        scriptFile = fopen(argv[1], "r");
        if (scriptFile == NULL) {
            printf("Error: Failed to open script file '%s'.\n", argv[1]);
            return 1;
        }
        script_mode = 1;
    }

    while (!exit_requested) {
        if (themeColor != NULL) {
            printf("%s", themeColor);
        }
        if (scriptFile == NULL) {
            printf("%s", prompt);
        }

        printf("\033[37m"); // default color for user input

        if (scriptFile != NULL) {
            if (getline(&lineptr, &n, scriptFile) == -1) {
                exit_requested = 1;
                break; 
            }
        } else {
            if (getline(&lineptr, &n, stdin) == -1) {
                break; 
            }
        }

        lineptr[strcspn(lineptr, "\n")] = '\0';

        if (themeColor != NULL) {
            printf("%s", themeColor);
        }

        // storing the first word separately
        char *program = strtok(lineptr, " "); 
        if (program != NULL) {
            char *firstWord = strdup(program);

            // storing the first word in the command history
            commandHistory[commandCount].name = firstWord;
            time_t currentTime = time(NULL);
            struct tm* timestamp = localtime(&currentTime);
            commandHistory[commandCount].time = *timestamp; 
            commandHistory[commandCount].returnValue = 0; 
            commandCount++;
        }
        // built-in commands
        if (strcmp(program, "exit") == 0) {
            // exit
            char *argument = strtok(NULL, " ");
            if (argument == NULL) {
                printf("Bye!\n");
                exit_requested = 1;
            } else {
                printf("Error: Too Many Arguments Detected\n");
                commandHistory[commandCount-1].returnValue = -1;
            }
        } else if (lineptr[0] == '$') {
            // environment variable 
            char *varName = lineptr + 1;
            char *equalsSign = strchr(lineptr, '=');

            if (equalsSign != NULL && equalsSign != varName && equalsSign[1] != '\0') {
                // to check if there are additional equal signs 
                char *nextEqualsSign = strchr(equalsSign + 1, '=');
                if (nextEqualsSign != NULL) {
                    printf("Error: Invalid variable assignment. Multiple or separated equal signs are not allowed.\n");
                    commandHistory[commandCount-1].returnValue = -1;
                } else {
                    *equalsSign = '\0'; 
                    char *varValue = equalsSign + 1;

                    // to check for invalid characters in varName
                    int isValidVarName = 1;
                    for (int i = 0; varName[i] != '\0'; i++) {
                        if (!((varName[i] >= 'a' && varName[i] <= 'z') ||
                            (varName[i] >= 'A' && varName[i] <= 'Z') ||
                            (varName[i] >= '0' && varName[i] <= '9') ||
                            varName[i] == '_' ||
                            varName[i] == '$')) {
                            isValidVarName = 0;
                            break;
                        }
                    }

                    if (!isValidVarName) {
                        printf("Error: Invalid variable name. Only alphanumeric characters, '$', and '_' are allowed.\n");
                        commandHistory[commandCount-1].returnValue = -1;
                    } else {
                        // to check for leading and trailing spaces in varValue
                        int hasLeadingSpaces = strspn(varValue, " ") > 0;
                        int hasTrailingSpaces = strcspn(varValue, " ") < strlen(varValue);

                        if (hasLeadingSpaces || hasTrailingSpaces) {
                            printf("Error: Invalid variable assignment. Leading or trailing spaces not allowed.\n");
                            commandHistory[commandCount-1].returnValue = -1;
                        } else {
                            // remove leading and trailing whitespaces from varValue
                            char *end = varValue + strlen(varValue) - 1;
                            while (end > varValue && *end == ' ')
                                end--;

                            *(end + 1) = '\0';

                            if (strlen(varValue) > 0) {
                                setenv(varName, varValue, 1);
                                int index = -1;
                                for (int i = 0; i < envVarCount; i++) {
                                    if (strcmp(envVars[i].name, varName) == 0) {
                                        index = i;
                                        break;
                                    }
                                }

                                if (index != -1) {
                                    free(envVars[index].value);
                                    envVars[index].value = strdup(varValue);
                                } else {
                                    if (envVarCount < 100) {
                                        // initializing name and value
                                        envVars[envVarCount].name = strdup(varName);
                                        envVars[envVarCount].value = strdup(varValue);
                                        envVarCount++;
                                    } else {
                                        printf("Error: Maximum number of environment variables exceeded.\n");
                                        commandHistory[commandCount-1].returnValue = -1;
                                    }
                                }
                            }
                            else {
                                unsetenv(varName);
                            }
                        }
                    }
                }
            } else {
                // built-in command recognition
                char *varValue = getenv(varName);
                if (varValue != NULL) {
                    if (strcmp(varValue, "exit") == 0) {
                        printf("Bye!\n");
                        exit_requested = 1;
                    } else if (strcmp(varValue, "log") == 0) {
                        // log history
                        for (int i = 0; i < commandCount; i++) {
                            char timestampStr[80];
                            strftime(timestampStr, sizeof(timestampStr), "%a %b %d %H:%M:%S %Y", &(commandHistory[i].time));
                            printf("%s %d\n%s\n", commandHistory[i].name, commandHistory[i].returnValue, timestampStr);
                        }
                    } else if (strcmp(varValue, "theme") == 0) {
                        char *color = strtok(NULL, " ");
                        if (color != NULL) {
                            char *ansiColorCode = getANSIColorCode(color);

                            if (ansiColorCode != NULL) {
                                themeColor = ansiColorCode;
                                exitColor = themeColor;
                            } else {
                                printf("unsupported theme\n");
                                commandHistory[commandCount-1].returnValue = -1;
                            }
                        } else {
                            printf("Error: Missing argument for 'theme' command\n");
                            commandHistory[commandCount-1].returnValue = -1;
                        }
                    } else if (strcmp(varValue, "print") == 0) {
                        // print command implementation
                        char *varName = strtok(NULL, "");

                        if (varName != NULL) {
                            char *token = strtok(varName, " ");
                            while (token != NULL) {
                                if (token[0] == '$') {
                                    char *name = token + 1;
                                    int found = 0;

                                    for (int i = 0; i < envVarCount; i++) {
                                        if (strcmp(envVars[i].name, name) == 0) {
                                            printf("%s", envVars[i].value);
                                            found = 1;
                                            break;
                                        }
                                    }

                                    if (!found) {
                                        printf("Error: Environment variable $%s not initialized", name);
                                        commandHistory[commandCount-1].returnValue = -1;
                                    }
                                } else {
                                    // print the literal value
                                    printf("%s", token);
                                }

                                token = strtok(NULL, " ");
                                if (token != NULL) {
                                    printf(" "); // print space between tokens
                                }
                            }
                            printf("\n");
                        } else {
                            printf("Error: Missing argument for 'print' command\n");
                            commandHistory[commandCount-1].returnValue = -1;
                        }
                    }
                    else {
                        printf("%s\n", varValue);
                    }
                } else {
                    printf("Error: Environment variable $%s not initialized\n", varName);
                    commandHistory[commandCount-1].returnValue = -1;
                }
            }
        } else if (strcmp(program, "print") == 0) {
            // print command implementation
            char *varName = strtok(NULL, "");

            if (varName != NULL) {
                char *token = strtok(varName, " ");
                while (token != NULL) {
                    if (token[0] == '$') {
                        char *name = token + 1;
                        int found = 0;

                        for (int i = 0; i < envVarCount; i++) {
                            if (strcmp(envVars[i].name, name) == 0) {
                                printf("%s", envVars[i].value);
                                found = 1;
                                break;
                            }
                        }
                        if (!found) {
                            printf("Error: Environment variable $%s not initialized", name);
                            commandHistory[commandCount-1].returnValue = -1;
                        }
                    } else {
                        // print the literal value
                        printf("%s", token);
                    }

                    token = strtok(NULL, " ");
                    if (token != NULL) {
                        printf(" "); // print space between tokens
                    }
                }
                printf("\n");
            } else {
                printf("Error: Missing argument for 'print' command\n");
                commandHistory[commandCount-1].returnValue = -1;
            }
        } else if (strcmp(program, "log") == 0) {
            // log history
            if (strtok(NULL, " ") == NULL) {
                for (int i = 0; i < commandCount; i++) {
                    char timestampStr[80];
                    strftime(timestampStr, sizeof(timestampStr), "%a %b %d %H:%M:%S %Y", &(commandHistory[i].time));
                    printf("%s %d\n%s\n", commandHistory[i].name, commandHistory[i].returnValue, timestampStr);
                }
            } else {
                printf("Error: Too Many Arguments Detected\n");
                commandHistory[commandCount-1].returnValue = -1;
            }
        } else if (strcmp(program, "theme") == 0) {
            // change color theme
            char *color = strtok(NULL, " ");

            if (color != NULL) {
                char *ansiColorCode = getANSIColorCode(color);

                if (ansiColorCode != NULL) {
                    themeColor = ansiColorCode;
                    exitColor = themeColor;
                } else {
                    printf("unsupported theme\n");
                    commandHistory[commandCount-1].returnValue = -1;
                }
            } else {
                printf("Error: Missing argument for 'theme' command\n");
                commandHistory[commandCount-1].returnValue = -1;
            }
        } else {
            pid_t pid;
            int stdoutPipe[2]; // pipe for the child process

            if (pipe(stdoutPipe) == -1) {
                printf("Error: Failed to create pipe.\n");
                exit(-1);
            }

            pid = fork();

            if (pid == 0) {
                // child process
                close(stdoutPipe[0]);

                if (themeColor != NULL) {
                    printf("%s", themeColor);
                }

                dup2(stdoutPipe[1], STDOUT_FILENO);
                close(stdoutPipe[1]); 

                char *args[100];
                args[0] = program; 
                char *arg = strtok(NULL, " ");
                int i = 1;
                while (arg != NULL) {
                    args[i++] = arg;
                    arg = strtok(NULL, " ");
                }
                args[i] = NULL; 

                // executing non-built-in command
                execvp(program, args);
                printf("Missing keyword or command, or permission problem\n");
                exit(-1);
            } else if (pid > 0) {
                // parent process 
                close(stdoutPipe[1]); 

                char outputBuffer[4096];
                ssize_t bytesRead;
                while ((bytesRead = read(stdoutPipe[0], outputBuffer, sizeof(outputBuffer) - 1)) > 0) {
                    outputBuffer[bytesRead] = '\0';
                    if (themeColor != NULL) {
                        printf("%s", themeColor);
                    }
                    printf("%s", outputBuffer);
                }
                close(stdoutPipe[0]); 

                int childStatus;
                waitpid(pid, &childStatus, 0);
                if (WIFEXITED(childStatus)) {
                    int returnValue = WEXITSTATUS(childStatus);
                    if (returnValue == 255) {
                        returnValue = -1; 
                    }
                    commandHistory[commandCount-1].returnValue = returnValue;
                } else {
                    commandHistory[commandCount-1].returnValue = -1;
                }
            } else {
                printf("Error: Fork failed.\n");
                exit(-1);
            }
        }
    }
    if (script_mode && exit_requested) {
        if (exitColor != NULL) {
            printf("%s", exitColor); 
        }
        printf("Bye!\n");  
    }
    if (scriptFile != NULL) {
        fclose(scriptFile);
    }
    for (int i = 0; i < envVarCount; i++) {
        free(envVars[i].name);
        free(envVars[i].value);
    }
    for (int i = 0; i < commandCount; i++) {
        free(commandHistory[i].name);
    }
    free(lineptr);
    return 0;
}