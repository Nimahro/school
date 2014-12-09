#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/varargs.h>

void Trace(char *pszTrace, ... );

int main(int argc, char const *argv[])
{
	int fichlog;
	/*******************Redirige l'entrée d'erreur sur un fichier trace************************/
	if((fichlog = open("rechtrace.log",O_CREAT|O_TRUNC|O_WRONLY, 0777)) == -1 )
	  {
	   printf("Erreur du fichier trace");
	   exit(0); 
	  }
	  
	  close(2);
	  dup(fichlog);

	  Trace("reception de %s et %s\n", argv[1], argv[2]);
	
	return 0;
}

void Trace(char *pszTrace, ... )
{
  char szBuffer[80];
  va_list arg;
  va_start(arg,pszTrace);
  vsprintf(szBuffer,pszTrace,arg);
  fprintf(stderr,"%s\n",szBuffer);
  va_end(arg);
  return ;
}

