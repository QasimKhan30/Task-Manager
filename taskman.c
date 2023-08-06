
/* Fill in your Name and GNumber in the following two comment fields
 * Name: Qasim Khan
 * GNumber: G01139446
 */

#include <sys/wait.h>
#include "taskman.h"
#include "parse.h"
#include "util.h"

/* Constants */
#define DEBUG 0
#define NUM_PATHS 2

/* Structures */
typedef struct Node_t
{
    char *instruction;      // only instruction without flags
    char **argv;            // string array holding instruction and all flags
    char *command;          // entire instruction with flags
    struct Node_t *next;    // next task in LList
    int state;              // state of task
    int taskID;             // Id of task
    int is_background_task; // 0 if run in foreground, 1 if run in background
    pid_t pid;              // unique pid of task
    int exit_status;        // exit status of process
    int stopped;            // 1 if process is stopped, 0 by default

} Node_t;

typedef struct Tasks_t
{
    Node_t *head; // Points to FIRST node of linked list. No Dummy Nodes.
    int count;    // Number of items in list
} Tasks_t;

/*Function Stubs*/
void *dmalloc(size_t size);
Node_t *create_node(Instruction *i, char *cmd, int taskid, char *argv[]);
int get_task_id(Tasks_t *tasks);
int insertEnd(Tasks_t *queue, Node_t *node);
void print_tasks(Tasks_t *tasks);
int is_busy(Node_t *node);
void delete (Tasks_t *tasks, int taskid);
void insertAscendingOrder(Tasks_t *tasks, Node_t *node);
Node_t *find_node(Tasks_t *tasks, int taskid);
void run_task(Node_t *node, char *file);
void bg(Node_t *node, char *filename);
void log_task(Node_t *node, int taskid, char *filename);
void output(char *file);
int file_exists(char *filename);
Node_t *find_node_from_pid(Tasks_t *tasks, pid_t processID);
void cancel(Node_t *node);
void suspend(Node_t *node);
void resume(Node_t *node);
void print_node(Node_t *node);
void print_tasks2(Tasks_t *tasks);
void reaper(int status, pid_t pid);
void fg_reaper(pid_t pid);

/* globals */
int num_logged_files = 0;
Tasks_t *global_tasks = NULL;
Node_t *global_node = NULL;

/* Signal Handling
 * Four different reasons that a process could send a SIGCHLD to its parent,
 * 1)Child process exits
 * 2)Child process is killed by a signal
 * 3)Child process is stopped by a signal
 * 4)Child process is continued by a signal
 */
void child_handler()
{
    pid_t pid = 0;
    int status = 0;


    // wnohang: dont wait if process has not terminated or stopped
    // wuntraced: request status information from stopped processes as well
    //  as processes that have terminated.

    (pid = waitpid(-1, &status, WNOHANG | WUNTRACED));

    while (pid > 0)
    {
        

        Node_t *task = find_node_from_pid(global_tasks, pid);

        if (WIFSIGNALED(status))
        {

            log_status_change(task->taskID, task->pid, task->is_background_task, task->command, LOG_CANCEL_SIG);
            task->state = LOG_STATE_KILLED;
            task->exit_status = WEXITSTATUS(status);
            return;
        }
        else if (WIFSTOPPED(status))
        {
            log_status_change(task->taskID, task->pid, task->is_background_task, task->command, LOG_SUSPEND);
            task->state = LOG_STATE_SUSPENDED;
            task->exit_status = WEXITSTATUS(status);
            return;
        }
        else if (WIFEXITED(status))
        {
            log_status_change(task->taskID, task->pid, task->is_background_task, task->command, LOG_CANCEL);
            task->state = LOG_STATE_COMPLETE;
            task->exit_status = WEXITSTATUS(status);
            return;
        }
    }
}

void sigint_handler()
{
    log_ctrl_c();
    if(!global_node){return;}
    if (!(global_node->is_background_task) && (global_node->state == LOG_STATE_WORKING))
    { //if it is foreground and working
        global_node->stopped = 1;
        global_node->state = LOG_STATE_KILLED;
        kill(global_node->pid, SIGINT);
    }
}

void sigtstp_handler()
{
    log_ctrl_z();
    if(!global_node){return;}
    if (!(global_node->is_background_task) && (global_node->state == LOG_STATE_WORKING))
    { // if it is foreground and working
        global_node->state = LOG_STATE_SUSPENDED;
        global_node->stopped = 1;
        kill(global_node->pid, SIGTSTP);
    }
}
void sigcont_handler()
{
    if(!global_node){return;}
    log_status_change(global_node->taskID, global_node->pid, global_node->is_background_task, global_node->command, LOG_RESUME);
}

void signal_hanlder(int signal)
{
    if (signal == SIGCHLD)
    {
        child_handler();
    }
    else if (signal == SIGINT)
    {
        sigint_handler();
    }
    else if (signal == SIGTSTP)
    {
        sigtstp_handler();
    }
    else if (signal == SIGCONT)
    {
        sigcont_handler();
    }
    else
    {
        return;
    }
}


/* The entry of your task management program */
int main()
{
    char cmdline[MAXLINE]; /* Command line */
    char *cmd = NULL;

    /* Intial Prompt and Welcome */
    log_intro();
    log_help();

    /* Initialization */
    Tasks_t *tasks = (Tasks_t *)dmalloc(sizeof(Tasks_t));
    tasks->count = 0;
    tasks->head = NULL;

    struct sigaction act;            // for storing signal handler overhead
    memset(&act, 0, sizeof(act));    // initialize to zero
    act.sa_handler = signal_hanlder; // child_handler will be our signal handler
    sigaction(SIGCHLD, &act, NULL);  // sets a new interrupt handler
    sigaction(SIGINT, &act, NULL);
    sigaction(SIGCONT, &act, NULL);
    sigaction(SIGTSTP, &act, NULL);

    /* Shell looping here to accept user command and execute */
    while (1)
    {
        char *argv[MAXARGS]; /* Argument list */
        Instruction inst;    /* Instruction structure: check parse.h */

        /* Print prompt */
        log_prompt();

        /* Read a line */
        // note: fgets will keep the ending '\n'
        errno = 0;
        if (fgets(cmdline, MAXLINE, stdin) == NULL)
        {
            if (errno == EINTR)
            {
                continue;
            }
            exit(-1);
        }

        if (feof(stdin))
        { /* ctrl-d will exit text processor */
            exit(0);
        }

        /* Parse command line */
        if (strlen(cmdline) == 1) /* empty cmd line will be ignored */
            continue;

        cmdline[strlen(cmdline) - 1] = '\0'; /* remove trailing '\n' */

        cmd = malloc(strlen(cmdline) + 1);
        snprintf(cmd, strlen(cmdline) + 1, "%s", cmdline);

        /* Bail if command is only whitespace */
        if (!is_whitespace(cmd))
        {
            initialize_command(&inst, argv); /* initialize arg lists and instruction */
            parse(cmd, &inst, argv);         /* call provided parse() */

            if (DEBUG)
            { /* display parse result, redefine DEBUG to turn it off */
                debug_print_parse(cmd, &inst, argv, "main (after parse)");
            }

            /* After parsing: your code to continue from here */
            /*================================================*/

            if (strcmp(inst.instruct, "help") == 0)
            {
                log_help();
                continue;
            }
            else if (strcmp(inst.instruct, "quit") == 0)
            {
                log_quit();
                exit(0);
            }
            else if (strcmp(inst.instruct, "tasks") == 0)
            {
                log_num_tasks(tasks->count);
                print_tasks(tasks);
                continue;
            }
            else if (strcmp(inst.instruct, "delete") == 0)
            {
                delete (tasks, inst.id);
                continue;
            }
            else if (strcmp(inst.instruct, "run") == 0)
            {
                Node_t *temp = find_node(tasks, inst.id);
                if (!temp)
                {
                    log_task_id_error(inst.id);
                    continue;
                }
                else if (is_busy(temp))
                {
                    log_status_error(temp->taskID, temp->state);
                    continue;
                }
                global_node = temp;
                run_task(temp, inst.file);
                continue;
            }
            else if (strcmp(inst.instruct, "bg") == 0)
            {
                Node_t *temp = find_node(tasks, inst.id);
                global_node = temp;
                if (!temp)
                {
                    log_task_id_error(inst.id);
                    continue;
                }
                else if (is_busy(temp))
                {
                    log_status_error(temp->taskID, temp->state);
                    continue;
                }
                bg(temp, inst.file);
                continue;
            }
            else if (strcmp(inst.instruct, "log") == 0)
            {
                Node_t *temp = find_node(tasks, inst.id);
                global_node = temp;
                if (!temp)
                {
                    log_task_id_error(inst.id);
                    continue;
                }
                else if (is_busy(temp))
                {
                    log_status_error(temp->taskID, temp->state);
                    continue;
                }
                log_task(temp, inst.id, inst.file);
                continue;
            }
            else if (strcmp(inst.instruct, "output") == 0)
            {
                if (find_node(tasks, inst.id) == NULL)
                {
                    log_task_id_error(inst.id);
                    continue;
                }
                char output_filename[100] = "log";
                snprintf(output_filename, sizeof(output_filename), "log%d", inst.id);
                strcat(output_filename, ".txt");
                if (!file_exists(output_filename))
                {
                    log_output_unlogged(inst.id);
                    continue;
                }
                log_output_begin(inst.id);
                output(output_filename);
                continue;
            }
            else if (strcmp(inst.instruct, "cancel") == 0)
            {
                Node_t *temp = find_node(tasks, inst.id);
                global_node = temp;
                if (!temp)
                {
                    log_task_id_error(inst.id);
                }
                if (!(is_busy(temp)))
                {
                    log_status_error(inst.id, temp->state);
                }
                cancel(temp);
                continue;
            }
            else if (strcmp(inst.instruct, "suspend") == 0)
            {
                Node_t *temp = find_node(tasks, inst.id);
                global_node = temp;
                if (!temp)
                {
                    log_task_id_error(inst.id);
                }
                if (!(is_busy(temp)))
                {
                    log_status_error(inst.id, temp->state);
                }

                suspend(temp);
                continue;
            }
            else if (strcmp(inst.instruct, "resume") == 0)
            {
                Node_t *temp = find_node(tasks, inst.id);
                global_node = temp;
                if (!temp)
                {
                    log_task_id_error(inst.id);
                }
                if (!(is_busy(temp)))
                {
                    log_status_error(inst.id, temp->state);
                }
                resume(temp);
                continue;
            }
            Node_t *node = create_node(&inst, cmdline, get_task_id(tasks), argv);

            insertAscendingOrder(tasks, node);
            log_task_init(node->taskID, cmdline);
            global_tasks = tasks;

        }

        free(cmd);
        cmd = NULL;
        free_command(&inst, argv);
    }
    return 0;
}
void *dmalloc(size_t size)
{
    void *p = malloc(size);
    if (!p)
    {
        printf("memory allocation failed\n");
        exit(1);
    }
    return p;
}

Node_t *create_node(Instruction *i, char *cmd, int taskid, char *argv[])
{
    Node_t *node = (Node_t *)dmalloc(sizeof(Node_t));
    node->command = string_copy(cmd);
    node->instruction = string_copy(i->instruct);
    node->state = LOG_STATE_STANDBY; // all tasks start out in the standby state.
    node->taskID = taskid;
    node->argv = clone_argv(argv);
    node->stopped = 0;
    return node;
}

int get_task_id(Tasks_t *tasks)
{

    int task = 1;
    Node_t *current = tasks->head;
    while (current)
    {
        if (task != current->taskID)
        {
            return task;
        }
        current = current->next;
        task++;
    }
    return task;
}

/*
 * This method inserts a node at the end of the specified queue
 */
int insertEnd(Tasks_t *tasks, Node_t *node)
{

    if (!tasks || !node)
    {
        printf("Insert failed\n");
        return -1;
    }

    if (!(tasks->head))
    {
        tasks->head = node;
        tasks->count++;
        return 0;
    }

    Node_t *current = tasks->head;
    while (current->next)
    {
        current = current->next;
    }
    current->next = node;
    node->next = NULL;
    tasks->count++;
    return 1;
}

void print_tasks(Tasks_t *tasks)
{
    Node_t *current = tasks->head;
    while (current)
    {
        log_task_info(current->taskID, current->state, current->exit_status, current->pid, current->command);
        current = current->next;
    }
}

/*
 * A task is busy if it is working or suspended
 * Returns one if node is busy.
 */
int is_busy(Node_t *node)
{
    if ((node->state == LOG_STATE_WORKING) || (node->state == LOG_STATE_SUSPENDED))
    {
        // log_status_error(node->taskID, node->state);
        return 1;
    }
    return 0;
}

void delete (Tasks_t *tasks, int taskid)
{

    Node_t *current = tasks->head;

    if (current->taskID == taskid)
    {
        if (is_busy(current))
        {
            log_status_error(taskid, current->state);
            return;
        }
        tasks->head = current->next;
        tasks->count--;
        log_delete(taskid);
        return;
    }

    while (current->next)
    {

        if (current->next->taskID == taskid)
        {
            if (is_busy(current->next))
            {
                log_status_error(taskid, current->next->state);
                return;
            }
            current->next = current->next->next;
            log_delete(taskid);
            tasks->count--;
            return;
        }

        current = current->next;
    }
    log_task_id_error(taskid);
    return;
}

void insertAscendingOrder(Tasks_t *tasks, Node_t *node)
{

    if (!(tasks->head))
    {
        tasks->head = node;
        tasks->count++;
        return;
    }

    if (tasks->head->taskID > node->taskID)
    {
        node->next = tasks->head;
        tasks->head = node;
        tasks->count++;
        return;
    }

    Node_t *current = tasks->head;
    while (current->next)
    {
        if (current->next->taskID > node->taskID)
        {
            break;
        }
        current = current->next;
    }
    node->next = current->next;
    current->next = node;
    tasks->count++;
    return;
}

/*
 * If succesfully found, it returns the node, otherwise returns null
 */
Node_t *find_node(Tasks_t *tasks, int taskid)
{

    Node_t *previous = tasks->head;
    if (previous->taskID == taskid)
    {
        return previous;
    }
    Node_t *current = previous->next;
    while (current) // && current->taskID != taskid)
    {
        if (current->taskID == taskid)
        {
            return current;
        }
        current = current->next;
        previous = previous->next;
    }
    return NULL;

}

void run_task(Node_t *node, char *filename)
{
    char full_path[100] = "./";
    strcat(full_path, node->instruction);
    char second_path[100] = "/usr/bin/";
    strcat(second_path, node->instruction);

    pid_t pid;

    node->is_background_task = 0;
    node->state = LOG_STATE_WORKING;

    if ((pid = fork()) == 0)
    {
        setpgid(0, 0);

        if (filename)
        {
            int file = open(filename, O_RDONLY);
            if (file == -1)
            {
                log_file_error(node->taskID, filename);
                return;
            }
            int success = dup2(file, STDIN_FILENO);
            if (success == -1)
            { // dup2 failed
                exit(1);
            }
        }

        execv(full_path, node->argv);
        execv(second_path, node->argv);
        log_run_error(node->command);
        exit(1);
    }

    node->pid = pid;
    log_status_change(node->taskID, pid, LOG_FG, node->command, LOG_START);
    fg_reaper(pid);

}

void bg(Node_t *node, char *filename)
{
    char full_path[100] = "./";
    strcat(full_path, node->instruction);
    char second_path[100] = "/usr/bin/";
    strcat(second_path, node->instruction);

    pid_t pid;
    node->is_background_task = 1;
    node->state = LOG_STATE_WORKING;

    if ((pid = fork()) == 0)
    {
        setpgid(0, 0);

        if (filename)
        {
            int file = open(filename, O_RDONLY);
            if (file == -1)
            {
                log_file_error(node->taskID, filename);
                return;
            }
            int success = dup2(file, STDIN_FILENO);
            if (success == -1)
            { // dup2 failed
                exit(1);
            }
        }
        execv(full_path, node->argv);
        execv(second_path, node->argv);
        log_run_error(node->command);
        exit(1);
    }

    node->pid = pid;
    log_status_change(node->taskID, node->pid, LOG_LOG_BG, node->command, LOG_START);
}

void log_task(Node_t *node, int taskid, char *filename)
{
    char output_filename[100] = "log";
    snprintf(output_filename, sizeof(output_filename), "log%d", taskid);
    strcat(output_filename, ".txt");

    char full_path[100] = "./";
    strcat(full_path, node->instruction);

    char second_path[100] = "/usr/bin/";
    strcat(second_path, node->instruction);


    pid_t pid;
    int pipefd[2];
    pipe(pipefd);
    num_logged_files++;
    node->is_background_task = 1;
    if ((pid = fork()) == 0)
    { // run command and output to process running tee
        setpgid(0, 0);
        node->state = LOG_STATE_WORKING;
        dup2(pipefd[1], STDOUT_FILENO); // write to pipefd[1] instead of stdout
        close(pipefd[0]);
        execv(full_path, node->argv);
        execv(second_path, node->argv);
        log_run_error(node->command);

        exit(1);
    }
    pid_t pid2;
    if ((pid2 = fork()) == 0)
    { // get input from previous child and use it for tee
        setpgid(0, 0);
        node->pid = getpid();
        dup2(pipefd[0], STDIN_FILENO);
        close(pipefd[1]);
        close(pipefd[0]);
        execl("/usr/bin/tee", "tee", output_filename, NULL);
    }
    node->pid = pid;
    log_status_change(node->taskID, node->pid, LOG_LOG_BG, node->command, LOG_START);
}

void output(char *file)
{

    pid_t pid;
    if (!(pid = fork()))
    {
        execl("/usr/bin/cat", "cat", file, NULL);
        printf("output failed\n");
        exit(1);
    }
    waitpid(pid, NULL, 0);
}

/*
 * This helper function returns 1 if a file exists
 */
int file_exists(char *filename)
{
    FILE *f;
    if ((f = fopen(filename, "r")))
    {
        fclose(f);
        return 1;
    }
    return 0;
}

/*
 * If succesfully found, it returns the node, otherwise returns null
 */
Node_t *find_node_from_pid(Tasks_t *tasks, pid_t processID)
{

    Node_t *previous = tasks->head;
    if (previous->pid == processID)
    {
        return previous;
    }
    Node_t *current = previous->next;
    while (current && current->pid != processID)
    {
        current = current->next;
        previous = previous->next;
    }
    if (!current)
    {
        return NULL;
    }
    return current;
}

void cancel(Node_t *node)
{
    log_sig_sent(LOG_CMD_CANCEL, node->taskID, node->pid);
    kill(node->pid, SIGINT);
    node->state = LOG_STATE_KILLED;
}

void suspend(Node_t *node)
{
    log_sig_sent(LOG_CMD_SUSPEND, node->taskID, node->pid);
    kill(node->pid, SIGTSTP);
    node->state = LOG_STATE_SUSPENDED;
}

void resume(Node_t *node)
{
    log_sig_sent(LOG_CMD_RESUME, node->taskID, node->pid);
    kill(node->pid, SIGCONT);
    node->state = LOG_STATE_WORKING;
}


void reaper(int status, pid_t pid)
{

    Node_t *task = find_node_from_pid(global_tasks, pid);

    if (!task->is_background_task)
    {
        return;
    }

    if (WIFSIGNALED(status))
    {

        log_status_change(task->taskID, task->pid, task->is_background_task, task->command, LOG_CANCEL_SIG);
        task->state = LOG_STATE_KILLED;
        task->exit_status = WEXITSTATUS(status);
        return;
    }
    else if (WIFSTOPPED(status))
    {
        log_status_change(task->taskID, task->pid, task->is_background_task, task->command, LOG_SUSPEND);
        task->state = LOG_STATE_SUSPENDED;
        task->exit_status = WEXITSTATUS(status);
        return;
    }
    else if (WIFEXITED(status))
    {
        log_status_change(task->taskID, task->pid, task->is_background_task, task->command, LOG_CANCEL);
        task->state = LOG_STATE_COMPLETE;
        task->exit_status = WEXITSTATUS(status);
        return;
    }
}

void fg_reaper(pid_t pid)
{
    int status;
    waitpid(pid, &status, 0);
    if(global_node->stopped){return;} // dont check for stopped process
    if (WIFSIGNALED(status))
    {
        log_status_change(global_node->taskID, global_node->pid, global_node->is_background_task, global_node->command, LOG_CANCEL_SIG);
        global_node->state = LOG_STATE_KILLED;
        global_node->exit_status = WEXITSTATUS(status);
        return;
    }

    if (WIFEXITED(status))
    {
        log_status_change(global_node->taskID, global_node->pid, global_node->is_background_task, global_node->command, LOG_CANCEL);
        global_node->state = LOG_STATE_COMPLETE;
        global_node->exit_status = WEXITSTATUS(status);
        return;
    }
}