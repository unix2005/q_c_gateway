#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>

#include "../include/employee.h"
#include "../include/log.h"

#define MAX_EMPLOYEES 100
#define MAX_PHOTO_SIZE 10 * 1024 * 1024 // 10MB

typedef struct {
    char id[64];
    char name[64];
    int age;
    char department[64];
    char position[64];
} Employee;

static Employee employees[MAX_EMPLOYEES];
static int employee_count = 0;
static pthread_mutex_t employee_mutex = PTHREAD_MUTEX_INITIALIZER;

int employee_init() {
    // 初始化雇员数据
    // 这里添加一些示例数据
    strcpy(employees[0].id, "1");
    strcpy(employees[0].name, "张三");
    employees[0].age = 30;
    strcpy(employees[0].department, "技术部");
    strcpy(employees[0].position, "工程师");

    strcpy(employees[1].id, "2");
    strcpy(employees[1].name, "李四");
    employees[1].age = 25;
    strcpy(employees[1].department, "市场部");
    strcpy(employees[1].position, "经理");

    employee_count = 2;

    // 创建照片存储目录
    mkdir("photos", 0755);

    log_info("Employee module initialized");
    return 0;
}

void employee_cleanup() {
    log_info("Employee module cleaned up");
}

void employee_info_get(int client_fd, const char *emp_id) {
    pthread_mutex_lock(&employee_mutex);

    // 查找雇员
    Employee *employee = NULL;
    for (int i = 0; i < employee_count; i++) {
        if (strcmp(employees[i].id, emp_id) == 0) {
            employee = &employees[i];
            break;
        }
    }

    pthread_mutex_unlock(&employee_mutex);

    if (employee == NULL) {
        const char *response = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n";
        write(client_fd, response, strlen(response));
        return;
    }

    // 构建响应
    char response[1024];
    snprintf(response, sizeof(response), 
             "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: %d\r\n\r\n{\"id\":\"%s\",\"name\":\"%s\",\"age\":%d,\"department\":\"%s\",\"position\":\"%s\"}",
             (int)(strlen(employee->id) + strlen(employee->name) + 40), // 估算长度
             employee->id, employee->name, employee->age, employee->department, employee->position);
    write(client_fd, response, strlen(response));
}

void employee_info_post(int client_fd, const char *body, int body_len) {
    // 简化处理，直接返回成功
    const char *response = "HTTP/1.1 201 Created\r\nContent-Length: 0\r\n\r\n";
    write(client_fd, response, strlen(response));
}

void employee_list_get(int client_fd) {
    pthread_mutex_lock(&employee_mutex);

    // 构建响应
    char response[4096] = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: ";
    char body[3072] = "[";

    for (int i = 0; i < employee_count; i++) {
        if (i > 0) {
            strcat(body, ",");
        }
        char emp_str[256];
        snprintf(emp_str, sizeof(emp_str), 
                 "{\"id\":\"%s\",\"name\":\"%s\",\"age\":%d,\"department\":\"%s\",\"position\":\"%s\"}",
                 employees[i].id, employees[i].name, employees[i].age, employees[i].department, employees[i].position);
        strcat(body, emp_str);
    }
    strcat(body, "]");

    char len_str[32];
    snprintf(len_str, sizeof(len_str), "%d", (int)strlen(body));
    strcat(response, len_str);
    strcat(response, "\r\n\r\n");
    strcat(response, body);

    pthread_mutex_unlock(&employee_mutex);

    write(client_fd, response, strlen(response));
}

void employee_photo_get(int client_fd, const char *emp_id) {
    // 构建照片文件路径
    char photo_path[256];
    snprintf(photo_path, sizeof(photo_path), "photos/%s.jpg", emp_id);

    // 打开照片文件
    int fd = open(photo_path, O_RDONLY);
    if (fd == -1) {
        const char *response = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n";
        write(client_fd, response, strlen(response));
        return;
    }

    // 获取文件大小
    struct stat stat_buf;
    fstat(fd, &stat_buf);
    off_t file_size = stat_buf.st_size;

    // 构建响应头
    char response[512];
    snprintf(response, sizeof(response), 
             "HTTP/1.1 200 OK\r\nContent-Type: image/jpeg\r\nContent-Length: %lld\r\n\r\n",
             (long long)file_size);
    write(client_fd, response, strlen(response));

    // 发送文件内容
    char buffer[4096];
    ssize_t n;
    while ((n = read(fd, buffer, sizeof(buffer))) > 0) {
        write(client_fd, buffer, n);
    }

    close(fd);
}

void employee_photo_post(int client_fd, const char *emp_id, const char *body, int body_len) {
    // 检查文件大小
    if (body_len > MAX_PHOTO_SIZE) {
        const char *response = "HTTP/1.1 413 Payload Too Large\r\nContent-Length: 0\r\n\r\n";
        write(client_fd, response, strlen(response));
        return;
    }

    // 构建照片文件路径
    char photo_path[256];
    snprintf(photo_path, sizeof(photo_path), "photos/%s.jpg", emp_id);

    // 写入照片文件
    int fd = open(photo_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd == -1) {
        const char *response = "HTTP/1.1 500 Internal Server Error\r\nContent-Length: 0\r\n\r\n";
        write(client_fd, response, strlen(response));
        return;
    }

    write(fd, body, body_len);
    close(fd);

    const char *response = "HTTP/1.1 201 Created\r\nContent-Length: 0\r\n\r\n";
    write(client_fd, response, strlen(response));
}