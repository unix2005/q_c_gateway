#ifndef EMPLOYEE_H
#define EMPLOYEE_H

int employee_init();
void employee_cleanup();
void employee_info_get(int client_fd, const char *emp_id);
void employee_info_post(int client_fd, const char *body, int body_len);
void employee_list_get(int client_fd);
void employee_photo_get(int client_fd, const char *emp_id);
void employee_photo_post(int client_fd, const char *emp_id, const char *body, int body_len);

#endif