// fat16 driver
// https://wiki.osdev.org/FAT16
// https://en.wikipedia.org/wiki/Design_of_the_FAT_file_system
// http://www.c-jump.com/CIS24/Slides/FAT/FAT.html

#include "fat.h"
#include "string.h"

// 8.3 directory structure

uint32_t baseAddr = 64000;

fat_bpb_t *fat_bpb = NULL;
fat_ebr_t *fat_ebr;
uint32_t noSectors;
uint32_t noClusters;

void fat_get_info() {
   // get drive formatting info
   free((uint32_t)fat_bpb, sizeof(fat_bpb_t) + sizeof(fat_ebr_t));

   uint8_t *buf = ata_read_exact(true, true, baseAddr, sizeof(fat_bpb_t) + sizeof(fat_ebr_t));

   fat_bpb = (fat_bpb_t*)(&buf[0]);
   fat_ebr = (fat_ebr_t*)(&buf[sizeof(fat_bpb_t)]); // immediately after bpb

   noSectors = (fat_bpb->noSectors == 0) ? fat_bpb->largeNoSectors : fat_bpb->noSectors;
   noClusters = noSectors/fat_bpb->sectorsPerCluster;
}

void fat_parse_dir_entry(fat_dir_t *fat_dir) {
   if(fat_dir->firstClusterNo < 2) return;
   if((fat_dir->attributes & 0x02) == 0x02) return; // hidden

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
   
   gui_writestr(" <", 0);
   gui_writenum(fat_dir->firstClusterNo, 0);
   gui_writestr(">\n", 0);
}

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

void fat_read_root(fat_dir_t *items) {
   uint32_t rootSector = fat_bpb->noReservedSectors + fat_bpb->noTables*fat_bpb->sectorsPerFat;
   uint32_t rootDirAddr = rootSector*fat_bpb->bytesPerSector + baseAddr;

   //gui_writeuint(rootDirAddr, 0);
   gui_writestr("\n", 0);

   uint32_t offset = 0;

   // get each file/dir in root
   for(int i = 0; i < fat_bpb->noRootEntries; i++) {
      fat_dir_t *dir = (fat_dir_t*)ata_read_exact(true, true, rootDirAddr + offset, sizeof(fat_dir_t));
      items[i] = *dir;

      if(dir->filename[0] == 0) {
         // no more files/dirs in directory
         free((uint32_t)dir, sizeof(fat_dir_t));
         break;
      }

      offset+=32; // each entry is 32 bytes

      free((uint32_t)dir, sizeof(fat_dir_t));
   }

}

void fat_setup() {
   
   fat_get_info();

   fat_dir_t *items = malloc(32 * fat_bpb->noRootEntries);
   fat_read_root(items);
   for(int i = 0; i < fat_bpb->noRootEntries; i++) {
      if(items[i].filename[0] == 0) break;
      fat_parse_dir_entry(&items[i]);
   }
   free((uint32_t)items, 32 * fat_bpb->noRootEntries);

}

int fat_get_dir_size(uint16_t clusterNo) {
   uint32_t rootSize = ((fat_bpb->noRootEntries * 32) + (fat_bpb->bytesPerSector - 1)) / fat_bpb->bytesPerSector; // in sectors
   uint32_t rootSector = fat_bpb->noReservedSectors + fat_bpb->noTables*fat_bpb->sectorsPerFat;
   uint32_t firstDataSector = rootSector + rootSize;
   uint32_t dirFirstSector = ((clusterNo - 2) * fat_bpb->sectorsPerCluster) + firstDataSector;

   uint32_t offset = 0;

   uint32_t dirAddr = baseAddr + dirFirstSector*fat_bpb->bytesPerSector;

   bool loop = true;
   // get each file/dir in dir
   while(loop) {
      fat_dir_t *dir = (fat_dir_t*)ata_read_exact(true, true, dirAddr + offset, sizeof(fat_dir_t));
      
      if(dir->filename[0] == 0) loop = false; // no more files/dirs in directory

      offset+=32; // each entry is 32 bytes
      free((uint32_t)dir, sizeof(fat_dir_t));
   }
   return offset/32;
}

void fat_read_dir(uint16_t clusterNo, fat_dir_t *items) {
   uint32_t rootSize = ((fat_bpb->noRootEntries * 32) + (fat_bpb->bytesPerSector - 1)) / fat_bpb->bytesPerSector; // in sectors
   uint32_t rootSector = fat_bpb->noReservedSectors + fat_bpb->noTables*fat_bpb->sectorsPerFat;
   uint32_t firstDataSector = rootSector + rootSize;
   uint32_t dirFirstSector = ((clusterNo - 2) * fat_bpb->sectorsPerCluster) + firstDataSector;

   uint32_t offset = 0;

   uint32_t dirAddr = baseAddr + dirFirstSector*fat_bpb->bytesPerSector;

   int i = 0;
   bool loop = true;
   // get each file/dir in dir
   while(loop) {
      fat_dir_t *dir = (fat_dir_t*)ata_read_exact(true, true, dirAddr + offset, sizeof(fat_dir_t));
      items[i] = *dir;

      if(dir->filename[0] == 0) loop = false; // no more files/dirs in directory

      offset+=32; // each entry is 32 bytes
      free((uint32_t)dir, sizeof(fat_dir_t));

      i++;
   }

}

fat_dir_t *fat_find_in_root(char* filename, char* extension) {
   uint32_t rootSector = fat_bpb->noReservedSectors + fat_bpb->noTables*fat_bpb->sectorsPerFat;
   uint32_t rootDirAddr = rootSector*fat_bpb->bytesPerSector + baseAddr;

   // get each file/dir in root

   // read entire root in
   uint8_t *rootBuf = ata_read_exact(true, true, rootDirAddr, sizeof(fat_dir_t) * fat_bpb->noRootEntries);

   for(int i = 0; i < fat_bpb->noRootEntries; i++) {
      fat_dir_t *fat_dir = (fat_dir_t*)(rootBuf + i * sizeof(fat_dir_t));
      if(fat_dir->filename[0] == 0) break; // no more files/dirs in directory

      if(fat_entry_matches_filename(fat_dir, filename, extension))
         return fat_dir;
   }

   free((uint32_t)rootBuf, sizeof(fat_dir_t) * fat_bpb->noRootEntries);

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

uint8_t *fat_read_file(uint16_t clusterNo, uint32_t size) {

   bool readEntireFile = (size == 0); // read entry entry as stored on disk or the size supplied

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

   uint32_t fileSizeDisk = clusterCount*fat_bpb->sectorsPerCluster*fat_bpb->bytesPerSector; // size on disk

   int allocate = (readEntireFile) ? fileSizeDisk : size;
   uint8_t *fileContents = malloc(allocate);

   uint32_t byte = 0;
   int cluster = 0;
   while(true) { // until we reach the end of the cluster chain or byte >= size
      // read all sectors of cluster
      uint32_t diskAddr = baseAddr + (fileFirstSector + cluster*fat_bpb->sectorsPerCluster) * fat_bpb->bytesPerSector;
      uint8_t *clusterBuf = ata_read_exact(true, true, diskAddr, fat_bpb->sectorsPerCluster * fat_bpb->bytesPerSector);
      for(int i = 0; i < fat_bpb->sectorsPerCluster; i++) {
         uint8_t *buf = (uint8_t*)(clusterBuf + i * fat_bpb->bytesPerSector); // sector buf
         uint32_t memOffset = fat_bpb->bytesPerSector * (cluster*fat_bpb->sectorsPerCluster + i);
         // copy to master buffer
         for(int b = 0; b < fat_bpb->bytesPerSector; b++) {
            if(!readEntireFile && byte >= size) break;
            fileContents[memOffset + b] = buf[b];
            byte++;
         }

      }
      free((uint32_t)clusterBuf, fat_bpb->sectorsPerCluster * fat_bpb->bytesPerSector);

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

   gui_writestr("Loaded into 0x", 0);
   gui_writeuint_hex((uint32_t)fileContents, 0);
   gui_writestr(" / ", 0);
   gui_writeuint((uint32_t)fileContents, 0);
   gui_drawchar('\n', 0);

   free((uint32_t)fatTable, 2*noClusters);

   return fileContents;

}

fat_dir_t *fat_follow_path_chain(char *pathElement, fat_dir_t *dir) {
   if(strlen(pathElement) == 0)
      return dir;

   if(strlen(pathElement) > 12)
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
      /*
      if(curDir == NULL)
         gui_writenum(0, 0);
      else
         gui_writenum(curDir->firstClusterNo, 0);
      
      gui_writestr(":", 0);
      gui_writestr(pathElement, 0);
      gui_drawchar('\n', 0);*/

      i++;
   }
   
   // if(strlen(pathRemaining) == 0) // ends with trailing slash
   fat_dir_t *lastDir = curDir;
   curDir = fat_follow_path_chain(pathRemaining, curDir);
   
   /*if(curDir == NULL)
         gui_writenum(0, 0);
      else
         gui_writenum(curDir->firstClusterNo, 0);
      
   gui_writestr(":", 0);
   gui_writestr(pathRemaining, 0);
   gui_drawchar('\n', 0);*/

   free((uint32_t)tmp, strlen(path));
   free((uint32_t)pathRemaining, strlen(path));
   free((uint32_t)pathElement, strlen(path));

   if(lastDir != curDir)
      free((uint32_t)lastDir, sizeof(fat_dir_t));

   return curDir; // note that NULL = file not found or root
}

fat_bpb_t fat_get_bpb() {
   return *fat_bpb;
}