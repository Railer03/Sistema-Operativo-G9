1) Propósito de la aplicación
Este proyecto en C++ permite agregar, listar y eliminar usuarios con ID, nombre, username, contraseña y perfil (ADMIN o GENERAL).

Los datos se guardan en un archivo definido en .env (USER_FILE=usuarios.txt) y se mantienen de forma persistente.

El programa muestra un menú interactivo en consola y permite actualizar la base de usuarios en el archivo de texto.

2) Como se debe ejecutar
Abrir una terminal.
Clonar el repositorio.
Entrar en la carpeta del proyecto.
Compilar el archivo modulo.cpp con "g++ -o modulo modulo.cpp" para generar un archivo ejecutable llamado modulo.
Ejecutar el programa escribiendo ./modulo en la terminal.

3) Descripción de las variables de entorno
La variable de entorno USER_FILE se define dentro del archivo .env y su función es indicar el nombre  del archivo donde se almacenarán los usuarios.
