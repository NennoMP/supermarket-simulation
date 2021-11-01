CC 			= gcc
DFLAG			= -g
CFLAGS 		= -std=c99
THREADS		= -pthread

INCDIR		= ./include
TESTDIR	= ./tests

CONFIG_FILE = config/config.txt
LOG_FILE = file_out.log

TARGET = $(BINDIR)/manager $(BINDIR)/supermarket

CONFIG_FILE = config/config.txt
LOG_FILE = file_out.log

.PHONY: all clean test1 test2

all: manager supermarket

manager: manager.c
	$(CC) $(CFLAGS) $(THREADS) -o manager manager.c

supermarket: supermarket.c
	$(CC) $(DFLAG) $(CFLAGS) $(THREADS) -o supermarket supermarket.c


clean:
	rm -f ./manager ./supermarket
	rm -f *.log
	
	
test1:
	echo [+] Starting Manager process...
	./manager 1 & echo $$! > m.PID
	echo [+] Manager is running...
	echo [+] Starting Supermarket process...
	valgrind --leak-check=full \
		--show-leak-kinds=all \
		./supermarket 2 20 5 500 80 30 & echo $$! > sm.PID
	echo [+] Supermarket is running...
	sleep 15
	
	@if [ -e m.PID ]; then \
		echo [+] sending SIGHUP to manager...; \
		kill -3 $$(cat m.PID) || true; \
		echo [+] Manager waiting Supermarket to quit...; \
		while sleep 1; do ps -p $$(cat m.PID) 1>/dev/null || break; done; \
	fi;

	@rm -f m.PID sm.PID
	@echo
	@echo
	@chmod +x ./analisi.sh 
	@./analisi.sh $(LOG_FILE)
	@rm -f file_out.log
	@echo
	@echo


test2:	
	echo [+] Starting Manager process...
	./manager 2 & echo $$! > m.PID
	echo [+] Manager is running...
	sleep 25
	
	@if [ -e m.PID ]; then \
		echo [+] sending SIGHUP to manager...; \
		kill -1 $$(cat m.PID) || true; \
		echo [+] Manager waiting Supermarket to close...; \
		while sleep 1; do ps -p $$(cat m.PID) 1>/dev/null || break; done; \
	fi;

	@rm -f m.PID sm.PID
	@echo
	@echo
	@chmod +x ./analisi.sh 
	@./analisi.sh $(LOG_FILE)
	@rm -f file_out.log
	@echo
	@echo

	
