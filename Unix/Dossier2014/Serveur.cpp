/* Serveur.c
*/
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/varargs.h>
#include <sys/types.h>

#include "Fichier.ini"

#define MEMORY 1498

int idMsg;
int idShm;
int memoire;
char *memPart;

TABWINDOW	*Tab; 
void AfficheTab();
void hcoupeserv (int NumSig);
void Trace(char *pszTrace, ... );


int main()
{

int	rc,pid1 = 0 ,pid2 = 0, fichlog;
MESSAGE M, TransferConnecte;
char *mempub;
char  B[80];
char transfer[255];
int i,j,k;

Tab = (TABWINDOW*) malloc(sizeof(TABWINDOW)*5);

/*******************Redirige l'entrée d'erreur sur un fichier trace************************/
if((fichlog = open("traceServ1.log",O_CREAT|O_TRUNC|O_WRONLY, 0777)) == -1 )
  {
   printf("Erreur du fichier trace");
   exit(0); 
  }
  
  close(2);
  dup(fichlog);

/**********************Amorce les signaux*********************************/
struct sigaction Fct;

Fct.sa_handler = hcoupeserv;
sigemptyset(&Fct.sa_mask);
Fct.sa_flags = 0;
sigaction (SIGINT, &Fct, NULL);

/**************************Creation des resources**************************/
if ((idMsg = msgget(KEY,IPC_CREAT|IPC_EXCL|0600)) == -1)
   { 
      if ((idMsg = msgget(KEY,0)) == -1)
      {
        Trace("erreur de creation de file");
       exit(1);
      }
   }

if((memoire = shmget(MEMORY, 256, IPC_CREAT|IPC_EXCL|0600)) == -1)
{
  if ((memoire = shmget(MEMORY,0, NULL)) == -1)
  {
    Trace("erreur de creation de memoire");
    exit(1);
  }
}

Trace("Demarrage serveur memoire %d", getpid());
if((memPart = (char*)shmat(memoire, NULL, 0)) == (char*) -1)
{
  Trace("Erreur du rattachement à la mémoire");
  exit(1);
}

memcpy(memPart, Tab, sizeof(TABWINDOW)*5);

Trace("test");
fflush(stderr);

memcpy(Tab, memPart, sizeof(TABWINDOW)*5);

//mempub = memPart+(sizeof(TABWINDOW)*5);

//memcpy(mempub, "cochonou", sizeof("cochonou"));

Trace("%d, %s", Tab[1].Pid, Tab[1].NomUtilisateur);

while(1)
{
rc = msgrcv(idMsg,&M,sizeof(M) - sizeof(long),1L,0);//attend la réception d'un message
switch(M.Requete)
   { 

      case LOGIN:

          memcpy(Tab, memPart, sizeof(TABWINDOW)*5);
          for(i = 0; i < 5 && Tab[i].Pid != 0; i++);

          if(i == 5 && Tab[i].Pid != 0)
          {
            Trace("plus de place pour %d", M.idPid);
            continue;
          }

          Trace("%d", M.idPid);

          Tab[i].Pid = M.idPid;
          memcpy(memPart, Tab, sizeof(TABWINDOW)*5);
          break;

      case NEWWINDOW:

            memcpy(Tab, memPart, sizeof(TABWINDOW)*5);
            for(i = 0; i < 5 && Tab[i].Pid != M.idPid; i++);// On cherchele PID de l'utilisateur qui veut se log 

            if(strlen(Tab[i].NomUtilisateur)!=0) //si l'utilisateur est deja log on arrête
              continue;

            strcpy(Tab[i].NomUtilisateur, M.Donnee); //transfer du om de l'utilisateur log dans le tableau

            TransferConnecte.Type = M.idPid; // on prepare une structure qui enverra le nom du nouvel utilisateur aux autres fenêtre
            TransferConnecte.Requete = NEWWINDOW;

            for(i = 0; i < 5; i++)// on cherche tous les autres utilisateurs log pour leur envoyer le nouvel utilisateur
            {
              if(Tab[i].Pid != M.idPid && Tab[i].Pid != 0 && strlen(Tab[i].NomUtilisateur) > 0)
              {
                M.Type = Tab[i].Pid;
                if (msgsnd (idMsg, &M, strlen(M.Donnee) + sizeof(long) + 1 + sizeof(int), 0) == -1)//envois du nouvel utilisateur aux progs logs
                {
                  Trace("Erreur envois de nouvel utilisateur process %d", M.idPid);
                  exit(0);
                }

                strcpy(TransferConnecte.Donnee, Tab[i].NomUtilisateur);
                if (msgsnd (idMsg, &TransferConnecte, strlen(M.Donnee) + sizeof(long) + 1 + sizeof(int), 0) == -1)//dans la foulée on envoit au nouvel utilisateur les utilisateurs connectés
                {
                  Trace("Erreur envois de nouvel utilisateur process %d", TransferConnecte.idPid);
                   exit(0);
                }
              

                kill (Tab[i].Pid, SIGUSR1);
                kill (TransferConnecte.Type, SIGUSR1);
              }

            }

          Trace("Client %d login", M.idPid);
          AfficheTab();
          memcpy(memPart, Tab, sizeof(TABWINDOW)*5);

        break;

     case ENDWINDOW:
            memcpy(Tab, memPart, sizeof(TABWINDOW)*5);
            for(i = 0; i < 5 && Tab[i].Pid != M.idPid; i++); //recherche de l'utilisateur qui se delog
              
            strcpy(Tab[i].NomUtilisateur, "\0");//remplace son nom par /0
            Tab[i].Pid = 0;
            j = 0;
            while(j < 6)
            {
              Tab[i].Autre[j] = 0;
              j++;
            }
            Trace("Client %d deconnecté", M.idPid);
            AfficheTab();
            memcpy(memPart, Tab, sizeof(TABWINDOW)*5);
        break;

     case TERMINER:
            memcpy(Tab, memPart, sizeof(TABWINDOW)*5);
            for(i = 0; i < 5 && Tab[i].Pid != M.idPid; i++); //recherche de l'utilisateur qui se delogs

            strcpy(M.Donnee, Tab[i].NomUtilisateur);
            strcpy(Tab[i].NomUtilisateur, "");//remplace son nom par /0

            for(j = 0; j < 6; j++) // on remet à 0 le champs des contact accepté pour l'utilisateur qui se delog
            {
              Tab[i].Autre[j] = 0;
            }

            for(i = 0; i < 5; i++)// On boucle pour enlever l'utilisateur de la table des autres
            {

              if(Tab[i].Pid != M.idPid && Tab[i].Pid != 0 && strlen(Tab[i].NomUtilisateur) > 0)
              {
                M.Type = Tab[i].Pid;
                if (msgsnd (idMsg, &M, strlen(M.Donnee) + sizeof(long) + 1 + sizeof(int), 0) == -1)//envois du nouvel utilisateur aux progs logs
                {
                  Trace("Erreur envois de nouvel utilisateur process %d", M.idPid);
                  exit(0);
                }
              
                kill (Tab[i].Pid, SIGUSR1);

                for(j = 0; Tab[i].Autre[j] != M.idPid && j < 6; j++);

                if(Tab[i].Autre[j] == M.idPid)
                  Tab[i].Autre[j] = 0;

              }
            }
            AfficheTab();
            memcpy(memPart, Tab, sizeof(TABWINDOW)*5);
        break;

     case RECHERCHER:
        break;

     case ANNULER:
        break;

     case MODIFIER:
        break;

     case ENVOYER:
          memcpy(Tab, memPart, sizeof(TABWINDOW)*5);
          for(i=0; i < 5 && M.idPid != Tab[i].Pid; i++); //on cherche dans la table le pid de celui qui a envoyé le message

          sprintf(transfer,"(%s)%s", Tab[i].NomUtilisateur, M.Donnee);//on prepare le message
          strcpy(M.Donnee, transfer);
          M.Requete = ENVOYER;

          for(j=0; j < 6 ; j++)// on parcours la table des utilisateurs acceptés
          {
            if(Tab[i].Autre[j] != 0)
            {  
              M.idPid = Tab[i].Autre[j];
              M.Type = Tab[i].Autre[j];
              if (msgsnd (idMsg, &M, strlen(M.Donnee) + sizeof(long) + 1 + sizeof(int), 0) == -1)
              {
                Trace("Erreur envois de message process %d", M.idPid);
                exit(0);
              }
              kill (M.idPid, SIGUSR1);
            }
            
          }
          memcpy(memPart, Tab, sizeof(TABWINDOW)*5);
        break;

     case ACCEPTER:
          memcpy(Tab, memPart, sizeof(TABWINDOW)*5);
          for(i = 0; i < 5 && strcmp(M.Donnee, Tab[i].NomUtilisateur); i++);//chercher le nom à accepter
          k = Tab[i].Pid;
          Trace("%s %d", Tab[i].NomUtilisateur, k);
          for(i = 0; i < 5 && Tab[i].Pid != M.idPid; i++);
          Trace("%s %d", Tab[i].NomUtilisateur, k);
          for (j = 0; i < 5 && Tab[i].Autre[j] != 0 ;j++);
          Tab[i].Autre[j] = k;
          memcpy(memPart, Tab, sizeof(TABWINDOW)*5);
        break;

     case REFUSER:
        
        break;
      }
   }
}


void AfficheTab()
{
int i = 0;
while (i < 5) 
   { Trace("%5d --%-10s-- %5d %5d %5d %5d %5d\n",
         Tab[i].Pid,Tab[i].NomUtilisateur,
         Tab[i].Autre[0],Tab[i].Autre[1],Tab[i].Autre[2],
         Tab[i].Autre[3],Tab[i].Autre[4]);
     i++;
   }
return ;
}

void hcoupeserv (int NumSig)
{
  msgctl(idMsg,IPC_RMID, NULL);
  shmdt(memPart);
  shmctl(MEMORY,IPC_RMID,NULL);    // Suppression de la memoire partagee
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


