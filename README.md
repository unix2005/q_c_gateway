# 微服务网关系统

基于C语言开发的高性能微服务网关系统，支持HTTP/HTTPS协议，IPv4和IPv6双栈，动态服务注册和注销，健康检查，负载均衡等功能。

## 项目结构

```
├── gateway/             # 网关核心代码
│   ├── src/             # 源代码
│   ├── include/         # 头文件
│   ├── config/          # 配置文件
│   └── logs/            # 日志目录
├── service/             # 服务实例代码
│   ├── src/             # 源代码
│   ├── include/         # 头文件
│   ├── config/          # 配置文件
│   ├── logs/            # 日志目录
│   └── photos/          # 照片存储目录
├── 微服务网关系统.md    # 需求文档
└── README.md            # 本文件
```

## 功能特性

### 网关核心功能
- ✅ HTTP/HTTPS协议支持
- ✅ IPv4和IPv6双栈支持
- ✅ 动态服务注册和注销
- ✅ 服务健康检查
- ✅ 负载均衡（轮询、加权轮询、最少连接）
- ✅ 速率限制（基于令牌桶算法）
- ✅ 认证授权
- ✅ 日志审计
- ✅ 配置参数管理

### 服务实例功能
- ✅ 雇员信息查询
- ✅ 雇员照片上传和下载（最大10MB）
- ✅ 支持POST请求
- ✅ 照片存储到文件系统

## 技术特点

- **高性能设计**：使用SO_REUSEPORT+多线程+每个线程独立EPOLL/kqueue
- **跨平台支持**：兼容Linux和macOS
- **模块化架构**：清晰的代码结构和模块划分
- **C语言实现**：高效、轻量、可靠

## 快速开始

### 环境要求

- **编译工具**：GCC 4.9+ 或 Clang
- **操作系统**：Linux 3.10+ 或 macOS 10.12+
- **依赖库**：pthread

### 编译项目

#### 编译网关

```bash
cd gateway && make
```

#### 编译服务实例

```bash
cd service && make
```

### 运行项目

#### 1. 运行服务实例

```bash
cd service && ./service
```

服务实例默认运行在 **8081** 端口。

#### 2. 运行网关

```bash
cd gateway && ./gateway
```

网关默认运行在 **8080** 端口。

## 测试

### 服务实例API

| 接口 | 方法 | 描述 |
|------|------|------|
| `/employee` | GET | 获取所有雇员信息 |
| `/employee/{id}` | GET | 获取指定ID的雇员信息 |
| `/employee/photo/{id}` | GET | 获取指定ID的雇员照片 |
| `/employee/photo/{id}` | POST | 上传指定ID的雇员照片 |
| `/employee` | POST | 创建新雇员信息 |

### 测试示例

#### 获取所有雇员信息

```bash
curl http://localhost:8081/employee
```

#### 获取单个雇员信息

```bash
curl http://localhost:8081/employee/1
```

#### 上传雇员照片

```bash
curl -X POST -H "Content-Type: image/jpeg" --data-binary @photo.jpg http://localhost:8081/employee/photo/1
```

#### 获取雇员照片

```bash
curl -o photo.jpg http://localhost:8081/employee/photo/1
```

## 配置

### 网关配置

配置文件：`gateway/config/gateway.conf`

主要参数：
- `port`：网关监听端口（默认8080）
- `thread_pool_size`：线程池大小（默认4）
- `rate_limit_max_tokens`：速率限制最大令牌数（默认1000）
- `is_ipv6`：是否开启IPv6（0: 不开启, 1: 开启, 2: 双栈）
- `is_https`：是否开启HTTPS（0: 不开启, 1: 开启）

### 服务实例配置

配置文件：`service/config/service.conf`

主要参数：
- `service_name`：服务名称（默认user-service）
- `service_port`：服务监听端口（默认8081）
- `gateway_url`：网关URL（默认http://localhost:8080）
- `heartbeat_interval`：心跳间隔（秒，默认30）

## 日志

- 网关日志：`gateway/logs/`
- 服务实例日志：`service/logs/`

## 部署建议

1. **生产环境**：建议在Linux服务器上部署，开启HTTPS
2. **高可用**：可以部署多个网关实例，使用负载均衡器
3. **监控**：建议配置监控系统，监控网关和服务实例的运行状态
4. **安全**：生产环境中应配置TLS证书，开启认证授权

## 开发指南

### 添加新功能

1. 在相应模块的 `include/` 目录中添加头文件
2. 在 `src/` 目录中实现功能
3. 更新 `Makefile` 添加新文件
4. 编译并测试

### 代码风格

- 使用4空格缩进
- 函数名使用小写+下划线
- 变量名使用小写+下划线
- 结构体名使用大驼峰命名
- 头文件使用 `#ifndef` 保护

## 故障排查

### 常见问题

1. **端口被占用**：修改配置文件中的端口号
2. **编译失败**：检查依赖库是否安装，编译器版本是否符合要求
3. **服务不可用**：检查服务实例是否运行，网络连接是否正常
4. **性能问题**：调整线程池大小和速率限制参数

### 日志查看

```bash
# 查看网关日志
tail -f gateway/logs/gateway

# 查看服务实例日志
tail -f service/logs/service
```

## 许可证

MIT License

## 贡献

欢迎提交Issue和Pull Request！

## 联系方式

- GitHub: [https://github.com/unix2005/q_c_gateway](https://github.com/unix2005/q_c_gateway)
