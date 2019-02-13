#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <errno.h>

#define PORT 6000
#define MAX_BUFFER 100

typedef struct {
  int reserve;// boolean, 0: place libre, 1: place reservee
  char nom[30];
  char numero[10];// numero de dossier
} infos;// les informations d'une place

typedef struct {
  infos siege[10][10];
} toutesInfos;// les informations de toutes les places

char tampon[MAX_BUFFER];// pour send() et recv()
int monsem;//mon semaphore

void preparationServeur();// configuration du serveur
void lireMessage(char tampon[]);// lire un message
void initialiserPlace(toutesInfos *places);// initialiser les places
void afficherPlace(toutesInfos *places);// afficher les places
void reserver(int fdSocketCommunication);// reserver une place
void annuler(int fdSocketCommunication);// annuler une reservation
void p(int semid);//operation P pour semaphore
void v(int semid);//operation V pour semaphore

int main(int argc , char const *argv[]) {
  preparationServeur();
}

void preparationServeur(){
  int fdSocketAttente;// pour socket()
  int fdSocketCommunication;// pour accept()
  struct sockaddr_in coordonneesServeur;//coord serveur
  struct sockaddr_in coordonneesAppelant;//coord client
  int longueurAdresse;
  int nbRecu;// pour send() et recv()

  fdSocketAttente = socket(PF_INET, SOCK_STREAM, 0);// ou AF_INET
  if (fdSocketAttente < 0) {
      printf("socket incorrecte\n");
      exit(EXIT_FAILURE);
  }
  else{
      printf("C'est un serveur\n");
  }
  // On prépare l’adresse d’attachement locale
  longueurAdresse = sizeof(struct sockaddr_in);
  memset(&coordonneesServeur, 0x00, longueurAdresse);
  coordonneesServeur.sin_family = PF_INET;
  // toutes les interfaces locales disponibles
  coordonneesServeur.sin_addr.s_addr = htonl(INADDR_ANY);
  // toutes les interfaces locales disponibles
  coordonneesServeur.sin_port = htons(PORT);
  if (bind(fdSocketAttente, (struct sockaddr *) &coordonneesServeur,
              sizeof(coordonneesServeur)) == -1) {
      // coordonneesServeur sockaddr_in => sockaddr
      printf("erreur de bind\n");
      exit(EXIT_FAILURE);
  }
  if (listen(fdSocketAttente, 5) == -1) {
      printf("erreur de listen\n");
      exit(EXIT_FAILURE);
  }
  else{
      printf("Serveur pret\n");
  }
  socklen_t tailleCoord = sizeof(coordonneesAppelant);// pourquoi socklen_t???
  int pid=fork();
  if(pid!=0){// le pere
    //partie semaphore----------------------------------------
    key_t clefsem=ftok(".", 20);
    monsem=semget(clefsem, 1, IPC_CREAT|0666);//initialiser semaphore
    semctl(monsem, 0, SETVAL, 1);
    if(monsem==-1){
      printf("Erreur de semget\n");
      exit(0);
    }
    //--------------------------------------------------------
    //partie memoire partage----------------------------------
    key_t clef=ftok(".", 19);
    int shmid=shmget(clef, sizeof(toutesInfos), 0666|IPC_CREAT);
    if(shmid==-1){
      printf("Allocation de la shm en echec!\n");
      printf("%s\n", strerror(errno));
      exit(0);
    }
    toutesInfos *places;//une structure qui contient 10x10 infos de place
    p(monsem);// operation P semaphore
    places=shmat(shmid, NULL, 0);//s'attacher a un segment de memoire partage
    if(places!=(void *)(-1)){
      initialiserPlace(places);
      afficherPlace(places);
      shmdt(places);//se detacher du segment de memoire partage
      v(monsem);// operation V semaphore
    }
    //------------------------------------------------------
    while(pid!=0){// un miracle, si c'est 1, ca va pas marcher
      if ((fdSocketCommunication = accept(fdSocketAttente,
                 (struct sockaddr *) &coordonneesAppelant,
                 &tailleCoord)) == -1) {
          printf("erreur de l'accept\n");
      }
      else{
          char *ip;
          ip=inet_ntoa(coordonneesAppelant.sin_addr);//sin_addr: adresse IPV4
          // inet_ntoa, de network byte a l'adresse IP
          printf("Client connecté %s: %d\n",
                 ip, ntohs(coordonneesAppelant.sin_port));
          //sin_port: numero de port, network to host short
          pid=fork();
      }
    }
  }
  if(pid==0){//le fils
    while(1){
      //partie semaphore--------------------------
      key_t clefsem=ftok(".", 20);
      int monsem=semget(clefsem, 1, IPC_CREAT|0666);// acceder au semaphore
      if(monsem==-1){
        printf("Erreur de semget\n");
        exit(0);
      }
      //-----------------------------------------------
      nbRecu = recv(fdSocketCommunication, tampon, MAX_BUFFER, 0);
      if (nbRecu > 0) {
        if(strcmp(tampon,"1")==0){//demande de reservation une place
          reserver(fdSocketCommunication);
        }
        else if(strcmp(tampon, "0")==0){
          annuler(fdSocketCommunication);
        }
      }
    }
  }
  //fin
  close(fdSocketCommunication);
  close(fdSocketAttente);
  //return EXIT_SUCCESS;
}

void lireMessage(char tampon[]){
   printf("Entrer: ");
   fgets(tampon, MAX_BUFFER, stdin);
   strtok(tampon, "\n");
}

void initialiserPlace(toutesInfos *places){
  int i, j;
  for(i=0;i<10;i++){
    for(j=0;j<10;j++){
      if(i==j){
        (*places).siege[i][j].reserve=1;// place occupe
      }
      else{
        (*places).siege[i][j].reserve=0;// place libre
      }
    }
  }
}

void afficherPlace(toutesInfos *places){
  int i, j;
  for(i=0;i<10;i++){
    for(j=0;j<10;j++){
      printf("%d ", (*places).siege[i][j].reserve);
    }
    printf("\n");//saut de ligne
  }
}

void reserver(int fdSocketCommunication){
  printf("Demande de réservation\n");
  int nbRecu = recv(fdSocketCommunication, tampon, MAX_BUFFER, 0);
  if (nbRecu > 0) {
     printf("Recu du client : %s\n", tampon);
  }
  char nom[30];
  strcpy(nom, tampon);
  //partie memoire partagee-----------------------------
  key_t clef=ftok(".", 19);
  int shmid=shmget(clef, sizeof(toutesInfos), 0666|IPC_CREAT);//c'est quoi 0666?
  if(shmid==-1){
    printf("Allocation de la shm en echec!\n");
    printf("%s\n", strerror(errno));
    exit(0);
  }
  toutesInfos* places;
  p(monsem);// operation P semaphore
  places=shmat(shmid, NULL, 0);//s'attacher a un segment de memoire partage
  if(places==(void *)(-1)){
    printf("Erreur memoire partagee\n");
    exit(0);
  }
  //---------------------------------------------------------
  int i, j;
  for(i=0;i<10;i++){
    for(j=0;j<10;j++){
      tampon[i*10+j]=(*places).siege[i][j].reserve;//sauvegarder les donnees
    }
  }
  afficherPlace(places);//afficher les places
  shmdt(places);//se detacher du segment de memoire partage
  v(monsem);// semaphore
  send(fdSocketCommunication, tampon, 100, 0);//envoyer aux clients
  //comme je veux envoyer que des chiffires,
  //il faut pas utiliser strlen(), qui pose probleme
  printf("(Attends la reponse...)\n");

  int occupe=1;
  while(occupe==1){
    int nbRecu = recv(fdSocketCommunication, tampon, MAX_BUFFER, 0);
    if (nbRecu > 0) {
       printf("Recu du client : %s\n", tampon);
    }
  //Cet partie est pour verifier si la place demandee est libre
    char copie[5];
    strcpy(copie, tampon);//on fait une copie de tampon a copie
    char *ligne, *colonne;
    ligne=strtok(copie, ", ");//couper par "," et " "
    colonne=strtok(NULL, ", ");//pour couper une meme chaine, faut entrer null
    i=atoi(ligne);
    j=atoi(colonne);//pour transformer de string a int
    p(monsem);// semaphore
    places=shmat(shmid, NULL, 0);//s'attacher a un segment de memoire partage
    if((*places).siege[i][j].reserve==1){
      shmdt(places);//se detacher du segment de memoire partage
      v(monsem);// semaphore
      printf("Cet place est occupée! Veuillez entrer à nouveau:\n");
      strcpy(tampon, "occupe");
      send(fdSocketCommunication, tampon, 100, 0);// on envoie 0
      printf("(Attends la reponse...)\n");
    }
    else{
      occupe=0;//on sort de la boucle
      (*places).siege[i-1][j-1].reserve=1;// on occupe la place
      strcpy((*places).siege[i-1][j-1].nom, nom);// on saisit le nom
      printf("Place (%d, %d) Réservé\n", i, j);
      // on genere un numero de dossier pour cette place
      sprintf((*places).siege[i-1][j-1].numero, "%d", 1000000000+i*10+j);//de int a string
      printf("Le numéro de dossier: %s\n", (*places).siege[i-1][j-1].numero);
      strcpy(tampon, (*places).siege[i-1][j-1].numero);
      afficherPlace(places);
      shmdt(places);//se detacher du segment de memoire partage
      v(monsem);// operation P semaphore
      send(fdSocketCommunication, tampon, 100, 0);// on envoie les infos de place
      printf("(Attends la reponse...)\n");
    }
  }
}

void annuler(int fdSocketCommunication){
  printf("Demande d'anuulation\n");
  strcpy(tampon, "ok");
  send(fdSocketCommunication, tampon, 100, 0);//envoyer aux clients
  int nbRecu = recv(fdSocketCommunication, tampon, MAX_BUFFER, 0);
  if (nbRecu > 0) {
    printf("%s\n", tampon);
    char *nom, *numero;
    nom=strtok(tampon, ", ");
    numero=strtok(NULL, ", ");
    int i=(atoi(numero)-1000000000)/10-1;//de string a int
    int j=(atoi(numero)-1000000000)%10-1;//de string a int
    //partie memoire partagee--------------------------
    key_t clef=ftok(".", 19);
    int shmid=shmget(clef, sizeof(toutesInfos), 0666|IPC_CREAT);//c'est quoi 0666?
    if(shmid==-1){
      printf("Allocation de la shm en echec!\n");
      printf("%s\n", strerror(errno));
      exit(0);
    }
    toutesInfos* places;
    p(monsem);// operation P semaphore
    places=shmat(shmid, NULL, 0);//s'attacher a un segment de memoire partage
    if(shmat(shmid, NULL, 0)==(void *)(-1)){
      printf("Erreur memoire partagee\n");
      exit(0);
    }
    //----------------------------------------------
    if(strcmp((*places).siege[i][j].nom, nom)==0 &&
              strcmp((*places).siege[i][j].numero, numero)==0){
      (*places).siege[i][j].reserve=0;
      strcpy((*places).siege[i][j].nom, "");
      strcpy((*places).siege[i][j].numero, "");
      afficherPlace(places);
      printf("Place (%d, %d) annulé\n", i+1, j+1);
      shmdt(places);
      v(monsem);// operation V semaphore
      strcpy(tampon, "ok");
      send(fdSocketCommunication, tampon, 100, 0);//envoyer aux clients
    }
    strcpy(tampon, "echoue");
    send(fdSocketCommunication, tampon, 100, 0);//envoyer aux clients
  }
}

void p(int semid){
  int rep;
  struct sembuf sb={0, -1, 0};
  //num sema, valeur a ajouter = -1, pas d'option
  rep=semop(semid, &sb, 1);
  //1 = un seul semaphore concerne
}

void v(int semid){
  int rep;
  struct sembuf sb={0, 1, 0};
  //num sema, valeur a ajouter = +1, pas d'option
  rep=semop(semid, &sb, 1);
}
