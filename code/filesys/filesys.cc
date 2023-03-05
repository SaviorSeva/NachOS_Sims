// filesys.cc
//	Routines to manage the overall operation of the file system.
//	Implements routines to map from textual file names to files.
//
//	Each file in the file system has:
//	   A file header, stored in a sector on disk
//		(the size of the file header data structure is arranged
//		to be precisely the size of 1 disk sector)
//	   A number of data blocks
//	   An entry in the file system directory
//
// 	The file system consists of several data structures:
//	   A bitmap of free disk sectors (cf. bitmap.h)
//	   A directory of file names and file headers
//
//      Both the bitmap and the directory are represented as normal
//	files.  Their file headers are located in specific sectors
//	(sector 0 and sector 1), so that the file system can find them
//	on bootup.
//
//	The file system assumes that the bitmap and directory files are
//	kept "open" continuously while Nachos is running.
//
//	For those operations (such as Create, Remove) that modify the
//	directory and/or bitmap, if the operation succeeds, the changes
//	are written immediately back to disk (the two files are kept
//	open during all this time).  If the operation fails, and we have
//	modified part of the directory and/or bitmap, we simply discard
//	the changed version, without writing it back to disk.
//
// 	Our implementation at this point has the following restrictions:
//
//	   there is no synchronization for concurrent accesses
//	   files have a fixed size, set when the file is created
//	   files cannot be bigger than about 3KB in size
//	   there is no hierarchical directory structure, and only a limited
//	     number of files can be added to the system
//	   there is no attempt to make the system robust to failures
//	    (if Nachos exits in the middle of an operation that modifies
//	    the file system, it may corrupt the disk)
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation
// of liability and disclaimer of warranty provisions.

#include "copyright.h"

#include "disk.h"
#include "bitmap.h"
#include "directory.h"
#include "filehdr.h"
#include "filesys.h"

// Sectors containing the file headers for the bitmap of free sectors,
// and the directory of files.  These file headers are placed in well-known
// sectors, so that they can be located on boot-up.
#define FreeMapSector 0
#define RootSector 1

// Initial file sizes for the bitmap and directory; until the file system
// supports extensible files, the directory size sets the maximum number
// of files that can be loaded onto the disk.
#define FreeMapFileSize (NumSectors / BitsInByte)
#define DirectoryFileSize sizeof(DirectoryEntry) * NumDirEntries

//----------------------------------------------------------------------
// FileSystem::FileSystem
// 	Initialize the file system.  If format = TRUE, the disk has
//	nothing on it, and we need to initialize the disk to contain
//	an empty directory, and a bitmap of free sectors (with almost but
//	not all of the sectors marked as free).
//
//	If format = FALSE, we just have to open the files
//	representing the bitmap and the directory.
//  
//	"format" -- should we initialize the disk?
//----------------------------------------------------------------------

FileSystem::FileSystem(bool format){
    DEBUG('f', "Initializing the file system.\n");
    if (format) {
        BitMap *freeMap = new BitMap(NumSectors);
        currentDir = new Directory(NumDirEntries);
        FileHeader *mapHdr = new FileHeader;
        FileHeader *dirHdr = new FileHeader;

        currentDir->getTableAt(CurrentDirectoryIndex).sector = 1;
        currentDir->getTableAt(ParentDirectoryIndex).sector = 1;
        
        DEBUG('f', "Formatting the file system.\nDirectorFileSize:%d, NumDirEntries: %d\n",DirectoryFileSize,NumDirEntries);

        // First, allocate space for FileHeaders for the bitmap and the root directory
        // (make sure no one else grabs these!)
        freeMap->Mark(FreeMapSector);
        freeMap->Mark(RootSector);

        // Second, allocate space for the data blocks containing the contents
        // of the directory and bitmap files.  There better be enough space!

        ASSERT(mapHdr->AllocateDirectory(freeMap, FreeMapFileSize));
        ASSERT(dirHdr->AllocateDirectory(freeMap, DirectoryFileSize));

        // Flush the bitmap and directory FileHeaders back to disk
        // We need to do this before we can "Open" the file, since open
        // reads the file header off of disk (and currently the disk has garbage
        // on it!).

        DEBUG('f', "Writing headers back to disk.\n");
        mapHdr->WriteBack(FreeMapSector);
        dirHdr->WriteBack(RootSector);

        // OK to open the bitmap and directory files now
        // The file system operations assume these two files are left open
        // while Nachos is running.

        freeMapFile = new OpenFile(FreeMapSector);
        directoryFile = new OpenFile(RootSector);

        // Once we have the files "open", we can write the initial version
        // of each file back to disk.  The directory at this point is completely
        // empty; but the bitmap has been changed to reflect the fact that
        // sectors on the disk have been allocated for the file headers and
        // to hold the file data for the directory and bitmap.

        DEBUG('f', "Writing bitmap and directory back to disk.\n");
        freeMap->WriteBack(freeMapFile); // flush changes to disk
        //directory->WriteBack(directoryFile);
        currentDir->WriteBack(directoryFile);
        
        if (DebugIsEnabled('f')){
            freeMap->Print();
            currentDir->Print();

            delete freeMap;
            delete mapHdr;
            delete dirHdr;
        }
        // openedID = new int[10];
        // for(int i=0; i<10; i++) openedID[i] = -1;
    }
    else{
        // if we are not formatting the disk, just open the files representing
        // the bitmap and directory; these are left open while Nachos is running
        freeMapFile = new OpenFile(FreeMapSector);
        directoryFile = new OpenFile(RootSector);
        currentDir = new Directory(NumDirEntries);
        currentDir->FetchFrom(directoryFile);
    }
}

//----------------------------------------------------------------------
// FileSystem::Create
// 	Create a file in the Nachos file system (similar to UNIX create).
//	Since we can't increase the size of files dynamically, we have
//	to give Create the initial size of the file.
//
//	The steps to create a file are:
//	  Make sure the file doesn't already exist
//        Allocate a sector for the file header
// 	  Allocate space on disk for the data blocks for the file
//	  Add the name to the directory
//	  Store the new file header on disk
//	  Flush the changes to the bitmap and the directory back to disk
//
//	Return TRUE if everything goes ok, otherwise, return FALSE.
//
// 	Create fails if:
//   		file is already in directory
//	 	no free space for file header
//	 	no free entry for file in directory
//	 	no free space for data blocks for the file
//
// 	Note that this implementation assumes there is no concurrent access
//	to the file system!
//
//	"name" -- name of file to be created
//	"initialSize" -- size of file to be created
//----------------------------------------------------------------------

bool FileSystem::Create(const char *name, int initialSize){
    BitMap *freeMap;
    FileHeader *hdr;
    int sector;
    bool success;

    DEBUG('f', "Creating file %s, size %d\n", name, initialSize);

    if (currentDir->Find(name) != -1){
        success = FALSE; // file is already in directory
        DEBUG('f',"File %s already in current directory\n", name);
    }
    else{
        freeMap = new BitMap(NumSectors);
        freeMap->FetchFrom(freeMapFile);
        sector = freeMap->Find(); // find a sector to hold the file header
        if (sector == -1){
            DEBUG('f',"No free block for file header\n", name);
            success = FALSE; // no free block for file header
        }
        else if (!currentDir->Add(name, sector)){
            DEBUG('f',"No space in directory\n", name);
            success = FALSE; // no space in directory
        }
        else{
            hdr = new FileHeader;
            if (!hdr->Allocate(freeMap, initialSize))
                success = FALSE; // no space on disk for data
            else{
                success = TRUE;
                // everthing worked, flush all changes back to disk
                hdr->WriteBack(sector);

                // Find the correct header to write back the changes
                OpenFile* currentHdrFile = new OpenFile(currentDir->getSector());
                currentDir->WriteBack(currentHdrFile);
                
                freeMap->WriteBack(freeMapFile);

                delete currentHdrFile;
            }
            delete hdr;
        }
        delete freeMap;
    }
    return success;
}

//----------------------------------------------------------------------
// FileSystem::Open
// 	Open a file for reading and writing.
//	To open a file:
//	  Find the location of the file's header, using the directory
//	  Bring the header into memory
//
//	"name" -- the text name of the file to be opened
//----------------------------------------------------------------------

OpenFile *
FileSystem::Open(const char *name){
    Directory *directory = new Directory(NumDirEntries);
    OpenFile *openFile = NULL;
    int sector;

    // Get current directory header file
    OpenFile* currentHdrFile = new OpenFile(currentDir->getSector());

    DEBUG('f', "Opening file %s\n", name);
    
    directory->FetchFrom(currentHdrFile);
    sector = directory->Find(name);
    if (sector >= 0){
        openFile = new OpenFile(sector); // name was found in directory
    }  
    delete directory;
    return openFile; // return NULL if not found
}

//----------------------------------------------------------------------
// FileSystem::Remove
// 	Delete a file from the file system.  This requires:
//	    Remove it from the directory
//	    Delete the space for its header
//	    Delete the space for its data blocks
//	    Write changes to directory, bitmap back to disk
//
//	Return TRUE if the file was deleted, FALSE if the file wasn't
//	in the file system.
//
//	"name" -- the text name of the file to be removed
//----------------------------------------------------------------------

bool FileSystem::Remove(const char *name){
    BitMap *freeMap;
    FileHeader *fileHdr;
    int sector;

    sector = currentDir->Find(name);
    if (sector == -1){
        return FALSE; // file not found
    }
    // Get the header of the file
    fileHdr = new FileHeader;
    fileHdr->FetchFrom(sector);

    // Get the free map file
    freeMap = new BitMap(NumSectors);
    freeMap->FetchFrom(freeMapFile);

    fileHdr->Deallocate(freeMap);   // remove data blocks
    freeMap->Clear(sector);         // remove header block
    currentDir->Remove(name);       // remove the file from current directory

    // Get current directory header file
    OpenFile* currentHdrFile = new OpenFile(currentDir->getSector());

    freeMap->WriteBack(freeMapFile);     // flush to disk
    currentDir->WriteBack(currentHdrFile); // flush to disk
    delete fileHdr;
    delete freeMap;
    delete currentHdrFile;             
    return TRUE;
}

//----------------------------------------------------------------------
// FileSystem::List
// 	List all the files in the file system directory.
//----------------------------------------------------------------------

void FileSystem::List(){
    currentDir->List();
}

//----------------------------------------------------------------------
// FileSystem::Print
// 	Print everything about the file system:
//	  the contents of the bitmap
//	  the contents of the directory
//	  for each file in the directory,
//	      the contents of the file header
//	      the data in the file
//----------------------------------------------------------------------

void FileSystem::Print(){
    FileHeader *bitHdr = new FileHeader;
    FileHeader *dirHdr = new FileHeader;
    BitMap *freeMap = new BitMap(NumSectors);
    Directory *directory = new Directory(NumDirEntries);

    printf("Bit map file header:\n");
    bitHdr->FetchFrom(FreeMapSector);
    bitHdr->PrintDirectory();

    printf("Directory file header:\n");
    dirHdr->FetchFrom(RootSector);
    dirHdr->PrintDirectory();

    freeMap->FetchFrom(freeMapFile);
    freeMap->Print();

    printf("\n\n-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------\n\n");

    // Get current directory header file
    OpenFile* currentHdrFile = new OpenFile(currentDir->getSector());

    directory->FetchFrom(currentHdrFile);
    directory->Print();

    delete bitHdr;
    delete dirHdr;
    delete currentHdrFile;
    delete freeMap;
    delete directory;
}

//----------------------------------------------------------------------
// FileSystem::CreateDirectory
//   Create a directory under the current directory
//   Return 0 if successful, -1 if current directory is full, -2 if have confilct name.
//
//  "name" -- The name of the directory to be created
//----------------------------------------------------------------------

int FileSystem::CreateDirectory(const char *name){
    BitMap *freeMap;
    FileHeader *hdr;
    int sector;
    DEBUG('f', "Creating directory %s,\n", name);
    
    if (currentDir->Find(name) != -1)
        return -2; // file/dir with the same name is already in directory
    else{
        freeMap = new BitMap(NumSectors);
        freeMap->FetchFrom(freeMapFile);
        sector = freeMap->Find(); // find a sector to hold the directory header
        if (sector == -1)
            return -1; // no free block for directory header
        else if (!currentDir->AddDirectory(name, sector))
            return -1; // no space in directory
        else{
            hdr = new FileHeader;
            if (!hdr->AllocateDirectory(freeMap, DirectoryFileSize))
                return -1; // no space on disk for data
            
            // everything worked, flush all changes back to disk

            // Write the directory header to disk
            hdr->WriteBack(sector);

            // Get current directory header file
            OpenFile* currentHdrFile = new OpenFile(currentDir->getSector());

            // Write current directory data to disk
            currentDir->WriteBack(currentHdrFile);

            // Create new directory
            Directory* newDir = new Directory(NumDirEntries);
            newDir->getTableAt(CurrentDirectoryIndex).sector = sector; 
            newDir->getTableAt(ParentDirectoryIndex).sector = currentDir->getSector();
            
            // Open new directory headerfile and write new directory data in disk
            OpenFile* newDirHdrFile = new OpenFile(sector);
            newDir->WriteBack(newDirHdrFile);

            // Write freeMap back
            freeMap->WriteBack(freeMapFile);

            // Delete created variables
            delete currentHdrFile;
            delete newDirHdrFile;
            delete newDir;
            }
        }
    delete freeMap;
    delete hdr;
    return 0;
}

//----------------------------------------------------------------------
// FileSystem::RemoveDirectory
//   Remove a directory under the current directory
//   Return 0 if successful, -1 if directory to remove is not empty,-2 if the directory is not found.
//
//  "name" -- The name of the directory to be removed
//----------------------------------------------------------------------

int FileSystem::RemoveDirectory(const char* name){
    BitMap *freeMap;
    FileHeader *dirHdr, *currDirHdr;
    Directory *directory;
    
    int index = currentDir->FindIndex(name);
    if (index == -1) return -2;

    // Get the directory to be removed
    int sector = currentDir->getTableAt(index).sector;

    dirHdr = new FileHeader;
    dirHdr->FetchFrom(sector);
    
    directory = new Directory(NumDirEntries);

    // Get current directory header file
    OpenFile* currentHdrFile = new OpenFile(currentDir->getSector());
    
    directory->FetchFrom(currentHdrFile);
    if(!directory->isEmpty()) return -1;

    freeMap = new BitMap(NumSectors);
    freeMap->FetchFrom(freeMapFile);
    
    dirHdr->DeallocateDirectory(freeMap);
    freeMap->Clear(sector);

    currDirHdr = new FileHeader;
    currDirHdr->FetchFrom(currentDir->getSector());
    
    currentDir->setInUseOfIndex(index, FALSE);

    freeMap->WriteBack(freeMapFile);
    currentDir->WriteBack(currentHdrFile);
    delete freeMap;
    delete dirHdr;
    delete currDirHdr;
    delete currentHdrFile;
    delete directory;
    return 0;
}

//----------------------------------------------------------------------
// FileSystem::ChangeDirectory
//   Change to a different directory based on the current directory
//   Return 0 if successful, -1 if name is not found in current directory.
//
//  "name" -- The name of the directory to move to
//----------------------------------------------------------------------

int FileSystem::ChangeDirectory(const char* name){
    // Find which child directory we want to go.
    int index = currentDir->FindIndex(name);
    if(index == -1) return -1;

    // Get next directory header file
    OpenFile* nextDirFile = new OpenFile(currentDir->getTableAt(index).sector);

    // Get next directory data 
    Directory *nextDir = new Directory(NumDirEntries);
    nextDir->FetchFrom(nextDirFile);
    
    // Change current directory
    currentDir = nextDir;
    return 0;
}

