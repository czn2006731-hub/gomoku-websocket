#ifndef __M_DB_H__
#define __M_DB_H__
#include "util.hpp"
#include <mutex>
#include <cassert>
#include <cinttypes>

class user_table{
   private:
          MYSQL *_mysql; //mysql操作句柄
          std::mutex _mutex;//互斥锁保护数据库的访问操作
          std::string _host;
          std::string _username;
          std::string _password;
          std::string _dbname;
          uint16_t _port;

          // 调用方已持有 _mutex。mysql_ping 可发现 MySQL 的空闲连接超时，
          // 此时使用启动时的连接参数建立一条新连接。
          bool ensure_connection_locked() {
               if (_mysql != NULL && mysql_ping(_mysql) == 0) {
                    return true;
               }

               ELOG("mysql connection unavailable; reconnecting");
               mysql_util::mysql_destroy(_mysql);
               _mysql = mysql_util::mysql_create(
                    _host, _username, _password, _dbname, _port);
               return _mysql != NULL;
          }

          bool mysql_exec_locked(const std::string &sql) {
               if (ensure_connection_locked() == false) {
                    ELOG("mysql reconnect failed");
                    return false;
               }
               return mysql_util::mysql_exec(_mysql, sql);
          }
   public:
          user_table(const std::string &host,
               const std::string &username,
               const std::string &password,
               const std::string &dbname,
               uint16_t port = 3306)
               : _mysql(NULL), _host(host), _username(username), _password(password),
                 _dbname(dbname), _port(port) {
               _mysql = mysql_util::mysql_create(_host, _username, _password, _dbname, _port);
               assert(_mysql != NULL);
          }
          ~user_table() {
               mysql_util::mysql_destroy(_mysql);
               _mysql = NULL;
          }
          //注册时新增用户
          bool insert(Json::Value &user) {
#define INSERT_USER "insert user values(null, '%s', '%s', 1000, 0, 0);"
               // sprintf(void *buf, char *format, ...)
               if (user["password"].isNull() || user["username"].isNull()) {
                    DLOG("INPUT PASSWORD OR USERNAME");
                    return false;
               }
               char sql[4096] = {0};
               sprintf(sql, INSERT_USER, user["username"].asCString(), user["password"].asCString());
               std::unique_lock<std::mutex> lock(_mutex);
               bool ret = mysql_exec_locked(sql);
               if (ret == false) {
                    DLOG("insert user info failed!!\n");
                    return false;
               }
               return true;
          }
          //登录验证，并返回详细的用户信息
          bool login(Json::Value &user) {
               if (user["password"].isNull() || user["username"].isNull()) {
                    DLOG("INPUT PASSWORD OR USERNAME");
                    return false;
               }
               //以用户名和密码共同作为查询过滤条件，查询到数据则表示用户名密码一致，没有信息则用户名密码错误
#define LOGIN_USER "select id, score, total_count, win_count from user where username='%s' and password='%s';"
               char sql[4096] = {0};
               sprintf(sql, LOGIN_USER, user["username"].asCString(), user["password"].asCString());
               MYSQL_RES *res = NULL;
               {
                    std::unique_lock<std::mutex> lock(_mutex);
                    bool ret = mysql_exec_locked(sql);
                    if (ret == false) {
                         DLOG("user login failed!!\n");
                         return false;
                    }
                    //按理说要么有数据，要么没有数据，就算有数据也只能有一条数据
                    res = mysql_store_result(_mysql);//拉取MySQL结果，返回到MYSQL_RES结构体中
                    if (res == NULL) {
                         DLOG("have no login user info!!");
                         return false;
                    }
               }
               int row_num = mysql_num_rows(res);
               if (row_num != 1) {
                    DLOG("the user information queried is not unique!!");
                    return false;
               }
               //登记验证函数，返回数据
               MYSQL_ROW row = mysql_fetch_row(res);
               user["id"] = (Json::UInt64)std::stol(row[0]);
               user["score"] = (Json::UInt64)std::stol(row[1]);
               user["total_count"] = std::stoi(row[2]);
               user["win_count"] = std::stoi(row[3]);
               mysql_free_result(res);
               return true;
          }
          // 通过用户名获取用户信息
          bool select_by_name(const std::string &name, Json::Value &user) {
#define USER_BY_NAME "select id, score, total_count, win_count from user where username='%s';"
               char sql[4096] = {0};
               sprintf(sql, USER_BY_NAME, name.c_str());
               MYSQL_RES *res = NULL;
               {
                    std::unique_lock<std::mutex> lock(_mutex);
                    bool ret = mysql_exec_locked(sql);
                    if (ret == false) {
                         DLOG("get user by name failed!!\n");
                         return false;
                    }
                    //按理说要么有数据，要么没有数据，就算有数据也只能有一条数据
                    res = mysql_store_result(_mysql);
                    if (res == NULL) {
                         DLOG("have no user info!!");
                         return false;
                    }
               }
               int row_num = mysql_num_rows(res);
               if (row_num != 1) {
                    DLOG("the user information queried is not unique!!");
                    return false;
               }
               MYSQL_ROW row = mysql_fetch_row(res);
               user["id"] = (Json::UInt64)std::stol(row[0]);
               user["username"] = name;
               user["score"] = (Json::UInt64)std::stol(row[1]);
               user["total_count"] = std::stoi(row[2]);
               user["win_count"] = std::stoi(row[3]);
               mysql_free_result(res);
               return true;
          }
          // 通过用户名获取用户信息
          bool select_by_id(uint64_t id, Json::Value &user) {
#define USER_BY_ID "select username, score, total_count, win_count from user where id=%d;"
               char sql[4096] = {0};
               sprintf(sql, USER_BY_ID, id);
               MYSQL_RES *res = NULL;
               {
                    std::unique_lock<std::mutex> lock(_mutex);
                    bool ret = mysql_exec_locked(sql);
                    if (ret == false) {
                         DLOG("get user by id failed!!\n");
                         return false;
                    }
                    //按理说要么有数据，要么没有数据，就算有数据也只能有一条数据
                    res = mysql_store_result(_mysql);
                    if (res == NULL) {
                         DLOG("have no user info!!");
                         return false;
                    }
               }
               int row_num = mysql_num_rows(res);
               if (row_num != 1) {
                    DLOG("the user information queried is not unique!!");
                    return false;
               }
               MYSQL_ROW row = mysql_fetch_row(res);
               user["id"] = (Json::UInt64)id;
               user["username"] = row[0];
               user["score"] = (Json::UInt64)std::stol(row[1]);
               user["total_count"] = std::stoi(row[2]);
               user["win_count"] = std::stoi(row[3]);
               mysql_free_result(res);
               return true;
          }
          //胜利时天梯分数增加30分，战斗场次增加1，胜利场次增加1
          bool win(uint64_t id) {
#define USER_WIN "update user set score=score+30, total_count=total_count+1, win_count=win_count+1 where id=%d;"
               char sql[4096] = {0};
               sprintf(sql, USER_WIN, id);
               std::unique_lock<std::mutex> lock(_mutex);
               bool ret = mysql_exec_locked(sql);
               if (ret == false) {
                    DLOG("update win user info failed!!\n");
                    return false;
               }
               return true;
          }
          //失败时天梯分数减少30，战斗场次增加1，其他不变
          bool lose(uint64_t id) {
#define USER_LOSE "update user set score=score-30, total_count=total_count+1 where id=%d;"
               char sql[4096] = {0};
               sprintf(sql, USER_LOSE, id);
               std::unique_lock<std::mutex> lock(_mutex);
               bool ret = mysql_exec_locked(sql);
               if (ret == false) {
                    DLOG("update lose user info failed!!\n");
                    return false;
               }
               return true;
          }
};
#endif
