// fat16 driver
// https://wiki.osdev.org/FAT16
// https://en.wikipedia.org/wiki/Design_of_the_FAT_file_system
// http://www.c-jump.com/CIS24/Slides/FAT/FAT.html

#include <stdint.h>
#include <stdbool.h>

#include "memory.h"
#include "gui.h"

// https://www.freebsd.org/cgi/man.cgi?query=newfs_msdos&apropos=0&sektion=0&manpath=FreeBSD+5.2-RELEASE&format=html
// BPB / bios parameter block
typedef struct {
   uint8_t start[3];
   uint8_t identifier[8];
   uint16_t bytesPerSector;
   uint8_t sectorsPerCluster;
   uint16_t noReservedSectors;
   uint8_t noTables;
   uint16_t noRootEntries;
   uint16_t noSectors;
   uint8_t mediaDescriptor;
   uint16_t sectorsPerFat;
   uint16_t sectorsPerTrack;
   uint16_t noDriveHeads;
   uint32_t noHiddenSectors;
   uint32_t largeNoSectors; // used if value doesn't fit noSectors

} __attribute__((packed)) fat_bpb_t;

// extended boot record
typedef struct {
   uint8_t driveNo;
   uint8_t flags;
   uint8_t signature;
   uint32_t volumeID;
   uint8_t volumeLabel[11];
   uint8_t fatTypeStr[8];
   uint32_t bootCode[112];
   uint16_t bootSignature;
} __attribute__((packed)) fat_ebr_t;

// 8.3 directory structure
typedef struct {
   uint8_t filename[11];
   uint8_t attributes;
   uint8_t reserved;
   uint8_t creationTimeFine; // in 10ths of a second
   uint16_t creationTime;
   uint16_t creationDate;
   uint16_t lastAccessDate;
   uint16_t zero; // high 16 bits of entry's first cluster no in other fat vers
   uint16_t lastModifyTime;
   uint16_t lastModifyDate;
   uint16_t firstClusterNo;
   uint32_t fileSize; // bytes
} __attribute__((packed)) fat_dir_t;

uint32_t baseAddr = 42000 + 512*3;

fat_bpb_t *fat_bpb;
fat_ebr_t *fat_ebr;
uint32_t noSectors;
uint32_t noClusters;

extern void ata_read(bool primaryBus, bool masterDrive, uint32_t lba, uint16_t *buf);
extern uint8_t *ata_read_exact(bool primaryBus, bool masterDrive, uint32_t addr, uint32_t bytes);

void fat_read_dir(uint16_t clusterNo);
void fat_read_file(uint16_t clusterNo, uint32_t size);

void fat_get_info() {
   // get drive formatting info
   uint8_t *buf = ata_read_exact(true, true, baseAddr, sizeof(fat_bpb_t) + sizeof(fat_ebr_t));

   fat_bpb = (fat_bpb_t*)(&buf[0]);
   fat_ebr = (fat_ebr_t*)(&buf[sizeof(fat_bpb_t)]); // immediately after bpb

   noSectors = (fat_bpb->noSectors == 0) ? fat_bpb->largeNoSectors : fat_bpb->noSectors;
   noClusters = noSectors/fat_bpb->sectorsPerCluster;

   /*
   gui_writestr("\nBITS PER SECTOR: ", 0);
   gui_writeuint(fat_bpb->bytesPerSector, 0);
   gui_writestr("\nSECTORS: ", 0);
   gui_writeuint(noSectors, 0);
   gui_writestr("\nSECTORS PER CLUSTER: ", 0);
   gui_writeuint(fat_bpb->sectorsPerCluster, 0);
   gui_writestr("\nCLUSTERS: ", 0);
   gui_writeuint(noClusters, 0);*/

}

void fat_parse_dir_entry(fat_dir_t *fat_dir) {
   if(fat_dir->firstClusterNo < 2) return;

   char fileName[9];
   char extension[4];
   strcpy_fixed((char*)fileName, (char*)fat_dir->filename, 8);
   strcpy_fixed((char*)extension, (char*)fat_dir->filename+8, 3);
   strsplit((char*)fileName, NULL, (char*)fileName, ' '); // null terminate at first space
   strsplit((char*)extension, NULL, (char*)extension, ' '); // null terminate at first space
   gui_writestr(fileName, 0);
   if(extension[0] != '\0')
      gui_drawchar('.', 0);
   gui_writestr(extension, 0);
   gui_writestr(": ", 0);
   if((fat_dir->attributes & 0x10) == 0x10) // directory
      gui_writestr("DIR", 4);
   else
      gui_writenum(fat_dir->fileSize, 4);
   gui_drawchar(' ', 0);
   gui_writenum(fat_dir->firstClusterNo, 0);
   gui_drawchar('\n', 0);
}

extern bool strcmp(char* str1, char* str2);
// return clusterNo from filename and extension in a specific directory
bool fat_entry_matches_filename(fat_dir_t *fat_dir, char* name, char* extension) {
   if(fat_dir->firstClusterNo < 2) return false;

   char entryName[9];
   char entryExtension[4];
   strcpy_fixed((char*)entryName, (char*)fat_dir->filename, 8);
   strcpy_fixed((char*)entryExtension, (char*)fat_dir->filename+8, 3);
   strsplit((char*)entryName, NULL, (char*)entryName, ' '); // null terminate at first space
   strsplit((char*)entryExtension, NULL, (char*)entryExtension, ' '); // null terminate at first space
   strtoupper((char*)name, (char*)name); // fat ignores file case
   strtoupper((char*)extension, (char*)extension);
   strtoupper((char*)entryName, (char*)entryName); // fat ignores file case
   strtoupper((char*)entryExtension, (char*)entryExtension);

   if(!strcmp(entryName, name)) return false;
   if(!strcmp(entryExtension, extension)) return false;
   
   return true;
}

void fat_read_root() {
   uint32_t rootSector = fat_bpb->noReservedSectors + fat_bpb->noTables*fat_bpb->sectorsPerFat;
   uint32_t rootDirAddr = rootSector*fat_bpb->bytesPerSector + baseAddr;

   gui_writestr("\n", 0);

   uint32_t offset = 0;
   // get each file/dir in root
   for(int i = 0; i < fat_bpb->noRootEntries; i++) {
      uint8_t *buf2 = ata_read_exact(true, true, rootDirAddr + offset, sizeof(fat_dir_t));
      fat_dir_t *fat_dir = (fat_dir_t*)buf2;
      if(fat_dir->filename[0] == 0) break; // no more files/dirs in directory

      fat_parse_dir_entry(fat_dir);

      offset+=32; // each entry is 32 bytes
      free((uint32_t)buf2, sizeof(fat_dir_t));
   }

}

void fat_setup() {
   
   fat_get_info();

   fat_read_root();

   //gui_drawchar('\n', 0);

   //fat_read_dir(3);

   //fat_read_file(7, 0);
}

void fat_read_dir(uint16_t clusterNo) {
   uint32_t rootSize = ((fat_bpb->noRootEntries * 32) + (fat_bpb->bytesPerSector - 1)) / fat_bpb->bytesPerSector; // in sectors
   uint32_t rootSector = fat_bpb->noReservedSectors + fat_bpb->noTables*fat_bpb->sectorsPerFat;
   uint32_t firstDataSector = rootSector + rootSize;
   uint32_t dirFirstSector = ((clusterNo - 2) * fat_bpb->sectorsPerCluster) + firstDataSector;

   uint32_t offset = 0;

   uint32_t dirAddr = baseAddr + dirFirstSector*fat_bpb->bytesPerSector;

   // get each file/dir in dir
   while(true) {
      uint8_t *buf2 = ata_read_exact(true, true, dirAddr + offset, sizeof(fat_dir_t));
      fat_dir_t *fat_dir = (fat_dir_t*)buf2;
      if(fat_dir->filename[0] == 0) break; // no more files/dirs in directory

      fat_parse_dir_entry(fat_dir);
      
      offset+=32; // each entry is 32 bytes
      free((uint32_t)buf2, sizeof(fat_dir_t));
   }

}

fat_dir_t *fat_find_in_root(char* filename, char* extension) {
   uint32_t rootSector = fat_bpb->noReservedSectors + fat_bpb->noTables*fat_bpb->sectorsPerFat;
   uint32_t rootDirAddr = rootSector*fat_bpb->bytesPerSector + baseAddr;

   uint32_t offset = 0;
   // get each file/dir in root
   for(int i = 0; i < fat_bpb->noRootEntries; i++) {
      uint8_t *buf2 = ata_read_exact(true, true, rootDirAddr + offset, sizeof(fat_dir_t));
      fat_dir_t *fat_dir = (fat_dir_t*)buf2;
      if(fat_dir->filename[0] == 0) break; // no more files/dirs in directory

      if(fat_entry_matches_filename(fat_dir, filename, extension))
         return fat_dir;

      offset+=32; // each entry is 32 bytes
      free((uint32_t)buf2, sizeof(fat_dir_t));
   }

   return NULL;

}

// and return info
fat_dir_t *fat_find_in_dir(uint16_t clusterNo, char* filename, char* extension) {
   uint32_t rootSize = ((fat_bpb->noRootEntries * 32) + (fat_bpb->bytesPerSector - 1)) / fat_bpb->bytesPerSector; // in sectors
   uint32_t rootSector = fat_bpb->noReservedSectors + fat_bpb->noTables*fat_bpb->sectorsPerFat;
   uint32_t firstDataSector = rootSector + rootSize;
   uint32_t dirFirstSector = ((clusterNo - 2) * fat_bpb->sectorsPerCluster) + firstDataSector;

   uint32_t offset = 0;

   uint32_t dirAddr = baseAddr + dirFirstSector*fat_bpb->bytesPerSector;

   // get each file/dir in dir
   while(true) {
      uint8_t *buf2 = ata_read_exact(true, true, dirAddr + offset, sizeof(fat_dir_t));
      fat_dir_t *fat_dir = (fat_dir_t*)buf2;
      if(fat_dir->filename[0] == 0) break; // no more files/dirs in directory

      if(fat_entry_matches_filename(fat_dir, filename, extension))
         return fat_dir;
      
      offset+=32; // each entry is 32 bytes
      free((uint32_t)buf2, sizeof(fat_dir_t));
   }
   return NULL;
}

void fat_read_file(uint16_t clusterNo, uint32_t size) {

   uint32_t rootSize = ((fat_bpb->noRootEntries * 32) + (fat_bpb->bytesPerSector - 1)) / fat_bpb->bytesPerSector; // in sectors
   uint32_t rootSector = fat_bpb->noReservedSectors + fat_bpb->noTables*fat_bpb->sectorsPerFat;
   uint32_t firstDataSector = rootSector + rootSize;
   uint32_t fileFirstSector = ((clusterNo - 2) * fat_bpb->sectorsPerCluster) + firstDataSector;

   // read entire fat table
   uint32_t fatTableAddr = baseAddr + fat_bpb->noReservedSectors*fat_bpb->bytesPerSector;
   uint8_t *fatTable = ata_read_exact(true, true, fatTableAddr, 2*noClusters);

   // get no clusters
   uint16_t c = clusterNo;
   uint16_t clusterCount = 1;
   while(true) {
      uint16_t tableVal = ((uint16_t*)fatTable)[c];
      if(tableVal >= 0xFFF8) {
         break; // no more clusters in chain
      } else if(tableVal == 0xFFF7) {
         break; // bad cluster
      } else {
         c = tableVal;
         clusterCount++;
      }
   }

   gui_writeuint(clusterCount, 0);
   gui_writestr(" clusters ", 0);

   uint32_t fileSize = clusterCount*fat_bpb->sectorsPerCluster*fat_bpb->bytesPerSector;
   uint8_t *fileContents = malloc(fileSize);

   gui_writeuint(fileSize, 0);
   gui_writestr(" bytes\n", 0);

   int cluster = 0;
   while(true) { // until we reach the end of the cluster chain
      // read each sector of cluster
      for(int i = 0; i < fat_bpb->sectorsPerCluster; i++) {
         uint32_t sectorAddr = (fileFirstSector+cluster*fat_bpb->sectorsPerCluster+i)*fat_bpb->bytesPerSector + baseAddr;
         uint8_t *buf = ata_read_exact(true, true, sectorAddr, fat_bpb->bytesPerSector);
         uint32_t memOffset = fat_bpb->bytesPerSector * (cluster*fat_bpb->sectorsPerCluster + i);
         // copy to master buffer
         for(int b = 0; b < fat_bpb->bytesPerSector; b++)
            fileContents[memOffset + b] = buf[b];

         free((uint32_t)buf, fat_bpb->bytesPerSector);
      }

      // check if theres more clusters to read
      uint16_t tableVal = ((uint16_t*)fatTable)[clusterNo];
      if(tableVal >= 0xFFF8) {
         // no more clusters in chain
         break;
      } else if(tableVal == 0xFFF7) {
         // bad cluster
         break;
      } else { 
         clusterNo = tableVal; // table value is the next cluster
         cluster++;
      }
   }

   gui_writestr("Loaded into ", 0);
   gui_writeuint_hex((uint32_t)fileContents, 0);
   gui_writestr(" / ", 0);
   gui_writeuint((uint32_t)fileContents, 0);
   gui_drawchar('\n', 0);

   free((uint32_t)fatTable, 2*noClusters);

}

extern int strlen(char* str);

fat_dir_t *fat_follow_path_chain(char *pathElement, fat_dir_t *dir) {
   if(strlen(pathElement) == 0)
      return dir;

   if(strlen(pathElement) > 11)
      return NULL;

   char name[9];
   char ext[4];
   if(!strsplit(name, ext, pathElement, '.')) {
      strcpy(name, pathElement);
      ext[0] = '\0';
   }

   if(dir == NULL) {
      return fat_find_in_root((char*)name, (char*)ext);
   } else {
      return fat_find_in_dir(dir->firstClusterNo, (char*)name, (char*)ext);
   }
}

fat_dir_t *fat_parse_path(char *path) {
   char *pathRemaining = malloc(strlen(path));
   char *tmp = malloc(strlen(path));
   char *pathElement = malloc(strlen(path));
   strcpy(pathRemaining, path);

   int i = 0; // arg no

   fat_dir_t *curDir = NULL; // NULL = root

   while(strsplit(pathElement, tmp, pathRemaining, '/')) {
      strcpy(pathRemaining, tmp);

      fat_dir_t *lastDir = curDir;
      curDir = fat_follow_path_chain(pathElement, curDir);
      
      if(curDir == lastDir) {
         if(i == 0)
            curDir = NULL; // begins with /, set environment to root
         // otherwise ignore
      } else {
         free((uint32_t)lastDir, sizeof(fat_dir_t));

         if(curDir == NULL) {
            free((uint32_t)tmp, strlen(path));
            free((uint32_t)pathRemaining, strlen(path));
            free((uint32_t)pathElement, strlen(path));
            return NULL; // file not found
         }
      }

      // relative to current directory
      if(curDir == NULL)
         gui_writenum(0, 0);
      else
         gui_writenum(curDir->firstClusterNo, 0);
      
      gui_writestr(":", 0);
      gui_writestr(pathElement, 0);
      gui_drawchar('\n', 0);

      i++;
   }
   
   // if(strlen(pathRemaining) == 0) // ends with trailing slash
   fat_dir_t *lastDir = curDir;
   curDir = fat_follow_path_chain(pathRemaining, curDir);
   if(curDir == NULL)
         gui_writenum(0, 0);
      else
         gui_writenum(curDir->firstClusterNo, 0);
      
   gui_writestr(":", 0);
   gui_writestr(pathRemaining, 0);
   gui_drawchar('\n', 0);

   free((uint32_t)tmp, strlen(path));
   free((uint32_t)pathRemaining, strlen(path));
   free((uint32_t)pathElement, strlen(path));

   if(lastDir != curDir)
      free((uint32_t)lastDir, sizeof(fat_dir_t));

   return curDir; // note that NULL = file not found or root
}

void fat_test() {
   char *path = malloc(40);
   strcpy(path, "/bmp/file.bmp");
   fat_dir_t *dir = fat_parse_path(path);
   gui_writenum(dir->fileSize, 0);
   gui_drawchar('\n', 0);
   fat_read_file(dir->firstClusterNo, dir->fileSize);

   // example: find and load file "/bmp/file.bmp"
   //fat_dir_t *bmpDir = fat_find_in_root("bmp", "");
   //fat_dir_t *file = fat_find_in_dir(bmpDir->firstClusterNo, "file", "bmp");
   //fat_read_file(file->firstClusterNo, file->fileSize);
}