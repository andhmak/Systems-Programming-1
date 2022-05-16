sniffer: build/manager.o build/manager_funcs.o build/close_report.o
	@echo " Link sniffer ...";
	g++ ./build/manager.o build/manager_funcs.o build/close_report.o -o sniffer

build/manager.o: src/manager.cpp
	@echo " Compile manager ...";
	g++ -I ./include/ -c -o ./build/manager.o ./src/manager.cpp

build/manager_funcs.o: src/manager_funcs.cpp
	@echo " Compile manager_funcs ...";
	g++ -I ./include/ -c -o ./build/manager_funcs.o ./src/manager_funcs.cpp

bin/worker: build/worker.o build/worker_funcs.o build/close_report.o
	@echo " Link worker ...";
	g++ ./build/worker.o ./build/worker_funcs.o ./build/close_report.o -o ./bin/worker

build/worker.o: src/worker.cpp
	@echo " Compile worker ...";
	g++ -I ./include/ -c -o ./build/worker.o ./src/worker.cpp

build/worker_funcs.o: src/worker_funcs.cpp
	@echo " Compile worker_funcs ...";
	g++ -I ./include/ -c -o ./build/worker_funcs.o ./src/worker_funcs.cpp

build/close_report.o: src/close_report.cpp
	@echo " Compile close_report ...";
	g++ -I ./include/ -c -o ./build/close_report.o ./src/close_report.cpp

all: sniffer bin/worker

run: sniffer bin/worker
	@echo " Run sniffer with no arguments ...";
	./sniffer

clean:
	@echo " Delete binary and build ...";
	-rm -f ./bin/* ./build/* sniffer

cleanup:
	@echo " Delete output ...";
	-rm -f ./output/*.out
