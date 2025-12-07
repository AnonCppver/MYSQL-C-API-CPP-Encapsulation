///////////////////////////////////////////////////
//  程序名:_mci.cpp,c++操作mysql数据库的定义文件
//  作者:  AnontOkyO
///////////////////////////////////////////////////

#include "_mci.h"

using namespace std;

connection::connection()
{
    m_conn = mysql_init(nullptr);
    memset(&m_err, 0, sizeof(ERR_INFO));

    m_err.rc = 12050;
    strncpy(m_err.message, "database not connected.", 128);
}

connection::~connection()
{
    disconnect();
}

bool connection::connecttodb(const char* host, const char* user, const char* passwd, const char* db, unsigned short port, const char* charset, const char* unix_socket, unsigned long clientflag)
{
    // 如果已连接上数据库，就不再连接
    // 所以，如果想重连数据库，必须显示的调用disconnect()方法后才能重连
    if (m_state == connected)
        return true;
    if (mysql_real_connect(m_conn, host, user, passwd, db, port, unix_socket, clientflag) == nullptr)
        return false;
    // 设置字符集
    mysql_set_character_set(m_conn, charset);
    // 关闭自动提交
    autocommit(false);
    m_state = connected;

    return true;
}

bool connection::autocommit(bool mode)
{
    return mysql_autocommit(m_conn, mode);
}

bool connection::commit()
{
    return mysql_commit(m_conn);
}

bool connection::rollback()
{
    return mysql_rollback(m_conn);
}

bool connection::isopen()
{
    if (m_state == disconnected)
        return false;
    if (m_conn == nullptr)
        return false;
    if (mysql_ping(m_conn) == 0)
        return true;
    m_state = disconnected;
    return false;
}

void connection::disconnect()
{
    memset(&m_err, 0, sizeof(m_err));

    rollback();

    if (m_conn != nullptr)
    {
        mysql_close(m_conn);
    }

    m_conn = nullptr;
    m_state = disconnected;
}

bool connection::execute(const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int len = vsnprintf(nullptr, 0, fmt, ap);
    va_end(ap);
    if (len <= 0)
        return -1;
    va_start(ap, fmt);
    string strsql;
    strsql.resize(len);
    vsnprintf(&strsql[0], len + 1, fmt, ap);
    va_end(ap);

    if (mysql_query(m_conn, strsql.c_str()))
    {
        err_report();
        return false;
    }

    return true;
}

void connection::err_report()
{
    if (m_state == disconnected)
    {
        m_err.rc = 12050;
        strncpy(m_err.message, "database not open.", 128);
        return;
    }

    memset(&m_err, 0, sizeof(ERR_INFO));

    strncpy(m_err.message, mysql_error(m_conn), 1023);
    m_err.rc = mysql_errno(m_conn);
}

preparedstatement::preparedstatement()
{
    m_state = disconnected;

    memset(&m_err, 0, sizeof(ERR_INFO));

    m_err.rc = 12051;
    strncpy(m_err.message, "sqlstatement not connect to connection.\n", 128);
}

preparedstatement::~preparedstatement()
{
    disconnect();
}

void preparedstatement::err_report()
{
    m_err.rc = mysql_stmt_errno(m_stmt);
    strncpy(m_err.message, mysql_stmt_error(m_stmt), 1023);
}

bool preparedstatement::connect(connection* conn)
{
    // 注意，一个preparedstatement在程序中只能指定一个connection，不允许指定多个connection。
    // 所以，只要这个preparedstatement已指定connection，直接返回成功。
    if (m_state == connected) return true;

    memset(&m_err, 0, sizeof(ERR_INFO));

    m_conn = conn;

    // 如果数据库连接对象的指针为空，直接返回失败
    if (m_conn == nullptr)
    {
        m_err.rc = 12050; strncpy(m_err.message, "database not open.\n", 128); return false;
    }

    // 如果数据库连接不可用，直接返回失败
    if (m_conn->m_state == disconnected)
    {
        m_err.rc = 12050; strncpy(m_err.message, "database not open.\n", 128); return false;
    }

    m_stmt = mysql_stmt_init(m_conn->m_conn);

    m_state = connected;

    return true;
}

void preparedstatement::disconnect()
{
    if (m_state == disconnected) return;

    memset(&m_err, 0, sizeof(ERR_INFO));

    m_state = disconnected;

    if (m_stmt_need_rst)
        mysql_stmt_free_result(m_stmt);
    if (m_stmt != nullptr)
        mysql_stmt_close(m_stmt);
    if (m_bindin != nullptr)
        delete[] m_bindin;
    if (m_bindout != nullptr)
        delete[] m_bindout;
    if (m_stbinds != nullptr)
        delete[] m_stbinds;

    m_stmt_need_rst = false;
    m_stmt = nullptr;
    m_bindin = nullptr;
    m_bindout = nullptr;
    m_stbinds = nullptr;
    m_paramin = 0;
    m_paramout = 0;
    m_is_bindin = false;
    m_is_bindout = false;

    m_err.rc = 12051;
    strncpy(m_err.message, "cursor not open.", 128);

    return;
}

bool preparedstatement::isopen()
{
    if (m_state == disconnected)
        return false;
    if (m_stmt == nullptr)
        return false;
    if (m_conn == nullptr)
        return false;
    return m_conn->isopen();
}

// 把字符串中的小写字母转换成大写，忽略不是字母的字符。
// 这个函数只在prepare方法中用到。
void OR__ToUpper(char* str)
{
    if (str == 0) return;

    if (strlen(str) == 0) return;

    int istrlen = strlen(str);

    for (int ii = 0; ii < istrlen; ii++)
    {
        if ((str[ii] >= 'a') && (str[ii] <= 'z')) str[ii] = str[ii] - 32;
    }
}

// 删除字符串左边的空格。
// 这个函数只在prepare方法中用到。
void OR__DeleteLChar(char* str, const char chr)
{
    if (str == 0) return;
    if (strlen(str) == 0) return;

    char strTemp[strlen(str) + 1];

    int iTemp = 0;

    memset(strTemp, 0, sizeof(strTemp));
    strcpy(strTemp, str);

    while (strTemp[iTemp] == chr)  iTemp++;

    memset(str, 0, strlen(str) + 1);

    strcpy(str, strTemp + iTemp);

    return;
}

bool preparedstatement::prepare(const char* fmt, ...)
{
    memset(&m_err, 0, sizeof(ERR_INFO));

    if (m_state == disconnected)
    {
        m_err.rc = 12051; strncpy(m_err.message, "cursor not open.\n", 128); return -1;
    }

    m_sql.clear();

    va_list ap;
    va_start(ap, fmt);
    int len = vsnprintf(nullptr, 0, fmt, ap);
    va_end(ap);
    if (len <= 0) return -1;
    va_start(ap, fmt);
    m_sql.resize(len);
    vsnprintf(&m_sql[0], len + 1, fmt, ap);
    va_end(ap);

    int ret = mysql_stmt_prepare(m_stmt, m_sql.c_str(), strlen(m_sql.c_str()));
    if (ret != 0)
    {
        err_report();
        return false;
    }

    // 判断是否是查询语句，如果是，把m_sqltype设为false，其它语句设为true。
    m_sqltype = true;

    // 从待执行的SQL语句中截取30个字符，如果是以"select"打头，就认为是查询语句。
    char strtemp[31]; memset(strtemp, 0, sizeof(strtemp));
    strncpy(strtemp, m_sql.c_str(), 30);
    OR__ToUpper(strtemp); OR__DeleteLChar(strtemp, ' ');
    if (strncmp(strtemp, "SELECT", 6) == 0)  m_sqltype = false;

    // 获得bindin参数个数
    m_paramin = mysql_stmt_param_count(m_stmt);
    if (m_paramin != 0)
    {
        // 初始化参数的结构体
        m_stbinds = new st_bind[m_paramin];
        memset(m_stbinds, 0, sizeof(st_bind) * m_paramin);

        // 初始化bindin结构体数组
        m_bindin = new MYSQL_BIND[m_paramin];
        memset(m_bindin, 0, sizeof(MYSQL_BIND) * m_paramin);

        // 绑定参数
        for (int i = 0; i < m_paramin; i++)
        {
            m_stbinds[i].length = 0;
            m_bindin[i].length = &m_stbinds[i].length;

            m_stbinds[i].is_null = false;
            m_bindin[i].is_null = &m_stbinds[i].is_null;
        }
    }

    resize_bindout(16);
    return true;
}

void preparedstatement::resize_bindout(int len)
{
    m_paramout = len <= 16 ? 16 : len;
    // 如果是查询语句 创建bindout数组 初始长度16
    if (m_sqltype == false)
    {
        if (m_bindout != nullptr)
            delete[]m_bindout;
        // 初始化bindout结构体数组
        m_bindout = new MYSQL_BIND[m_paramout];
        memset(m_bindout, 0, sizeof(MYSQL_BIND) * m_paramout);
    }
}

bool preparedstatement::bindin(int i, const string& val)
{
    if (i < 0 || i >= m_paramin)
        return false;

    m_bindin[i].buffer_type = MYSQL_TYPE_STRING;

    m_bindin[i].buffer = (char*)val.c_str(); // 内存地址的映射

    return true;
}

bool preparedstatement::bindin(int i, const char* val)
{
    if (i < 0 || i >= m_paramin)
        return false;

    m_bindin[i].buffer_type = MYSQL_TYPE_STRING;

    m_bindin[i].buffer = (char*)val; // 内存地址的映射

    return true;
}

bool preparedstatement::bindin(int i, const double& val)
{
    if (i < 0 || i >= m_paramin)
        return false;
    m_bindin[i].buffer_type = MYSQL_TYPE_DOUBLE;

    m_bindin[i].buffer = (char*)&val;

    return true;
}

bool preparedstatement::bindin(int i, const int& val)
{
    if (i < 0 || i >= m_paramin)
        return false;
    m_bindin[i].buffer_type = MYSQL_TYPE_LONG;

    m_bindin[i].buffer = (char*)&val;

    return true;
}

bool preparedstatement::bindin(int i, const int64_t& val)
{
    if (i < 0 || i >= m_paramin)
        return false;
    m_bindin[i].buffer_type = MYSQL_TYPE_LONGLONG;

    m_bindin[i].buffer = (char*)&val;

    return true;
}
/*
bool mysql::setdecimal(int i, std::string &val)
{
    if (i < 0 || i >= m_params)
        return false;
    m_bind[i].buffer_type = MYSQL_TYPE_DECIMAL;

    m_bind[i].buffer = (char *)val.c_str(); // 内存地址的映射

    m_stbinds[i].is_null = false;
    m_bind[i].is_null = &m_stbinds[i].is_null;

    m_stbinds[i].length = val.size();
    m_bind[i].length = &m_stbinds[i].length;

    return true;
}
*/
bool preparedstatement::bindin(int i, const bool& val)
{
    if (i < 0 || i >= m_paramin)
        return false;
    m_bindin[i].buffer_type = MYSQL_TYPE_TINY;

    m_bindin[i].buffer = (char*)&val;

    return true;
}

bool preparedstatement::bindin(int i, const MYSQL_TIME& val)
{
    if (i < 0 || i >= m_paramin)
        return false;
    m_bindin[i].buffer_type = MYSQL_TYPE_DATETIME;

    m_bindin[i].buffer = (char*)&val;

    return true;
}

bool preparedstatement::setnull(int i)
{
    if (i < 0 || i >= m_paramin)
        return false;

    m_bindin[i].buffer_type = MYSQL_TYPE_NULL;

    m_stbinds[i].is_null = true;
    m_bindin[i].is_null = &m_stbinds[i].is_null;

    return true;
}

bool preparedstatement::bindout(int i, const char* val, unsigned long len)
{
    if (i < 0 || i >= m_paramout)
        return false;
    m_bindout[i].buffer_type = MYSQL_TYPE_STRING;

    m_bindout[i].buffer = (char*)val;

    m_bindout[i].buffer_length = len;

    return true;
}

bool preparedstatement::bindout(int i, string& val, unsigned long len)
{
    if (i < 0 || i >= m_paramout)
        return false;
    m_bindout[i].buffer_type = MYSQL_TYPE_STRING;

    val.resize(len + 1);

    m_bindout[i].buffer = (char*)val.c_str();

    m_bindout[i].buffer_length = len;

    return true;
}

bool preparedstatement::bindout(int i, const int& val)
{
    if (i < 0 || i >= m_paramout)
        return false;
    m_bindout[i].buffer_type = MYSQL_TYPE_LONG;

    m_bindout[i].buffer = (char*)&val;

    return true;
}

bool preparedstatement::bindout(int i, const int64_t& val)
{
    if (i < 0 || i >= m_paramout)
        return false;
    m_bindout[i].buffer_type = MYSQL_TYPE_LONGLONG;

    m_bindout[i].buffer = (char*)&val;

    return true;
}

bool preparedstatement::bindout(int i, const double& val)
{
    if (i < 0 || i >= m_paramout)
        return false;
    m_bindout[i].buffer_type = MYSQL_TYPE_DOUBLE;

    m_bindout[i].buffer = (char*)&val;

    return true;
}

bool preparedstatement::bindout(int i, const bool& val)
{
    if (i < 0 || i >= m_paramout)
        return false;
    m_bindout[i].buffer_type = MYSQL_TYPE_TINY;

    m_bindout[i].buffer = (char*)&val;

    return true;
}

bool preparedstatement::bindout(int i, const MYSQL_TIME& val)
{
    if (i < 0 || i >= m_paramout)
        return false;
    m_bindout[i].buffer_type = MYSQL_TYPE_DATETIME;

    m_bindout[i].buffer = (char*)&val;

    return true;
}

bool preparedstatement::execute(bool save)
{
    memset(&m_err, 0, sizeof(ERR_INFO));

    if (m_state == disconnected)
    {
        m_err.rc = 12051; strncpy(m_err.message, "cursor not open.\n", 128); return -1;
    }
    // 对于bindin数组,需要知道长度,否则char转类型时会错误
    for (int i = 0; i < m_paramin; i++)
    {
        if (m_bindin[i].buffer_type == MYSQL_TYPE_STRING)
        {
            m_stbinds[i].length = strlen((const char*)m_bindin[i].buffer);
        }
    }
    // 如果已经绑定过bindin,不需要再绑定
    if (m_is_bindin == false)
    {
        if (mysql_stmt_bind_param(m_stmt, m_bindin))
        {
            err_report();
            return false;
        }
        m_is_bindin = true;
    }
    // 只有select语句需要绑定bindout
    if (m_sqltype == false && m_is_bindout == false)
    {
        if (mysql_stmt_bind_result(m_stmt, m_bindout))
        {
            err_report();
            return false;
        }
        m_is_bindout = true;
    }

    // 执行
    if (mysql_stmt_execute(m_stmt))
    {
        err_report();
        return false;
    }

    // 是否需要保存结果集到内存
    m_stmt_need_rst = save;
    if (m_sqltype == false && m_stmt_need_rst == true)
    {
        if (mysql_stmt_store_result(m_stmt))
        {
            err_report();
            return false;
        }
    }

    // 统计操作行数
    if (m_sqltype == false)
        m_err.rpc = mysql_stmt_num_rows(m_stmt);
    else
        m_err.rpc = mysql_stmt_affected_rows(m_stmt);
    return true;
}

int preparedstatement::next()
{
    // 注意，在该函数中，不可用memset(&m_cda,0,sizeof(m_cda))，否则会清空m_cda.rpc的内容
    if (m_state == disconnected)
    {
        m_err.rc = 12051; strncpy(m_err.message, "cursor not open.\n", 128); return -1;
    }

    // 如果语句未执行成功，直接返回失败。
    if (m_err.rc != 0) return m_err.rc;

    // 判断是否是查询语句，如果不是，直接返回错误
    if (m_sqltype == true)
    {
        m_err.rc = 12052; strncpy(m_err.message, "no recordset found.\n", 128); return -1;
    }

    int ret = mysql_stmt_fetch(m_stmt);
    if (ret != 0)
    {
        err_report();
    }
    return ret;
}

MYSQL_TIME stringToMysqlTime(const string& datetimeStr)
{
    MYSQL_TIME t;
    memset(&t, 0, sizeof(t));

    // 解析字符串，支持格式：YYYY-MM-DD HH:MM:SS
    int year, month, day, hour, minute, second;
    if (sscanf(datetimeStr.c_str(), "%d-%d-%d %d:%d:%d",
        &year, &month, &day, &hour, &minute, &second) == 6)
    {
        t.year = year;
        t.month = month;
        t.day = day;
        t.hour = hour;
        t.minute = minute;
        t.second = second;
        t.second_part = 0;
        t.neg = false;
        t.time_type = MYSQL_TIMESTAMP_DATETIME;
    }
    else
    {
        // 格式错误时，全部清零
        memset(&t, 0, sizeof(t));
        t.time_type = MYSQL_TIMESTAMP_ERROR;
    }

    return t;
}

string mysqlTimeToString(const MYSQL_TIME& t)
{
    char buf[20];
    memset(buf, 0, sizeof(buf));
    snprintf(buf, sizeof(buf),
        "%04d-%02d-%02d %02d:%02d:%02d",
        t.year, t.month, t.day,
        t.hour, t.minute, t.second);
    return buf;
}