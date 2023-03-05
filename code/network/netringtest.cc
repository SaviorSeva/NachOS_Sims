// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation
// of liability and disclaimer of warranty provisions.

#include "copyright.h"

#include "system.h"
#include "network.h"
#include "post.h"
#include "interrupt.h"

// Test out message delivery, by doing the following:
//	1. send a message to the machine with ID "farAddr", at mail box #0
//	2. wait for the other machine's message to arrive (in our mailbox #0)
//	3. send an acknowledgment for the other machine's message
//	4. wait for an acknowledgement from the other machine to our
//	    original message

void RingTest(int n){
    if(n<1){
        interrupt->Halt();
    }
    PacketHeader outPktHdr, inPktHdr;
    MailHeader outMailHdr, inMailHdr;
    const char *ack = "Got it!";
    char buffer[MaxMailSize];

    int netAddr = postOffice->GetNetAddr();
    printf("Machine %d, making a ring of %d\n",netAddr,n);

    if(netAddr==0){
        const char *data = "Hello there !";
        // Making to headers for the data 
        outPktHdr.to = 1%n;
        outMailHdr.to = 0;
        outMailHdr.from = 1;
        outMailHdr.length = strlen(data) + 1;
        // Sending the data
        postOffice->Send(outPktHdr, outMailHdr, data);
        // Waiting for the data to be received
        postOffice->Receive(0, &inPktHdr, &inMailHdr, buffer);
        printf("Got \"%s\" from %d, box %d\n", buffer, inPktHdr.from, inMailHdr.from);
        fflush(stdout);

        // Making the headers for the acknowledgment and sending it
        outPktHdr.to = inPktHdr.from;
        outMailHdr.to = inMailHdr.from;
        outMailHdr.length = strlen(ack) + 1;
        postOffice->Send(outPktHdr, outMailHdr, ack);

        // Wait for the ack from the other machine to the first message we sent.
        postOffice->Receive(1, &inPktHdr, &inMailHdr, buffer);
        printf("Got \"%s\" from %d, box %d\n", buffer, inPktHdr.from, inMailHdr.from);
        fflush(stdout);
    }else{
        // Waiting for the data to be received
        postOffice->Receive(0, &inPktHdr, &inMailHdr, buffer);
        // Making to headers for the data 
        outPktHdr.to = (netAddr+1)%n;
        outMailHdr.to = 0;
        outMailHdr.from = 1;
        outMailHdr.length = strlen(buffer) + 1;
        // Sending the data
        postOffice->Send(outPktHdr, outMailHdr, buffer);

        // Making the headers for the acknowledgment and sending it
        outPktHdr.to = inPktHdr.from;
        outMailHdr.to = inMailHdr.from;
        outMailHdr.length = strlen(ack) + 1;
        postOffice->Send(outPktHdr, outMailHdr, ack);

        // Wait for the ack from the other machine to the first message we sent.
        postOffice->Receive(1, &inPktHdr, &inMailHdr, buffer);
        printf("Got \"%s\" from %d, box %d\n", buffer, inPktHdr.from, inMailHdr.from);
        fflush(stdout);
       
    }
    // Then we're done!
    interrupt->Halt();
}
