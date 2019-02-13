#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/sem.h>

#define PORT 6000
#define MAX_BUFFER 100

char tampon[MAX_BUFFER];// pour send() et recv()
int place[10][10];// place du concert de forme 0 et 1

void preparationClient();// configuration du client
void lireMessage(char tampon[]);// entrer un message
void afficherPlace();// afficher les places
void reserver(int fdSocket);// reserver une place
void annuler(int fdSocket);// annuler une reservation

int main(int argc , char const *argv[]) {
  preparationClient();
}

void preparationClient(){
  int fdSocket;
  int nbRecu;
  struct sockaddr_in coordonneesServeur;
  int longueurAdresse;

  fdSocket = socket(AF_INET, SOCK_STREAM, 0);
  if (fdSocket < 0) {
      printf("socket incorrecte\n");
      exit(EXIT_FAILURE);
  }
  else{
      printf("C'est un client\n");
  }
  // On prépare les coordonnées du serveur
  longueurAdresse = sizeof(struct sockaddr_in);
  memset(&coordonneesServeur, 0x00, longueurAdresse);
  coordonneesServeur.sin_family = PF_INET;
  // adresse du serveur
  inet_aton("127.0.0.1", &coordonneesServeur.sin_addr);
  // de l'adresse IP a network byte
  // toutes les interfaces locales disponibles
  coordonneesServeur.sin_port = htons(PORT);
  if (connect(fdSocket, (struct sockaddr *) &coordonneesServeur,
          sizeof(coordonneesServeur)) == -1) {
      printf("Connexion impossible\n");
      exit(EXIT_FAILURE);
  }
  else{
       printf("Connexion ok\n");
  }

  printf("Bienvenue dans notre logiciel!\n");
  while(1){
    printf("Voulez-vous réserver(1) une place ou annuler(0) une réservation?\n");
    printf("(quitter(9) ce programme)\n");
    lireMessage(tampon);
    if(strcmp(tampon, "1")==0){//pour reserver
      reserver(fdSocket);
    }
    else if(strcmp(tampon, "0")==0){// pour annuler
      annuler(fdSocket);
    }
    else if(strcmp(tampon, "9")==0){// pour quitter
      close(fdSocket);
      printf("Merci d'utiliser notre programme\n");
      exit(0);
    }
  }
}

void lireMessage(char tampon[]){
    printf("Entrer: ");
    fgets(tampon, MAX_BUFFER, stdin);
    strtok(tampon, "\n");
}

void afficherPlace(){
  int i, j;
  printf("    1 2 3 4 5 6 7 8 9 10\n");
  for(i=0;i<10;i++){
    if(i==9){
      printf("10| ");
    } else {
      printf(" %d| ", i+1);
    }
    for(j=0;j<10;j++){
      printf("%d ", place[i][j]);
    }
    printf("\n");//saut de ligne
  }
}

void reserver(int fdSocket){
  send(fdSocket, tampon, 100, 0);//envoie "1" au serveur pour reserever
  printf("(Attends la reponse...)\n");
  printf("Entrer votre nom:\n");
  lireMessage(tampon);
  send(fdSocket, tampon, 100, 0);//envoie nom au serveur pour reserever
  // on attend la réponse du serveur
  int nbRecu = recv(fdSocket, tampon, MAX_BUFFER, 0);
  if(nbRecu>0){
    printf("Réservez-vous une place: (0 place libre, 1 place occupé)\n");
    for(int i=0;i<10;i++){
      for(int j=0;j<10;j++){
        place[i][j]=tampon[i*10+j];//remplir les donnees a la place[][]
      }
    }
    afficherPlace();//afficher les donnees
    printf("Quelle place voulez-vous réserver?(exemple: a, b)\n");
    int occupe=1;
    while(occupe==1){//on commence la premiere fois
      //ou on repete quand une place est occupee
      lireMessage(tampon);
      //cet partie est pour verifier les coordonnees entrees
      char copie[5];
      strcpy(copie, tampon);//on fait une copie de tampon a
      char *ligne, *colonne;
      ligne=strtok(copie, ", ");//couper par "," et " "
      colonne=strtok(NULL, ", ");//pour couper une meme chaine, faut entrer null
      int i=atoi(ligne), j=atoi(colonne);//pour transformer de string a int
      if(place[i-1][j-1]==1){
        printf("Cet place est occupée! Veuillez entrer à nouveau:\n");
      }
      else{
        occupe=0;//on sort de la boucle
        printf("Réserver en cours...\n");
        send(fdSocket, tampon, 100, 0);// on envoie les coordonnees de place
        printf("(Attends la reponse...)\n");
        int nbRecu = recv(fdSocket, tampon, MAX_BUFFER, 0);
        if (nbRecu > 0) {
          if(strcmp(tampon, "occupe")==0){
            printf("Cet place est occupée! Veuillez entrer à nouveau:\n");
          }else{
            printf("La place (%d, %d) bien réservée!\n", i, j);
            printf("Le numéro de dossier est %s\n", tampon);
          }
        }
      }
    }
  }
}

void annuler(int fdSocket){
  strcpy(tampon, "0");
  send(fdSocket, tampon, 100, 0);//envoie "0" au serveur pour reserever
  printf("(Attends la reponse...)\n");
  // on attend la réponse du serveur
  int nbRecu = recv(fdSocket, tampon, MAX_BUFFER, 0);
  if(nbRecu>0 && strcmp(tampon, "ok")==0){
    printf("Entrer votre nom et numéro de dossier: (exemple: nom, numero)\n");
    strcpy(tampon,"");
    lireMessage(tampon);
    send(fdSocket, tampon, 100, 0);//envoyer nom et numero au serveur
    printf("(Attends la reponse...)\n");
  }
  nbRecu = recv(fdSocket, tampon, MAX_BUFFER, 0);
  if(nbRecu>0){
    if(strcmp(tampon, "ok")==0){
      printf("Place annulée\n");
    }
    else if(strcmp(tampon, "echoue")==0){
      printf("Echoué\n");
    }
  }
}
