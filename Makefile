
all:
	rm -f menu matriz modulo indice paralelo
	g++ -fopenmp mainmenu.cpp -o menu
	g++ -fopenmp multimatriz.cpp -o matriz
	g++ -fopenmp modulo.cpp -o modulo
	g++ -fopenmp createindex.cpp -o indice
	g++ -fopenmp createindexparalelo.cpp -o paralelo
	$(MAKE) -C juego
