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

En el .env se guardan las siguientes:

USER_FILE: indica el archivo donde se guarda la información principal de usuarios (ID, nombre, username, contraseña, tipo de perfil)
PERFIL_FILE: indica el archivo donde se limitan las opciones para distintos perfil de usuarios en el menu
MATRIZ_A: indica el archivo  donde se guarda la primera matriz de la multiplicación de matrices
MATRIZ_B: indica el archivo  donde se guarda la segunda matriz de la multiplicación de matrices
