#include <bits/stdc++.h>
#include <chrono>
using namespace std;
#include "tcp.h"
#include "stats.h"
#include "thread.h"
#include "system.h"
#include "filesys.h"

#define PBSTR "||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||"
#define PBWIDTH 60

void WakeUpHandler(int arg){
    TCPostOffice* tcp = (TCPostOffice*) arg;
    tcp->waitSem->V();
    tcp->stopSem->V();
}

int CutString(string in, string* out){
    ASSERT(out!=NULL);

    int pieces = (in.size()/MaxMailSize) + (in.size()%MaxMailSize==0?0:1);
    int begin = 0;
    for(int i=0; i<pieces-1; i++){
        out[i] = in.substr(begin,MaxMailSize);
        begin+=MaxMailSize;
    }
    out[pieces-1] = in.substr(begin);
    return pieces;
}

/* Progress bar function from user razz on StackOverflow
 * Posted on March 30, 2016 at 17:33
 * https://stackoverflow.com/questions/14539867/how-to-display-a-progress-indicator-in-pure-c-c-cout-printf
 */
void printProgress(double percentage) {
    int val = (int) (percentage * 100);
    int lpad = (int) (percentage * PBWIDTH);
    int rpad = PBWIDTH - lpad;
    printf("\r%3d%% [%.*s%*s]", val, lpad, PBSTR, rpad, "");
    fflush(stdout);
}

/***************** Message Constructors *****************/

//----------------------------------------------------------------------
// Message::Message
// 	Create a Message object to be used by the TCPostoffice to send and 
//  receive mail. Allocates the 2 headers and the data.
//
//	"dataSize" -- the size of the payload in 'data'
//  "msgType" -- the enum type to identify the mailboxe to send the mail 
//----------------------------------------------------------------------
Message::Message(int dataSize, PktType msgType){
    this->pktHdr = new PacketHeader;
    this->mailHdr = new MailHeader;
    this->data = new char[dataSize];
    for(int j=0; j<dataSize; j++) this->data[j] = '\0';         // Clear the data in case the pointer is a duplicate (yes, it happened)
    this->type = msgType;
    this->mailHdr->length = dataSize;

    this->mailHdr->to = type;
    this->mailHdr->from = type;
}

//----------------------------------------------------------------------
// Message::~Message
// 	Delete the Message object, deallocate everything we created.
//----------------------------------------------------------------------
Message::~Message(){
    delete this->pktHdr;
    delete this->mailHdr;
    delete[] this->data;
}

/***************** TCPostOffice Constructors *****************/

//----------------------------------------------------------------------
// TCPostOffice::TCPostOffice
// 	Creation of the reliable post office, giving access to the 
//  methods to send and receive data.
//----------------------------------------------------------------------
TCPostOffice::TCPostOffice(){
    id = postOffice->GetNetAddr();
    stopSem = new Semaphore("StopSemaphore",0);
    waitSem = new Semaphore("TCPostOfficeWait",0);
    connectedID = -1;
    stoppingConnection = false;
}

//----------------------------------------------------------------------
// TCPostOffice::~TCPostOffice
// 	Delete the TCPostOffice object, de allocate the structures used. 
//----------------------------------------------------------------------
TCPostOffice::~TCPostOffice(){
    delete stopSem;
    delete waitSem;
}


/***************** Public functions for the user *****************/

//----------------------------------------------------------------------
// TCPostOffice::SyncWithMachine
// 	Send a synchronization request to a given machine. Will wait for one 
//  after successfully sending it. Returns the id of the machine it 
//  connected to if it worked, returns -1 if it didn't.
//
//	Note : If the Machine we are trying to connect with is not on the 
//  network, it will crash.
//
//	"targetMachine" -- the id of the machine to connect
//----------------------------------------------------------------------
int TCPostOffice::SyncWithMachine(int targetMachine){
    if(connectedID!=-1) return -1;
    /* Initializing the out going messages */
    Message * outSync = new Message(2,SYNC);
    Message * outAck = new Message(2,ACK);
    
    outSync->pktHdr->from = id;
    outSync->pktHdr->to = targetMachine;
    outAck->pktHdr->from = id;
    outAck->pktHdr->to = targetMachine;
    
    /* Initializing the incoming headers */
    Message * inSync = new Message(2,SYNC);
    Message * inAck = new Message(2,ACK);

    /* Initializing the tokens */
    outSync->data[0] = (char) 64 + id; outSync->data[1] = '\0';
    inSync->mailHdr->length = strlen(outSync->data)+1;
    inAck->mailHdr->length = strlen(outSync->data)+1;
    outSync->mailHdr->length = strlen(outSync->data)+1;
    outAck->mailHdr->length = strlen(outSync->data)+1;

    /* Sending the SYNC request and waiting for ACK */
    if(SendWait(outSync,inAck,MAXCONNECTIONREEMISSIONS,"SynchSend")){
        /* Waiting for the SYNC back */
        if(!ReceiveWait(inSync,outAck,"SynchReceive")){
            DEBUG('n',"Received an end connection, stopping synch.\n");
        }
        connectedID = targetMachine;        
    }
    if(connectedID==-1)
        DEBUG('n',"Could not synchronize to machine %d.\n",targetMachine);

    delete outSync;
    delete outAck; 
    delete inSync; 
    delete inAck; 

    return connectedID;
} 

//----------------------------------------------------------------------
// TCPostOffice::OpenConnection
// 	Listens for synchronization requests and sends one back if it received 
//  one. Will return the id of the machine it connected to, returns -1 if 
//  a problem arose hile connecting.
//
//	Note : When calling this function, the program will be blocked until
//  it receives a SYNC requests.
//----------------------------------------------------------------------
int TCPostOffice::OpenConnection(){
    if(connectedID!=-1) return -1;

    Message *outSync = new Message(2,SYNC);
    outSync->pktHdr->from = id;

    Message *outAck = new Message(2,ACK);
    outAck->pktHdr->from = id;

    Message *inSync = new Message(2,SYNC);

    outSync->data = new char[2];
    outSync->data[0] = (char) 64 + id; outSync->data[1] = '\0';
    
    //Receive SYNC
    postOffice->Receive(SYNC,inSync->pktHdr,inSync->mailHdr,inSync->data);

    outAck->data[0] = inSync->data[0]+1; outAck->data[1] = '\0';
    outAck->pktHdr->to = inSync->pktHdr->from;
    outSync->pktHdr->to = inSync->pktHdr->from;

    postOffice->Send(*outAck->pktHdr,*outAck->mailHdr,outAck->data);
    CatchWithAnswer(SYNC,outAck);
    inSync->type = ACK;
    if(SendWait(outSync,inSync,MAXCONNECTIONREEMISSIONS,"SendSync"))
        connectedID = outSync->pktHdr->to;

    delete inSync;
    delete outSync;
    delete outAck;
    
    return connectedID;
}

//----------------------------------------------------------------------
// TCPostOffice::EndConnection
// 	End the current connection this post office has. Returns the id of 
//  the machine it was connected with if it diconnected successfully.
//  Returns -1 if we could not stop the connection (or if no machine is 
//  connected).
//----------------------------------------------------------------------
int TCPostOffice::EndConnection(){
    if(connectedID==-1) return -1;

    Message *outEnd = new Message(2,STOP);
    outEnd->pktHdr->from = id;
    outEnd->pktHdr->to = connectedID;
    outEnd->data[0] = (char) 64 + id; outEnd->data[1] = '\0'; 
    
    Message *inAck = new Message(2,ACK);    

    /* Sending the STOP request and waiting for ACK */
    int retVal = -1;
    stoppingConnection = false;
    if(SendWait(outEnd,inAck,MAXCONNECTIONREEMISSIONS,"SendStop")){
        DEBUG('n',"Stoped connection with machine %d successfully.\n",connectedID);
        retVal = connectedID;
    }

    if(retVal==-1) DEBUG('n',"Something went wrong, could not end connection with machine %d.\n", connectedID);

    if(retVal==connectedID) connectedID = -1;

    delete inAck;
    delete outEnd;

    return retVal;
}

//----------------------------------------------------------------------
// TCPostOffice::ListenForEndConnection
//  Listens for a request to stop the connection from the connected machine. 
//  Sends back an acknowledgment and sets the stopping condition to true 
//  when received.
// 
//----------------------------------------------------------------------
int TCPostOffice::ListenForEndConnection(){
    Message * in = new Message(2,STOP);  
    Message *out = new Message(2,ACK);
    out->pktHdr->from = id;

    postOffice->Receive(in->type,in->pktHdr,in->mailHdr,in->data);

    out->pktHdr->to = in->pktHdr->from;
    out->data[0] = in->data[0]+1; out->data[1] = '\0';

    postOffice->Send(*out->pktHdr,*out->mailHdr,out->data);
    CatchWithAnswer(in->type,out);

    stoppingConnection = true;
    stopSem->V();

    delete in;
    delete out;
    return true;
}

//----------------------------------------------------------------------
// TCPostOffice::
// 	Send a message of max size MaxMailSize to the currently connected machine.
//  Return value indicates wether the message was sent successfully or not.
//
//	Note : This call does not block, if we fail to receive acknowledment
//  for the messages we send, we will give up and return false.
//
//	"data" -- char string to send (size<=MaxMailSize)
//----------------------------------------------------------------------
bool TCPostOffice::SendMessage(const char *data){   
    if(connectedID==-1) return false;
    Message *out = new Message(strlen(data),MSG);

    SetMessageToFrom(out);
    bcopy(data,out->data,strlen(data));

    Message *in = new Message(2,ACK);
    bool value = SendWait(out,in,MAXREEMISSIONS,"SendFixedMessage"); 

    delete out;
    delete in;
    if(!value){
        if(stoppingConnection){
            DEBUG('n',"Connection stopping, we stopped sending.\n");
            stoppingConnection = false;
        }
        else
            DEBUG('n',"Machine %d didn't received acknowledgment from machine %d.\n",id,connectedID);
    }
    return value;
} 

//----------------------------------------------------------------------
// TCPostOffice::ReceiveMessage
//  Waits for a message to be sent from 
//  the connected machine. Returns back an acknowledgment
//
//
//	"data" -- pointer of receiving data
//----------------------------------------------------------------------
bool TCPostOffice::ReceiveMessage(char *data){
    if(connectedID==-1) return false;

    Message * in = new Message(MaxMailSize,MSG);

    Message *out = new Message(2,ACK);
    SetMessageToFrom(out);

    bool ret = true;
    if(!ReceiveWait(in,out,"ReceiveMessage")){
        DEBUG('n',"Connection stopping, we stopped waiting for a message.\n");
        stoppingConnection = false;
        ret = false;
    }
    sprintf(data,"%s",in->data);
    delete in;
    delete out;
    return ret;
}

//----------------------------------------------------------------------
// TCPostOffice::
// 	Send a variable size message to the currently connected machine. Will first
//  send the number of packages to expect using the STRING_INFO mailboxe,
//  then send the data itself.
//
//  "data" -- char string to send
//----------------------------------------------------------------------
bool TCPostOffice::SendString(char *data){
    if(connectedID==-1) return false;

    string msg (data);
    int nbPieces = (msg.size()/MaxMailSize) + (msg.size()%MaxMailSize==0?0:1);
    string* msgList = new string[nbPieces];
    CutString(msg, msgList);

    Message* info = new Message(MaxMailSize,STRING_INFO);
    strcpy(info->data,to_string(nbPieces).c_str());
    SetMessageToFrom(info);
    
    Message* infoAck = new Message(2,ACK);
    bool value = SendWait(info,infoAck,MAXREEMISSIONS,"SendStringInfo"); 
    delete info;
    delete infoAck;
    if(!value) {
        if(stoppingConnection){
            DEBUG('n',"Connection stopping, we don't send the string.\n");
            stoppingConnection = false;
        }
        return false;
    }
    for(int i=0; i<nbPieces; i++){
        if(!SendMessage(msgList[i].c_str())) return false;
        interrupt->Schedule(WakeUpHandler,(int)this,MAXCATCH*TEMPO,NetworkSendInt);
        stopSem->P();
        waitSem->P();
    }
    return true;
}

//----------------------------------------------------------------------
// TCPostOffice::ReceiveString
//  Waits to receive a string from the connected machine.
//  Sends back an acknowledgement and returns true if the string was received. 
//	
//
//	"data" -- pointer to received data 
//----------------------------------------------------------------------
bool TCPostOffice::ReceiveString(char *data){
    if(connectedID==-1) return false;

    Message * in = new Message(MaxMailSize,STRING_INFO);
    Message *out = new Message(2,ACK);
    SetMessageToFrom(out);
    if(!ReceiveWait(in,out,"ReceiveStringInfo")){
        DEBUG('n',"Connection stopping, we stop receiving the string.\n");
        delete in;
        delete out;
        return false;
    }

    int size = atoi(in->data);

    delete in;
    delete out;
    
    string result = "";
    for(int i=0; i<size; i++){
        char* piece = new char[MaxMailSize];
        for(unsigned int j=0; j<MaxMailSize; j++) piece[j] = '\0';
        bool status = ReceiveMessage(piece);
        if(!status) return false;
        result.append(piece);
        delete[] piece;
    }
    DEBUG('n',"Received the following string: %s \n", result.c_str());
    sprintf(data,"%s",result.c_str());
    
    return true;
}

//---------------------------------------------------------------------
// TCPostOffice::SendFile
// 	Sends a file to the currectly connected machine. Will first send the 
//  number of packedge to be received, the the data itself. Returns a 
//  boolean value to show if the file was sent successfully.
//
//	"filename" -- the name/path of the file to send
//----------------------------------------------------------------------
bool TCPostOffice::SendFile(char* filename){
    if(connectedID==-1) return false;

    OpenFile *f = fileSystem->Open(filename);
    if(f==NULL) return false;

    int nbPieces = (f->Length()/MaxMailSize) + (f->Length()%MaxMailSize==0?0:1);

    Message* info = new Message(MaxMailSize,FILE_INFO);
    strcpy(info->data,to_string(nbPieces).c_str());
    SetMessageToFrom(info);

    Message* infoAck = new Message(2,ACK);
    bool value = SendWait(info,infoAck,MAXREEMISSIONS,"SendFileInfo"); 
    delete info;
    delete infoAck;
    if(!value) {
        if(stoppingConnection){
            DEBUG('n',"Connection stopping, we don't send the string.\n");
            stoppingConnection = false;
        }
        delete f;
        return false;
    }
    printProgress(0);
    std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
    for(int i=0; i<nbPieces; i++){
        char* buffer = new char[MaxMailSize+1];
        for(unsigned int j=0; j<MaxMailSize+1; j++) buffer[j] = '\0';
        f->Read(buffer,MaxMailSize);
        buffer[MaxMailSize] = '\0';
        if(!SendMessage(buffer)){
            delete f;
            return false;
        }
        interrupt->Schedule(WakeUpHandler,(int)this,MAXCATCH*TEMPO,NetworkSendInt);
        stopSem->P();
        waitSem->P();
        printProgress((double)(i+1)/(double)(nbPieces));
    }
    std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
    int64_t diff = (end - begin).count();       // Value in nanoseconds
    printf("\n%llu bytes/s\n",(1000000000*(int64_t)nbPieces*(int64_t)MaxMailSize)/diff);
    DEBUG('n',"The file '%s' has been sent.\n",filename);
    delete f;
    return true;
}

//---------------------------------------------------------------------
// TCPostOffice::ReceiveFile
// 	Receive a file and save it. Returns a boolean value to show if the 
//  file was received successfully.
//
//	Note : will wait until it received the file infos, won't stop before
//
//	"savename" -- the name/path of the file where we put the data
//----------------------------------------------------------------------
bool TCPostOffice::ReceiveFile(char* savename){
    if(connectedID==-1) return false;

    Message * in = new Message(MaxMailSize,FILE_INFO);
    Message *out = new Message(2,ACK);
    SetMessageToFrom(out);
    if(!ReceiveWait(in,out,"ReceiveFileInfo")){
        DEBUG('n',"Connection stopping, we stop receiving the file.\n");
        delete in;
        delete out;
        return false;
    }
    int size = atoi(in->data);

    delete in;
    delete out;
    
    fileSystem->Create(savename,size*MaxMailSize);
    OpenFile *f = fileSystem->Open(savename);

    printProgress(0);
    std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
    for(int i=0; i<size; i++){
        char *buffer = new char[MaxMailSize];
        for(unsigned int j=0; j<MaxMailSize; j++) buffer[j] = '\0';
        if(!ReceiveMessage(buffer)){
            delete[] buffer;
            return false;
        }
        buffer[MaxMailSize] = '\0';
        f->Write(buffer,strlen(buffer)); 
        delete[] buffer;
        printProgress((double)(i+1)/(double)(size));
    }
    std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
    int64_t diff = (end - begin).count();
    printf("\n%llu bytes/s\n",(1000000000*(int64_t)size*(int64_t)MaxMailSize)/diff);
    DEBUG('n',"The file has been received and saved into.\n",savename);
    return true;
}

/***************** Private functions used only inside the class *****************/

//---------------------------------------------------------------------
// TCPostOffice::CatchMail
// 	Will wait and receive mail coming into the given mailboxe. Will stop 
//  if it reached the number of mail it expected or if it tried MAXCATCH 
//  times without receiving anything. Returns the number of mail it catched.
//
//	"boxeID" -- the id of the boxe where we catch the incoming emails
//	"expected" -- the number of mails we expect to catch
//----------------------------------------------------------------------
int TCPostOffice::CatchMail(int boxeID, int expected){
    PacketHeader inPktHdr;
    MailHeader inMailHdr;
    char* dull = new char[MaxMailSize];

    int attempts = 0;
    int nbCatch = 0;
    while(attempts<MAXCATCH && nbCatch<expected){
        interrupt->Schedule(WakeUpHandler,(int)this,TEMPO,NetworkRecvInt);
        stopSem->P();
        if(stoppingConnection) return -2;
        waitSem->P();
        if(postOffice->NoBlockReceive(boxeID,&inPktHdr,&inMailHdr,dull)){
            attempts = 0;
            nbCatch++;
            if(nbCatch==expected){
                DEBUG('n',"Reached the expected amount of attempts in CatchMail, we stop.\n");
                break;
            } 
        }else{
            attempts++;
        }
    }
    if(attempts==MAXCATCH)
        DEBUG('n',"Reached the max amount of attempts in CatchMail, we stop.\n");
    delete[] dull;
    return nbCatch;
}

//----------------------------------------------------------------------
// TCPostOffice::SendWait
// 	Used to send a message, then wait for a message back (generally an ack). If it reached 
//  a number of tries equal to its limit, it will stop and return false. If it 
//  received an acknowledgment, will call CatchMail and return true.
//
//	Note : If we receive a STOP connection while trying to send something, it 
//  will break the loop and get out.
//
//	"out" -- the headers and data of the message to be sent
//	"in" -- the receiver of the headers and data to be received
//	"limit" -- waiting limit for re-sending a message if no ack received
//	"debug" -- character argument passed by the sender
//----------------------------------------------------------------------
bool TCPostOffice::SendWait(Message* out, Message* in, int limit, const char* debug){
    ASSERT(in!=NULL);
    ASSERT(limit>0);
    int attempts = 0;

    while(attempts<limit){
        if(out!=NULL) postOffice->Send(*out->pktHdr,*out->mailHdr,out->data);
        interrupt->Schedule(WakeUpHandler,(int)this,TEMPO,NetworkSendInt);
        stopSem->P();
        if(stoppingConnection) return false;
        waitSem->P();
        if(postOffice->NoBlockReceive(in->type,in->pktHdr,in->mailHdr,in->data)){
            DEBUG('n',"SendWait<%s> : Machine %d received something from machine %d after %d attempts.\n",debug,id,in->pktHdr->from,attempts+1);
            break;
        }else{
            attempts++;
        }
    }
    bool status = true;
    if(attempts==limit){
        DEBUG('n',"SendWait<%s> : Reached re-emission limit of %d.\n",debug,limit);
        status = false;
    }
    if(out!=NULL) CatchMail(in->type,attempts);
    if(stoppingConnection) return false;

    if(out!=NULL && out->data[0]+1!=in->data[0]) {
        DEBUG('n',"SendWait<%s> : Invalide token received, got '%c', expected '%c'.\n",debug, in->data[0],out->data[0]+1);
        status = false;
    }

    return status;
}

//----------------------------------------------------------------------
// TCPostOffice::CatchWithAnswer
// 	Similar to CatchMail, will wait for incoming messages but will also send 
//  a message back (generally an ack) whenever it received one.
//  Retunrs the number of mail it catched.
//
//	"boxeID" -- the id of the boxe where we want to catch the incoming mails
//  "out" -- the message headers and data to send back
//----------------------------------------------------------------------
int TCPostOffice::CatchWithAnswer(int boxeID, Message* out){
    PacketHeader inPktHdr;
    MailHeader inMailHdr;
    char* dull = new char[MaxMailSize];

    int attempts = 0;
    int nbCatch = 0;
    while(attempts<MAXCATCH){
        interrupt->Schedule(WakeUpHandler,(int)this,TEMPO,NetworkRecvInt);
        stopSem->P();
        if(stoppingConnection) return -2;
        waitSem->P();
        if(postOffice->NoBlockReceive(boxeID,&inPktHdr,&inMailHdr,dull)){
            if(dull[0]+1!=out->data[0]) break;
            postOffice->Send(*out->pktHdr,*out->mailHdr,out->data);
            attempts = 0;
            nbCatch++;
        }else{
            attempts++;
        }
    }
    DEBUG('n',"Stopping CatchWithAnswer after catching %d mails.\n",nbCatch);
    delete[] dull;
    return nbCatch;
}

//----------------------------------------------------------------------
// TCPostOffice::ReceiveWait
// 	Waits for a message to be received. Sends back a message given in 
//  argument (generally an ack). Will then call CatchWithAnswer and return
//  a boolean indicating if we received the message successfully or not.
//
//	Note  If we receive a STOP connection while trying to receive, it 
//  will break the loop and get out.
//
//	"in" -- the header and data of the message to be received
//	"out" -- the header and data of the message to be sent (usually an ack)
//	"debug" -- characeter argument passed by the receiver
//----------------------------------------------------------------------
bool TCPostOffice::ReceiveWait(Message* in, Message* out, const char* debug){
    ASSERT(in!=NULL && out!=NULL);
    while(true){
        interrupt->Schedule(WakeUpHandler,(int)this,TEMPO,NetworkRecvInt);
        stopSem->P();
        if(stoppingConnection) return false;
        waitSem->P();
        if(postOffice->NoBlockReceive(in->type,in->pktHdr,in->mailHdr,in->data)){
            if(out->type==ACK){
                out->data[0] = in->data[0] + 1; out->data[1] = '\0';
            }
            postOffice->Send(*out->pktHdr,*out->mailHdr,out->data);
            break;
        }
    }
    DEBUG('n',"ReceiveWait<%s> : Finally received a message, we stop.\n",debug);
    if(CatchWithAnswer(in->type,out)==-2) return false;
    return true;
}
