// fstest.cc 
//	Simple test routines for the file system.  
//
//	We implement:
//	   Copy -- copy a file from UNIX to Nachos
//	   Print -- cat the contents of a Nachos file 
//	   Perftest -- a stress test for the Nachos file system
//		read and write a really large file in tiny chunks
//		(won't work on baseline system!)
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"

#include "utility.h"
#include "filesys.h"
#include "system.h"
#include "thread.h"
#include "disk.h"
#include "stats.h"
#include "filehdr.h"

#define TransferSize 	10 	// make it small, just to be difficult

//----------------------------------------------------------------------
// Copy
// 	Copy the contents of the UNIX file "from" to the Nachos file "to"
//----------------------------------------------------------------------

void
Copy(const char *from, const char *to){
    FILE *fp;
    OpenFile* openFile;
    int fileLength;
    char *buffer;

// Open UNIX file
    if ((fp = fopen(from, "r")) == NULL) {	 
	printf("Copy: couldn't open input file %s\n", from);
	return;
    }

// Figure out length of UNIX file
    fseek(fp, 0, 2);		
    fileLength = ftell(fp);
    fseek(fp, 0, 0);

// Create a Nachos file of the same length
    DEBUG('f', "Copying file %s, size %d, to file %s\n", from, fileLength, to);
    if (!fileSystem->Create(to, fileLength)) {	 // Create Nachos file
        printf("Copy: couldn't create output file %s\n", to);
        fclose(fp);
        return;
    }
    DEBUG('f', "Opened nachos file %s with size %d\n",to, fileLength);
    openFile = fileSystem->Open(to);
    ASSERT(openFile != NULL);
    
// Copy the data in TransferSize chunks
    int i = 0, j = 0; 
    buffer = new char[SectorSize];
    FileHeader* hdrReader = new FileHeader;
    hdrReader->FetchFrom(openFile->getHdr().getDataSectors()[j]);
    while (fread(buffer, sizeof(char), SectorSize, fp) > 0){
    	synchDisk->WriteSector(*((int*)hdrReader + i), buffer);
        i++;
        if(i == SectorSize/sizeof(int)){
            i = 0;
            j++;
            hdrReader->FetchFrom(openFile->getHdr().getDataSectors()[j]);
        }
    }
    delete hdrReader;
    delete [] buffer;

// Close the UNIX and the Nachos files
    delete openFile;
    fclose(fp);
}

//----------------------------------------------------------------------
// Print
// 	Print the contents of the Nachos file "name".
//----------------------------------------------------------------------

void
Print(char *name){
    OpenFile *openFile;    
    int i, amountRead;
    char *buffer;

    if ((openFile = fileSystem->Open(name)) == NULL) {
	printf("Print: unable to open file %s\n", name);
	return;
    }
    
    buffer = new char[TransferSize];
    while ((amountRead = openFile->Read(buffer, TransferSize)) > 0){
        for (i = 0; i < amountRead; i++){
            printf("%c", buffer[i]);
            fflush(stdout);
        }
    }
    delete [] buffer;
    printf("\n<-------- File end -------->\n");
    fflush(stdout);

    delete openFile;		// close the Nachos file
    return;
}

//----------------------------------------------------------------------
// PerformanceTest
// 	Stress the Nachos file system by creating a large file, writing
//	it out a bit at a time, reading it back a bit at a time, and then
//	deleting the file.
//
//	Implemented as three separate routines:
//	  FileWrite -- write the file
//	  FileRead -- read the file
//	  PerformanceTest -- overall control, and print out performance #'s
//----------------------------------------------------------------------

#define FileName 	"TestFile"
#define Contents 	"1234567890"
#define ContentSize 	strlen(Contents)
#define FileSize 	((int)(ContentSize * 12000))

static void 
FileWrite(){
    OpenFile *openFile;    
    int i, numBytes;

    printf("Sequential write of %d byte file, in %zd byte chunks\n", 
	FileSize, ContentSize);
    if (!fileSystem->Create(FileName, FileSize)) {
      printf("Perf test: can't create %s\n", FileName);
      return;
    }
    openFile = fileSystem->Open(FileName);
    if (openFile == NULL) {
	printf("Perf test: unable to open %s\n", FileName);
	return;
    }
    for (i = 0; i < FileSize; i += ContentSize) {
        numBytes = openFile->Write(Contents, ContentSize);
	if (numBytes < 10) {
	    printf("Perf test: unable to write %s\n", FileName);
	    delete openFile;
	    return;
	}
    }
    delete openFile;	// close file
}

static void 
FileRead(){
    OpenFile *openFile;    
    char *buffer = new char[ContentSize];
    int i, numBytes;

    printf("Sequential read of %d byte file, in %zd byte chunks\n", 
	FileSize, ContentSize);

    if ((openFile = fileSystem->Open(FileName)) == NULL) {
	printf("Perf test: unable to open file %s\n", FileName);
	delete [] buffer;
	return;
    }
    for (i = 0; i < FileSize; i += ContentSize) {
        numBytes = openFile->Read(buffer, ContentSize);
	if ((numBytes < 10) || strncmp(buffer, Contents, ContentSize)) {
	    printf("Perf test: unable to read %s\n", FileName);
	    delete openFile;
	    delete [] buffer;
	    return;
	}
    }
    delete [] buffer;
    delete openFile;	// close file
}

void
PerformanceTest(){
    printf("Starting file system performance test:\n");
    stats->Print();
    FileWrite();
    FileRead();
    if (!fileSystem->Remove(FileName)) {
      printf("Perf test: unable to remove %s\n", FileName);
      return;
    }
    stats->Print();
}

void
Test1(){
    printf("Starting file system Test1:\n");
    stats->Print();
    fileSystem->CreateDirectory("dir1");
    fileSystem->ChangeDirectory("dir1");
    fileSystem->CreateDirectory("dir2");
    Copy("../filesys/test/small", "small1");
    if (!fileSystem->Remove("small1")) {
      printf("Perf test: unable to remove file %s\n", "small1");
      return;
    }
    if (!fileSystem->RemoveDirectory("dir2")) {
      printf("Perf test: unable to remove directory%s\n", "dir2");
      return;
    }
    stats->Print();
}

void
Test2(){
    printf("Starting file system Test1:\n");
    stats->Print();
    const char *buffer;
    Copy("../filesys/test/small", "small1");
    Copy("../filesys/test/medium", "med1");

    OpenFile* opens1 = fileSystem->Open("med1");
    OpenFile* opens2 = fileSystem->Open("small1");
    OpenFile* opens3 = fileSystem->Open("med1");

    buffer = "medmedmed";
    opens1->Write(buffer, TransferSize);	
    opens2->Write(buffer, TransferSize);
    buffer = "abcde";
    opens3->Write(buffer, 6);
    buffer = "012345678";
    opens1->Write(buffer, TransferSize);

    // int amountRead = 0;
    // char *buffer;
    // FILE *fp;
    // if ((fp = fopen("../filesys/test/medium", "r")) == NULL) {	 
    //     printf("Copy: couldn't open input file %s\n", "../filesys/test/mid");
    //     return;
    // }
    
    // while(TRUE){
    //     buffer = new char[TransferSize];
    //     while ((amountRead = fread(buffer, sizeof(char), TransferSize, fp)) > 0)
    //     opens->Write(buffer, amountRead);	
    //     delete [] buffer;

    //     fseek(fp, 0, 0);
    // }
    delete opens1;
    delete opens2;
    delete opens3;
    char* medname = new char[5];
    char* smaname = new char[7];

    strcpy(medname, "med1");
    strcpy(smaname, "small1");
    puts("Mediem file:\n");
    Print(medname);
    puts("smol file;\n");
    Print(smaname);
    stats->Print();
}

void
Test3(){
    printf("Starting file system Test3:\n");
    stats->Print();
    fileSystem->Create("AFILE", 0);
    stats->Print();
}
