1) Propósito de la aplicación
Este proyecto en C++ permite agregar, listar y eliminar usuarios con ID, nombre, username, contraseña y perfil (ADMIN o GENERAL).

Los datos se guardan en un archivo definido en .env (USER_FILE=usuarios.txt) y se mantienen de forma persistente.

El menú principal tiene siete opciones: administración de usuarios y perfiles (solo accesible para ADMIN), multiplicación de matrices NxN, un juego, verificación de palíndromos, cálculo de la función f(x)=x*x + 2x + 8, conteo sobre texto (vocales, consonantes, caracteres especiales y palabras) y salir del sistema. Además, se incluye un programa adicional para multiplicar matrices NxN a partir de archivos de texto, validando filas, columnas, separadores y contenido.

2) Como se debe ejecutar
Abrir una terminal.

Clonar el repositorio.

Entrar en la carpeta del proyecto.

Compilar el proyecto usando make, que genera los ejecutables mainmenu y multi.

Ejecutar el programa principal con:

./mainmenu -u usuario -p password -f "archivo.txt"

Para el programa de multiplicación de matrices:

./multi "/ruta/a/a.txt" "/ruta/a/b.txt" "#"

3) Descripción de las variables de entorno
La variable de entorno USER_FILE se define dentro del archivo .env y su función es indicar el nombre  del archivo donde se almacenarán los usuarios.
