CXX = g++
FLAGS = -std=c++17 -O2

matriz:
	$(CXX) $(FLAGS) -o matriz matriz.cpp
	./matriz | tee matriz.txt
	rm -f matriz

falso:
	$(CXX) $(FLAGS) -pthread -o falso falso_compartir.cpp
	./falso | tee falso.txt
	rm -f falso

paso:
	$(CXX) $(FLAGS) -o paso paso.cpp
	./paso | tee paso.txt
	rm -f paso

amdahl:
	$(CXX) $(FLAGS) -pthread -o amdahl amdahl.cpp
	./amdahl | tee amdahl.txt
	rm -f amdahl

todo: matriz falso paso amdahl

limpiar:
	rm -f matriz falso paso amdahl matriz.txt falso.txt paso.txt amdahl.txt
