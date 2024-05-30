#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>


#define PORT 9999
#define BUFFER_SIZE 20

pid_t start_process(const char *program, char *const argv[]) {
    pid_t pid = fork();

    if (pid < 0) {
        perror("fork failed");
        return -1;
    } else if (pid == 0) {
        // 子进程
        execvp(program, argv);
        // 如果 execvp 返回，则表示执行失败
        perror("execvp failed");
        exit(EXIT_FAILURE);
    }
    
    // 父进程返回子进程的 PID
    return pid;
}

int main() {
    int sockfd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len;
    char buffer[BUFFER_SIZE];

    // 创建套接字
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("socket");
        exit(1);
    }
    
	// 设置广播选项
	int broadcastPermission = 1;
	if (setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, (void *) &broadcastPermission, sizeof(broadcastPermission)) < 0) {
		perror("setsockopt() failed");
		close(sockfd);
		return -1;
	}
	
    // 设置服务器地址
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(PORT);
    //inet_aton("255.255.255.255", &server_addr.sin_addr);

    // 绑定套接字到服务器地址
    if (bind(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind failed");
        exit(1);
    }

    printf("Listening for broadcast messages...\n");
	
	char cmd[128];
	memset(cmd,0,sizeof(cmd));
	pid_t pid;
	int index = 0;//代表程序未启动
    while (1) {
        memset(buffer, 0, sizeof(buffer));
        client_len = sizeof(client_addr);
        if (recvfrom(sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr*)&client_addr, &client_len) == -1) {
            perror("recvfrom");
            exit(1);
        }

        printf("Received message: %s\n", buffer);
	
		if (index == 0 && strcmp(buffer, "192.168.1.25") == 0) 
		{
			//启动测速程序
			index = 1;
			char *argv[] = {"./client_tcp_arm", buffer, "8080", NULL};
			pid = start_process("./client_tcp_arm", argv);
		}
		else if(index == 1 && strcmp(buffer, "end") == 0)
		{
			//关闭测试程序
			index = 0;
			kill(pid, SIGTERM);
		}
    }

    close(sockfd);

    return 0;
}

