#ifndef __M_UTIL_H__
#define __M_UTIL_H__
#include "logger.hpp"
#include <iostream>
#include <sstream>
#include <fstream>
#include <string>
#include <memory>
#include <vector>
#include <cstdint>
#include <array>
#include <openssl/rand.h>
#include <jsoncpp/json/json.h>
#include <mysql/mysql.h>

#include <websocketpp/server.hpp>
#include <websocketpp/config/asio_no_tls.hpp>

typedef websocketpp::server<websocketpp::config::asio> wsserver_t;

class mysql_util {
public:
    static MYSQL *mysql_create(const std::string &host,
        const std::string &username,
        const std::string &password,
        const std::string &dbname,
        uint16_t port = 3306) {
            MYSQL *mysql = mysql_init(NULL);
            if (mysql == NULL) {
                ELOG("mysql init failed!");
                return NULL;
            }
            //2. 连接服务器,直接连接到库dbname
            if (mysql_real_connect(mysql, 
                host.c_str(), 
                username.c_str(), 
                password.c_str(), 
                dbname.c_str(), port, NULL, 0) == NULL) {
                ELOG("connect mysql server failed : %s", mysql_error(mysql));
                mysql_close(mysql);
                return NULL;
            }
            //3. 设置客户端字符集
            if (mysql_set_character_set(mysql, "utf8") != 0) {
                ELOG("set client character failed : %s", mysql_error(mysql));
                mysql_close(mysql);
                return NULL;
            }
            return mysql;
    }
        // 传入上面拿到的句柄，告诉 mysqlclient**操作哪一条数据库连接**，执行增删改查 SQL
        static bool mysql_exec(MYSQL *mysql, const std::string &sql) {
            int ret = mysql_query(mysql, sql.c_str());
            if (ret != 0) {
                // SQL 可能含有用户输入（包括密码），不能写入日志。
                ELOG("mysql query failed : %s", mysql_error(mysql));
                return false;
            }
            return true;
        }
        //释放网络资源和内存，防止内存泄漏
        static void mysql_destroy(MYSQL *mysql) {
            if (mysql != NULL) {
                mysql_close(mysql);
            }
            return ;
        }
};

class json_util{
    public:
        static bool serialize(const Json::Value &root, std::string &str) {
            Json::StreamWriterBuilder swb;//创建一个json流写入器    
            std::unique_ptr<Json::StreamWriter>sw(swb.newStreamWriter());
            std::stringstream ss;
            int ret = sw->write(root, &ss);
            if (ret != 0) {
                ELOG("json serialize failed!!");
                return false;
            }
            str = ss.str();
            return true;
        }
        static bool unserialize(const std::string &str, Json::Value &root) {
            Json::CharReaderBuilder crb;
            std::unique_ptr<Json::CharReader> cr(crb.newCharReader());
            std::string err;
            bool ret = cr->parse(str.c_str(), str.c_str() + str.size(), &root, &err);
            if (ret == false) {
                ELOG("json unserialize failed: %s", err.c_str());
                return false;
            }
            return true;
        }
};
/// 字符串工具类
class string_util{
  public:
        static int split(const std::string &src, const std::string &sep, std::vector<std::string> &res) {
            // 123,234,,,,345
            size_t pos, idx = 0;
            while(idx < src.size()) {
                pos = src.find(sep, idx);
                if (pos == std::string::npos) {
                    //没有找到,字符串中没有间隔字符了，则跳出循环
                    res.push_back(src.substr(idx));
                    break;
                }
                if (pos == idx) {
                    idx += sep.size();
                    continue;
                }
                res.push_back(src.substr(idx, pos - idx));
                idx = pos + sep.size();
            }
            return res.size();
        }
};

// Session ID 是身份凭证，不能使用自增编号、时间戳或 rand() 生成。
// 使用 OpenSSL 的 CSPRNG 生成 256 位随机字节，并编码为 64 位十六进制字符串。
class session_id_util {
  public:
        static bool generate(std::string &session_id) {
            std::array<unsigned char, 32> bytes{};
            if (RAND_bytes(bytes.data(), static_cast<int>(bytes.size())) != 1) {
                ELOG("generate secure session id failed");
                return false;
            }

            static const char hex[] = "0123456789abcdef";
            session_id.clear();
            session_id.reserve(bytes.size() * 2);
            for (unsigned char byte : bytes) {
                session_id.push_back(hex[(byte >> 4) & 0x0f]);
                session_id.push_back(hex[byte & 0x0f]);
            }
            return true;
        }
};
/// 文件工具类
class file_util {
   public:
        static bool read(const std::string &filename, std::string &body) {
            //打开文件
            std::ifstream ifs(filename, std::ios::binary);
            if (ifs.is_open() == false) {
                ELOG("%s file open failed!!", filename.c_str());
                return false;
            }
            //获取文件大小
            size_t fsize = 0;
            ifs.seekg(0, std::ios::end);
            fsize = ifs.tellg();
            ifs.seekg(0, std::ios::beg);
            body.resize(fsize);
            //将文件所有数据读取出来
            ifs.read(&body[0], fsize);
            if (ifs.good() == false) {
                ELOG("read %s file content failed!", filename.c_str());
                ifs.close();
                return false;
            }
            //关闭文件
            ifs.close();
            return true;
        }
};
#endif
