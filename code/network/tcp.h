#include "post.h"
#include "synch.h"


#define TEMPO 10000                     // Number of ticks to wait until checking if we received acknowledgement
#define MAXREEMISSIONS 200              // Number of times we try to send a message when we don't receive an acknowledgment
#define MAXCONNECTIONREEMISSIONS 50     // Number of times we try to connect to a machine when we don't receive a response
#define MAXCATCH 20


// Mail Boxe Usage
enum PktType{
    MSG,
    STRING_INFO,
    FILE_INFO,
    PROCESS_INFO,
    SYNC,
    ACK,
    STOP,
};

//char *PktTypeNames[7] = {"FIXED_MSG","STRING_MSG","FILE_MSG","PROCESS_MSG","SYNC","ACK","STOP"};

class Message{
public:
    PacketHeader* pktHdr;
    MailHeader* mailHdr;
    char* data;
    PktType type;

    Message(int dataSize, PktType msgType);
    ~Message();
};

class TCPostOffice{
public:
    TCPostOffice();
    ~TCPostOffice();
    int SyncWithMachine(int machineID);
    int OpenConnection();
    int EndConnection();
    int ListenForEndConnection();
    bool SendString(char *data);
    bool ReceiveString(char *data);
    bool SendFile(char* filename);
    bool ReceiveFile(char* savename);

    Semaphore* waitSem;
    Semaphore* stopSem;
private:
    int id;
    int connectedID;
    bool stoppingConnection;
    bool SendMessage(const char *data);
    bool ReceiveMessage(char *data);
    int CatchMail(int boxeID, int expected);
    bool SendWait(Message* out, Message* in, int limit, const char* debug);
    int CatchWithAnswer(int boxeID, Message* out);
    bool ReceiveWait(Message* in, Message* out, const char* debug);

    void SetMessageToFrom(Message* m){
        m->pktHdr->to = connectedID;
        m->pktHdr->from = id;
    }
};

bool SendMessage(int netIdTo, const char *data);
void ReceiveMessage(char *data);