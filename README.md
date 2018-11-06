TPE

El documento utilizado en la presentación se encuentra en el directorio pc-2018b-02.
El informe se encuentra en el directorio pc-2018b-02.

Para crear los ejecutables:
Moverse a la carpeta pc-2018b-02 con el comando cd.
ejecutar make all para construir el trabajo.
Los ejecutables se encontraran en los directorios: management, pop3filter y stripmime.
Para ejecutar el pop3filter: realizar cd pop3filter y luego ejecutar ./pop3filter con los parámetros que desee (leer instrucciones para la configuración).
Para ejecutar el cliente de management: realizar cd management y luego ./management con los parámetros que desee (leer instrucciones para la configuración).
Para ejecutar el stripmime: realizar cd stripmime y luego ./stripmime. 
Si se desean limpiar los archivos objeto moverse a la carpeta pc-2018b-02 y ejecutar: make clean.


En cuanto a los parámetros que pueden ser pasados al pop3filter se pasan por línea de comandos: todos los parámetros posibles se pueden ver ejecutando ./pop3filter -h. Las configuraciones posibles son:
-e: para elegir el archivo de errores.
-l: para la dirección del POP3
-L: para la dirección del management
-m: para el mensaje de reemplazo
-M: para las media types a censurar
-o: para setear el puerto de management
-p: para setear el puerto local
-P: para setear el puerto de origen
-t: comando para transformaciones externas
Todos los comandos son parámetros de ./pop3filter, además se debe especificar la dirección del servidor POP3 al final.
Por último en cuanto a los parámetros de la aplicación de management: todos los parámetros posibles se pueden ver ejecutando ./management -h. Las configuraciones posibles son:
-L: para setear la dirección de management del pop3filter.
-o: para setear el puerto de management del pop3filter.
