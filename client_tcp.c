#include <stdio.h>
#include <sys/socket.h>
#include <string.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/time.h>
#include <signal.h>
#include <errno.h>
#include <pthread.h>
#include <sched.h>

#define MAX_BYTE 	 1024*63
#define COUNT 1	

int sockfd = -1;

void* recv_from_server(void* arg)
{
	char *buf = malloc(sizeof(char)*MAX_BYTE);
	struct timeval currentTime,endTime;
	gettimeofday(&currentTime, NULL);
	unsigned long long currentMicros = currentTime.tv_sec * 1000000 + currentTime.tv_usec;
	unsigned long long endMicros;
	unsigned long long interval = 0;
	unsigned long long total_bytes = 0;
	double byteWidth = 0.0;
	int ret = 0;
	int count_time = 0;
	for(;;)
	{
		gettimeofday(&currentTime, NULL);
		ret = read(sockfd,buf,MAX_BYTE);
		gettimeofday(&endTime, NULL);
		currentMicros = currentTime.tv_sec * 1000000 + currentTime.tv_usec;
		endMicros = endTime.tv_sec * 1000000 + endTime.tv_usec;
		interval += endMicros - currentMicros;
		total_bytes += ret;
		if(interval >= 1000000)	
		{
			byteWidth = total_bytes/1024.0/128.0/(interval/1000000.0);
			printf("down_BW = %.3f Mbps\n",byteWidth);

			total_bytes = 0;
			interval = 0;
			
			currentMicros = endMicros;
		}
			

	}
}
void* send_to_server(void* arg)
{
	struct timeval start, end;
	char *buf = malloc(sizeof(char)*MAX_BYTE);
	memset(buf,'2',MAX_BYTE);
	
	struct timeval currentTime,endTime;
	gettimeofday(&currentTime, NULL);
	unsigned long long currentMicros = currentTime.tv_sec * 1000000 + currentTime.tv_usec;
	unsigned long long endMicros;
	unsigned long long interval;
	
	unsigned long long send_count = 0;
	float byteWidth = 0.0;
	int ret=  0;

	for(;;)
	{
		gettimeofday(&currentTime, NULL);
		ret = send(sockfd, buf, strlen(buf),MSG_NOSIGNAL);

		gettimeofday(&endTime, NULL);
		currentMicros = currentTime.tv_sec * 1000000 + currentTime.tv_usec;
		endMicros = endTime.tv_sec * 1000000 + endTime.tv_usec;
		interval += endMicros - currentMicros;
		send_count += ret;
		if(interval >= 1000000)	
		{
			byteWidth = send_count/1024.0/1024.0/(interval/1000000.0)*8;
			printf("up_BW = %.3f Mbps\n",byteWidth);

			send_count = 0;
			interval = 0;
			currentMicros = endMicros;
		}
		
	}

	free(buf);
	buf = NULL;
}

int main(int argc, char* argv[])
{
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if(-1 == sockfd)
	{
		perror("socket failed");
		return -1;
	}
	
	struct sockaddr_in ServerAddr;
	memset(&ServerAddr, 0, sizeof(ServerAddr));
	ServerAddr.sin_family = AF_INET;
	inet_pton(AF_INET, argv[1], &ServerAddr.sin_addr); 
	ServerAddr.sin_port = htons(atoi(argv[2]));
	
	int ret = connect(sockfd, (struct sockaddr*)&ServerAddr, sizeof(ServerAddr));
	if(-1 == ret)
	{
		perror("connet failed");
		return -1;
	}
	int core1 = 0;
    int core2 = 1;

	pthread_t recv_server;
	if(pthread_create(&recv_server,NULL,recv_from_server,&core1)!=0)
	{
		perror("pthread_create failed");
		return -1;
	}
	pthread_detach(recv_server);

	pthread_t send_server;
	if(pthread_create(&send_server,NULL,send_to_server,&core2)!=0)
	{
		perror("pthread_create failed");
		return -1;
	}
	pthread_detach(send_server);

	for(;;);
	close(sockfd);
	return 0;
}
