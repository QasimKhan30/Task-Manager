/* Do Not Modify This File */

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "logging.h"

#define BUFSIZE 255

#define textproc_log(s) fprintf(stderr,"\033[1;31m%s%s\033[0m",log_head, s); fflush(stderr)
#define textproc_unmarked_log(s) fprintf(stderr,"\033[1;31m%s\033[0m", s); fflush(stderr)

#define textproc_write(s) char output[BUFSIZE] = {0}; snprintf(output,BUFSIZE-1,"\033[1;31m%s%s\033[0m", log_head, s); write(STDERR_FILENO, output, strlen(output));

static const char *log_head = "[AALOG] ";
static const char *task_state[] = { "Standby", "Working", "Suspended", "Complete", "Killed", NULL };

/* Outputs an Introductory message at the start of the program */
void log_intro() { 
  textproc_log("Welcome to the A-A Task Manager!\n");
  textproc_log("\n");
}

/* Outputs the Help: All the Built-in Commands */
void log_help() { 
  textproc_log("Instructions:\n");
  textproc_log("    <COMMAND> [<ARGS>...],\n");
  textproc_log("    help, quit, tasks, delete <TASK>,\n");
  textproc_log("    run <TASK> [<FILE>],\n");
  textproc_log("    bg <TASK> [<FILE>], cancel <TASK>\n");
  textproc_log("    log <TASK> [<FILE>], output <TASK>\n");
  textproc_log("    suspend <TASK>, resume <TASK>\n");
  textproc_log("\n");
  textproc_log("Brackets denote optional arguments\n");
}

/* Outputs the message after running quit */
void log_quit(){
  textproc_log("Thanks for using the A-A Task Manager! Good-bye!\n");
}

/* Outputs the prompt */
void log_prompt() {
  printf("A-A: ");
  fflush(stdout);
}

/* Outputs a notification of a task deletion */
void log_delete(int task_id) {
  char buffer[BUFSIZE] = {0};
  sprintf(buffer, "Deleting Task ID #%d\n", task_id);
  textproc_log(buffer);
}

/* Outputs a notification of the start of a logged task output*/
void log_output_begin(int task_id) {
  char buffer[BUFSIZE] = {0};
  sprintf(buffer, "Output of Task ID #%d begin\n", task_id);
  textproc_log(buffer);
}

/* Outputs a notification of the start of a logged task output*/
void log_output_unlogged(int task_id) {
  char buffer[BUFSIZE] = {0};
  sprintf(buffer, "No saved output for Task ID #%d\n", task_id);
  textproc_log(buffer);
}

/* Outputs a notification of an error due to action in an incompatible state. */
void log_status_error(int task_id, int status) {
  char buffer[BUFSIZE] = {0};
  sprintf(buffer, "Error acting on Task ID #%d due to process in %s state\n", task_id, task_state[status]);
  textproc_log(buffer);
}

/* Outputs a notification of an file error */
void log_file_error(int task_id, const char *file) {
  char buffer[BUFSIZE] = {0};
  sprintf(buffer, "Error opening file %s for Task %d\n", file, task_id);
  textproc_log(buffer);
}

/* Output when the command is not found
 * eg. User typed in lss instead of ls and exec returns an error
 */ 
void log_run_error(const char *line) {
  char buffer[BUFSIZE] = {0};
  sprintf(buffer, "Error: %s: Command Cannot Load\n", line);
  textproc_log(buffer);
}

/* Output when activating a new task */
void log_task_init(int task_id, const char *cmd) {
  char buffer[BUFSIZE] = {0};
  sprintf(buffer, "Adding Task ID %d: %s (Standby)\n", task_id, cmd);
  textproc_write(buffer);
} 

/* Output when the given task id is not found */
void log_task_id_error(int task_id) {
  char buffer[BUFSIZE] = {0};
  sprintf(buffer, "Error: Task ID #%d Not Found in Task List\n", task_id);
  textproc_write(buffer);
}

/* Output when ctrl-c is received */
void log_ctrl_c() {
  textproc_write("Keyboard Combination control-c Received\n");
}

/* Output when ctrl-z is received */
void log_ctrl_z() {
  textproc_write("Keyboard Combination control-z Received\n");
}

/* Output when a signal is sent to a task's process */
void log_sig_sent(int sig_type, int task_id, int pid) {
  char buffer[BUFSIZE] = {0};
  static const char* sigs[] = {"Suspend", "Resume", "Cancel"};
  if (sig_type < 0 || sig_type >= 3) {
          textproc_write("Invalid input to log_sig_sent\n");
          return;
  }
  sprintf(buffer,"%s message sent to Task ID #%d (PID %d)\n", sigs[sig_type], task_id, pid);
  textproc_log(buffer);
}

/* Output when a job changes state.
 * (Signal Handler Safe Outputting)
 */
void log_status_change(int task_id, int pid, int type, const char *cmd, int transition) {
  char buffer[BUFSIZE] = {0};
  static const char* msgs[] = {"Terminated Normally", "Terminated by Signal", "Continued", "Stopped", "Started"};
  static const char* types[] = {"Foreground", "Background", "Logged Background"};
  if (transition < 0 || transition >= 5) {
          textproc_write("Invalid input to log_status_change\n");
          return;
  }
  sprintf(buffer,"%s Process %d (Task %d): %s (%s)\n",types[type], pid, task_id, cmd, msgs[transition]);
  textproc_write(buffer);
}

/* Output to list the task counts */
void log_num_tasks(int num_tasks){
  char buffer[BUFSIZE] = {0};
  sprintf(buffer, "%d Task(s)\n", num_tasks);
  textproc_log(buffer);
}

/* Output info about a single task */
void log_task_info(int task_id, int status, int exit_code, int pid, const char *cmd){
  char buffer[BUFSIZE] = {0};
  if (status < 0 || status >= 5) {
          textproc_write("Invalid input to log_task_info\n");
          return;
  }
  if (!cmd) 
  { sprintf(buffer, "Task %d: (%s)\n", task_id, task_state[status]); }
  else if (!pid) 
  { sprintf(buffer, "Task %d: %s (%s)\n", task_id, cmd, task_state[status]); }
  else if (status != LOG_STATE_COMPLETE && status != LOG_STATE_KILLED) 
  { sprintf(buffer, "Task %d: %s (PID %d; %s)\n", task_id, cmd, pid, task_state[status]); }
  else
  { sprintf(buffer, "Task %d: %s (PID %d; %s; exit code %d)\n", task_id, cmd, pid, task_state[status], exit_code); }

  textproc_log(buffer);
}
