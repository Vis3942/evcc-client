#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <syslog.h>
#include <stdint.h>
#include <math.h>
#include <inttypes.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <time.h>
#include <stdarg.h>
#include <errno.h>
#include <limits.h>
#include <unistd.h>
#include "utils.h"
#include "psal_types.h"

#define UNIX_SOCK_CLIENT2SERVER_UNCONNECTED 0x00
#define UNIX_SOCK_CLIENT2SERVER_CONNECTED 0x01
#define UNIX_SOCK_CLIENT2SERVER_ERROR     0x02
#define UNIX_SOCK_SERVER2CLIENT_UNCONNECTED 0x10
#define UNIX_SOCK_SERVER2CLIENT_CONNECTED 0x11
#define UNIX_SOCK_SERVER2CLIENT_ERROR     0x12

#define USOCK_READ_BUF_LEN  4096 // SAME SIZE AS RECV BUFFER

char *sep2_client2server_path = "/tmp/client2server";
char *sep2_server2client_path = "/tmp/server2client";


int cli2srv_usock_fd = -1;
int status_cli2srv_usock = UNIX_SOCK_CLIENT2SERVER_UNCONNECTED;
char rd_buf[USOCK_READ_BUF_LEN];

static u32_t (*process_messages)(char *buffer, int size) = NULL;

void register_process_messages_callback(u32_t (*ptr)( char *buffer, int size ))
{
  //debug
  printf("in register callback \n");
  process_messages = ptr;
}


void usock_client2server_close()
{
  if (cli2srv_usock_fd > 0)
  {
    close(cli2srv_usock_fd);
    cli2srv_usock_fd = -1;
    status_cli2srv_usock = UNIX_SOCK_CLIENT2SERVER_UNCONNECTED;
  }
}

int usock_client2server_open()
{
  struct sockaddr_un remote;
  size_t len;
  
  if (cli2srv_usock_fd > 0)
  {
      close(cli2srv_usock_fd);
      cli2srv_usock_fd = 0;
  }

  // return 0 if succ else return errno
  if ((cli2srv_usock_fd = socket(AF_UNIX, SOCK_SEQPACKET, 0)) == -1) {
      perror("socket");
      return errno;
  }

  // debug
  printf("Trying to connect...%s\n", sep2_client2server_path);
  printf("sock fd %d\n", cli2srv_usock_fd);
  //console_log("Trying to connect...%s\n", sep2_client2server_path);

  remote.sun_family = AF_UNIX;
  //strcpy(remote.sun_path, SOCK_PATH);
  strcpy(remote.sun_path, sep2_client2server_path);
  len = strlen(remote.sun_path) + sizeof(remote.sun_family);
  if (connect(cli2srv_usock_fd, (struct sockaddr *)&remote, len) == -1) {
      perror("client2server connect");
      status_cli2srv_usock = UNIX_SOCK_CLIENT2SERVER_ERROR;
      return errno;
  }
  //console_log("client2server usock Connected.\n");
  status_cli2srv_usock = UNIX_SOCK_CLIENT2SERVER_CONNECTED;
  printf("client2server usock Connected.\n");
  return 0;
}

int srv2cli_usock_fd = -1;
int status_srv2cli_usock = UNIX_SOCK_SERVER2CLIENT_UNCONNECTED;

void usock_server2client_close()
{
  if (srv2cli_usock_fd > 0)
  {
    close(srv2cli_usock_fd);
    srv2cli_usock_fd = -1;
    status_srv2cli_usock = UNIX_SOCK_SERVER2CLIENT_UNCONNECTED;
  }

}


int usock_server2client_open()
{
    struct sockaddr_un remote;
    size_t len;
    
    if (srv2cli_usock_fd > 0)
    {
        close(srv2cli_usock_fd);
        srv2cli_usock_fd = 0;
    }

    // return 0 if succ else return errno
    if ((srv2cli_usock_fd = socket(AF_UNIX, SOCK_SEQPACKET, 0)) == -1) {
        perror("socket");
        return errno;
    }

    printf("Trying to connect...%s\n", sep2_server2client_path);
    //console_log("Trying to connect...%s\n", sep2_server2client_path);

    remote.sun_family = AF_UNIX;
    //strcpy(remote.sun_path, SOCK_PATH);
    strcpy(remote.sun_path, sep2_server2client_path);
    len = strlen(remote.sun_path) + sizeof(remote.sun_family);
    if (connect(srv2cli_usock_fd, (struct sockaddr *)&remote, len) == -1) {
        perror("server2client connect");
        status_srv2cli_usock = UNIX_SOCK_SERVER2CLIENT_ERROR;
        return errno;
    }
    printf("server2client usock Connected.\n");
    status_srv2cli_usock = UNIX_SOCK_SERVER2CLIENT_CONNECTED;
    return 0;
}

int usock_client2server_write_message(char* send_buf, int send_buf_len) 
{
  int n;
  int sts;

  //debug
  printf("status cli2srv = %d, fd cli2srv = %d \n", status_cli2srv_usock, cli2srv_usock_fd);
  if (status_cli2srv_usock != UNIX_SOCK_CLIENT2SERVER_CONNECTED) {
    sts = usock_client2server_open();
    if (sts == 0) {
      status_cli2srv_usock = UNIX_SOCK_CLIENT2SERVER_CONNECTED;
    }
    else {
      cli2srv_usock_fd = -1;
      status_cli2srv_usock = UNIX_SOCK_CLIENT2SERVER_ERROR;
    }
  }

  n = -1;
  if ( ( status_cli2srv_usock == UNIX_SOCK_CLIENT2SERVER_CONNECTED ) && 
        (cli2srv_usock_fd > 0) 
  ) {
  	n = send(cli2srv_usock_fd, send_buf, send_buf_len, MSG_DONTWAIT);
    if (n < 0) {
      printf("error writing to server.");
      //console_log("error writing to server.");
      perror("write to server:");
      usock_client2server_close();
      usock_server2client_close();
      status_cli2srv_usock = UNIX_SOCK_CLIENT2SERVER_ERROR;
    }
    else {
      displayTime();
      printf(" bytes to client %d\n", n);
    }
  }
  return n;
}

int usock_write_message(char* send_buf, int send_buf_len) {
  // debug
  printf("usock write message client\n");
  return usock_client2server_write_message(send_buf, send_buf_len);
}

void usock_server2client_read_message() 
{
  int n;
  // read data that the client sent to the server
  memset(rd_buf, 0, USOCK_READ_BUF_LEN);
  n = recv(srv2cli_usock_fd, rd_buf, USOCK_READ_BUF_LEN, 0);
  if (n < 0) {
    perror("recv from server:");
    usock_server2client_close();
  } 
  else if (n == 0) {
    // connection closed
    printf("connection closed\n");
    usock_server2client_close();
    usock_client2server_close();
  }
  else {
    // process the message
    if (process_messages != NULL) {
      // debug
      printf("processing message at %0xld\n", process_messages);
      (*process_messages)(rd_buf, n);
    }
  }
}



void usock_message_handler () {
  struct timeval timeout;
  int sts;
  // debug
  // printf("in usock message handler \n");
  fd_set rfds, wfds;
  if ( status_srv2cli_usock != UNIX_SOCK_SERVER2CLIENT_CONNECTED ) {
    sts = usock_server2client_open();
    if (sts == 0) {
      status_srv2cli_usock = UNIX_SOCK_SERVER2CLIENT_CONNECTED;
    }
    else {
      status_srv2cli_usock = UNIX_SOCK_SERVER2CLIENT_ERROR;
      srv2cli_usock_fd = -1;
    }
  }

  if ( (status_srv2cli_usock == UNIX_SOCK_SERVER2CLIENT_CONNECTED) &&
    (srv2cli_usock_fd > 0) )
  {
    int nfds = srv2cli_usock_fd + 1;
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;
    FD_ZERO (&rfds); FD_ZERO (&wfds);
    FD_SET (srv2cli_usock_fd, &rfds);
    //printf("in message received \n");
    // no timeout
    //while (select (nfds, &rfds, NULL, NULL, NULL) > 0) {
    if (select (nfds, &rfds, NULL, NULL, &timeout) > 0) {
      //printf(" in select call with io recv \n");

      if (FD_ISSET (srv2cli_usock_fd, &rfds)) usock_server2client_read_message();
    }
  }
  else 
  {
    printf("in usock message handler => sts : %d \n", status_srv2cli_usock);
  }
}

void usock_init() {
  // debug
  printf(" in usock clien2server open\n");
  usock_client2server_open();
  printf(" in usock server2client open\n");
  usock_server2client_open();
}


void usock_task() {
  // printf("Usock client start task: init\n");
  printf("Usock client task \n");
  usock_init();
  //printf("start task: init completed\n");
  while (1) {
    usock_message_handler(); // does all read, for write use usock_write_message
    if ( (status_srv2cli_usock == UNIX_SOCK_SERVER2CLIENT_ERROR)  || 
         (status_srv2cli_usock == UNIX_SOCK_SERVER2CLIENT_UNCONNECTED) 
    ) {
      usock_init();
    }
    sleep(10);
  }
}
