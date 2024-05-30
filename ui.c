#include "ui.h"
#define CLIENT_LINE 20
Client_UiInfo clients_UiInfo[MAX_CLIENTS];

char server_addr[IPV4_SIZE] = {};
char broadcast_addr[IPV4_SIZE] ={};
int server_port = 0;
int broadcast_port = 0;

double up_max_bw = 0;
double up_min_bw = 0;
double up_total_bw = 0;
double up_average_bw = 0;

double down_max_bw = 0;
double down_min_bw = 0;
double down_total_bw = 0;
double down_average_bw = 0;
int client_number = 0;
int run_time = 0;

int time_test = 0;
int data_size = 0;
int limit_bandwidth = 1000;
int dispatcher_mode = 1;

int ui_condition = 0;
pthread_mutex_t ui_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t ui_cond = PTHREAD_COND_INITIALIZER;


void update_bw(double up_bw, double down_bw)
{
    if (up_bw > up_max_bw) {
        up_max_bw = up_bw;
    }
    if (up_bw < up_min_bw && up_bw != 0) {
        up_min_bw = up_bw;
    }
    
    if (down_bw > down_max_bw) {
        down_max_bw = down_bw;
    }
    if (down_bw < down_min_bw && down_bw != 0) {
        down_min_bw = down_bw;
    }

    run_time++;

    up_total_bw += up_bw;
    up_average_bw = up_total_bw / run_time;

    down_total_bw += down_bw;
    down_average_bw = down_total_bw / run_time;
}

void print_client_list()
{
    mvprintw(CLIENT_LINE-1, 0, "| ID | IP\t\t|PORT   |  UP\t\t|  DOWN\t\t|");
    mvprintw(CLIENT_LINE, 0, "=================================================================");
    
    int rank = CLIENT_LINE+1;
    for(int i = 0;i < MAX_CLIENTS;++i)
    {
        if(clients_UiInfo[i].sockfd != -1)
        {
            mvprintw(rank , 0, "|[%2d ] | %s\t| %d\t| %8.2f Mbps\t| %8.2f Mbps\t|",
                    clients_UiInfo[i].id, clients_UiInfo[i].ip, clients_UiInfo[i].port,
                    clients_UiInfo[i].up_bw, clients_UiInfo[i].down_bw);
            mvprintw(rank + 1, 0, "=================================================================");
            rank += 2;
        }
        
    }
    
}

void server_info(WINDOW *mainwin)
{
    mvwprintw(mainwin, 1, 1,  "Server    addr: %s",server_addr);
    mvwprintw(mainwin, 1, 40, "port: %d",server_port);
    mvwprintw(mainwin, 2, 1,  "broadcast addr: %s",broadcast_addr);
    mvwprintw(mainwin, 2, 40, "port: %d",broadcast_port);
    mvwprintw(mainwin, 3, 1, "test time:%8d sec", time_test);
    mvwprintw(mainwin, 3, 40, "pakg size:%8d bit", data_size);
    
    if (dispatcher_mode == 2) {
        mvwprintw(mainwin, 4, 1, "mode:     double");
    }
    else if (dispatcher_mode == 0) {
        mvwprintw(mainwin, 4, 1, "mode:     up");
    }
    else if (dispatcher_mode == 1) {
        mvwprintw(mainwin, 4, 1, "mode:     down");
    }
    
    if (limit_bandwidth >= MAX_BW) {
        mvwprintw(mainwin, 4, 40, "limit: no limit");
    }
    else {
        mvwprintw(mainwin, 4, 40, "limit:%8d Mbps", limit_bandwidth);
    }

    mvwprintw(mainwin, 10, 1,  "Connected device: %d",client_number);
    mvwprintw(mainwin, 10, 30, "Run time: %d",run_time);

    mvwprintw(mainwin, 14, 1,  "Max bw:     %8.2f",up_max_bw);
    mvwprintw(mainwin, 15, 1,  "Min bw:     %8.2f",up_min_bw);
    mvwprintw(mainwin, 16, 1,  "Average bw: %8.2f",up_average_bw);

    mvwprintw(mainwin, 14, 30, "Max bw    : %8.2f",down_max_bw);
    mvwprintw(mainwin, 15, 30, "Min bw    : %8.2f",down_min_bw);
    mvwprintw(mainwin, 16, 30, "Average bw: %8.2f",down_average_bw);

    mvwprintw(mainwin, 11, 1, "---------------------------------------------------------------");
    mvwprintw(mainwin, 12, 1, "UP                           DOWN");
    mvwprintw(mainwin, 13, 25, "|");
    mvwprintw(mainwin, 14, 25, "|");
    mvwprintw(mainwin, 15, 25, "|");
    mvwprintw(mainwin, 16, 25, "|");

    
}

int show_menu()
{
    initscr();      
    noecho();      
    keypad(stdscr, TRUE);
    curs_set(0);

    char *choices[] = {"BEGIN","STOP","LIMIT", "MODE","EXIT"};
    int n_choices = sizeof(choices) / sizeof(char *);
    int choice = INPUT;

    char *modes[] = {"UP","DOWN","DOUBLE"};
    int n_modes = sizeof(modes)/sizeof(char *);
    int index_mode = 0;
    int input_mode;
    
    char limit[21];
    int input;

    WINDOW *mainwin = newwin(18, 65, 0, 0);
    refresh();
    
    box(mainwin, 0, 0);
    wrefresh(mainwin);
    
    WINDOW *input_win = newwin(3, 63, 7, 1);
    box(input_win, 0, 0);

    int number = 0 ;
    int test = 0;

    int index_menu = 1;
    while (1) {
        werase(mainwin);
        mvwprintw(mainwin, 9, 1, "test:%d ",++test);
        wrefresh(mainwin);
        server_info(mainwin);
        wrefresh(mainwin);
        
        box(mainwin, 0, 0);
        wrefresh(mainwin);
        
        timeout(950);

        print_client_list();
        refresh();

        if(index_menu == 1){
            for (int i = 0; i < n_choices; i++)
            {
                if (i == choice)
                    wattron(mainwin, A_REVERSE);
                mvwprintw(mainwin, 6, i * 10 + 1, "%s", choices[i]);
                wattroff(mainwin, A_REVERSE);
            }
        }
        
        wrefresh(mainwin);

        input = getch();
        switch (input)
        {
        case KEY_LEFT: 
            choice--;
            if (choice < INPUT)
                choice = n_choices-1;
            break;
        case KEY_RIGHT: 
            choice++;
            if (choice > n_choices-1)
                choice = INPUT;
            break;
        case '\n': 
            if(choice == 0)
            {
                clear();
                refresh();
                ui_condition = 1;
                pthread_cond_signal(&ui_cond);

            }
            else if (choice == 1) 
            {
                werase(mainwin);
                mvwprintw(mainwin, 8, 1, "stop");
                wrefresh(mainwin);

                ui_condition = 3;
                pthread_cond_signal(&ui_cond);

            }
            else if(choice == 2)
            {
                werase(input_win);
                mvwprintw(input_win, 1, 1, "input");
                wrefresh(input_win);
                echo();            
                curs_set(1);           
                mvwgetnstr(input_win, 1, 10, limit, 20); 
                noecho();          
                curs_set(0); 
                werase(input_win); 
                wrefresh(input_win); 

                limit_bandwidth = atoi(limit);
                if(limit_bandwidth > MAX_BW)
                    limit_bandwidth = MAX_BW;
                else if(limit_bandwidth <= 0)
                    limit_bandwidth = MAX_BW;

                ui_condition = 2;
                pthread_cond_signal(&ui_cond);

            }
            else if(choice == 3)
            {
                werase(mainwin);
                mvwprintw(mainwin, 8, 1, "mode");
                wrefresh(mainwin);

                index_menu = 2;
                for(;;){
                    timeout(950);
                    werase(mainwin);
                    mvwprintw(mainwin, 9, 1, "test:%d ",++test);
                    server_info(mainwin);
                    box(mainwin, 0, 0);
                    for (int i = 0; i < n_modes; i++)
                    {
                        if (i == index_mode)
                            wattron(mainwin, A_REVERSE);
                        mvwprintw(mainwin, 6, i * 10 + 1, "%s", modes[i]);
                        wattroff(mainwin, A_REVERSE);
                    }
                    wrefresh(mainwin);
                
                    input_mode = getch();
                    switch(input_mode)
                    {
                        case KEY_LEFT:
                            index_mode--;
                            if (index_mode < 0)
                                index_mode = n_modes-1;
                            break;
                        case KEY_RIGHT: 
                            index_mode++;
                            if (index_mode > n_modes-1)
                                index_mode = 0;
                            break;
                        case '\n': 
                            dispatcher_mode = index_mode;
                            index_menu = 1;
                            break;
                    }
                    if(index_menu == 1)
                        break;
                }
            }
            else if (choice == 4) 
            { 
                endwin(); 
                exit(0);
            } 
        break; 
        
        default:
            break; 
        } 

        if(run_time == time_test)
        {
            ui_condition = 3;
            pthread_cond_signal(&ui_cond);
            pthread_cond_signal(&ui_cond);
        }

        refresh();  
        wrefresh(mainwin);
    }

    endwin();  
    return 0;
}
