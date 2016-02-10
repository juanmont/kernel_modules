#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

int main(){
	
	//entrada 
	printf("Programa de usuario del modulo multilist \n");
	int control = open ("/proc/multilist/control", O_WRONLY);
    int fdLists[10];
    int i;
	char *salida;
	//for(i = 0; i < 10; i++){
        write(control, "create list", strlen("create list"));
        fdLists[0] =  open ("/proc/multilist/list", O_WRONLY);
		write(fdLists[0], "add 1", strlen("add 1"));
		usleep(100000);
		read(fdLists[0],salida,strlen(int));
		printf("\nSalida de list\n");
		printf("%s \n", salida);
		
		write(control, "create list2", strlen("create list2"));
        fdLists[1] =  open ("/proc/multilist/list", O_WRONLY);
		write(fdLists[1], "add 2", strlen("add 2"));
		usleep(100000);
		read(fdLists[1],salida,strlen(int));
		printf("\nSalida de list2\n");
		printf("%s \n", salida);
		
		write(control, "create list3", strlen("create list3"));
        fdLists[2] =  open ("/proc/multilist/list", O_WRONLY);
		write(fdLists[2], "add 3", strlen("add 3"));
		usleep(100000);
		read(fdLists[2],salida,strlen(int));
		printf("\nSalida de list3\n");
		printf("%s \n", salida);
		
		printf("\nBorramos list\n");
		write(control, "delete list", strlen("delete list"));
		
		printf("\nIntentamos borrar control\n");
		write(control, "delete control", strlen("delete control"));	
		
	//}
	
	close(control);
    close(fdLists[0]);

}