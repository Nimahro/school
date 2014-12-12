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
#include <sys/stat.h>
#include <sys/sem.h>
#include <errno.h>

#include "Fichier.ini"

#define MEMORY 1498
#define SEMA 2314
#define FICHU "Utilisateur.dat"

union semun {
    int val;
  } argument;

int idMsg;
//int idShm;
int memoire;
int Sem;
MEMOIRE *Tab;

void AfficheTab();
void hcoupeserv (int NumSig);
void Trace(char *pszTrace, ... );


int main()
{

int	rc, fichlog, fichUser;
MESSAGE M, TransferConnecte;
//char  B[80];
char transfer[255];
int i,j,k, idRech;
sembuf operation;
UTILISATEUR lecFich;

operation.sem_num = 0;
operation.sem_flg = 0;

argument.val = 1;

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

 if(Tab->pid1 == 0)
  Tab->pid1 = getpid();
 
 else
 {
    if(Tab->pid2 == 0)
      Tab->pid2 = getpid();

    else
      exit(-5);
 }

 

/*****************************SEMAPHORES********************************************************/
if((Sem = semget(SEMA, 1, IPC_CREAT|IPC_EXCL|0600)) == -1)
{
  if((Sem = semget(SEMA, 0, 0)) == -1)
  {
    Trace("Erreur creation semaphore");
    exit(1);
  }
}
else
{
  semctl(Sem, 0, SETVAL, argument);
}

Trace("valeur semaphore %d", semctl(Sem, 0, GETVAL));
/************************Ouverture du fichier *************************************************/

while(1)
{
if((rc = msgrcv(idMsg,&M,sizeof(M) - sizeof(long),1L,0)) == -1)//attend la réception d'un message
{
  if(errno == EINTR)
    continue;

  Trace("erreur lecture message serveur");
  exit(-1);
}

Trace("Message reçu: %s %d type : %d\n", M.Donnee, M.Requete, M.Type);

switch(M.Requete)
   { 

      case LOGIN:

          operation.sem_op = -1;

          if((semop(Sem, &operation, 1)) == -1)
          {
            Trace("erreur MAJ semaphore");
            exit(-5);
          }

          for(i = 0; i < 5 && Tab->Utilisateur[i].Pid != 0; i++);

          if(i == 5 && Tab->Utilisateur[i].Pid != 0)
          {
            Trace("plus de place pour %d", M.idPid);
            continue;
          }

          Trace("%d", M.idPid);

          Tab->Utilisateur[i].Pid = M.idPid;

          operation.sem_op = 1;

          if((semop(Sem, &operation, 1)) == -1)
          {
            Trace("erreur MAJ semaphore");
            exit(-5);
          }
          
          break;

      case NEWWINDOW:

            operation.sem_op = -1;

            if((semop(Sem, &operation, 1)) == -1)
            {
              Trace("erreur MAJ semaphore");
              exit(-5);
            }

            if((fichUser = open(FICHU, O_RDONLY)) == -1)
            {
              Trace("Erreur fichier client");
              exit(-1);
            }

            while(read(fichUser, &lecFich, sizeof(UTILISATEUR)))
            {
              if(!strcmp(M.Donnee, lecFich.NomUtilisateur))
                break;
            }

            close(fichUser);

            if(strcmp(M.Donnee, lecFich.NomUtilisateur))
            {
              Trace("L'utilisateur n'existe pas %s", M.Donnee);
              continue;
            }
              
            for(i = 0; i < 5 && Tab->Utilisateur[i].Pid != M.idPid; i++);// On cherchele PID de l'utilisateur qui veut se log 

            strcpy(Tab->Utilisateur[i].NomUtilisateur, M.Donnee); //transfer du nom de l'utilisateur log dans le tableau

            TransferConnecte.Type = M.idPid; // Struc qui permettras de transferer le nom des utilisateurs connectés aux autres utilisateurs
            TransferConnecte.Requete = NEWWINDOW;

            for(i = 0; i < 5; i++)// on cherche tous les autres utilisateurs log pour leur envoyer le nouvel utilisateur
            {
              if(Tab->Utilisateur[i].Pid != M.idPid && Tab->Utilisateur[i].Pid != 0 && strlen(Tab->Utilisateur[i].NomUtilisateur) > 0)
              {
                M.Type = Tab->Utilisateur[i].Pid; 
                if (msgsnd (idMsg, &M, sizeof(MESSAGE) - sizeof(long), 0) == -1)//envois du nouvel utilisateur aux progs logs
                {
                  Trace("Erreur envois de nouvel utilisateur process %d", M.idPid);
                  exit(0);
                }

                strcpy(TransferConnecte.Donnee, Tab->Utilisateur[i].NomUtilisateur);
                Trace("transfer du nom : %s\n", TransferConnecte.Donnee);
                if (msgsnd (idMsg, &TransferConnecte, sizeof(MESSAGE) - sizeof(long), 0) == -1)//dans la foulée on envoit au nouvel utilisateur les utilisateurs connectés
                {
                  Trace("Erreur envois de nouvel utilisateur process %d", TransferConnecte.idPid);
                   exit(0);
                }
              

                kill (Tab->Utilisateur[i].Pid, SIGUSR1);
                kill (TransferConnecte.Type, SIGUSR1);
              }

            }

          Trace("Client %d login", M.idPid);
          

          operation.sem_op = 1;

            if((semop(Sem, &operation, 1)) == -1)
            {
              Trace("erreur MAJ semaphore");
              exit(-5);
            }
        break;

     case ENDWINDOW:
            
            operation.sem_op = -1;

            if((semop(Sem, &operation, 1)) == -1)
            {
              Trace("erreur MAJ semaphore");
              exit(-5);
            }

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

            operation.sem_op = 1;

            if((semop(Sem, &operation, 1)) == -1)
            {
              Trace("erreur MAJ semaphore");
              exit(-5);
            }
            
        break;

     case TERMINER:
            
            operation.sem_op = -1;

            if((semop(Sem, &operation, 1)) == -1)
            {
              Trace("erreur MAJ semaphore");
              exit(-5);
            }

            for(i = 0; i < 5 && Tab->Utilisateur[i].Pid != M.idPid; i++); //recherche de l'utilisateur qui se delogs

            strcpy(M.Donnee, Tab->Utilisateur[i].NomUtilisateur);//On prepare un message pour les autres clients
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

            operation.sem_op = 1;

            if((semop(Sem, &operation, 1)) == -1)
            {
              Trace("erreur MAJ semaphore");
              exit(-5);
            }
            
        break;

     case RECHERCHER:

        if ((idRech = fork()) == -1)
        {
          Trace("erreur fork rechercher");
          exit(-5);
        }
        Trace("fork ok %d ", idRech);
        if(!idRech)
        {
          sprintf(transfer, "%d", M.idPid);
          execl("./rechercher", "rechercher", M.Donnee, transfer, NULL);
        }
        break;

     case ANNULER:
        break;

     case MODIFIER:
        break;

     case ENVOYER:
          
          operation.sem_op = -1;

            if((semop(Sem, &operation, 1)) == -1)
            {
              Trace("erreur MAJ semaphore");
              exit(-5);
            }

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

          operation.sem_op = 1;

            if((semop(Sem, &operation, 1)) == -1)
            {
              Trace("erreur MAJ semaphore");
              exit(-5);
            }
        break;

     case ACCEPTER:
          
          operation.sem_op = -1;

            if((semop(Sem, &operation, 1)) == -1)
            {
              Trace("erreur MAJ semaphore");
              exit(-5);
            }

          for(i = 0; i < 5 && strcmp(M.Donnee, Tab->Utilisateur[i].NomUtilisateur); i++);//chercher le nom à accepter
          k = Tab->Utilisateur[i].Pid;//On enregistre le PID de l'utilisateur trouvé
          Trace("%s %d", Tab->Utilisateur[i].NomUtilisateur, k);
          for(i = 0; i < 5 && Tab->Utilisateur[i].Pid != M.idPid; i++);//On cherche l'utilisateur qui a accepté
          Trace("%s %d", Tab->Utilisateur[i].NomUtilisateur, k);
          for (j = 0; i < 5 && Tab->Utilisateur[i].Autre[j] != 0 ;j++);//On parcour le tableau des utilisateurs acceptés pour trouver un emplacement libre
          Tab->Utilisateur[i].Autre[j] = k;
          
          operation.sem_op = 1;

            if((semop(Sem, &operation, 1)) == -1)
            {
              Trace("erreur MAJ semaphore");
              exit(-5);
            }
        break;

     case REFUSER:

          operation.sem_op = -1;

          if((semop(Sem, &operation, 1)) == -1)
          {
            Trace("erreur MAJ semaphore");
            exit(-5);
          }

          for(i = 0; i < 5 && strcmp(M.Donnee, Tab->Utilisateur[i].NomUtilisateur); i++);//chercher le nom à refuser
          k = Tab->Utilisateur[i].Pid;//On enregistre l'ID à refuser

          for(i = 0; i < 5 && Tab->Utilisateur[i].Pid != M.idPid; i++);// On recherche l'utilisateur qui a refusé

          for (j = 0; i < 5 && Tab->Utilisateur[i].Autre[j] != k ;j++);// On parcour son tab des utilisateurs acceptés pour trouver les vides.
          Tab->Utilisateur[i].Autre[j] = 0;

          operation.sem_op = 1;

          if((semop(Sem, &operation, 1)) == -1)
          {
            Trace("erreur MAJ semaphore");
            exit(-5);
          }
        break;
      }

      AfficheTab();
      Trace("\n\n\n");
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
  if(!Tab->pid2 || !Tab->pid1)
  {
    msgctl(idMsg,IPC_RMID, NULL);
    shmctl(memoire, IPC_RMID, 0);
    semctl(Sem, 0, IPC_RMID, 0);
  }
  if(Tab->pid1 == getpid())
  {
    Tab->pid1 = 0;
    exit(0);
  }

  Tab->pid2 = 0;

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


