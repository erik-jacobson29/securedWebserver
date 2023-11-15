
const char * usage =
"                                                               \n"
"myhttpd:                                                       \n"
"                                                               \n"
"Simple server program that shows how to use socket calls       \n"
"in the server side.                                            \n"
"                                                               \n"
"To use it in one window type:                                  \n"
"                                                               \n"
"   daytime-server <port>                                       \n"
"                                                               \n"
"Where 1024 < port < 65536.                                     \n"
"                                                               \n"
"In another window type:                                       \n"
"                                                               \n"
"   telnet <host> <port>                                        \n"
"                                                               \n"
"where <host> is the name of the machine where daytime-server  \n"
"is running. <port> is the port number you used when you run   \n"
"daytime-server.                                               \n"
"                                                               \n"
"Then type your name and return. You will get a greeting and   \n"
"the time of the day.                                          \n"
"                                                               \n";


#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <cstring>
#include <iostream>
#include <signal.h>
#include <sys/wait.h>
#include <dirent.h>
#include <iostream>
#include <list>
#include <arpa/inet.h>
#include <dlfcn.h>
#include <link.h>

struct sockaddr_in clientIPAddress;

pthread_mutex_t mutex;
extern "C" void sigIntHandler(int sig) {
  if(sig == SIGCHLD) {
    pid_t pid = waitpid(-1,NULL,WNOHANG);
  }
}

int QueueLength = 5;

void processhttpRequest( int socket );
void createThreadForRequest(int clientsocket);
void forkServer(int serversocket, int clientsocket);
void poolOfThreads( int serverSocket, int clientSocket );
void* loopthread(int serverSocket, int ClientSocket);
void loadModule(char * path, char * args, int socket);
std::string getFilesInDir(const char* filepath);

typedef void (*httprun)(int ssock, char * query);

int flagType = 0;
int port = 0;
void portAndFlag( int argc, char ** argv);

//make headers and footers for our generated html files
std::string header = "<html> \n"
"<head>\n"
"<title> Directory Listing </title>\n"
"</head>\n"
"<body>\n"
"<hr>"
"<h1> Directory Listing </h1>\n";

std::string header2 = "<html> \n"
"<head>\n"
"<title> Directory Listing </title>\n"
"</head>\n"
"<body>\n"
"<hr>"
"<h1> Statistics </h1>\n";

std::string header3 = "<html> \n"
"<head>\n"
"<title> Directory Listing </title>\n"
"</head>\n"
"<body>\n"
"<hr>"
"<h1> Logs </h1>\n";

std::string footer = 
"<hr>\n"
"</body>\n"
"</html>\n";

std::string body = "";

clock_t start, end;
int num_requests = 0;
char * stimes;
char * etimes;
double cpu_time_used;

time_t stime,etime;

//min and max service times
double maxServiceTime = 0;
double minServiceTime = 10000000;

std::string logs; //global variable log list



int
main( int argc, char ** argv )
{
//Add some handling for the time
  stime;
  time(&stime);
  //stimes = ctime(&stime);
  struct sigaction sa;
  memset(&sa, 0,sizeof(sa));
  sa.sa_handler = &sigIntHandler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART;
  int e = sigaction(SIGCHLD, &sa, NULL);
  if(e) {
      perror("sigaction");
      exit(-1);
  }
  //call helper function to set the port and the flag type from the arguments
  portAndFlag(argc, argv);
  //port = atoi(argv[1]);
  //printf("PORT: %d\n", flagType);
  //port = atoi(argv[1]);
  // Set the IP address and port for this server
  struct sockaddr_in serverIPAddress; 
  memset( &serverIPAddress, 0, sizeof(serverIPAddress) );
  serverIPAddress.sin_family = AF_INET;
  serverIPAddress.sin_addr.s_addr = INADDR_ANY;
  serverIPAddress.sin_port = htons((u_short) port);
  
  // Allocate a socket
  int masterSocket =  socket(PF_INET, SOCK_STREAM, 0);
  if ( masterSocket < 0) {
    perror("socket");
    exit( -1 );
  }

  // Set socket options to reuse port. Otherwise we will
  // have to wait about 2 minutes before reusing the sae port number
  int optval = 1; 
  int err = setsockopt(masterSocket, SOL_SOCKET, SO_REUSEADDR, 
		       (char *) &optval, sizeof( int ) );
   
  // Bind the socket to the IP address and port
  int error = bind( masterSocket,
		    (struct sockaddr *)&serverIPAddress,
		    sizeof(serverIPAddress) );
  if ( error ) {
    perror("bind");
    exit( -1 );
  }
  
  // Put socket in listening mode and set the 
  // size of the queue of unprocessed connections
  error = listen( masterSocket, QueueLength);
  if ( error ) {
    perror("listen");
    exit( -1 );
  }

  while ( 1 ) {

    // Accept incoming connections
    int alen = sizeof( clientIPAddress );
    pthread_mutex_lock(&mutex);
    int slaveSocket = accept( masterSocket,
			      (struct sockaddr *)&clientIPAddress,
			      (socklen_t*)&alen);
    pthread_mutex_unlock(&mutex);

    if ( slaveSocket < 0 ) {
      perror( "accept" );
      exit( -1 );
    }

    if(flagType == 0) {
      processhttpRequest( slaveSocket );
    }
    if(flagType == 1) {
      //printf("here");
      forkServer(masterSocket,slaveSocket);
    }
    if(flagType == 2) {
      
      createThreadForRequest(slaveSocket);
    }
    if(flagType == 3) {
      poolOfThreads(masterSocket, slaveSocket);
    }
    //end = clock();
    // Close socket
    // shutdown(slaveSocket, SHUT_RDWR);
    // close( slaveSocket );
  }
  
}

void forkServer(int serverSocket, int clientSocket) {
  
    int ret = fork();
    if(ret == 0) { // then it is the child process
      processhttpRequest(clientSocket);
      exit(0);
    }
    close(clientSocket);

}

void createThreadForRequest(int clientSocket) {
  pthread_t thread;
  pthread_attr_t attr;
  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
  pthread_create(&thread, &attr, (void * (*)(void*))processhttpRequest, (void *) clientSocket);
}

void poolOfThreads( int serverSocket, int clientSocket ) {
  pthread_t threads[QueueLength];
  pthread_mutex_init(&mutex, NULL);
  for (int i=0; i<QueueLength; i++) {
    pthread_create(&threads[i], NULL, (void * (*)(void *))loopthread, (void *)serverSocket);
  }
  //pthread_join(threads[0], NULL);
  loopthread (serverSocket, clientSocket);
}

void *loopthread (int serverSocket, int clientSocket) {
  while( 1) {
    int alen = sizeof( clientIPAddress );
    pthread_mutex_lock(&mutex);
    int clientSocket = accept( serverSocket,
			      (struct sockaddr *)&clientIPAddress,
			      (socklen_t*)&alen);
    pthread_mutex_unlock(&mutex);
    processhttpRequest(clientSocket);
    close(clientSocket);
}

}


void portAndFlag(int argc, char ** argv) { //HELPER FUNCTION THAT GETS THE FLAG AND THE PORT
  
  if ( argc == 1 ) { //then we are running the iterative server with a random port
    
    port = 3290;
  }
  else if( argc == 2) { //then we need to check if the port or the flag is omitted
    
    if(strstr(argv[1], "-") != NULL) { //then the flag has been passed but not the port
      
      port = 3290;
      if(strcmp(argv[1], "-f") == 0 ) {
        //printf("-f\n");
        flagType = 1;
      }
      if(strcmp(argv[1], "-t") == 0 ) {
        //printf("-t\n");
        flagType = 2;
      }
      if(strcmp(argv[1], "-p") == 0 ) {
        //printf("-p\n");
        flagType = 3;
      }
    }
    else { //then the port has been passed but not the flag
      port = atoi(argv[1]); //
    }
  }
  else { //both the port and the flag type have been passed so set the port to argv2 and set the flag to argv[1]
    
    port = atoi(argv[2]);
    if(strcmp(argv[1], "-f") == 0 ) {
        //printf("-f\n");
        flagType = 1;
      }
      if(strcmp(argv[1], "-t") == 0 ) {
        //printf("-t\n");
        flagType = 2;
      }
      if(strcmp(argv[1], "-p") == 0 ) {
        //printf("-p\n");
        flagType = 3;
      }
  }
}

//this is going to be the helper function for browsing directories
std::string getFilesInDir(const char* filepath) {
  std::string head =  "./http-root-dir/htdocs";
  
  head += filepath;
  head += "/";
  std::list<std::string> files;
  DIR* dir = opendir(head.c_str());
  struct dirent *d;
  std::string body = "";
  while((d = readdir(dir)) != NULL) {
    //check if its .. or .
    if(strncmp(d->d_name, ".", 1) != 0) { 
      //add to a c++ list
      //std::string temp = head;
      //temp += d->d_name;
      files.push_back(std::string(d->d_name));
      //printf("Entry : %s\n", temp.c_str());
    }
  }
  files.sort(); //this will sort the files in alphabetical order
  
  std::list<std::string>::iterator f;
  for(auto const &i: files) {
    //now i want to create an <a href tag with the file extension in middle>
    std::string temp = "<li><a href=\"";
    temp += i;
    temp += "\">";
    temp+= i;
    temp+="</li>\n";

    body += temp;
  }
  printf("%s\n", body.c_str());
  closedir(dir);

  return body;
}

//helper function for loading modules
void loadModule(char * path, char * args, int socket) {
  void * lib = dlopen(path, RTLD_LAZY);
  httprun h = (httprun)dlsym(lib, "httprun");
  h(socket, args);
}


void
processhttpRequest( int fd )
{
  //lets start the time of the requst
  start = clock();
  // Buffer used to store the name received from the client
  const int MaxReq = 4096;
  char req[ MaxReq + 1 ];
  int reqLength = 0;
  int n;

  //increment number of requests
  num_requests++;

  //add same stuff as above to store the document
  const int maxDoc = 1024;
  char docName[ maxDoc + 1];
  int docLength = 0;

  // const int ctype = 100;
  // char docType[ctype + 1];
  // int clen = 0;

  // Currently character read
  unsigned char newChar;

  // Last character read
  unsigned char lastChar = 0;

  //
  // The client should send <name><cr><lf>
  // Read the name of the client character by character until a
  // <CR><LF> is found.
  //
  int numspace = 0;
  while ( reqLength < MaxReq &&
	  ( n = read( fd, &newChar, sizeof(newChar) ) ) > 0 ) {
    req[ reqLength ] = newChar;
    reqLength++;
    
    if (reqLength > 4 && req[reqLength-4] == '\r' && req[reqLength-3] == '\n' && req[reqLength-2] == '\r' && req[reqLength-1] == '\n'){
      break;
    }
    if(isspace(lastChar) != 0) { //increment the space counter all so we can get the name document
      numspace++;
    }
    //get the document requested
    if(numspace == 1) {
      //then the this is the start of the document requested
      docName[docLength] = newChar;
      docLength++;
    }
    //THIS CODE ABOVE GETS THE NAME OF THE DOC REQUESTED
    lastChar = newChar;
  }
  docName[docLength -1] = 0; //at this point we succesfully get the name of the doucment requested
  char * dtype = strchr(docName, '.'); //should succesfully get the datatype
  //printf("DTYPE:%s\n", dtype);
  //printf("DOCUMENT NAME: %s\n", docName);

  // Add null character at the end of the string
  req[ reqLength ] = 0;

  printf( "req=%s\n", req );
  //Now I am going to implement some HTTP AUTHENTICATION
    if(strstr(req, "Authorization") == NULL) { //then we need to authenticate
    const char * hdr12 = 
    "HTTP/1.1 401 Unauthorized\r\n" 
    "WWW-Authenticate: Basic realm=\"myhttpd-cs52\""; 
    const char * hdr3 = "\r\n\r\n";
  

    write(fd, hdr12, strlen(hdr12));
    write(fd, hdr3, strlen(hdr3));
    close(fd);
    return;
  } 
  //ABOVE CODE PROMPTS FOR HTTP AUTHENTICATION
  if(strstr(req, "Authorization: Basic ZXJpa2o6dGVzdHBhc3M=") == NULL) { //then there is no match
    const char * hdr12 = 
    "HTTP/1.1 401 Unauthorized\r\n" 
    "WWW-Authenticate: Basic realm=\"myhttpd-cs52\""; 
    const char * hdr3 = "\r\n\r\n";
    write(fd, hdr12, strlen(hdr12));
    write(fd, hdr3, strlen(hdr3));
    close(fd);
    return;
  }
  
  int isdex = 0; //boolean to see if its the index
  //Now what we want to do is add handling depending on what the get request is
  char filename[] = "./http-root-dir/htdocs"; //dir that we want to start in 
  if(strcmp(docName, "/") == 0) { //then we want to complete the file with the index
    char suffix[] = "/index.html";
    strcat(filename, suffix);
  }
  else if(strstr(docName, "..") != NULL) { //then the user is truing to go back
    close(fd);
    return;
  }
  //favicon.ico handling
  else if(strcmp(docName, "/favicon.ico") == 0) {
    num_requests --;
    close(fd);
    return;
  }
  //lets add cgi-bin handling
  
  else if (strstr(docName, "/cgi-bin") != NULL) {
    //then this is a 
    
    int len = strlen(filename);
    filename[len-7] = 0;
    strcat(filename, docName);
    //printf("filename: %s\n", filename);
    
    //now we have a usable filepath
    std::string temp = docName;
    if(strlen(docName) > 9) {
      temp = temp.substr(9);
    }
    
    //printf("temp: %s\n", temp.c_str());
    //check to see if temp has a ? in it
    std::string temp2 = "";
    char * question = strchr((char*) temp.c_str(), '?');
    char ** args = new char * [2];
    args[0] = (char*) malloc (strlen(filename));
    //initialize the array to 0
    for(int i =0 ; i < strlen(filename); i++) {
      args[0][i] = '\0';
    }
    
    if(question) {
      
      question ++;
      strcpy(args[0], question);
      //strcpy(args[0], temp2.substr(3).c_str());
    }
    args[1] = NULL;
    
    const char * hdr1 = 
    "HTTP/1.1 200 Document follows\r\n" 
    "Server: cs252 Lab5\r\n"; 
    write(fd, hdr1, strlen(hdr1));

    //first we need to check if this file is a .so file
    if(strstr(filename, ".so")) {
      
      //then we need to do load modeule
      if(strstr(filename, "?")) {
        strtok(filename, "?");
      }
      loadModule(filename,args[0],fd);
      
      close(fd);
      return;

      
    }

    int tmp = dup(1);
    dup2(fd,1);
    close(fd);
    int ret = fork();
    if(ret == 0) {
      //we are in the child process
      setenv("REQUEST_METHOD", "GET", 1);
      setenv("QUERY_STRING", args[0], 1);
      execvp(filename, args);
      exit(2);
    }
    dup2(tmp, 1);
    close(tmp);
    return;
  }
  else if(strcmp(docName, "/stats") == 0) {
    //do all the stats handling
  const char * hdr1 = 
  "HTTP/1.1 200 Document follows\r\n" 
  "Server: cs252 Lab5\r\n"; 
  const char * hdr2 = 
  "Content-type: ";
  const char * contentType = "text/html";
  const char * hdr3 = "\r\n\r\n";

  write(fd, hdr1, strlen(hdr1));
  write(fd, hdr2, strlen(hdr2));
  write(fd, contentType, strlen(contentType));
  write(fd, hdr3, strlen(hdr3));

  std::string middle = "Name of Student: Erik Jacobson <br>"
  "Server Time: ";
  
  //double cpu_time_used = ((double)  (end - start)) / CLOCKS_PER_SEC;
  time_t etime;
  time(&etime);
  
  std::string dtime = std::to_string(difftime(etime,stime));

  std::string time = std::string(dtime);
  std::string count = std::to_string(num_requests);
  std::string max = std::to_string(maxServiceTime);
  std::string min = std::to_string(minServiceTime);
  middle += time + "<br> Number of Requests: " + count + "<br> Min Service Time: " + min + "<br> Max Service Time: " + max + "<br>";
  std::string full = header2 + middle + footer;
  
  write(fd, full.c_str(), full.size());
  close(fd);
  return;
  }

  //now lets perform some handling for the logs page
  else if(strcmp(docName, "/logs") ==0 ) {
  const char * hdr1 = 
  "HTTP/1.1 200 Document follows\r\n" 
  "Server: cs252 Lab5\r\n"; 
  const char * hdr2 = 
  "Content-type: ";
  const char * contentType = "text/html";
  const char * hdr3 = "\r\n\r\n";

  write(fd, hdr1, strlen(hdr1));
  write(fd, hdr2, strlen(hdr2));
  write(fd, contentType, strlen(contentType));
  write(fd, hdr3, strlen(hdr3));
  
  //loop through the list of things in the log and add them to the middle of the string
  
  std::string full = header3 + logs + footer;
  write(fd, full.c_str(), full.size());
  close(fd);
  return;
  }
  
  
  //now we are going to handle a case where it is not the index file and the user actually requests a file
  else {
    strcat(filename, docName);
  }

  //THE ABOVE CODE WORKS FOR SIMPLE TEST
  
  //add handling for different types of files
  
  
  //add some c++ handling to get the contents of the index file
  
  
  FILE * f = fopen((const char *)filename, "r");
  fseek(f, 0, SEEK_END);
  size_t size = ftell(f);
  char * doc = new char[size];
  rewind(f);
  fread(doc,sizeof(char), size, f);
  fclose(f);
  
  //This Code Works really good and fast
  
  //now i want to add some handling to get the content type of the document
  printf("DTYPE%s\n", dtype);
  char ctype[100];
  if(dtype != NULL && strcmp(dtype, ".html") == 0) { //handling for html
    //make the data type equal to text/html
    strcpy(ctype,"text/html");
  }
  else if(dtype != NULL && strcmp(dtype, ".png") == 0) { //handling for pngs
    //make the data type equal to text/html
    strcpy(ctype,"image/png");
  }
  else if(dtype != NULL && strcmp(dtype, ".gif") == 0) { //handling for gifs
    //make the data type equal to text/html
    strcpy(ctype,"image/gif");
  }
  else if(dtype != NULL && strcmp(dtype, ".jpg") == 0) { //handling for jpgs
    //make the data type equal to text/html
    strcpy(ctype,"image/jpg");
  }
  else if(dtype != NULL && strcmp(dtype, ".svg") == 0) { //handling for svgs
    //make the data type equal to text/html
    strcpy(ctype,"image/svg+xml");
  }
  else if(dtype != NULL && strcmp(dtype, ".txt") == 0) { //handling for plain text
    //make the data type equal to text/html
    strcpy(ctype,"plain/text");
  }
  //ABOVE IS HANDLING FOR ALL THE TEXT FORMATS
  int bl = 0;
  //NOW I want to check and make sure that and see if it is a directory
  std::string head =  filename;
  //printf("HEAD: %s\n", head.c_str());
  if(opendir(head.c_str()) != NULL) { //then it is a directory
    body = getFilesInDir(docName);
    strcpy(ctype, "text/html");
    bl = 1;
    //call a helper function to browse the directories
  }
  
  
  
  //send reply
  const char * hdr1 = 
  "HTTP/1.1 200 Document follows\r\n" 
  "Server: cs252 Lab5\r\n"; 
  const char * hdr2 = 
  "Content-type: ";
  const char * contentType = ctype;
  const char * hdr3 = "\r\n\r\n";
  const char * document = doc;

  write(fd, hdr1, strlen(hdr1));
  write(fd, hdr2, strlen(hdr2));
  write(fd, contentType, strlen(contentType));
  write(fd, hdr3, strlen(hdr3));
  if(bl == 0) {
    write(fd, document, size); //regular write to the file
  }
  
  else {
    //printf("HERE\n");
    std::string full = header + body + footer;
    write(fd, full.c_str(), full.size()); //create a html file and display that instead
  }

  // Close socket
  end = clock();
  cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
  if(cpu_time_used > maxServiceTime) {
      maxServiceTime = cpu_time_used;
  }
  if(cpu_time_used < minServiceTime) {
    minServiceTime = cpu_time_used;
  }
  logs += std::string(inet_ntoa(clientIPAddress.sin_addr)) + "&nbsp;&nbsp" + docName + "<br>\n";
  close( fd );
  free(doc);
  
}
