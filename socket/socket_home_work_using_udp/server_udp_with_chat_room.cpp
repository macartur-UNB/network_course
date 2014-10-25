#include <stdio.h>     // printf
#include <stdlib.h>    // exit
#include <signal.h>    // signal
#include <sys/socket.h>// socket
#include <unistd.h>    // socket
#include <fcntl.h>     // fcntl
#include <iostream>    // cin, cout
#include <arpa/inet.h> // String ip, inet_ntoa(from.sin_addr)
#include <vector> // vector
#include <time.h> // time(), difftime()
#include <string.h> // memcpy
#include <sstream> // ostream

#define TIMEOUT 2

typedef struct connection
{
  sockaddr_in addr;
  int descriptor;
  time_t last_time;
  char* nome;
  int color;
  int chatid;
}_connection;

int ia, ib, ic, id, ip;

using namespace std;

vector<connection> clients;
vector<char*> chatlist;
int chatid = -1;

sockaddr_in address_mine;
sockaddr_in address_send;

pthread_t client_p;
int isServer;
int handle;

char* color[14] = {
  (char*)"\x1B[1;31m",
  (char*)"\x1B[1;32m",
  (char*)"\x1B[1;33m",
  (char*)"\x1B[1;34m",
  (char*)"\x1B[1;35m",
  (char*)"\x1B[1;36m",
  (char*)"\x1B[1;37m",
  (char*)"\x1B[0;31m",
  (char*)"\x1B[0;32m",
  (char*)"\x1B[0;33m",
  (char*)"\x1B[0;34m",
  (char*)"\x1B[0;35m",
  (char*)"\x1B[0;36m",
  (char*)"\x1B[0;37m"
};
int next_color = 0;
int max_color = 14;

bool silent_ping = true;

bool initSocket() {
    handle = socket( AF_INET, SOCK_DGRAM, IPPROTO_UDP );

    int port = 0;
    if(isServer == 1)
        port = ip;

    if(handle <= 0) {
        printf("Erro ao criar socket\n");
        return false;
    }

    address_mine.sin_family = AF_INET;
    address_mine.sin_addr.s_addr = INADDR_ANY;
    address_mine.sin_port = htons( (unsigned short) port);
    if( bind( handle, (const sockaddr*) &address_mine, sizeof(sockaddr_in) ) < 0 ) {
        printf("erro ao registrar(bind) socket, port %d\n",port);
        return false;
    }

    #if PLATAFORM == PLATAFORM_MAC || PLATAFORM == PLATAFORM_UNIX
        int nonBlocking = 1;
        if(fcntl(handle, F_SETFL, O_NONBLOCK, nonBlocking ) == -1)
        {
            printf("Erro ao setar não blocante\n");
            return false;
        }
    #elif PLATAFORM == PLATAFORM_WINDOWS
        DWORD nonBlocking = 0;
        if(ioctlsocket(handle, FIONBIO, &nonBlocking) != 0)
        {
            printf("Erro ao setar não blocante\n");
            return false;
        }
    #endif // PLATAFORM

    return true;
}

bool sendSocket(unsigned int a, unsigned int b, unsigned int c, unsigned int d, unsigned short port, char* packet_data, bool silent=false)
{
    unsigned int destination_address = (a << 24 ) | (b << 16) | (c << 8) | d;
    unsigned short destination_port = port;

    sockaddr_in address_send;
    address_send.sin_family = AF_INET;
    address_send.sin_addr.s_addr = htonl(destination_address);
    address_send.sin_port = htons(destination_port);

    int packet_size = sizeof(char) * strlen(packet_data)+1;
    if(!silent)
      printf("\t-> Enviando %d bytes\n", packet_size);
    int sent_bytes = sendto( handle, (const char*)packet_data, packet_size,
                             0, (sockaddr*)&address_send, sizeof(sockaddr_in) );

    if ( sent_bytes != packet_size ) {
        printf("Erro ao enviar packet, retorno = %d\n", sent_bytes);
        return false;
    }
    return true;
}

bool isNewClient(sockaddr_in& from, int* id) {
  for(unsigned int i = 0; i < clients.size(); i++)
  {
    if( from.sin_addr.s_addr == clients[i].addr.sin_addr.s_addr &&
        from.sin_port == clients[i].addr.sin_port) {
      time(&clients[i].last_time);
      *id = i;
      return false;
    }
  }

  return true;
}

void checkTimeouts()
{
  //Aproximado, evita chamar time() toda hora uma vez que ele só acha segundos com o diff
  time_t start;
  time(&start);
  for(unsigned int i = 0; i < clients.size(); i++)
  {
    if( difftime(start, clients[i].last_time) > TIMEOUT ) {
      printf("Client timeout: %s\n", inet_ntoa(clients[i].addr.sin_addr));
      clients.erase(clients.begin() + i--);
    } 
  }
}

void propagateToClients(char* data, sockaddr_in from, unsigned int from_port, int id) {
  for(unsigned int i = 0; i < clients.size(); i++)
  {
    // if( from.sin_addr.s_addr == clients[i].addr.sin_addr.s_addr &&
    //     from.sin_port == clients[i].addr.sin_port) {
    //   continue;
    // }

    if(clients[i].chatid != clients[id].chatid)
      continue;

    char msg[1024];
  // printf("%c[1;31mHello, world!\n", 27); // red
    sprintf(msg, "%s%s(%s:%d)[%s]:\x1B[0m %s", color[clients[id].color], clients[id].nome, inet_ntoa(from.sin_addr), from_port, chatlist[clients[id].chatid], data);

    int packet_size = strlen(msg)*sizeof(char)+1;

    sendto( clients[i].descriptor, (const char*)msg, packet_size,
             0, (sockaddr*)&clients[i].addr, sizeof(sockaddr_in) );
  }

}

void listenSockets()
{
    cout << "Escutando... \n";
    printf("Port: %d\n", htons( address_mine.sin_port) );

    time_t start;
    time(&start);
    while(true)
    {
      //Verifica se timeout a cada 1s
      time_t stop;
      time(&stop);
      if(difftime(stop, start) >= 1) {
        start = stop;
        checkTimeouts();
      }

      char packet_data[256];
      unsigned int maximum_packet_size = sizeof(packet_data);
      sockaddr_in from;
      socklen_t fromLength = sizeof(from);

      int received_bytes = recvfrom( handle, (char*)packet_data, maximum_packet_size,
        0, (sockaddr*)&from, &fromLength);
      if(received_bytes <= 0) {
        continue;
      }

      int id = -1;

      //Verifica se novo client e atualiza tempo se existente
      if(isNewClient(from, &id)) {
        
        printf("Novo cliente conectado: %s(%s)\n", packet_data, inet_ntoa(from.sin_addr) );
//Envia chatlist para o novo cliente
        for(int i = 0; i < (int)chatlist.size(); i++) {
          int packet_size = strlen(chatlist[i])*sizeof(char)+1;
          sendto( handle, (const char*)chatlist[i], packet_size,
             0, (sockaddr*)&from, sizeof(sockaddr_in) );
        }
//Envia \0 para indicar o fim
        sendto( handle, (const char*)"\0", 1,
             0, (sockaddr*)&from, sizeof(sockaddr_in) );
        
        connection conn;
        conn.addr = from;
        conn.descriptor = handle;
        conn.color = next_color++;
        if(next_color >= max_color) 
          next_color = 0;

        time(&conn.last_time);
        //Registra nome, primeira mensagem
        conn.nome = (char*)malloc(strlen(packet_data)+1);
        strcpy(conn.nome, packet_data);
        clients.push_back(conn);
        continue;
      }

      //Verifica se ping
      if(packet_data[0] == '\\') {
        clients[id].chatid = (packet_data[1]-'0');
        //cout << "recebido: " << (clients[id].chatid-'0') << endl;
        continue;
      }

      unsigned int from_port = ntohs(from.sin_port);

      if(!silent_ping){
        printf("Recebido %d bytes de %s:%d\n", received_bytes, inet_ntoa(from.sin_addr), from_port);
      }
      printf("%s(%s:%d)[%s]: %s\n", clients[id].nome, inet_ntoa(from.sin_addr), from_port, chatlist[clients[id].chatid], packet_data);

      propagateToClients((char*)packet_data, from, from_port, id);
  }
}

void* clientPingThread(void*)
{
  ostringstream oss;
  oss << "\\" << chatid;
  sendSocket(ia,ib,ic,id,ip, (char*)oss.str().c_str(), silent_ping);

  time_t start;
  time(&start);
  while(true)
  {
    time_t stop;
    time(&stop);
    if(difftime(stop, start) >= 1) {
      start = stop;
      ostringstream os;
      os << "\\" << chatid;
      sendSocket(ia,ib,ic,id,ip, (char*)os.str().c_str(), silent_ping);
    }

    //Recebe mensagens do servidor
    unsigned char packet_data[256];
    unsigned int maximum_packet_size = sizeof(packet_data);
    sockaddr_in from;
    socklen_t fromLength = sizeof(from);

    int received_bytes = recvfrom( handle, (char*)packet_data, maximum_packet_size,
      0, (sockaddr*)&from, &fromLength);
    if(received_bytes <= 0) {
      continue;
    }else {
      printf("\33[2K\r");
      printf("%s\n", packet_data);
    }

  }

  return (void*)0;
}

void clientLoop()
{
  size_t nbytes = 256;
  char* msg_data = (char*)malloc(256+1);

//Envia nome ao servidor
  printf("Digite seu nome: ");
  getline(&msg_data, &nbytes, stdin); //Remove \n
  getline(&msg_data, &nbytes, stdin);
  msg_data[strlen(msg_data)-1] = '\0';
  sendSocket(ia, ib, ic, id, ip, (char*)msg_data, silent_ping);
//Recebe lista de chats

  while(strcmp(msg_data, "\0") != 0) {
    int rbytes = recvfrom( handle, (char*)msg_data, nbytes, 
    0, NULL, NULL);    

    if(strcmp(msg_data, "\0") == 0)
      break;

    if(rbytes > 0) {
      char* nome = NULL;
      nome = (char*)malloc(strlen(msg_data)+1);
      strcpy(nome, msg_data);
      chatlist.push_back(nome);
    }
  }
//List
  printf("Escolha a sala: \n");
  for(int i = 0; i < (int)chatlist.size(); i++) {
    printf("%d - %s\n", i, chatlist[i] );
  }

  while(chatid == -1) {
    char op;
    op = getc(stdin);
    chatid = op - '0';
  }

  cout << "Escolha: " << chatlist[chatid] << endl;

  pthread_create(&client_p, NULL, &clientPingThread, NULL);

  usleep(100);

  time_t start;
  time(&start);
  while(true)
  {
    printf("%s: ", chatlist[chatid]);
    getline(&msg_data, &nbytes, stdin);
    if(msg_data[0] == '\n') 
      continue;
    msg_data[strlen(msg_data)-1] = '\0';
    sendSocket(ia, ib, ic, id, ip, (char*)msg_data, silent_ping);
  }
}

void cancel(int)
{
  if(isServer == false) {
    pthread_cancel(client_p);
    pthread_join(client_p, NULL);
  }
  close(handle);
  exit(0);
}
int main(int argc, char** args) {

  signal(SIGINT, cancel);
  cout << "1 server, 0 client: ";

  cin >> isServer;

  cout << "Escolhido: " << isServer << endl;

  if(argc > 5) {
    ia = atoi(args[1]);
    ib = atoi(args[2]);
    ic = atoi(args[3]);
    id = atoi(args[4]);
    ip = atoi(args[5]);
  } else {
    printf("Uso: ./program 127 0 0 0 3000\n");
    ia = 127;ib = 0;ic = 0;id = 1;
    ip = 3000;
    char* nome = NULL;
    nome = (char*)malloc(7+1);
    strcat(nome, "default");
    chatlist.push_back(nome);
  }
  if(argc > 6) {
    for(int i = 6; i < argc; i++) {
      char* nome = NULL;
      nome = (char*)malloc(strlen(args[i])+1);
      strcpy(nome, args[i]);
      chatlist.push_back(nome);
    }
  }

  for(int i = 0; i < (int)chatlist.size();i++)
    cout << chatlist[i] << endl;

  initSocket();

  if(isServer == 1) {
    listenSockets();
  } else if(isServer == 0) {
    clientLoop();
  }

  close(handle);
  return 0;
}
