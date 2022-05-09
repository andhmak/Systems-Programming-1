bin/sniffer: build/manager.o
	@echo " Link sniffer ...";
	g++ -g -L ./lib/ -Wl,-rpath,./lib/ ./build/manager.o -o ./bin/sniffer

build/manager.o: src/manager.cpp
	@echo " Compile manager ...";
	g++ -I ./include/ -g -c -o ./build/manager.o ./src/manager.cpp

bin/worker: build/worker.o
	@echo " Link worker ...";
	g++ -g -L ./lib/ -Wl,-rpath,./lib/ ./build/worker.o -o ./bin/worker

build/worker.o: src/worker.cpp
	@echo " Compile worker ...";
	g++ -I ./include/ -g -c -o ./build/worker.o ./src/worker.cpp

all: bin/sniffer bin/worker

run: bin/sniffer bin/worker
	./bin/sniffer -p ~/watch

clean:
	@echo " Delete binary, build and output ...";
	-rm -f ./bin/sniffer ./bin/worker ./build/manager.o ./build/worker.o ./output/*.out ./pipes/*