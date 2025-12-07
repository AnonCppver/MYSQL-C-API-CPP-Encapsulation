///////////////////////////////////////////////////
//  程序名:_mci.h,c++操作mysql数据库的声明文件
//  作者:  AnontOkyO
///////////////////////////////////////////////////

#ifndef __MCI_H
#define __MCI_H 1

// C/C++库常用头文件
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string>
#include <mysql/mysql.h>
#include <mutex>

struct ERR_INFO
{
    unsigned int rc;             // 返回值
    unsigned long rpc;  // 如果是insert、update和delete，保存影响记录的行数，如果是select，保存结果集的行数。
    char message[1024]; // 执行SQL语句如果失败，存放错误描述信息。
};

// 用于持久化bind的参数
struct st_bind
{
    bool is_null;
    unsigned long length;
};

class connection;
class preparedstatement;

class connection
{
    friend class preparedstatement;

private:
    MYSQL* m_conn = nullptr; // MYSQL连接结构体
    ERR_INFO m_err;          // 存放错误信息
    enum
    {
        disconnected,
        connected
    };
    int m_state = disconnected; // 存放连接状态

    connection(const connection&) = delete;            // 禁用拷贝构造函数。
    connection& operator=(const connection&) = delete; // 禁用赋值函数。
    void err_report();  // 错误报告

public:
    connection();  // 构造函数。
    ~connection(); // 析构函数。
    bool connecttodb(const char* host, const char* user, const char* passwd, const char* db, unsigned short port = 3306, const char* charset = "utf8mb4", const char* unix_socket = NULL, unsigned long clientflag = 0);
    bool autocommit(bool mode);
    // 判断数据库是否已连接。
    bool isopen();
    // 提交事务。
    bool commit();
    // 回滚事务。
    bool rollback();
    // 断开与数据库的连接。
    // 注意，断开与数据库的连接时，全部未提交的事务自动回滚。
    void disconnect();
    // 执行静态的SQL语句。
    // 如果SQL语句不需要绑定输入和输出变量（无绑定变量、非查询语句），可以直接用此方法执行。
    // 参数说明：这是一个可变参数，用法与printf函数相同。
    // 程序中必须检查execute方法的返回值。
    bool execute(const char* fmt, ...);
    // 获取错误代码。
    int rc() { return m_err.rc; }
    // 获取错误描述。
    const char* message() { return m_err.message; }
};

class preparedstatement
{
private:
    connection* m_conn = nullptr;   // 连接类指针
    MYSQL_STMT* m_stmt = nullptr;   // 预处理指针
    unsigned long m_paramin = 0;    // bindin数量
    MYSQL_BIND* m_bindin = nullptr;
    bool m_is_bindin = false;         // bindin是否绑定
    unsigned long m_paramout = 0;  // bindout数量
    MYSQL_BIND* m_bindout = nullptr;
    bool m_is_bindout = false;       // bindout是否绑定
    st_bind* m_stbinds = nullptr;  // 保存bindin设置
    bool m_stmt_need_rst = false;  // 查询结果保存到本地
    bool m_sqltype;  // SQL语句的类型，false-查询语句；true-非查询语句。
    void err_report();                // 错误报告。

    preparedstatement(const preparedstatement&) = delete;             // 禁用拷贝构造函数。
    preparedstatement& operator=(const preparedstatement&) = delete;  // 禁用赋值函数。

    // 与数据库连接的关联状态，connected-已关联；disconnect-未关联。
    enum { disconnected, connected };
    int m_state;

    std::string m_sql;              // SQL语句的文本。
    ERR_INFO m_err;       // 执行SQL语句的结果。

public:
    preparedstatement();      // 构造函数。
    ~preparedstatement();
    // 指定数据库连接。
    // conn：数据库连接connection对象的地址。
    // 只要conn参数是有效的，并且数据库的游标资源足够，connect方法不会返回失败。
    // 注意，每个preparedstatement只需要指定一次，在指定新的connection前，必须先显式的调用disconnect方法。
    bool connect(connection* conn);
    bool isopen();   // 判断是否指定数据库连接。
    // 取消与数据库连接的关联。
    void disconnect();
    // 准备SQL语句。
    // 参数说明：这是一个可变参数，用法与printf函数相同。
    // 注意：如果SQL语句没有改变，只需要prepare一次就可以了。
    bool prepare(const std::string& strsql) { return prepare(strsql.c_str()); }
    bool prepare(const char* fmt, ...);
    void resize_bindout(int len);
    // 为了防止绑定失效，应当确保数据长度在string容量之内
    bool bindin(int i, const std::string& val);
    // 为了防止绑定失效，应当确保数据长度在char[]容量之内
    bool bindin(int i, const char* val);
    bool bindin(int i, const int& val);
    bool bindin(int i, const int64_t& val);
    bool bindin(int i, const double& val);
    // bool setdecimal(int i, std::string &val);
    bool bindin(int i, const bool& val);
    bool bindin(int i, const MYSQL_TIME& val);
    bool setnull(int i);
    // 截断长度，可以大于实际长度但必须小于数组长度
    bool bindout(int i, const char* val, unsigned long len);
    bool bindout(int i, std::string& val, unsigned long len);
    bool bindout(int i, const int& val);
    bool bindout(int i, const int64_t& val);
    bool bindout(int i, const double& val);
    bool bindout(int i, const bool& val);
    bool bindout(int i, const MYSQL_TIME& val);

    // save==true 如果是查询语句，那么将结果集保存至本地，同时rpc生效
    bool execute(bool save = false);
    // 查询的预处理语句获取下一行
    // 0  -正常
    // 100-到底
    // 101-缓冲区溢出
    int next();
    // 获取sql文本
    const char* sql() { return m_sql.c_str(); }
    // 错误信息
    const char* rm() { return m_err.message; }
    // 错误代码
    unsigned int rc() { return m_err.rc; };
    // 影响行数
    unsigned long rpc() { return m_err.rpc; };
};
// 将格式化字符串转换为MYSQL_TIME
// yyyy-mm-dd hh:mm:ss
MYSQL_TIME stringToMysqlTime(const std::string& datetimeStr);
// 将MYSQL_TIME转换为格式化字符串
// yyyy-mm-dd hh:mm:ss
std::string mysqlTimeToString(const MYSQL_TIME& t);
#endif