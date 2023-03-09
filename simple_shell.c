#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>

#define MAX_INPUT_LENGTH 1024
#define MAX_ARGS 64

pid_t ppid;

char current_working_directory[MAX_INPUT_LENGTH];

void parent_signal_handler(int signal_num)
{
    int child_status;
    pid_t child_pid;
    int status=1;

    while ((child_pid = waitpid(-1, &child_status, WNOHANG)) > 0)
    {
        status=0;
        if (WIFEXITED(child_status))
        {
            FILE *fptr;
            fptr = fopen("output.txt", "a");
            if (fptr == NULL)
            {
                printf("Error opening file!\n");
                exit(1);
            }
            kill(child_pid, SIGKILL);
            fprintf(fptr, "Child process %d terminated \n", child_pid);
            fclose(fptr);
        }
    }
    if (status)
    {
        FILE *fptr;
        fptr = fopen("output.txt", "a");
        if (fptr == NULL)
        {
            printf("Error opening file!\n");
            exit(1);
        }
        fprintf(fptr, "Child process %d terminated \n", ppid);
        fclose(fptr);
    }
}

void setup_environment()
{
    getcwd(current_working_directory, MAX_INPUT_LENGTH);
}

void execute_shell_builtin(char **args)
{
    if (strcmp(args[0], "cd") == 0)
    {
        if (args[1] == NULL || strcmp(args[1], "~") == 0)
        {
            chdir(getenv("HOME"));
        }
        else
        {
            chdir(args[1]);
        }
        getcwd(current_working_directory, MAX_INPUT_LENGTH);
    }
    else if (strcmp(args[0], "echo") == 0)
    {
        for (int i = 1; args[i] != NULL; i++)
        {
            if (args[i][0] == '$')
            {
                char *varname = args[i] + 1;
                char *varvalue = getenv(varname);
                if (varvalue != NULL)
                {
                    args[i] = varvalue;
                }
            }
            printf("%s ", args[i]);
        }
        printf("\n");
    }
    else if (strcmp(args[0], "export") == 0)
    {
        if (args[1] != NULL && strchr(args[1], '=') != NULL)
        {
            char result[100] = "";

            for (int i = 2; args[i] != NULL; i++)
            {
                strcat(result, " ");
                strcat(result, args[i]);
            }

            strcat(args[1], result);

            int len = strlen(args[1]);
            for (int i = 0; i < len; i++)
            {
                if (args[1][i] == '\"')
                {
                    for (int j = i; j < len; j++)
                    {
                        args[1][j] = args[1][j + 1];
                    }
                    len--;
                    i--;
                }
            }
            char *token = strtok(args[1], "=");
            char *name = token;
            token = strtok(NULL, "=");
            char *value = token;
            setenv(name, value, 1);
        }
    }
}
void execute_command(char **args, int foreground)
{
    pid_t pid = fork();
    if (pid < 0)
    {
        printf("Error: fork() failed\n");
        return;
    }
    else if (pid == 0)
    {
        for (int i = 0; args[i] != NULL; i++)
        {

            if (args[i][0] == '$')
            {
                char *varname = args[i] + 1;
                char *varvalue = getenv(varname);
                if (varvalue != NULL)
                {
                    int arg_count = i;
                    char *token = strtok(varvalue, " ");
                    while (token != NULL)
                    {
                        args[arg_count] = token;
                        arg_count++;
                        token = strtok(NULL, " ");
                    }
                }
            }
        }

        if (execvp(args[0], args) < 0)
        {
            printf("Error: execvp() failed\n");
            exit(1);
        }
    }
    else
    {
        if (foreground)
        {
            ppid = pid;
            waitpid(pid, NULL, 0);
        }
    }
}

void parse_input(char *input, char **args, int *foreground)
{
    char *token = strtok(input, " \t\n");
    int i = 0;
    while (token != NULL)
    {
        args[i++] = token;
        token = strtok(NULL, " \t\n");
    }
    args[i] = NULL;
    if (i > 0 && strcmp(args[i - 1], "&") == 0)
    {
        *foreground = 0;
        args[i - 1] = NULL;
    }
    else
    {
        *foreground = 1;
    }
}
void signal_handler()
{
    struct sigaction sigact;
    sigemptyset(&sigact.sa_mask);
    sigact.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    sigact.sa_handler = parent_signal_handler;
    sigaction(SIGCHLD, &sigact, NULL);
}

void shell()
{
    char input[MAX_INPUT_LENGTH];
    char *args[MAX_ARGS];
    int foreground;
    while (1)
    {
        printf("%s$ ", current_working_directory);
        fgets(input, MAX_INPUT_LENGTH, stdin);
        if (feof(stdin))
        {
            printf("\n");
            exit(0);
        }
        parse_input(input, args, &foreground);
        if (args[0] == NULL)
        {
            continue;
        }
        else if (strcmp(args[0], "exit") == 0)
        {
            FILE *fptr;
            fptr = fopen("output.txt", "a");
            if (fptr == NULL)
            {
                printf("Error opening file!\n");
                exit(1);
            }
            fprintf(fptr, "All processes terminated \n");
            fclose(fptr);
            kill(0, SIGKILL);
            exit(0);
        }
        else if (strcmp(args[0], "cd") == 0 || strcmp(args[0], "echo") == 0 || strcmp(args[0], "export") == 0)
        {
            execute_shell_builtin(args);
        }
        else
        {
            execute_command(args, foreground);
        }
    }
}
int main()
{
    signal_handler();
    setup_environment();
    shell();
}
