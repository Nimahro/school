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
MEMOIRE *Tab;

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

/*****************************CREATION MEMOIRE PARTAGEE*******************/
if((memoire = shmget(MEMORY, sizeof(MEMOIRE), IPC_CREAT|IPC_EXCL|0600)) == -1) //creation d'une nouvelle memoire si elle existe pas
{
  if ((memoire = shmget(MEMORY,0, NULL)) == -1) //si elle existe on recupere ses infos ici
  {
    Trace("erreur de creation de memoire"); // sinon on a un problème
    exit(1);
  }
}

Trace("Demarrage serveur memoire %d", getpid());
if((Tab = (MEMOIRE*)shmat(memoire, NULL, 0)) == (MEMOIRE*) -1)//On se raccroche à la mémoire avec un pointeur de struct memoire
{
  Trace("Erreur du rattachement à la mémoire");
  exit(1);
}

/*****************************SEMAPHORES********************************************************/

while(1)
{
rc = msgrcv(idMsg,&M,sizeof(M) - sizeof(long),1L,0);//attend la réception d'un message
Trace("Message reçu: %s %d", M.Donnee, M.Requete, )
switch(M.Requete)
   { 

      case LOGIN:

          
          for(i = 0; i < 5 && Tab->Utilisateur[i].Pid != 0; i++);

          if(i == 5 && Tab->Utilisateur[i].Pid != 0)
          {
            Trace("plus de place pour %d", M.idPid);
            continue;
          }

          Trace("%d", M.idPid);

          Tab->Utilisateur[i].Pid = M.idPid;
          
          break;

      case NEWWINDOW:

            
            for(i = 0; i < 5 && Tab->Utilisateur[i].Pid != M.idPid; i++);// On cherchele PID de l'utilisateur qui veut se log 

            if(strlen(Tab->Utilisateur[i].NomUtilisateur)!=0) //si l'utilisateur est deja log on arrête
              continue;

            strcpy(Tab->Utilisateur[i].NomUtilisateur, M.Donnee); //transfer du om de l'utilisateur log dans le tableau

            TransferConnecte.Type = M.idPid; // on prepare une structure qui enverra le nom du nouvel utilisateur aux autres fenêtre
            TransferConnecte.Requete = NEWWINDOW;

            for(i = 0; i < 5; i++)// on cherche tous les autres utilisateurs log pour leur envoyer le nouvel utilisateur
            {
              if(Tab->Utilisateur[i].Pid != M.idPid && Tab->Utilisateur[i].Pid != 0 && strlen(Tab->Utilisateur[i].NomUtilisateur) > 0)
              {
                M.Type = Tab->Utilisateur[i].Pid; 
                if (msgsnd (idMsg, &M, strlen(M.Donnee) + sizeof(long) + 1 + sizeof(int), 0) == -1)//envois du nouvel utilisateur aux progs logs
                {
                  Trace("Erreur envois de nouvel utilisateur process %d", M.idPid);
                  exit(0);
                }

                strcpy(TransferConnecte.Donnee, Tab->Utilisateur[i].NomUtilisateur);
                if (msgsnd (idMsg, &TransferConnecte, strlen(M.Donnee) + sizeof(long) + 1 + sizeof(int), 0) == -1)//dans la foulée on envoit au nouvel utilisateur les utilisateurs connectés
                {
                  Trace("Erreur envois de nouvel utilisateur process %d", TransferConnecte.idPid);
                   exit(0);
                }
              

                kill (Tab->Utilisateur[i].Pid, SIGUSR1);
                kill (TransferConnecte.Type, SIGUSR1);
              }

            }

          Trace("Client %d login", M.idPid);
          AfficheTab();
          

        break;

     case ENDWINDOW:
            
            for(i = 0; i < 5 && Tab->Utilisateur[i].Pid != M.idPid; i++); //recherche de l'utilisateur qui se delog
              
            strcpy(Tab->Utilisateur[i].NomUtilisateur, "\0");//remplace son nom par \0
            Tab->Utilisateur[i].Pid = 0;
            j = 0;
            while(j < 6)
            {
              Tab->Utilisateur[i].Autre[j] = 0;
              j++;
            }
            Trace("Client %d deconnecté", M.idPid);
            AfficheTab();
            
        break;

     case TERMINER:
            
            for(i = 0; i < 5 && Tab->Utilisateur[i].Pid != M.idPid; i++); //recherche de l'utilisateur qui se delogs

            strcpy(M.Donnee, Tab->Utilisateur[i].NomUtilisateur);
            strcpy(Tab->Utilisateur[i].NomUtilisateur, "");//remplace son nom par \0

            for(j = 0; j < 6; j++) // on remet à 0 le champs des contact accepté pour l'utilisateur qui se delog
            {
              Tab->Utilisateur[i].Autre[j] = 0;
            }

            for(i = 0; i < 5; i++)// On boucle pour enlever l'utilisateur de la table des autres
            {

              if(Tab->Utilisateur[i].Pid != M.idPid && Tab->Utilisateur[i].Pid != 0 && strlen(Tab->Utilisateur[i].NomUtilisateur) > 0)
              {
                M.Type = Tab->Utilisateur[i].Pid; 
                if (msgsnd (idMsg, &M, strlen(M.Donnee) + sizeof(long) + 1 + sizeof(int), 0) == -1)//envois du nouvel utilisateur aux progs logs
                {
                  Trace("Erreur envois de nouvel utilisateur process %d", M.idPid);
                  exit(0);
                }
              
                kill (Tab->Utilisateur[i].Pid,  SIGUSR1);

                for(j = 0; Tab->Utilisateur[i].Autre[j] != M.idPid && j < 6; j++);

                if(Tab->Utilisateur[i].Autre[j] == M.idPid)
                  Tab->Utilisateur[i].Autre[j] = 0;

              }
            }
            AfficheTab();
            
        break;

     case RECHERCHER:
        break;

     case ANNULER:
        break;

     case MODIFIER:
        break;

     case ENVOYER:
          
          for(i=0; i < 5 && M.idPid != Tab->Utilisateur[i].Pid; i++); //on cherche dans la table le pid de celui qui a envoyé le message

          sprintf(transfer,"(%s)%s", Tab->Utilisateur[i].NomUtilisateur, M.Donnee);//on prepare le message
          strcpy(M.Donnee, transfer);
          M.Requete = ENVOYER;

          for(j=0; j < 6 ; j++)// on parcours la table des utilisateurs acceptés
          {
            if(Tab->Utilisateur[i].Autre[j] != 0)
            {  
              M.idPid = Tab->Utilisateur[i].Autre[j];
              M.Type = Tab->Utilisateur[i].Autre[j];
              if (msgsnd (idMsg, &M, strlen(M.Donnee) + sizeof(long) + 1 + sizeof(int), 0) == -1)
              {
                Trace("Erreur envois de message process %d", M.idPid);
                exit(0);
              }
              kill (M.idPid, SIGUSR1);
            }
            
          }
          
        break;

     case ACCEPTER:
          
          for(i = 0; i < 5 && strcmp(M.Donnee, Tab->Utilisateur[i].NomUtilisateur); i++);//chercher le nom à accepter
          k = Tab->Utilisateur[i].Pid;
          Trace("%s %d", Tab->Utilisateur[i].NomUtilisateur, k);
          for(i = 0; i < 5 && Tab->Utilisateur[i].Pid != M.idPid; i++);
          Trace("%s %d", Tab->Utilisateur[i].NomUtilisateur, k);
          for (j = 0; i < 5 && Tab->Utilisateur[i].Autre[j] != 0 ;j++);
          Tab->Utilisateur[i].Autre[j] = k;
          
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
         Tab->Utilisateur[i].Pid, Tab->Utilisateur[i].NomUtilisateur,
         Tab->Utilisateur[i].Autre[0],Tab->Utilisateur[i].Autre[1],Tab->Utilisateur[i].Autre[2],
         Tab->Utilisateur[i].Autre[3],Tab->Utilisateur[i].Autre[4]);
     i++;
   }
return ;
}

void hcoupeserv (int NumSig)
{
  msgctl(idMsg,IPC_RMID, NULL);
  shmdt(Tab);    // Detachement de la memoire partagee
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


