#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

#define SERVER_STRING "Server: trierbo/1.0.0\r\n"

void error_die(const char*);
int startup(u_short*);
int get_line(int, char*, int);
void unimplemented(int);
void not_found(int);
void headers(int, const char*);
void cat(int, FILE*);
void serve_file(int, const char*);
void bad_request(int);
void cannot_execute(int);
void execute_cgi(int, const char*, const char*, const char*);
void accept_request(int);

/**
 * 异常退出函数
 */
void error_die(const char* msg) {
  perror(msg);
  exit(1);
}

/**
 * 开启web连接监听服务
 * 如果端口是0,则动态生成一个端口
 * 返回：socket描述符
 */
int startup(u_short* port) {
  int httpd = 0;
  // 监听socket地址
  struct sockaddr_in name;
  /**
   * int  socket(int protofamily, int type, int protocol);
   * protofamily：即协议域，又称为协议族（family）
   * type：指定socket类型
   * protocol：指定协议
   * 当protocol为0时，会自动选择type类型对应的默认协议
   */
  httpd = socket(AF_INET,SOCK_STREAM,0);
  if(httpd == -1)
    error_die("创建监听socket失败");
  memset(&name, 0, sizeof(name));
  /**
   * struct sockaddr_in {
   *   sa_family_t    sin_family; // address family: AF_INET
   *   in_port_t      sin_port;   // port in network byte order
   *   struct in_addr sin_addr;   // internet address
   * };
   * struct in_addr {
   *   uint32_t       s_addr;     // address in network byte order
   * };
   */
  name.sin_family = AF_INET;
  name.sin_port = htons(*port);
  // IP地址设置成INADDR_ANY,让系统自动获取本机的IP地址
  name.sin_addr.s_addr = htonl(INADDR_ANY);
  if(bind(httpd, (struct sockaddr*)&name, sizeof(name)) < 0)
    error_die("监听socket绑定地质失败");
  if(*port == 0) {
    socklen_t namelen = sizeof(name);
    // int getsockname(int sockfd, struct sockaddr *addr, socklen_t *addrlen)
    if(getsockname(httpd, (struct sockaddr *)&name, &namelen))
      error_die("获取监听socket端口失败");
    *port = ntohs(name.sin_port);
  }
  if(listen(httpd, 5) < 0)
    error_die("启动socket监听失败");
  return httpd;
}

/**
 * 使用连接socket接受数据
 * 每次读取一行数据
 * 每行数据以回车或者\r\n结束
 */
int get_line(int sock, char* buf, int size) {
  int i = 0;
  char c = '\0';
  int n;
  // 终止符统一设置为\n
  while((i < size - 1) && (c != '\n')) {
    n = recv(sock, &c, 1, 0);
    if(n > 0) {
      if(c == '\r') {
        // MSG_PEEK 查看下个字符，但是位置没变
        n = recv(sock, &c, 1, MSG_PEEK);
        if((n > 0) && (c == '\n'))
          // 获取之前查看的字符
          recv(sock, &c, 1, 0);
        else
          c = '\n';
      }
      buf[i] = c;
      i++;
    } else
      c = '\n';
  }
  buf[i] = '\0';
  return i;
}

/**
 * 通知客户端请求方法不支持时响应报文
 */
void unimplemented(int client) {
  char buf[1024];

  // 报文状态行
  sprintf(buf, "HTTP/1.1 501 Method Not Implement\r\n");
  send(client, buf, strlen(buf), 0);
  sprintf(buf, SERVER_STRING);
  send(client, buf, strlen(buf), 0);
  // 报文首部行
  sprintf(buf, "Content-Type: text/html\r\n");
  send(client, buf, strlen(buf), 0);
  // 报文空行
  sprintf(buf, "\r\n");
  send(client, buf, strlen(buf), 0);
  // 报文实体
  sprintf(buf, "<html><head><title>Method Not Implement\r\n");
  send(client, buf, strlen(buf), 0);
  sprintf(buf, "</title></head>\r\n");
  send(client, buf, strlen(buf), 0);
  sprintf(buf, "<body><p>HTTP Request Method Not Supported.\r\n");
  send(client, buf, strlen(buf), 0);
  sprintf(buf, "</p></body></html>/r/n");
  send(client, buf, strlen(buf), 0);
}

/**
 * 文件不存在
 */
void not_found(int client) {
  char buf[1024];

  sprintf(buf, "HTTP/1.1 404 NOT FOUND\r\n");
  send(client, buf, strlen(buf), 0);
  sprintf(buf, SERVER_STRING);
  send(client, buf, strlen(buf), 0);
  sprintf(buf, "Content-Type: text/html\r\n");
  send(client, buf, strlen(buf), 0);
  sprintf(buf, "\r\n");
  send(client, buf, strlen(buf), 0);
  sprintf(buf, "<html><head><title>NOT FOUND\r\n");
  send(client, buf, strlen(buf), 0);
  sprintf(buf, "</title></head>\r\n");
  send(client, buf, strlen(buf), 0);
  sprintf(buf, "<body><p>The server could not fulfill\r\n");
  send(client, buf, strlen(buf), 0);
  sprintf(buf, "your request because the resource specified\r\n");
  send(client, buf, strlen(buf), 0);
  sprintf(buf, "is unavailable or nonexisted.\r\n");
  send(client, buf, strlen(buf), 0);
  sprintf(buf, "</p></body></html>\r\n");
  send(client, buf, strlen(buf), 0);
}

/**
 * 返回读取的文件的头信息
 */
void headers(int client, const char* filename) {
  char buf[1024];
  (void)filename;

  sprintf(buf, "HTTP/1.1 200 OK\r\n");
  send(client, buf, strlen(buf), 0);
  sprintf(buf, SERVER_STRING);
  send(client, buf, strlen(buf), 0);
  // 可以根据filename返回类型
  sprintf(buf, "Content-Type: text/html\r\n");
  send(client, buf, strlen(buf), 0);
  sprintf(buf, "\r\n");
  send(client, buf, strlen(buf), 0);
}

/**
 * 读取文件内容
 */
void cat(int client, FILE* resource) {
  char buf[1024];

  fgets(buf, sizeof(buf), resource);
  while(!feof(resource)) {
    send(client, buf, strlen(buf), 0);
    fgets(buf, sizeof(buf), resource);
  }
}

/**
 * 发送普通文件至客户端
 * 并向客户端报告错误
 */
void serve_file(int client, const char* filename) {
  FILE* resource = NULL;
  int numchars = 1;
  char buf[1024];

  // 读取并丢弃请求报文头部数据
  buf[0] = 'A';
  buf[1] = '\0';
  while((numchars > 0) && (strcmp("\n",buf)))
    numchars = get_line(client, buf, sizeof(buf));

  resource = fopen(filename, "r");
  if(resource == NULL)
    not_found(client);
  else {
    headers(client, filename);
    cat(client, resource);
  }
  fclose(resource);
}

void bad_request(int client) {
  char buf[1024];

  sprintf(buf, "HTTP/1.1 400 BAD REQUEST\r\n");
  send(client, buf, sizeof(buf), 0);
  sprintf(buf, "Content-type: text/html\r\n");
  send(client, buf, sizeof(buf), 0);
  sprintf(buf, "\r\n");
  send(client, buf, sizeof(buf), 0);
  sprintf(buf, "<html><head><title>BAD REQUEST\r\n");
  send(client, buf, sizeof(buf), 0);
  sprintf(buf, "</title></head>");
  send(client, buf, sizeof(buf), 0);
  sprintf(buf, "<body><p>Your brower sent a bad request,\r\n");
  send(client, buf, sizeof(buf), 0);
  sprintf(buf, "such as a POST without a Content-Length.\r\n");
  send(client, buf, sizeof(buf), 0);
  sprintf(buf, "</p></body></html>\r\n");
  send(client, buf, sizeof(buf), 0);
}

void cannot_execute(int client) {
  char buf[1024];

  sprintf(buf, "HTTP/1.1 500 Internal Server Error\r\n");
  send(client, buf, strlen(buf), 0);
  sprintf(buf, "Content-type: text/html\r\n");
  send(client, buf, strlen(buf), 0);
  sprintf(buf, "\r\n");
  send(client, buf, strlen(buf), 0);
  sprintf(buf, "<html><head><title>Can't Execute</title></head>\r\n");
  send(client, buf, strlen(buf), 0);
  sprintf(buf, "<body><p>Error prohibited CGI execution.\r\n");
  send(client, buf, strlen(buf), 0);
  sprintf(buf, "</p></body></html>\r\n");
}

void execute_cgi(int client, const char* path, const char* method, const char* query_string) {
  char buf[1024];
  int numchars = 1;
  int content_length = -1;
  int cgi_output[2];
  int cgi_input[2];
  pid_t pid;
  char c;
  int status;

  buf[0] = 'A';
  buf[1] = '\0';
  if(strcasecmp(method, "GET") == 0)
    while((numchars > 0) && strcmp("\n", buf))
      numchars = get_line(client, buf, sizeof(buf));
  else { // POST
    numchars = get_line(client, buf, sizeof(buf));
    while ((numchars > 0) && strcmp("\n", buf)) {
      buf[15] = '\0';
      if(strcasecmp(buf, "Content-Length:") == 0)
        content_length = atoi(&buf[16]); // 字符串转整型
      numchars = get_line(client, buf, sizeof(buf));
    }
    if (content_length == -1) {
      bad_request(client);
      return;
    }
  }
  // 创建管道
  if(pipe(cgi_output) < 0) {
    cannot_execute(client);
    return;
  }
  if(pipe(cgi_input) < 0) {
    cannot_execute(client);
    return;
  }
  if((pid = fork()) < 0) {
    cannot_execute(client);
    return;
  }

  sprintf(buf, "HTTP/1.1 200 OK\r\n");
  send(client, buf, strlen(buf), 0);
 
  if(pid == 0) { // 子进程
    char meth_env[255];
    char query_env[255];
    char length_env[255];

    // 目标描述符（dup2函数的第二个参数）将变成源描述符（dup2函数的第一个参数）的复制品
    dup2(cgi_output[1], 1);
    dup2(cgi_input[0], 0);
    close(cgi_output[0]);
    close(cgi_input[1]);

    sprintf(meth_env, "REQUEST_METHOD=%s", method);
    putenv(meth_env);
    if(strcasecmp(method, "GET") == 0) {
      sprintf(query_env, "QUERY_STRING=%s", query_string);
      putenv(query_env);
    } else {
      sprintf(length_env, "CONTENT_LENGTH=%d", content_length);
      putenv(length_env);
    }
    execl(path, path, NULL);
  } else { // 父进程
    close(cgi_output[1]);
    close(cgi_input[0]);
    if(strcasecmp(method, "POST") == 0)
      for(int i = 0; i < content_length; i++) {
        recv(client, &c, 1 ,0);
        write(cgi_input[1], &c, 1);
      }
    
    while(read(cgi_output[0], &c, 1) > 0)
      send(client, &c, 1, 0);

    close(cgi_output[0]);
    close(cgi_input[1]);
    waitpid(pid, &status, 0);
  }
}

void accept_request(int client) {
  char buf[1024];
  int numchars;
  char method[255];
  char url[255];
  char path[512];
  size_t i, j;
  struct stat st;
  int cgi = 0;
  char *query_string;

  numchars = get_line(client, buf, sizeof(buf));

  // HTTP请求报文字段使用空格分割，第一行第一个字段表示请求方法
  i = 0;
  j = 0;
  while(!isspace(buf[j]) && (i < sizeof(method) - 1)) {
    method[i] = buf[j];
    i++;
    j++;
  }
  method[i] = '\0';

  if(strcasecmp(method, "GET") && strcasecmp(method, "POST")) {
    unimplemented(client);
    close(client);
    return;
  }

  if(strcasecmp(method, "POST") == 0)
    cgi = 1;

  // 读取url
  i = 0;
  while(isspace(buf[j]) && (j < sizeof(buf)))
    j++;
  while(!isspace(buf[j]) && (i < sizeof(url) - 1) && (j < sizeof(buf))) {
    url[i] = buf[j];
    i++;
    j++;
  }
  url[i] = '\0';

  if(strcasecmp(method, "GET") == 0) {
    query_string = url;
    while((*query_string != '?') && (*query_string != '\0'))
      query_string++;
    if(*query_string == '?') {
      cgi = 1;
      *query_string = '\0';
      query_string++;
    }
  }

  /**
   * struct stat {
   *   dev_t         st_dev;       //文件的设备编号
   *   ino_t         st_ino;       //节点
   *   mode_t        st_mode;      //文件的类型和存取的权限
   *   nlink_t       st_nlink;     //连到该文件的硬连接数目，刚建立的文件值为1
   *   uid_t         st_uid;       //用户ID
   *   gid_t         st_gid;       //组ID
   *   dev_t         st_rdev;      //(设备类型)若此文件为设备文件，则为其设备编号
   *   off_t         st_size;      //文件字节数(文件大小)
   *   unsigned long st_blksize;   //块大小(文件系统的I/O 缓冲区大小)
   *   unsigned long st_blocks;    //块数
   *   time_t        st_atime;     //最后一次访问时间
   *   time_t        st_mtime;     //最后一次修改时间
   *   time_t        st_ctime;     //最后一次改变时间(指属性)
   * };
   */
  sprintf(path, "htdocs%s", url);
  if(path[strlen(path) - 1] == '/')
    strcat(path, "index.html");
  if(stat(path, &st) == -1) {
    while ((numchars > 0) && strcmp("\n", buf))
      numchars = get_line(client, buf, sizeof(buf));
    not_found(client);
  } else {
    // 判断是不是文件夹
    if((st.st_mode & S_IFMT) == S_IFDIR)
      strcat(path, "/index.html");
    // 判断是否有执行权限
    if((st.st_mode & S_IXUSR) ||
       (st.st_mode & S_IXGRP) ||
       (st.st_mode & S_IXOTH))
      cgi = 1;
    if (!cgi)
      serve_file(client, path);
    else
      execute_cgi(client, path, method, query_string);
  }
  
  close(client);
}

int main() {
  // 监听socket
  int server_sock = -1;
  u_short port = 0;
  // 连接socket
  int client_sock = -1;
  struct sockaddr_in client_name;
  socklen_t client_name_len = sizeof(client_name);
  server_sock = startup(&port);
  printf("httpd running on port %d\n", port);
  while(1) {
    client_sock = accept(server_sock, (struct sockaddr*)&client_name, &client_name_len);
    if(client_sock == -1)
      error_die("创建连接socket失败");
    accept_request(client_sock);
  }
  close(server_sock);
  return 0;
}
