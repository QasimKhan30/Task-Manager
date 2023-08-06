/* Do not modify this file */
#ifndef LOGGING_H
#define LOGGING_H

#define LOG_FG      0
#define LOG_BG      1
#define LOG_LOG_BG  2

#define LOG_STATE_STANDBY    0
#define LOG_STATE_WORKING    1
#define LOG_STATE_SUSPENDED  2
#define LOG_STATE_COMPLETE   3
#define LOG_STATE_KILLED     4

#define LOG_CMD_SUSPEND 0
#define LOG_CMD_RESUME  1
#define LOG_CMD_CANCEL  2

#define LOG_CANCEL     0
#define LOG_CANCEL_SIG 1
#define LOG_RESUME     2
#define LOG_SUSPEND    3
#define LOG_START      4

void log_intro();
void log_prompt();
void log_help();
void log_quit();
void log_num_tasks(int num_tasks);
void log_task_info(int task_id, int status, int exit_code, int pid, const char *cmd);
void log_task_init(int task_id, const char *cmd);
void log_task_id_error(int task_id);
void log_delete(int task_id);
void log_status_error(int task_id, int status);
void log_status_change(int task_id, int pid, int type, const char *cmd, int transition);
void log_run_error(const char *line);
void log_sig_sent(int sig_type, int task_id, int pid);
void log_output_begin(int task_id);
void log_output_unlogged(int task_id);
void log_file_error(int task_id, const char *file);
void log_ctrl_c();
void log_ctrl_z();

#endif /*LOGGING_H*/
