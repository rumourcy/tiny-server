#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>

void error_die(const char*);
int startup(u_short*);
int get_line(int sock, char* buf, int size);

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
    printf("%s\n", "success");
    char buf[1024];
    get_line(client_sock, buf, sizeof(buf));
    printf("%s", buf);
    close(client_sock);
  }
  close(server_sock);
  return 0;
}
