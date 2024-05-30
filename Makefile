.PHONY: bin all clean

ARMCC = arm-openwrt-linux-gcc

ARM_TARGETS = listen_bdcst_ip_arm client_tcp_arm
LOCAL_TARGETS = server_tcp

REMOTE_PATH = /test

REMOTE_HOST1 = root@192.168.1.4
REMOTE_HOST2 = root@192.168.1.5
REMOTE_HOST3 = root@192.168.1.1

CFLAGS = -pthread
NCURSES = -lncurses

bin:
	./server_tcp 8080
	
all: $(LOCAL_TARGETS) $(ARM_TARGETS)

server_tcp: server_tcp.c 
	$(CC) $(CFLAGS) $< ui.c -o $@ $(NCURSES)
	
listen_bdcst_ip_arm: listen_bdcst_ip.c
	$(ARMCC) $(CFLAGS) $< -o $@

client_tcp_arm: client_tcp.c
	$(ARMCC) $(CFLAGS) $< -o $@	

transfer: $(ARM_TARGETS)
	scp $(ARM_TARGETS) $(REMOTE_HOST1):$(REMOTE_PATH)
	scp $(ARM_TARGETS) $(REMOTE_HOST2):$(REMOTE_PATH)
	scp $(ARM_TARGETS) $(REMOTE_HOST3):$(REMOTE_PATH)

clean:
	rm -f $(LOCAL_TARGETS)
	rm -f $(ARM_TARGETS)
	
remote_exec:
	ssh $(REMOTE_HOST1) "cd $(REMOTE_PATH); ./listen_bdcst_ip_arm"

	
.DEFAULT_GOAL := all

