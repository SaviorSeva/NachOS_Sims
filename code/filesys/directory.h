// directory.h
//	Data structures to manage a UNIX-like directory of file names.
//
//      A directory is a table of pairs: <file name, sector #>,
//	giving the name of each file in the directory, and
//	where to find its file header (the data structure describing
//	where to find the file's data blocks) on disk.
//
//      We assume mutual exclusion is provided by the caller.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation
// of liability and disclaimer of warranty provisions.

#include "copyright.h"

#ifndef DIRECTORY_H
#define DIRECTORY_H

#include "openfile.h"

#define FileNameMaxLen 9          // for simplicity, we assume file names are <= 8 characters long
#define CurrentDirectoryIndex 0   // Since every directory has a current directory(.) and
#define ParentDirectoryIndex 1    // a parent directory(..) we reserve them  
#define NumDirEntries 10


// The following class defines a "directory entry", representing a file
// in the directory.  Each entry gives the name of the file, and where
// the file's header is to be found on disk.
//
// Internal data structures kept public so that Directory operations can
// access them directly.

class DirectoryEntry{
public:
    char name[FileNameMaxLen + 1]; //  Text name for file, with +1 for
                                   //  the trailing '\0'
    char inUseAndIsDir;            //  Added by us :
                                   //   Is this directory entry a directory or file?
    int sector;                    // Location on disk to find the
                                    //   FileHeader for this file
   
};

// The following class defines a UNIX-like "directory".  Each entry in
// the directory describes a file, and where to find it on disk.
//
// The directory data structure can be stored in memory, or on disk.
// When it is on disk, it is stored as a regular Nachos file.
//
// The constructor initializes a directory structure in memory; the
// FetchFrom/WriteBack operations shuffle the directory information
// from/to disk.

class Directory{
public:
    Directory(int size); // Initialize an empty directory
                         // with space for "size" files
    ~Directory();        // De-allocate the directory

    void FetchFrom(OpenFile *file); // Init directory contents from disk
    // void FetchFromSector(OpenFile *file, int sector); // Init directory contents from disk
    void WriteBack(OpenFile *file); // Write modifications to
    // void WriteBackAtSector(OpenFile *file, int sector);                  // directory contents back to disk

    int Find(const char *name); // Find the sector number of the
                                // FileHeader for file: "name"

    bool Add(const char *name, int newSector); // Add a file name into the directory

    bool AddDirectory(const char *name, int sector); // Add a child directory into the directory

    bool Remove(const char *name); // Remove a file from the directory

    void List();  // Print the names of all the files
                  //  in the directory
    void Print(); // Verbose print of the contents
                  //  of the directory -- all the file
                  //  names and their contents.]
    int FindIndex(const char *name); // Find the index into the directory
                                     //  table corresponding to "name"

    /* ----------------------- Added by us --------------------------*/
    // Is table[index] in use or not ? 
    bool isInUseOfIndex(int index){
        return table[index].inUseAndIsDir & 1;
    }

    // Is table[index] a directory or not ? 
    bool isDirOfIndex(int index){
        return (table[index].inUseAndIsDir & 2)>>1;
    }

    // Set the inUse value of table[index] with value 
    void setInUseOfIndex(int index, bool value){
        if(value){
            table[index].inUseAndIsDir |= value;
        }else{
            table[index].inUseAndIsDir &= ~1;
        }
    }

    // Set the isDir value of table[index] with value 
    void setIsDirOfIndex(int index, bool value){
        if(value){
            table[index].inUseAndIsDir |= value<<1;
        }else{
            table[index].inUseAndIsDir &= ~2;
        }
    }

    // Return the instance of directoryEntry of table[i]
    DirectoryEntry& getTableAt(int i){
        return table[i];
    } 

    // Return the sector where current directory header is located
    int getSector(){
        return table[CurrentDirectoryIndex].sector;
    }

    // Set table[i] with a given directory entry
    void setTableAt(int i, DirectoryEntry de){
        table[i] = de;
    } 

    int isEmpty(){
        for(int i = 2; i < NumDirEntries; i++){
            if(isInUseOfIndex(i)){
                return 0;
            }
        }
        return 1;
    } 
    int isFull(){
        for(int i = 2; i < NumDirEntries; i++){
            if(isInUseOfIndex(i)){
                return 0;
            }
        }
        return 1;
    }
    // void FetchFromHeader(OpenFile *file, FileHeader* dirHdr);
    // void WriteFromHeader(OpenFile *file, FileHeader* dirHdr);
    // bool insertTableAt(int i, DirectoryEntry de){
    //     table[i] = de;
    //     return true;
    // }          

private:
    int tableSize;         // Number of directory entries
    DirectoryEntry *table; // Table of pairs:
                           // <file name, file header location>

    
};

#endif // DIRECTORY_H
