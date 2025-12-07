# MYSQL-C-API-CPP-Encapsulation
这是MYSQL C API的C++封装代码，目的是简化MYSQL C++开发流程，包含了connection和preparedstatement两个类来支持更简洁预处理语句。
支持用string、字符数据来接收、写入任何类型的字段(除了blob等大类型)，需要注意控制字符数组和string的大小。
This project is a C++ wrapper for the MySQL C API, designed to simplify MySQL development in C++.It provides two core classes — Connection and PreparedStatement — enabling a cleaner and more intuitive workflow when working with MySQL databases. 
Supports using std::string and character arrays to read from and write to any field type (except large types such as BLOB).Note that you should properly manage the size of both character buffers and strings.

