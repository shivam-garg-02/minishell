#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif

/* HEADER Files */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <errno.h>
#include <time.h>
#include <assert.h>
#include <pwd.h>
#include <signal.h>
#include <termios.h>

/* MACROS Defination */
#define FALSE 0
#define TRUE 1
#define LINE_LEN 80
#define MAX_ARGS 64
#define MAX_ARG_LEN 64
#define MAX_PATHS 64
#define MAX_PATH_LEN 96
#define WHITESPACE " .,\t&"
#define STD_INPUT 0
#define STD_OUTPUT 1
#ifndef NULL
#define NULL 0
#endif

/* Command Structure */
struct command_t
{
    char *name;
    int argc;
    char *argv[MAX_ARGS];
};

/* Variables */
struct command_t command;
static char commandInput = '\0';
static int buff = 0;
static char *paths_array[MAX_PATHS];
static char commandLine[LINE_LEN];
char welcomestring[150];
char hostname[50], cwd[50];
struct passwd *passwd;
int status;
extern char **environ;
char **s;
static int commandCount = 0;

FILE *historyfile = tmpfile();

/* Functions */
void parsePath(char **);
void createWelcomeString();
void addHistory(char *);
int parseCommand();
void changeDirectory();
void clearScreen();
void pwd();
void showHistory();
void echoDollar();
int printenvn();
void setenvn();
int isInternal();
char *lookUpPath(char **, char **);
int executeFileInRedirection(char *, char **, char *);
int processFileInRedirection(int);
int executeFileOutRedirection(char *, char **, char *);
int processFileOutRedirection(int);
int executeFileAppendRedirection(char *, char **, char *);
int processFileAppendRedirection(int);
int executeFileInOutRedirection(char *, char **, char *, char *);
int processFileInOutRedirection(int, int);
int executeFileInAppendRedirection(char *, char **, char *, char *);
int processFileInAppendRedirection(int, int);
void executePipedCommand(char **, char **, char *, char *);
int processPipedCommand(int);
int excuteCommand();
int processCommand();

/* Funtion to parse the path separated by the tokens(:) */
void parsePath(char *path_array[])
{
    for (int i = 0; i < MAX_ARGS; i++)
        path_array[i] = NULL;
    char *allPaths = (char *)getenv("PATH");
    char *allPathsCopied = (char *)malloc(strlen(allPaths) + 1);
    strcpy(allPathsCopied, allPaths);

    char *path = strtok(allPathsCopied, ":");
    int j = 0;
    while (path != NULL)
    {
        path_array[j] = path;
        j++;
        path = strtok(NULL, ":");
    }
}

/* Function to create Welcome string i.e. prompt */
void createWelcomeString()
{
    gethostname(hostname, 50);
    passwd = getpwuid(getuid());
    getcwd(cwd, 50);
    if (!strncmp(cwd, strcat(strdup("/home/"), passwd->pw_name), 6 + strlen(passwd->pw_name)))
    {
        char *tempcwd = strchr(strchr(cwd + 1, '/') + 1, '/');
        sprintf(cwd, "~%s$ ", tempcwd == NULL ? "" : tempcwd);
    }
    if (cwd == NULL)
    {
        strcpy(cwd, "");
    }
    sprintf(welcomestring, "%s@%s:%s$ ", passwd->pw_name, hostname, cwd);
}

/* Function to add the given command to history */
void addHistory(char *commandLine)
{
    fprintf(historyfile, "%d\t%s\n", commandCount + 1, commandLine);
    commandCount++;
}

/* Function to parse the command separated by whitespace */
int parseCommand()
{
    char *commandSeparated = strtok(commandLine, " ");
    int i = 0;
    while (commandSeparated != NULL)
    {
        command.argv[i] = commandSeparated;
        commandSeparated = strtok(NULL, " ");
        i++;
    }
    command.argc = i;
    command.argv[i++] = NULL;

    return 0;
}

/* Function for BUILTIN cd i.e. to change directory */
void changeDirectory()
{
    if (command.argv[1] == NULL)
    {
        chdir(getenv("HOME"));
    }
    else
    {
        if (chdir(command.argv[1]) == -1)
        {
            printf(" %s: no such directory\n", command.argv[1]);
        }
    }
    createWelcomeString();
}

/* Function for BUILTIN clear i.e. to clear screen */
void clearScreen()
{
    printf("\033[2J\033[1;1H");
}

/* Function for BUILTIN pwd command */
void pwd()
{
    char curr_path[MAX_PATH_LEN];
    if (getcwd(curr_path, sizeof(curr_path)) == NULL)
        perror("getcwd(): error\n");
    else
    {
        printf("%s\n", curr_path);
    }
}

/* Function for BUILTIN history i.e. to show history */
void showHistory()
{
    char temp;
    rewind(historyfile);
    while ((temp = getc(historyfile)) != EOF)
    {
        printf("%c", temp);
    }
}

/* Function for BUILTIN echo i.e for echo $variable or echo $$ */
void echoDollar()
{
    if (strncmp(command.argv[0], "echo", 4) == 0)
    {
        if (command.argv[1] != NULL)
        {
            char *echo_str = (char *)malloc(sizeof(strlen(command.argv[1])));
            strcpy(echo_str, command.argv[1]);
            if (strcmp(echo_str, "$$") == 0)
            {
                printf("PID: %d\n", getpid());
                free(echo_str);
                return;
            }

            if (strcmp(echo_str, "$?") == 0)
            {
                printf("%d\n", status);
                free(echo_str);
                return;
            }
            if (!strncmp(echo_str, "$", 1))
            {
                char *path;
                if (strlen(echo_str) == 1)
                {
                    printf("$\n");
                }
                else
                {
                    path = getenv(echo_str + 1);
                    if (path)
                    {
                        printf("%s\n", path);
                        return;
                    }
                    else
                    {
                        printf("\n");
                    }
                }
            }
        }
    }
}

/* Function for BUILTIN printenv i.e. to print environment variables */
int printenvn()
{
    if (!command.argv[1])
    {
        s = environ;

        for (; *s; s++)
        {
            printf("%s\n", *s);
        }
        return 1;
    }
    else
    {
        char *print_str = (char *)malloc(sizeof(strlen(command.argv[1])));
        strcpy(print_str, command.argv[1]);
        char *path;
        path = getenv(print_str);
        if (!path)
            return 0;
        printf("%s\n", path);
        return 1;
    }
}

/* Function for BUILTIN setenv i.e. to set environment variables */
void setenvn()
{
    setenv(command.argv[1], command.argv[3], 1);
}

/* Function to check if the given command is a an internal or BUILTIN function */
int isInternal()
{
    if (!strcmp("cd", command.argv[0]))
    {
        changeDirectory();
        return 1;
    }
    if (!strcmp("clear", command.argv[0]))
    {
        clearScreen();
        return 1;
    }
    if (!strcmp("pwd", command.argv[0]))
    {
        pwd();
        return 1;
    }
    if (!strcmp("history", command.argv[0]))
    {
        showHistory();
        return 1;
    }
    if (!strcmp("echo", command.argv[0]))
    {
        if (!command.argv[1])
        {
            printf("\n");
            return 1;
        }
        else if (!strncmp("$", command.argv[1], 1))
        {
            echoDollar();
            return 1;
        }
        else
        {
            int i = 1;
            while (command.argv[i] != NULL)
            {
                printf("%s ", command.argv[i]);
                i++;
            }
            printf("\n");
            return 1;
        }
    }
    if (!strcmp("printenv", command.argv[0]))
    {
        int answer;
        answer = printenvn();
        return answer;
    }
    if (!strcmp("setenv", command.argv[0]))
    {
        setenvn();
        return 1;
    }

    return 0;
}

/* Function to check and return the path of the extenal functions and executable */
char *lookUpPath(char **argv, char **path_array)
{
    char *result;
    char curr_path[MAX_PATH_LEN];
    if (*argv[0] == '/')
    {
        return argv[0];
    }
    else if (*argv[0] == '.')
    {
        if (*++argv[0] == '.')
        {
            if (getcwd(curr_path, sizeof(curr_path)) == NULL)
                perror("getcwd(): error\n");
            else
            {
                *--argv[0];
                asprintf(&result, "%s%s%s", curr_path, "/", argv[0]);
            }
            return result;
        }
        *--argv[0];
        if (*++argv[0] == '/')
        {
            if (getcwd(curr_path, sizeof(curr_path)) == NULL)
                perror("getcwd(): error\n");
            else
            {
                asprintf(&result, "%s%s", curr_path, argv[0]);
                *--argv[0];
            }
            return result;
        }
    }

    int i;
    for (i = 0; i < MAX_PATHS; i++)
    {
        if (path_array[i] != NULL)
        {
            asprintf(&result, "%s%s%s", path_array[i], "/", argv[0]);
            if (access(result, X_OK) == 0)
            {
                return result;
            }
        }
        else
            continue;
    }

    fprintf(stderr, "%s: command not found\n", argv[0]);
    return NULL;
}

/* Function to execute input redirection */
int executeFileInRedirection(char *commandName, char *argv[], char *input_file)
{
    int pipe_array[2];

    if (pipe(pipe_array))
    {
        perror("pipe");
        exit(127);
    }

    int pid = fork();
    if (pid == -1)
    {
        perror("fork()");
        exit(127);
    }
    else if (pid == 0)
    {
        close(pipe_array[0]);
        dup2(pipe_array[1], 1);
        close(pipe_array[1]);
        FILE *in_file;
        char str;

        in_file = fopen(input_file, "r");
        if (in_file == NULL)
            perror("Error opening file");
        else
        {
            while ((str = fgetc(in_file)) != EOF)
            {
                putchar(str);
            }
            fclose(in_file);
        }
        exit(EXIT_SUCCESS);
    }
    else
    {
        close(pipe_array[1]);
        dup2(pipe_array[0], 0);
        close(pipe_array[0]);

        execve(commandName, argv, 0);
        perror(commandName);
        exit(125);
    }
    return 0;
}

/* Function to process input redirection */
int processFileInRedirection(int infile)
{
    char *argv[5];
    char *commandName;

    int j;
    for (j = 0; j < infile; j++)
    {
        argv[j] = command.argv[j];
    }
    argv[j] = NULL;
    commandName = lookUpPath(argv, paths_array);

    int status;
    fflush(stdout);

    int pid = fork();
    if (pid == -1)
    {
        perror("fork");
    }
    else if (pid == 0)
    {
        executeFileInRedirection(commandName, argv, command.argv[infile + 1]);
    }
    else
    {
        pid = wait(&status);
        return 0;
    }
    return 0;
}

/* Function to execute output redirection */
int executeFileOutRedirection(char *commandName, char *argv[], char *output_file)
{
    int std_out = dup(1);

    int out_file = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, S_IRWXU | S_IRGRP | S_IROTH);
    if (out_file < 0)
        return 1;

    if (dup2(out_file, 1) < 0)
        return 1;
    int pid = fork();
    if (pid == 0)
    {
        close(out_file);
        close(std_out);
        execve(commandName, argv, 0);
        return 0;
    }
    dup2(std_out, 1);
    close(out_file);
    close(std_out);
    wait(NULL);
    close(out_file);
    return 0;
}

/* Function to process output redirection */
int processFileOutRedirection(int outfile)
{
    char *argv[5];
    char *commandName;
    int j;
    for (j = 0; j < outfile; j++)
    {
        argv[j] = command.argv[j];
    }
    argv[j] = NULL;
    commandName = lookUpPath(argv, paths_array);

    return executeFileOutRedirection(commandName, argv, command.argv[outfile + 1]);
}

/* Function to execute append output redirection */
int executeFileAppendRedirection(char *commandName, char *argv[], char *append_file)
{
    int std_out = dup(1);

    //First, we're going to open a file
    int app_file = open(append_file, O_WRONLY | O_CREAT | O_APPEND);
    if (app_file < 0)
        return 1;

    //Now we redirect standard output to the file using dup2
    if (dup2(app_file, 1) < 0)
        return 1;
    int pid = fork();
    if (pid == 0)
    {
        close(app_file);
        close(std_out);
        execve(commandName, argv, 0);
        return 0;
    }
    dup2(std_out, 1);
    close(app_file);
    close(std_out);
    wait(NULL);
    close(app_file);
    return 0;
}

/* Function to process append output redirection */
int processFileAppendRedirection(int appendfile)
{
    char *argv[5];
    char *commandName;
    int j;
    for (j = 0; j < appendfile; j++)
    {
        argv[j] = command.argv[j];
    }
    argv[j] = NULL;
    commandName = lookUpPath(argv, paths_array);

    return executeFileAppendRedirection(commandName, argv, command.argv[appendfile + 1]);
}

/* Function to execute input and output redirection together */
int executeFileInOutRedirection(char *commandName, char *argv[], char *input_file, char *output_file)
{
    int std_in = dup(0);
    int std_out = dup(1);

    //First, we're going to open both files
    int in_file = open(input_file, O_RDONLY);
    int out_file = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, S_IRWXU | S_IRGRP | S_IROTH);
    if (in_file < 0)
        return 1;
    if (out_file < 0)
        return 1;

    //Now we redirect standard output and standard input to the respective files using dup2
    if (dup2(in_file, 0) < 0)
        return 1;
    if (dup2(out_file, 1) < 0)
        return 1;
    int pid = fork();
    if (pid == 0)
    {
        execve(commandName, argv, 0);
        return 0;
    }
    wait(NULL);
    dup2(std_in, 0);
    dup2(std_out, 1);
    close(in_file);
    close(out_file);
    close(std_in);
    close(std_out);
    return 0;
}

/* Function to process input and output redirection together */
int processFileInOutRedirection(int infile, int outfile)
{
    int i = 0;
    if (infile < outfile)
        i = infile;
    else
        i = outfile;

    char *argv[5];
    char *commandName;
    int j;
    for (j = 0; j < i; j++)
    {
        argv[j] = command.argv[j];
    }
    argv[j] = NULL;
    commandName = lookUpPath(argv, paths_array);

    return executeFileInOutRedirection(commandName, argv, command.argv[infile + 1], command.argv[outfile + 1]);
}

/* Function to execute input and append output redirection together */
int executeFileInAppendRedirection(char *commandName, char *argv[], char *input_file, char *append_file)
{
    int std_in = dup(0);
    int std_out = dup(1);

    //First, we're going to open both files
    int in_file = open(input_file, O_RDONLY);
    int app_file = open(append_file, O_WRONLY | O_CREAT | O_APPEND);
    if (in_file < 0)
        return 1;
    if (app_file < 0)
        return 1;

    //Now we redirect standard output and standard input to the respective files using dup2
    if (dup2(in_file, 0) < 0)
        return 1;
    if (dup2(app_file, 1) < 0)
        return 1;
    int pid = fork();
    if (pid == 0)
    {
        execve(commandName, argv, 0);
        return 0;
    }
    wait(NULL);
    dup2(std_in, 0);
    dup2(std_out, 1);
    close(in_file);
    close(app_file);
    close(std_in);
    close(std_out);
    return 0;
}

/* Fucntion to process input and append output redirection together */
int processFileInAppendRedirection(int infile, int appendfile)
{
    int i = 0;
    if (infile < appendfile)
        i = infile;
    else
        i = appendfile;

    char *argv[5];
    char *commandName;
    int j;
    for (j = 0; j < i; j++)
    {
        argv[j] = command.argv[j];
    }
    argv[j] = NULL;
    commandName = lookUpPath(argv, paths_array);
    return executeFileInAppendRedirection(commandName, argv, command.argv[infile + 1], command.argv[appendfile + 1]);
}

/* Function to process piped command */
int processPipedCommand(int i)
{
    char *argument1[5];
    char *argument2[5];
    char *random1, *random2;

    for (int count1 = 0; count1 < i; count1++)
    {
        argument1[count1] = command.argv[count1];
    }
    argument1[i] = NULL;
    random1 = lookUpPath(argument1, paths_array);

    int count3 = 0;
    for (int count2 = i + 1; count2 < command.argc; count2++)
    {
        argument2[count3] = command.argv[count2];
        count3 = count3 + 1;
    }
    argument2[count3] = NULL;
    random2 = lookUpPath(argument2, paths_array);

    int status;
    fflush(stdout);

    int pid = fork();

    if (pid == -1)
    {
        perror("fork");
    }
    else if (pid == 0)
    {
        executePipedCommand(argument1, argument2, random1, random2);
    }
    else
    {
        pid = wait(&status);
        return 0;
    }
    return 1;
}

/* Function to execute the external functions and executable */
int excuteCommand()
{
    pid_t pid = fork();
    if (pid == -1)
    {
        fprintf(stderr, "Fork fails: \n");
        return 1;
    }
    else if (pid == 0)
    {
        execve(command.name, command.argv, 0);
        printf("Child process failed\n");
        return 1;
    }
    else
    {
        wait(NULL);
        return 1;
    }
}

/* Function to execute piped command */
void executePipedCommand(char *argument1[], char *argument2[], char *random1, char *random2)
{
    int array_pipe[2];

    if (pipe(array_pipe))
    {
        perror("pipe");
        exit(127);
    }
    int pid = fork();
    if (pid == -1)
    {
        perror("fork()");
        exit(127);
    }
    else if (pid == 0)
    {
        close(array_pipe[0]);
        dup2(array_pipe[1], 1);
        close(array_pipe[1]);
        execve(random1, argument1, 0);
        perror(random1);
        exit(126);
    }
    else
    {
        close(array_pipe[1]);
        dup2(array_pipe[0], 0);
        close(array_pipe[0]);
        execve(random2, argument2, 0);
        perror(random2);
        exit(125);
    }
}

/* Function to check if command contains redirection or pipes */
int processCommand()
{
    int infile = 0, outfile = 0, pipeLine = 0, appendfile = 0;
    int redirect = 0;

    for (int i = 0; i < command.argc; i++)
    {
        if (strcmp(command.argv[i], ">") == 0)
        {
            redirect++;
            outfile = i;
        }
        else if (strcmp(command.argv[i], "<") == 0)
        {
            redirect++;
            infile = i;
        }
        else if (strcmp(command.argv[i], ">>") == 0)
        {
            redirect++;
            appendfile = i;
        }
        else if (strcmp(command.argv[i], "|") == 0)
        {
            return processPipedCommand(i);
        }
    }
    if (redirect == 1)
    {
        if (outfile > 0)
        {
            return processFileOutRedirection(outfile);
        }
        else if (infile > 0)
        {
            return processFileInRedirection(infile);
        }
        else if (appendfile > 0)
        {
            return processFileAppendRedirection(appendfile);
        }
    }
    else if (redirect == 2)
    {
        if ((infile > 0) && (outfile > 0))
        {
            return processFileInOutRedirection(infile, outfile);
        }
        else if ((infile > 0) && (appendfile > 0))
        {
            return processFileInAppendRedirection(infile, appendfile);
        }
    }
    else
        return excuteCommand();

    return 0;
}

/* MAIN Function */
int main(int argc, char *argv[], char *envp[])
{
    parsePath(paths_array);
    printf(
        "\n-----------------------Welcome to MYSHELL-a mini shell-------------------------\n");
    printf("---------------------------SHIVAM GARG, 200101120------------------------------\n\n");

    while (TRUE)
    {
        createWelcomeString();
        printf(
            "%s",
            welcomestring);

        commandInput = getchar();
        if (commandInput == '\n')
        {
            continue;
        }
        else
        {
            buff = 0;
            while ((commandInput != '\n') && (buff < LINE_LEN))
            {
                commandLine[buff++] = commandInput;
                commandInput = getchar();
            }
            commandLine[buff] = '\0';

            addHistory(commandLine);

            if ((strcmp(commandLine, "exit") == 0) || (strcmp(commandLine, "quit") == 0))
                break;

            parseCommand();

            if (isInternal() == 0)
            {
                command.name = lookUpPath(command.argv, paths_array);

                if (command.name == NULL)
                {
                    printf("Stub: error\n");
                    continue;
                }

                processCommand();
            }
        }
    }

    printf(
        "\n------------------------------Exiting MYSHELL----------------------------------\n\n");
    exit(EXIT_SUCCESS);
}