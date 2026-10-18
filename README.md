# Gomoku WebSocket

一个基于 C++、WebSocket 和 MySQL 的在线五子棋对战项目。用户可以注册、登录、匹配，并在浏览器中进行实时对局。

## 功能

- 用户注册与登录
- 在线匹配与房间管理
- WebSocket 实时对战
- 对局数据与用户积分保存到 MySQL
- 浏览器端静态页面

## 项目结构

```text
.
├── example/              # 练习与示例代码
├── source/
│   ├── wwwroot/          # 前端页面、样式和 JavaScript
│   ├── gobang.cc         # 程序入口
│   ├── server.hpp        # WebSocket 服务
│   ├── db.hpp            # 数据库访问
│   ├── room.hpp          # 房间管理
│   ├── matcher.hpp       # 匹配模块
│   ├── session.hpp       # 会话管理
│   ├── db.sql            # 数据库初始化脚本
│   └── Makefile
└── .gitignore
```

## 环境依赖

建议在 Ubuntu / Debian Linux 环境中构建和运行。

- C++11 编译器（g++）
- MySQL Server 与 MySQL C 客户端开发库
- jsoncpp 开发库
- OpenSSL 开发库
- WebSocket++ 头文件库

以 Ubuntu 为例：

```bash
sudo apt update
sudo apt install g++ make mysql-server libmysqlclient-dev libjsoncpp-dev libssl-dev libwebsocketpp-dev
```

## 初始化数据库

登录 MySQL 后执行项目中的初始化脚本：

```bash
mysql -u root -p < source/db.sql
```

该命令会创建 `gobang` 数据库和 `user` 表。注意：当前 `db.sql` 会先删除已有的 `gobang` 数据库；如果已有重要数据，请先备份或移除第一行再执行。

## 构建与运行

数据库密码不写入源码，而是通过环境变量提供：

```bash
cd source
make
export GOBANG_DB_PASSWORD='你的 MySQL root 密码'
./go
```

服务默认监听 `8085` 端口。浏览器访问服务器地址即可打开游戏页面，例如：

```text
http://服务器IP:8085/
```

### 后台运行

```bash
nohup ./go > gobang.log 2>&1 &
tail -f gobang.log
```

按 `Ctrl+C` 只会退出日志查看，不会停止后台服务。

## 更新部署

本地修改并推送到 GitHub：

```bash
git add .
git commit -m "描述本次修改"
git push
```

服务器端拉取、编译并重启服务：

```bash
git pull
cd source
make
```

## 注意事项

- 不要提交 `GOBANG_DB_PASSWORD`、数据库导出文件或服务器日志。
- `source/go` 是编译产物，已经被 `.gitignore` 忽略。
- 运行新版本前，确认旧进程没有继续占用 `8085` 端口：

```bash
ss -ltnp | grep ':8085'
```

## License

暂未指定开源许可证。
