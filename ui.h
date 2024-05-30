#ifndef _UI_H
#define _UI_H


#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <ncurses.h>
#include <pthread.h>

#define INPUT 0
#define EXIT 1
#define IPV4_SIZE 20
#define MAX_BW 1000
#define MAX_CLIENTS 	100

typedef struct Client_UiInfo
{
    int sockfd;
    int id;
    char ip[20];
    int port;
	double up_bw;
	double down_bw;

} Client_UiInfo;

extern Client_UiInfo clients_UiInfo[MAX_CLIENTS];

extern int client_number;
extern int rank;
extern char server_addr[IPV4_SIZE];
extern char broadcast_addr[IPV4_SIZE];
extern int server_port;
extern int broadcast_port ;

extern double up_max_bw ;
extern double up_min_bw ;
extern double up_total_bw ;
extern double up_average_bw ;

extern double down_max_bw ;
extern double down_min_bw;
extern double down_total_bw ;
extern double down_average_bw ;
extern int run_time ;

extern int time_test ;
extern int data_size ;
extern int limit_bandwidth ;
extern int dispatcher_mode ;

extern int ui_condition;
extern pthread_mutex_t ui_mutex;
extern pthread_cond_t ui_cond;

extern int show_menu();

static void update_bw(double up_bw, double down_bw);
static void print_client_list();
static void server_info(WINDOW *mainwin);


#endif
