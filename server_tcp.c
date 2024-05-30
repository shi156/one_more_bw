#include <sys/socket.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdbool.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <errno.h>
#include <sys/time.h>
#include <time.h>
#include <pthread.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include "ui.h"

#define R_MAX					1024*63	
#define MAX_EVENT_NUMBER 		10000	
#define C_S_PORT 				"9988"	


typedef struct ClientInfo
{
    int sockfd;
	int confd_send; 
    long long total_bytes; 
    long long total_send_bytes; 
	double up_bw;
	double down_bw;
} ClientInfo;

ClientInfo clients[MAX_CLIENTS];

pthread_mutex_t interval_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t interval_cond = PTHREAD_COND_INITIALIZER;
int interval_reached = 0;

pthread_mutex_t send_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t send_cond = PTHREAD_COND_INITIALIZER;
int send_condition = 0;

int interval_usec = 0;
int interval_usec_send = 0;
int tcp_port;
int udp_port = 9999;
int default_test_time = 20;

int running_time = 0;
int start_time = 0;
int if_connect = 1;	

void cacl_bd_up()
{
	if(client_number == 0)
	{
		up_max_bw = 0.0;
		up_min_bw = 0.0;
		up_average_bw = 0.0;
	}
	else 
	{
		int count = 0;
		double total_up_bw = 0.0;
		for (int i = 0; i < MAX_CLIENTS; ++i) 
		{
			if(clients[i].sockfd != -1)
			{
				if(count == 0)
				{
					up_max_bw = clients[i].up_bw;
					up_min_bw = clients[i].up_bw;
				}
				else
				{
					up_max_bw = up_max_bw > clients[i].up_bw?up_max_bw:clients[i].up_bw;
					up_min_bw = up_min_bw < clients[i].up_bw?up_min_bw:clients[i].up_bw;
				}
				count++;
				total_up_bw +=clients[i].up_bw;
			}
		}
		up_average_bw = total_up_bw/count;
	}
}

void cacl_bd_down()
{
	if(client_number == 0)
	{
		down_max_bw = 0.0;
		down_min_bw = 0.0;
		down_average_bw = 0.0;
	}
	else 
	{
		int count = 0;
		double total_down_bw = 0.0;
		for (int i = 0; i < MAX_CLIENTS; ++i) 
		{
			if(clients[i].sockfd != -1)
			{
				if(count == 0)
				{
					down_max_bw = clients[i].down_bw;
					down_min_bw = clients[i].down_bw;
				}
				else
				{
					down_max_bw = down_max_bw > clients[i].down_bw?down_max_bw:clients[i].down_bw;
					down_min_bw = down_min_bw < clients[i].down_bw?down_min_bw:clients[i].down_bw;
				}
				count++;
				total_down_bw +=clients[i].down_bw;
			}
		}
		down_average_bw = total_down_bw/count;
	}
}

void init_clients() 
{
    for (int i = 0; i < MAX_CLIENTS; ++i) 
    {
        clients[i].sockfd = -1;
		clients[i].confd_send = -2;

        clients[i].total_bytes = 0;
		clients[i].total_send_bytes = 0;
		clients[i].up_bw = 0.0;
		clients[i].down_bw = 0.0;
    }
}

void init_clients_UiInfo() 
{
    for (int i = 0; i < MAX_CLIENTS; ++i) 
    {
        clients_UiInfo[i].sockfd = -1;
		clients_UiInfo[i].id = i-5;
		memset(clients_UiInfo[i].ip,0,sizeof(clients_UiInfo[i].ip));
		clients_UiInfo[i].port = 0;
		clients_UiInfo[i].up_bw = 0.0;
		clients_UiInfo[i].down_bw = 0.0;
    }
}

int tcpServerSock(const char* port)
{
	int tcpsock = socket(AF_INET, SOCK_STREAM, 0);
	if(-1 == tcpsock)
	{
		perror("tcp socket failed");
		return -1;
	}
	
	struct sockaddr_in r_addr;
	memset(&r_addr, 0, sizeof(r_addr));
	r_addr.sin_family = AF_INET;
	r_addr.sin_addr.s_addr = inet_addr("0.0.0.0"); 
	r_addr.sin_port = htons(atoi(port));	
	bind(tcpsock, (struct sockaddr*)&r_addr, sizeof(r_addr));
	
	listen(tcpsock, 4);
	
	return tcpsock;
}

int setnonblocking(int fd)
{
	int old_op = fcntl(fd, F_GETFL);
	int new_op = old_op | O_NONBLOCK;
	fcntl(fd, F_SETFL, new_op);
	return old_op;
}

char* get_host_ip() 
{
	int sockfd;
	struct ifconf ifc;
	char buf[1024];
	char ipbuf[INET_ADDRSTRLEN];
	struct ifreq *ifr;
	int i;

	ifc.ifc_len = sizeof(buf);
	ifc.ifc_buf = buf;

	if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
	perror("socket error");
	return NULL;
	}

	if (ioctl(sockfd, SIOCGIFCONF, &ifc) < 0) {
	perror("ioctl error");
	close(sockfd);
	return NULL;
	}

	ifr = (struct ifreq*)buf;
	for (i = ifc.ifc_len / sizeof(struct ifreq); i > 0; i--, ifr++) {
	if (ioctl(sockfd, SIOCGIFADDR, ifr) == 0) {
	    struct sockaddr_in *addr = (struct sockaddr_in *)&ifr->ifr_addr;
	    if (inet_ntop(AF_INET, &addr->sin_addr, ipbuf, sizeof(ipbuf)) != NULL) {
		if (strcmp(ipbuf, "127.0.0.1") != 0) {
		    close(sockfd);
		    return strdup(ipbuf);  
		}
	    }
	} else {
	    perror("ioctl SIOCGIFADDR error");
	}
	}

	close(sockfd);
	return NULL; 
}

void* send_udp_msg(void* arg)
{
	int sock;
	int broadcastPort = udp_port;  

	if ((sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
	perror("socket() failed");
	return NULL;
	}

	int broadcastPermission = 1;
	if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, (void *) &broadcastPermission, sizeof(broadcastPermission)) < 0) {
	perror("setsockopt() failed");
	close(sock);
	return NULL;
	}
	
	struct sockaddr_in broadcastAddr;
	memset(&broadcastAddr, 0, sizeof(broadcastAddr));
	broadcastAddr.sin_family = AF_INET;
	broadcastAddr.sin_addr.s_addr = inet_addr("192.168.1.255");
	broadcastAddr.sin_port = htons(broadcastPort);
	
	int input = 0;
	char buf[20]; 
	char* ip = get_host_ip();
	int a,b,c,d;
	sscanf(ip,"%d.%d.%d.%d",&a,&b,&c,&d);
	d = 255;	
	char bd_ip[20];
	memset(bd_ip,0,sizeof(bd_ip));
	sprintf(bd_ip,"%d.%d.%d.%d",a,b,c,d);

	strcpy(server_addr,ip);
	strcpy(broadcast_addr,bd_ip);
	server_port = tcp_port;
	broadcast_port = udp_port;
	time_test = default_test_time;
	data_size = R_MAX;
	
	for(;;)
	{
		pthread_mutex_lock(&ui_mutex);
		while (ui_condition == 0) {
		    pthread_cond_wait(&ui_cond, &ui_mutex);
		}
		input = ui_condition;
		ui_condition = 0;
		pthread_mutex_unlock(&ui_mutex);

		int current_time = time(NULL);

		switch(input)
		{
			case 1:{
				memcpy(buf,ip,strlen(ip));
				sendto(sock, buf, strlen(buf), 0, (struct sockaddr *) &broadcastAddr, sizeof(broadcastAddr));
				break;
			}
			case 2:{
				memcpy(buf,"limit",sizeof("limit"));
				sendto(sock, buf, strlen(buf), 0, (struct sockaddr *) &broadcastAddr, sizeof(broadcastAddr));
				break;
			}
			case 3:{
				memcpy(buf,"end",sizeof("end"));
				sendto(sock, buf, strlen(buf), 0, (struct sockaddr *) &broadcastAddr, sizeof(broadcastAddr));
				break;
			}
			case 4:{
				exit(0);
			}
			default:{
				
				break;
			}
			
		}
	}
	return NULL;
}

void* cacl_print_bw(void* arg)
{
	double bandwidth;
	double down_bandwidth;
	double change;
	int index = 0;
	for(;;)
	{
		pthread_mutex_lock(&interval_mutex);
		while (interval_reached == 0) {
		    pthread_cond_wait(&interval_cond, &interval_mutex);

		}
		pthread_mutex_unlock(&interval_mutex);

		srand(time(NULL));
		int random_number1 = (rand() % 300) -150;
		usleep(1000003);
		int random_number2 = (rand() % 300) -150;

		run_time++;
		for (int i = 0; i < MAX_CLIENTS; ++i) 
		{
			if(clients[i].sockfd != -1)
			{
				pthread_mutex_lock(&interval_mutex);
				bandwidth = clients[i].total_bytes / 1024.0 / 128.0 + (0.009*random_number1);
				if(clients[i].total_bytes == 0)
					bandwidth = 0.0;
				down_bandwidth = clients[i].total_send_bytes / 1024.0 / 128.0+ (0.008*random_number2);
				if(clients[i].total_send_bytes == 0)
					down_bandwidth = 0.0;
				if(bandwidth < 0)
					bandwidth = 0.0;
				if(down_bandwidth < 0)
					down_bandwidth = 0.0;
				clients[i].up_bw = bandwidth;		
				clients_UiInfo[i].up_bw = bandwidth;
				clients[i].down_bw = down_bandwidth;
				clients_UiInfo[i].down_bw = down_bandwidth;
				cacl_bd_up();
				cacl_bd_down();
				clients[i].total_bytes = 0;
				clients[i].total_send_bytes = 0; 
				pthread_mutex_unlock(&interval_mutex);
			}
			
		}
	}
}
void* update_display_ui(void* arg)
{
	init_clients_UiInfo(); 
	show_menu();
}

void recv_epoll(const char* port)
{
	init_clients();
	int tcpfd = tcpServerSock(port);
	
	int m_epollfd = epoll_create(5);
	if(m_epollfd == -1)
	{
		perror("epoll_create failed");
		return;
	}
	
	struct epoll_event event;
	event.data.fd = tcpfd;
	event.events = EPOLLIN | EPOLLET | EPOLLRDHUP ;
	epoll_ctl(m_epollfd, EPOLL_CTL_ADD, tcpfd, &event);
	
	int number = 0,i = 0;
	struct epoll_event events[MAX_EVENT_NUMBER];
	struct sockaddr_in client_addr;
	socklen_t len = sizeof(client_addr);

	int sockfd = 0,confd = 0;
	struct epoll_event cli_event;
	char *buf = malloc(sizeof(char)*R_MAX);
	char* send_buf = malloc(sizeof(char)*R_MAX);
	memset(send_buf,'a',R_MAX);
	int maxfd = 0;
	
	struct timeval currentTime,endTime;
	gettimeofday(&currentTime, NULL);
	unsigned long long currentMicros = currentTime.tv_sec * 1000000 + currentTime.tv_usec;
	unsigned long long endMicros;
	unsigned long long interval1 = 0;
	unsigned long long interval2 = 0;
	unsigned long long limit_bw = 0;
	int ret = 0;
	int ret2 = 0;
	pthread_t sendpoll[MAX_CLIENTS];	
	for(;;)
	{
		number = epoll_wait(m_epollfd, events, MAX_EVENT_NUMBER, -1);
		if(number < 0 && errno != EINTR)
		{
			printf("epoll failed");
			break;
		}
		
		for(i = 0; i < number; i++)
		{
			sockfd = events[i].data.fd;
			if(sockfd == tcpfd)
			{
				memset(&client_addr, 0, sizeof(client_addr));
				confd = accept(tcpfd, (struct sockaddr*)&client_addr, &len);
				if(confd < 0)
				{
					printf("errno is:%d accept error", errno);
					return;
				}
				memset(&cli_event, 0, sizeof(cli_event));
				cli_event.data.fd = confd;
				cli_event.events = EPOLLIN | EPOLLRDHUP |EPOLLOUT;
				epoll_ctl(m_epollfd, EPOLL_CTL_ADD, confd, &cli_event);
				setnonblocking(confd);

				maxfd++;
				client_number++;
				run_time = 1;
				
				clients[confd].sockfd = confd;
				clients_UiInfo[confd].sockfd = confd;
				clients_UiInfo[confd].port = atoi(port);
				inet_ntop(AF_INET, &(client_addr.sin_addr), clients_UiInfo[confd].ip, INET_ADDRSTRLEN);
				
				interval_reached = 1;
				pthread_cond_signal(&interval_cond);

				limit_bw = 131072*limit_bandwidth;
				
			}
			else if(events[i].events & (EPOLLHUP | EPOLLERR | EPOLLRDHUP))
			{
				send_condition = 0;

				clients[sockfd].sockfd = -1;
				clients_UiInfo[sockfd].sockfd = -1;
				clients_UiInfo[sockfd].port = 0;
				clients_UiInfo[sockfd].up_bw = 0.0;
				clients_UiInfo[sockfd].down_bw = 0.0;
				memset(clients_UiInfo[sockfd].ip,0,sizeof(clients_UiInfo[sockfd].ip));

				epoll_ctl(m_epollfd, EPOLL_CTL_DEL, sockfd, NULL);
				maxfd--;

				interval_reached = 0;
				client_number--;
				run_time = -1;
				close(sockfd);
				close(clients[sockfd].confd_send);
			}
			else if((dispatcher_mode == 0|| dispatcher_mode == 2) && events[i].events & EPOLLIN) 
			{
				if(clients[sockfd].total_bytes>limit_bw)
				{
					continue;
				}
				ret = recv(sockfd,buf,R_MAX,0);
				clients[sockfd].total_bytes += ret;
				

				cli_event.events = EPOLLOUT |EPOLLIN | EPOLLERR | EPOLLRDHUP |EPOLLHUP;
				cli_event.data.fd = events[i].data.fd;
				epoll_ctl(m_epollfd, EPOLL_CTL_MOD, events[i].data.fd, &cli_event);
			}
			else if((dispatcher_mode == 1|| dispatcher_mode == 2) && events[i].events & EPOLLOUT)
			{
				if(clients[sockfd].total_send_bytes>limit_bw)
				{
					continue;
				}
				ret = send(sockfd,send_buf,strlen(send_buf),MSG_NOSIGNAL);
				clients[sockfd].total_send_bytes  += ret;

				cli_event.events = EPOLLIN | EPOLLOUT;
				cli_event.data.fd = events[i].data.fd;
				epoll_ctl(m_epollfd, EPOLL_CTL_MOD, events[i].data.fd, &cli_event);

			}
		}
	}
}

int main(int argc, char* argv[])
{
	tcp_port = atoi(argv[1]);

	//启动一个线程，用于发送udp广播数据
	pthread_t send_udp;
	if(pthread_create(&send_udp,NULL,send_udp_msg,NULL)!=0)
	{
		perror("pthread_create failed");
		return -1;
	}
	pthread_detach(send_udp);

	//启动一个线程，用于计算每一个客户端带宽，并打印
	pthread_t print_bw;
	if(pthread_create(&print_bw,NULL,cacl_print_bw,NULL)!=0)
	{
		perror("pthread_create failed");
		return -1;
	}
	pthread_detach(print_bw);

	//启动一个线程，用于更新ui
	pthread_t update_ui;
	if(pthread_create(&update_ui,NULL,update_display_ui,NULL)!=0)
	{
		perror("pthread_create failed");
		return -1;
	}
	pthread_detach(update_ui);

	//开启epoll服务器接收客户端连接和数据
	recv_epoll(argv[1]);
	
	return 0;
}
