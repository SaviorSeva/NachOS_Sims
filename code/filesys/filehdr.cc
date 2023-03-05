// filehdr.cc
//	Routines for managing the disk file header (in UNIX, this
//	would be called the i-node).
//
//	The file header is used to locate where on disk the
//	file's data is stored.  We implement this as a fixed size
//	table of pointers -- each entry in the table points to the
//	disk sector containing that portion of the file data
//	(in other words, there are no indirect or doubly indirect
//	blocks). The table size is chosen so that the file header
//	will be just big enough to fit in one disk sector,
//
//      Unlike in a real system, we do not keep track of file permissions,
//	ownership, last modification date, etc., in the file header.
//
//	A file header can be initialized in two ways:
//	   for a new file, by modifying the in-memory data structure
//	     to point to the newly allocated data blocks
//	   for a file already on disk, by reading the file header from disk
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation
// of liability and disclaimer of warranty provisions.

#include "copyright.h"

#include "system.h"
#include "filehdr.h"

//----------------------------------------------------------------------
// FileHeader::Allocate
// 	Initialize a fresh file header for a newly created file.
//	Allocate data blocks for the file out of the map of free disk blocks.
//	Return FALSE if there are not enough free blocks to accomodate
//	the new file.
//
//	"freeMap" is the bit map of free disk sectors
//	"fileSize" is the size of the file
//----------------------------------------------------------------------

bool FileHeader::Allocate(BitMap *freeMap, int fileSize){
    numBytes = fileSize;
    numSectors = divRoundUp(fileSize, SectorSize);
    if (freeMap->NumClear() < numSectors)
        return FALSE; // not enough space

    int sectorsToFill = numSectors;
    
    for (unsigned j = 0; j < divRoundUp(numSectors, SectorSize/sizeof(int)); j++){
        FileHeader* sectorArray = new FileHeader;
        dataSectors[j] = freeMap->Find();
        for (int i = 0; i < sectorsToFill && i != (SectorSize/sizeof(int)); i++){
            if (i == 0){
                sectorArray->numBytes = freeMap->Find();
            }else if(i == 1){
                sectorArray->numSectors = freeMap->Find();
            }
            else{
                sectorArray->dataSectors[i-2] = freeMap->Find();
            }
        }
        sectorsToFill -= SectorSize/sizeof(int);
        sectorArray->WriteBack(dataSectors[j]);
        delete sectorArray;
    }

    return TRUE;
}

//----------------------------------------------------------------------
//  Added by us
//  FileHeader::AllocateDirectory
// 	Initialize a fresh directory header for a newly created directory.
//	Allocate data blocks for the directory out of the map of free disk blocks.
//	Return FALSE if there are not enough free blocks to accomodate
//	the new directory.
//a
//	"freeMap" is the bit map of free disk sectors
//	"fileSize" is the size of the directory
//----------------------------------------------------------------------
bool FileHeader::AllocateDirectory(BitMap *freeMap, int dirSize){
    numBytes = dirSize;
    numSectors = divRoundUp(dirSize, SectorSize);
    if (freeMap->NumClear() < numSectors)
        return FALSE; // not enough space

    for (int i = 0; i < numSectors; i++)
        dataSectors[i] = freeMap->Find();
    return TRUE;
}

//----------------------------------------------------------------------
// FileHeader::Deallocate
// 	De-allocate all the space allocated for data blocks for this file.
//
//	"freeMap" is the bit map of free disk sectors
//----------------------------------------------------------------------

void FileHeader::Deallocate(BitMap *freeMap){
    int sectorsToClear = numSectors;
    for (unsigned j = 0; j < divRoundUp(numSectors, SectorSize/sizeof(int)); j++){
        FileHeader* sectorArray = new FileHeader;
        sectorArray->FetchFrom(dataSectors[j]);
        for (int i = 0; i < sectorsToClear && (i!=0 && i%(SectorSize/sizeof(int)) != 0); i++){
            if(i == 0){
                freeMap->Clear(sectorArray->numBytes);
            } else if(i == 1){
                freeMap->Clear(sectorArray->numSectors);
            } else{
                freeMap->Clear(sectorArray->dataSectors[i-2]);
            }
        }
        sectorsToClear -= SectorSize/sizeof(int);
        freeMap->Clear(dataSectors[j]);
        delete sectorArray;
    }
}

//----------------------------------------------------------------------
//  Added by us
//  FileHeader::DeallocateDirectory
// 	De-allocate all the space allocated for data blocks for this file.
//
//	"freeMap" is the bit map of free disk sectors
//----------------------------------------------------------------------
void FileHeader::DeallocateDirectory(BitMap *freeMap){
    for (int i = 0; i < numSectors; i++)
    {
        ASSERT(freeMap->Test((int)dataSectors[i])); // ought to be marked!
        freeMap->Clear((int)dataSectors[i]);
    }
}

//----------------------------------------------------------------------
// FileHeader::FetchFrom
// 	Fetch contents of file header from disk.
//
//	"sector" is the disk sector containing the file header
//----------------------------------------------------------------------

void FileHeader::FetchFrom(int sector){
    synchDisk->ReadSector(sector, (char *)this);
}

//----------------------------------------------------------------------
// FileHeader::WriteBack
// 	Write the modified contents of the file header back to disk.
//
//	"sector" is the disk sector to contain the file header
//----------------------------------------------------------------------

void FileHeader::WriteBack(int sector){
    synchDisk->WriteSector(sector, (char *)this);
}

//----------------------------------------------------------------------
// FileHeader::ByteToSector
// 	Return which disk sector is storing a particular byte within the file.
//      This is essentially a translation from a virtual address (the
//	offset in the file) to a physical address (the sector where the
//	data at the offset is stored).
//
//	"offset" is the location within the file of the byte in question
//  "mode" -- the mode to for this function
//      - mode == 1 : Open a file stored in disk with 2 level data structure
//      - mode == 0 : Open a file stored in disk with 1 level data structure
//----------------------------------------------------------------------

int FileHeader::ByteToSector(int offset, int mode){
    if(mode == 0)
        return (dataSectors[offset / SectorSize]);
    else{
        FileHeader* newHdr = new FileHeader;
        int dataSectorNumber = offset / SectorSize;

        // Calculate which data entry sector to go
        int entrySector = dataSectorNumber / (SectorSize/sizeof(int));
        int sectorOffset = dataSectorNumber % (SectorSize/sizeof(int));
        
        // Get data on dataEntrySector
        newHdr->FetchFrom(dataSectors[entrySector]);

        return *((int*)newHdr + sectorOffset);
    }
    
}

//----------------------------------------------------------------------
// FileHeader::FileLength
// 	Return the number of bytes in the file.
//----------------------------------------------------------------------

int FileHeader::FileLength(){
    return numBytes;
}

//----------------------------------------------------------------------
// FileHeader::PrintFile
// 	Print the contents of the file header, and the contents of all
//	the data blocks pointed to by the file header.
//----------------------------------------------------------------------

void FileHeader::PrintFile(){
    int i, j, k, left;
    int *intData = new int[SectorSize];
    char *charData = new char[SectorSize];
    FileHeader* newHdr = new FileHeader;
    
    printf("FileHeader contents:  File size: %d.  File sector blocks locations: ", numBytes);
    for (i = 0; dataSectors[i] != 0; i++)
        printf("%d, ", dataSectors[i]);
    printf("\nList of sector blocks for the file data:\n");
    for (i = 0, k = 0; dataSectors[i] != 0; i++){
        newHdr->FetchFrom(dataSectors[i]);
        printf("\nSector %d, Contents:\n", dataSectors[i]);
        synchDisk->ReadSector(dataSectors[i], (char*)intData);
        for (j = 0; j < (int)(SectorSize/sizeof(int)) && k < numSectors; j++, k++){
            printf("%u ", *(intData + j));
        }
    }
    left = numBytes;
    printf("\nFile contents:\n\n");
    for (i = 0; dataSectors[i] != 0; i++){
        newHdr->FetchFrom(dataSectors[i]);
        for(j = 0; j < (int)(SectorSize/sizeof(int)); j++){
            synchDisk->ReadSector(*((int*)newHdr + j), charData);
            for(k = 0; k < SectorSize && left > 0; k++, left--){
                printf("%c", charData[k]);
            }
        }
    }
    
    // ↑ This good              ↓ This bad


/*
    int i, j, k;
    char *data = new char[SectorSize];

    printf("FileHeader contents.  File size: %d.  File blocks:\n", numBytes);
    
    printf("\nFile contents:\n");
    for (i = k = 0; i < numSectors; i++)
    {
        synchDisk->ReadSector(dataSectors[i], data);
        for (j = 0; (j < SectorSize) && (k < numBytes); j++, k++)
        {
            if ('\040' <= data[j] && data[j] <= '\176') // isprint(data[j])
                printf("%c", data[j]);
            else
                printf("\\%x", (unsigned char)data[j]);
        }
        printf("\n");
    }
    delete[] data;*/
}

//----------------------------------------------------------------------
// FileHeader::PrintDirectory
// 	Print the contents of the directory header
//----------------------------------------------------------------------
void FileHeader::PrintDirectory(){
    int i, j, k;
    char *data = new char[SectorSize];
    printf("Header size: %d, Header sectors: ", numBytes);
    for (i = 0; i < numSectors; i++)
        printf("%d, ", dataSectors[i]);
    printf("\nHeader contents:\n");
    for (i = k = 0; i < numSectors; i++)
    {
        synchDisk->ReadSector(dataSectors[i], data);
        for (j = 0; (j < SectorSize) && (k < numBytes); j++, k++)
        {
            if ('\040' <= data[j] && data[j] <= '\176') // isprint(data[j])
                printf("%c", data[j]);
            else
                printf("\\%x", (unsigned char)data[j]);
        }
        printf("\n");
    }
}