#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <qapplication.h>

#include "Client.h"

#include "Fichier.ini"
#include <signal.h>

void HNouvelUtilisateur(int);
void HNouveauMessage(int);
void HFinConnexion(int);
MESSAGE	M;

int 	idMsg;
Client* F1;
int main(int argc,char* argv[])
{
    struct sigaction Fct;

    Fct.sa_handler = HNouveauMessage;
    sigemptyset(&Fct.sa_mask);
    Fct.sa_flags = 0;
    sigaction (SIGUSR1, &Fct, NULL);

    Fct.sa_handler = HFinConnexion;
    sigemptyset(&Fct.sa_mask);
    Fct.sa_flags = 0;
    sigaction (SIGINT, &Fct, NULL);

	if ((idMsg = msgget(KEY, 0)) == -1)
    {
        printf("c'est ici");
        fflush(stdout);
        perror("Erreur msgget Client");
        exit(0);
    }

    M.Requete = NEWWINDOW;
    M.Type = 1L;
    M.idPid = getpid();

    if (msgsnd (idMsg, &M, strlen(M.Donnee) + sizeof(long) + 1 + sizeof(int), 0) == -1)
    {
        perror("Erreur msgsnd Client");
        exit(0);
    }

    M.Donnee[0] = '0';

	QApplication appl(argc,argv);
	F1 = new Client();
	F1->show();

	return appl.exec();
}

void HNouveauMessage(int Sig)
{
    int i = 0;

    while(msgrcv (idMsg, &M, sizeof(MESSAGE) - sizeof(long), getpid(), IPC_NOWAIT) != -1)
    {
        
        printf("%s\n", M.Donnee);
        fflush(stdout);
        switch(M.Requete)
        {
            case ENVOYER:
                F1->setMessage(M.Donnee);
            break;

            case NEWWINDOW:
                for(i = 0; strlen(F1->getPersonne(i)) != 0 && i < 5; i++);
                    
                if(strlen(F1->getPersonne(i)) == 0)
                    F1->setPersonne(i, M.Donnee);
            break;

            case TERMINER:
                for(i = 0; strcmp(F1->getPersonne(i), M.Donnee) != 0 && i < 5; i++);

                if(strcmp(F1->getPersonne(i), M.Donnee) == 0)
                {
                    F1->setPersonne(i, "");
                    F1->setCheckbox(i, false);
                }

            case UTTROUVE:
                F1->setGsm(M.Donnee);
            break;
        }
    }
    return;
}

void HFinConnexion(int Sig)
{
    M.Requete = ENDWINDOW;
    M.Type = 1L;
    M.idPid = getpid();
    if (msgsnd (idMsg, &M, strlen(M.Donnee) + sizeof(long) + 1 + sizeof(int), 0) == -1)
    {
        perror("Erreur Fin connexion");
        exit(0);
    }

    exit(0);
}

void HNouvelUtilisateur(int Sig)
{
	return ;
}
