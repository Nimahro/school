#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/varargs.h>
#include <errno.h>
#include <sys/wait.h>
#include <string.h>

#include "Fichier.ini"

void Trace(char *pszTrace, ... );

int main(int argc, char const *argv[])
{
	int fichlog, idMsg, fichUser;
	UTILISATEUR lecFich;

	struct flock verrou;

	/*******************Redirige l'entrée d'erreur sur un fichier trace************************/
	if((fichlog = open("rechtrace.log",O_CREAT|O_TRUNC|O_WRONLY, 0777)) == -1 )
	  {
	   printf("Erreur du fichier trace");
	   exit(0); 
	  }
	  
	  close(2);
	  dup(fichlog);

	/***********************Connexion file message***************************************/

	 if ((idMsg = msgget(KEY,0)) == -1)
     {
       	Trace("erreur de connexion à la de file");
      	exit(1);
     }

     Trace("%s , %s, %s\n", argv[1], argv[2], argv[3]);

	 if(!strcmp(argv[3], "0")) // Si il ne s'agit pas d'une recherche sur le même utilisateur
	 {
	 	if((fichUser = open(FICHU, O_RDONLY)) == -1)// ouverture du fichier utilisateur read only
        {
        	Trace("Erreur fichier utilisateur");
            exit(-1);
        }

        while(read(fichUser, &lecFich, sizeof(UTILISATEUR))) // rechercher
        {
            if(!strcmp(argv[1], lecFich.NomUtilisateur)) // si on trouve l'utilisateur on arrête la lecture
            break;
        }

        if(strcmp(argv[1], lecFich.NomUtilisateur))
        {
        	Trace("l'utilisateur rechercher n'existe pas");
        	exit(0);
        }

        verrou.l_type = F_RDLCK; // verrou partagé sur l'enregistrement lu
        verrou.l_whence = SEEK_SET;
        verrou.l_start = lseek(fichUser, -sizeof(UTILISATEUR), SEEK_CUR);
        verrou.l_len = sizeof(UTILISATEUR);

    	if((fcntl(fichUser, F_SETLK, &verrou)) == -1)// si le fichier est déjà verrouillé
    	{
    		Trace("erreur de lecture fichier modifier");
    		exit(0);
    	}

    	verrou.l_type = F_UNLCK; // on enleve le verrou
    	fcntl(fichUser, F_SETLK, &verrou);

    	Trace("OK envois du message");

	 	exit(0);
	 }

	/*************************UTILISATEUR SE CHERCHANT LUI MEME********************************/
	if((fichUser = open(FICHU, O_RDWR)) == -1)// ouverture du fichier utilisateur read only
    {
    	Trace("Erreur fichier utilisateur");
       	exit(-1);
    }

    while(read(fichUser, &lecFich, sizeof(UTILISATEUR))) // rechercher
    {
        if(!strcmp(argv[1], lecFich.NomUtilisateur)) // si on trouve l'utilisateur on arrête la lecture
        break;
    }

    if(strcmp(argv[1], lecFich.NomUtilisateur))
    {
    	Trace("l'utilisateur rechercher n'existe pas");
    	exit(0);
    }

    verrou.l_type = F_WRLCK; // verrou partagé sur l'enregistrement lu
    verrou.l_whence = SEEK_SET;
    verrou.l_start = lseek(fichUser, -sizeof(UTILISATEUR), SEEK_CUR);
    verrou.l_len = sizeof(UTILISATEUR);

    if((fcntl(fichUser, F_SETLKW, &verrou)) == -1)// si le fichier est déjà verrouillé
  	{
    	Trace("erreur de lecture fichier modifier");
    	exit(0);
    }


    sleep(20);


    verrou.l_type = F_UNLCK; // on enleve le verrou
    fcntl(fichUser, F_SETLK, &verrou);
	
	exit(0);
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

