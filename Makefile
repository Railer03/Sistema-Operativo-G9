
all:
	rm -f menu matriz modulo indice paralelo motor cache buscador benchmark_threads
	g++ -fopenmp mainmenu.cpp -o menu
	g++ -fopenmp multimatriz.cpp -o matriz
	g++ -fopenmp modulo.cpp -o modulo
	g++ -fopenmp createindex.cpp -o indice
	g++ -fopenmp createindexparalelo.cpp -o paralelo
	g++ -fopenmp motor.cpp -o motor
	g++ -fopenmp cache.cpp -o cache
	g++ -fopenmp buscador.cpp -o buscador
	g++ -fopenmp benchmark_threads.cpp -o benchmark_threads
	$(MAKE) -C juego
clean:
	rm -f menu matriz modulo indice paralelo motor cache buscador benchmark_threads
	$(MAKE) -C juego clean