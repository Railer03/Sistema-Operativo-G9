1) Propósito de la aplicación

El proyecto en C++ consiste en la construcción de un sistema, cuyo objetivo es gestionar la interacción con distintos perfiles de usuario y ofrecer un conjunto de funcionalidades accesibles desde un menú en consola.

Actualmente el proyecto permite:

-Agregar, listar y eliminar usuarios con ID, nombre, username, contraseña y perfil (ADMIN o GENERAL).

-Acceder a un menú con siete opciones: administración de usuarios y perfiles (solo accesible para ADMIN), multiplicación de matrices NxN, un juego, verificación de palíndromos, cálculo de la función f(x)=x^2 + 2x + 8, conteo sobre texto (vocales, consonantes, caracteres especiales y palabras) y salir del sistema. De las cuales las primeras tres están en desarrollo.

-Multiplicar matrices NxN a partir de archivos de texto.

2) Cómo se debe ejecutar
   
Abrir una terminal
Clonar el repositorio
Entrar en la carpeta del proyecto
Compilar el proyecto usando make, que genera los ejecutables menu, modulo y matriz

Ejecutar el programa de administración de perfiles:

./modulo

Ejecutar el programa principal con:

./menu -u usuario -p password -f ".env"

Para el programa de multiplicación de matrices:

./matriz


3) Descripción de las variables de entorno

En el archivo .env se guardan las siguientes variables:

USER_FILE: archivo con la información principal de usuarios (ID, nombre, username, contraseña, tipo de perfil)
PERFIL_FILE: archivo con las opciones permitidas para cada perfil de usuario
ADMIN_SYS: ejecutable para la administración de usuarios y perfiles
MUTLI_M: ejecutable para la multiplicación de matrices
GAME: ejecutable del juego pong
CREATE_INDEX: ejecutable para crear el índice invertido
PATH_IDX: carpeta donde se guardan los archivos de índices
LOG_FILE: carpeta donde se guardan los logs
INDICE-INVET-PARALELO: ejecutable para crear el índice invertido paralelo

4) Paralelización en createindexparalelo

La paralelización en createindexparalelo se realiza utilizando hilos (threads) en C++. El proceso divide el trabajo de indexar los libros en lotes, y cada hilo procesa un lote de archivos en paralelo. Esto permite aprovechar los núcleos del procesador y acelerar la creación del índice invertido, especialmente cuando hay muchos archivos de texto en la carpeta de libros. El usuario puede configurar el número de hilos (N_THREADS) y el tamaño de cada lote (N_LOTE) al ejecutar la opción correspondiente en el menú.


5) Juego Pong

El juego pong se ejecuta desde el menú principal (opción 3).

Controles:
- El jugador controla la paleta izquierda usando la tecla W para subir y la tecla S para bajar.
- La paleta derecha es controlada por la computadora.

Reglas y dinámica:
- El objetivo es evitar que la pelota pase tu paleta (lado izquierdo) y hacer que rebote contra la paleta o los bordes superior/inferior.
- Cada vez que la pelota rebota en tu paleta, sigue en juego. Si la pelota pasa tu paleta, el punto lo gana la computadora.
- Si la pelota pasa la paleta de la computadora (lado derecho), tú ganas un punto.
- El primer jugador en llegar a 10 puntos gana la partida.
- El juego muestra el puntaje actual en pantalla.

Consejos:
- Usa W y S para posicionar tu paleta y anticipar la trayectoria de la pelota.
- Mantente atento a la velocidad y ángulo de rebote, que pueden cambiar según el punto de impacto.

