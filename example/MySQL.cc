#include <stdio.h>
#include <string.h>
#include <cstdlib>
#include <mysql/mysql.h>

#define HOST "127.0.0.1"
#define PORT 3306
#define USER "root"
#define DBNAME "gobang"
int main()
{
    // 示例程序同样不保存真实密码；运行前设置 GOBANG_DB_PASSWORD。
    const char *db_pass = std::getenv("GOBANG_DB_PASSWORD");
    if (db_pass == NULL || db_pass[0] == '\0') {
        fprintf(stderr, "GOBANG_DB_PASSWORD is not set\n");
        return -1;
    }

    //1. 初始化mysql句柄
    // MYSQL *mysql_init(MYSQL *mysql);
    MYSQL *mysql = mysql_init(NULL);
    if (mysql == NULL) {
        printf("mysql init failed!\n");
        return -1;
    }
    //2. 连接服务器
    // MYSQL *mysql_real_connect(mysql, host, user, pass, dbname, port, unix_socket, flag);
    if (mysql_real_connect(mysql, HOST, USER, db_pass, DBNAME, PORT, NULL, 0) == NULL) {
        printf("connect mysql server failed : %s\n", mysql_error(mysql));
        mysql_close(mysql);
        return -1;
    }
    //3. 设置客户端字符集
    // int mysql_set_character_set(mysql, "utf8");
    if (mysql_set_character_set(mysql, "utf8") != 0) {
        printf("set client character failed : %s\n", mysql_error(mysql));
        mysql_close(mysql);
        return -1;
    }
    //4. 选择要操作的数据库
    // int mysql_select_db(mysql, dbname)
    // mysql_select_db(mysql, DBNAME);

    //5. 执行sql语句
    // int mysql_query(MYSQL *mysql,  char *sql);
    //char *sql = "insert stu values(null, '小明', 18, 53, 68, 87);";
    //char *sql = "update stu set ch=ch+40 where sn=1;";
    //char *sql = "delete from stu where sn=1;";
    char *sql = "select * from stu;";
    int ret = mysql_query(mysql, sql);
    if (ret != 0) {
        printf("%s\n", sql);
        printf("mysql query failed : %s\n", mysql_error(mysql));
        mysql_close(mysql);
        return -1;
    }
    //6. 如果sql语句是查询语句，则需要保存结果到本地
    // MYSQL_RES *mysql_store_result(MYSQL *mysql)
    MYSQL_RES *res = mysql_store_result(mysql);
    if (res == NULL) {
        mysql_close(mysql);
        return -1;
    }
    //7. 获取结果集中的结果条数
    // int mysql_num_rows(MYSQL_RES *res);
    // int mysql_num_fields(MYSQL_RES *res);
    int num_row = mysql_num_rows(res);
    int num_col = mysql_num_fields(res);
    //8. 遍历保存到本地的结果集
    for (int i = 0; i < num_row; i++) {
        // MYSQL_ROW mysql_fetch_row(MYSQL *res);
        MYSQL_ROW row = mysql_fetch_row(res);
        for (int i = 0; i < num_col; i++) {
            printf("%s\t", row[i]);
        }
        printf("\n");
    }
    //9. 释放结果集
    mysql_free_result(res);
    //10. 关闭连接，释放句柄
    mysql_close(mysql);
    return 0;
}
