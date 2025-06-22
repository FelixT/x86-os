// fat16 driver
// https://wiki.osdev.org/FAT16
// https://en.wikipedia.org/wiki/Design_of_the_FAT_file_system
// http://www.c-jump.com/CIS24/Slides/FAT/FAT.html

#include "fat.h"
#include "lib/string.h"
#include "windowmgr.h"
#include "events.h"

// 8.3 directory structure

uint32_t baseAddr = 256000;

fat_bpb_t *fat_bpb = NULL;
fat_ebr_t *fat_ebr;
uint32_t noSectors;
uint32_t noClusters;
uint32_t rootSize;
uint32_t rootSector;
uint32_t firstDataSector;


void fat_get_info() {
   // get drive formatting info
   free((uint32_t)fat_bpb, sizeof(fat_bpb_t) + sizeof(fat_ebr_t));

   uint8_t *buf = ata_read_exact(true, true, baseAddr, sizeof(fat_bpb_t) + sizeof(fat_ebr_t));

   fat_bpb = (fat_bpb_t*)(&buf[0]);
   fat_ebr = (fat_ebr_t*)(&buf[sizeof(fat_bpb_t)]); // immediately after bpb

   noSectors = (fat_bpb->noSectors == 0) ? fat_bpb->largeNoSectors : fat_bpb->noSectors;
   noClusters = noSectors/fat_bpb->sectorsPerCluster;

   rootSize = ((fat_bpb->noRootEntries * 32) + (fat_bpb->bytesPerSector - 1)) / fat_bpb->bytesPerSector; // in sectors
   rootSector = fat_bpb->noReservedSectors + fat_bpb->noTables * fat_bpb->sectorsPerFat;
   firstDataSector = rootSector + rootSize;
   debug_printf("%u clusters %u sectors\n", noClusters, noSectors);
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
   
   gui_printf(" <%i>\n", 0, fat_dir->firstClusterNo);
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

fat_dir_t *fat_read_root() {
   uint32_t rootDirAddr = rootSector*fat_bpb->bytesPerSector + baseAddr;

   return (fat_dir_t*)ata_read_exact(true, true, rootDirAddr, sizeof(fat_dir_t)*fat_bpb->noRootEntries);
}

void fat_setup() {
   
   fat_get_info();

   fat_dir_t *items = fat_read_root();
   for(int i = 0; i < fat_bpb->noRootEntries; i++) {
      if(items[i].filename[0] == 0) break;
      fat_parse_dir_entry(&items[i]);
   }
   free((uint32_t)items, sizeof(fat_dir_t) * fat_bpb->noRootEntries);

}

int fat_get_dir_size(uint16_t clusterNo) {
   uint32_t dirFirstSector = ((clusterNo - 2) * fat_bpb->sectorsPerCluster) + firstDataSector;

   uint32_t dirAddr = baseAddr + dirFirstSector * fat_bpb->bytesPerSector;
   uint32_t dirSize = fat_bpb->sectorsPerCluster * fat_bpb->bytesPerSector;

   // Read the whole directory cluster at once
   uint8_t *dirBuf = ata_read_exact(true, true, dirAddr, dirSize);

   int entries = dirSize / sizeof(fat_dir_t);
   int count = 0;
   for(int i = 0; i < entries; i++) {
      fat_dir_t *dir = (fat_dir_t*)(dirBuf + i * sizeof(fat_dir_t));
      if(dir->filename[0] == 0)
         break; // no more files/dirs in directory
      count++;
   }

   free((uint32_t)dirBuf, dirSize);
   return count;
}

void fat_read_dir(uint16_t clusterNo, fat_dir_t *items) {
   uint32_t dirFirstSector = ((clusterNo - 2) * fat_bpb->sectorsPerCluster) + firstDataSector;

   uint32_t dirAddr = baseAddr + dirFirstSector * fat_bpb->bytesPerSector;
   uint32_t dirSize = fat_bpb->sectorsPerCluster * fat_bpb->bytesPerSector;

   // Read the whole directory cluster at once
   uint8_t *dirBuf = ata_read_exact(true, true, dirAddr, dirSize);

   int entries = dirSize / sizeof(fat_dir_t);
   for(int i = 0; i < entries; i++) {
      fat_dir_t *dir = (fat_dir_t*)(dirBuf + i * sizeof(fat_dir_t));
      items[i] = *dir;
      if(dir->filename[0] == 0)
         break; // no more files/dirs in directory
   }

   free((uint32_t)dirBuf, dirSize);
}

fat_dir_t *fat_find_in_root(char* filename, char* extension) {
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
   uint32_t dirFirstSector = ((clusterNo - 2) * fat_bpb->sectorsPerCluster) + firstDataSector;

   uint32_t dirAddr = baseAddr + dirFirstSector * fat_bpb->bytesPerSector;

   // read a whole cluster (directory) at once
   uint32_t dirSize = fat_bpb->sectorsPerCluster * fat_bpb->bytesPerSector;
   uint8_t *dirBuf = ata_read_exact(true, true, dirAddr, dirSize);

   int entries = dirSize / sizeof(fat_dir_t);
   for(int i = 0; i < entries; i++) {
      fat_dir_t *fat_dir = (fat_dir_t*)(dirBuf + i * sizeof(fat_dir_t));
      if(fat_dir->filename[0] == 0)
         break; // no more files/dirs in directory

      if(fat_entry_matches_filename(fat_dir, filename, extension)) {
         // allocate and copy the found entry to return
         fat_dir_t *result = malloc(sizeof(fat_dir_t));
         *result = *fat_dir;
         free((uint32_t)dirBuf, dirSize);
         return result;
      }
   }

   free((uint32_t)dirBuf, dirSize);
   return NULL;
}

bool fat_update_in_dir(uint16_t clusterNo, char* filename, char* extension, fat_dir_t *dir) {
   uint32_t dirFirstSector = ((clusterNo - 2) * fat_bpb->sectorsPerCluster) + firstDataSector;

   uint32_t dirAddr = baseAddr + dirFirstSector * fat_bpb->bytesPerSector;

   // read a whole cluster (directory) at once
   uint32_t dirSize = fat_bpb->sectorsPerCluster * fat_bpb->bytesPerSector;
   uint8_t *dirBuf = ata_read_exact(true, true, dirAddr, dirSize);
   int entries = dirSize / sizeof(fat_dir_t);

   for(int i = 0; i < entries; i++) {
      fat_dir_t *fat_dir = (fat_dir_t*)(dirBuf + i * sizeof(fat_dir_t));
      if(fat_dir->filename[0] == 0)
         break; // no more files/dirs in directory

      if(fat_entry_matches_filename(fat_dir, filename, extension)) {
         memcpy_fast(dirBuf + i * sizeof(fat_dir_t), dir, sizeof(fat_dir_t)); // update the entry
         ata_write_exact(true, true, dirAddr, dirBuf, dirSize);
         free((uint32_t)dirBuf, dirSize);
         return true;
      }
   }

   debug_printf("Error: file not found for path '%s'\n", filename);
   free((uint32_t)dirBuf, dirSize);
   return false;
}

int fat_shrink_cluster_chain(uint16_t startCluster, uint32_t oldSize, uint32_t newSize) {
   uint32_t bytesPerCluster = fat_bpb->sectorsPerCluster * fat_bpb->bytesPerSector;
   uint32_t oldClusters = (oldSize + bytesPerCluster - 1) / bytesPerCluster;
   uint32_t newClusters = (newSize + bytesPerCluster - 1) / bytesPerCluster;

   if(newClusters >= oldClusters)
      return 0;

   uint32_t fatTableAddr = baseAddr + fat_bpb->noReservedSectors * fat_bpb->bytesPerSector;
   uint8_t *fatTable = ata_read_exact(true, true, fatTableAddr, 2 * noClusters);

   // find last cluster in chain
   uint16_t cur = startCluster;
   for(uint32_t i = 1; i < newClusters; i++) {
      uint16_t next = ((uint16_t*)fatTable)[cur];
      if(next >= 0xFFF8 || next == 0xFFF7) break;
      cur = next;
   }

   uint16_t toFree = ((uint16_t*)fatTable)[cur];
   ((uint16_t*)fatTable)[cur] = 0xFFFF; // mark cur as end of chain

   // free remaining clusters in chain
   int freed = 0;
   while(toFree < 0xFFF8 && toFree != 0xFFF7 && toFree != 0) {
      uint16_t next = ((uint16_t*)fatTable)[toFree];
      ((uint16_t*)fatTable)[toFree] = 0; // Mark as free
      toFree = next;
      freed++;
   }

   ata_write_exact(true, true, fatTableAddr, fatTable, 2 * noClusters);
   free((uint32_t)fatTable, 2 * noClusters);

   return freed;
}

int fat_extend_cluster_chain(uint16_t startCluster, uint32_t oldSize, uint32_t newSize) {
   uint32_t bytesPerCluster = fat_bpb->sectorsPerCluster * fat_bpb->bytesPerSector;
   uint32_t oldClusters = (oldSize + bytesPerCluster - 1) / bytesPerCluster;
   uint32_t newClusters = (newSize + bytesPerCluster - 1) / bytesPerCluster;

   if(newClusters <= oldClusters)
      return 0;

   uint32_t fatTableAddr = baseAddr + fat_bpb->noReservedSectors * fat_bpb->bytesPerSector;
   uint8_t *fatTable = ata_read_exact(true, true, fatTableAddr, 2 * noClusters);

   // find last cluster in chain
   uint16_t lastCluster = startCluster;
   for(uint32_t i = 1; i < oldClusters; i++) {
      uint16_t next = ((uint16_t*)fatTable)[lastCluster];
      if(next >= 0xFFF8 || next == 0xFFF7) break;
      lastCluster = next;
   }

   // allocate new clusters: find free cluster (entry=0) and link previous cluster to it
   uint16_t prev = lastCluster;
   int allocated = 0;
   for(uint32_t i = oldClusters; i < newClusters; i++) {
      uint16_t freeCluster = 2;
      while(freeCluster < noClusters && ((uint16_t*)fatTable)[freeCluster] != 0)
         freeCluster++;
      if(freeCluster >= noClusters) {
         free((uint32_t)fatTable, 2 * noClusters);
         return -1; // no free clusters
      }
      ((uint16_t*)fatTable)[prev] = freeCluster;
      // mark new cluster as end of chain (0xFFFF)
      ((uint16_t*)fatTable)[freeCluster] = 0xFFFF;
      prev = freeCluster;
      allocated++;
   }

   ata_write_exact(true, true, fatTableAddr, fatTable, 2 * noClusters);
   free((uint32_t)fatTable, 2 * noClusters);

   return allocated;
}

void fat_new_file(char *path, uint8_t *buffer, uint32_t size) {
   (void)buffer;
   (void)size;

   // get directory cluster
   fat_dir_t *dir = fat_parse_path(path, false);
   if(dir == NULL) {
      debug_printf("Error: directory not found for path '%s'\n", path);
      return;
   }

   fat_dir_t *filedir = malloc(sizeof(fat_dir_t));
   memset(filedir, 0, sizeof(fat_dir_t));

   // extract filename from path
   char filenamefull[12];
   char tmp[12];
   char filename[9];
   char extension[4];
   // split at last /
   while(path[0] != '\0') {
      strsplit(tmp, path, path, '/');
      strcpy_fixed(filenamefull, tmp, 12);
   }
   strsplit(filename, extension, filenamefull, '.'); // split at first dot
   strtoupper(filename, filename);
   strtoupper(extension, extension);
   memset(filedir->filename, ' ', 11);
   strcpy_fixed((char*)filedir->filename, filename, strlen(filename));
   filedir->filename[strlen(filename)] = ' ';
   strcpy_fixed((char*)filedir->filename+8, extension, strlen(extension));
   filedir->attributes = 0x20; // file
   filedir->firstClusterNo = 0; // to be set later
   filedir->fileSize = 0;

   // find free cluster
   uint32_t fatTableAddr = baseAddr + fat_bpb->noReservedSectors * fat_bpb->bytesPerSector;
   uint8_t *fatTable = ata_read_exact(true, true, fatTableAddr, 2 * noClusters);
   uint16_t freeCluster = 2;
   while(freeCluster < noClusters && ((uint16_t*)fatTable)[freeCluster] != 0)
      freeCluster++;
   if(freeCluster >= noClusters) {
      free((uint32_t)fatTable, 2 * noClusters);
      debug_printf("Error: no free clusters\n");
      return;
   }
   ((uint16_t*)fatTable)[freeCluster] = 0xFFFF; // mark as end of chain
   ((uint16_t*)fatTable)[dir->firstClusterNo] = freeCluster; // link to directory
   filedir->firstClusterNo = freeCluster;
   debug_printf("Found free cluster %u\n", freeCluster);

   // find free entry in directory
   uint32_t dirFirstSector = ((dir->firstClusterNo - 2) * fat_bpb->sectorsPerCluster) + firstDataSector;
   uint32_t dirAddr = baseAddr + dirFirstSector * fat_bpb->bytesPerSector;
   uint32_t dirSize = fat_bpb->sectorsPerCluster * fat_bpb->bytesPerSector;
   uint8_t *dirBuf = ata_read_exact(true, true, dirAddr, dirSize);
   int entries = dirSize / sizeof(fat_dir_t);

   for(int i = 0; i < entries; i++) {
      fat_dir_t *fat_dir = (fat_dir_t*)(dirBuf + i * sizeof(fat_dir_t));
      if(memcmp((char*)fat_dir->filename, (char*)filedir->filename, 11) == 0) {
         debug_printf("Error: file already exists\n");
         free((uint32_t)fatTable, 2 * noClusters);
         free((uint32_t)dirBuf, dirSize);
         free((uint32_t)filedir, sizeof(fat_dir_t));
         return;
      }
      if(fat_dir->filename[0] == '\0') {
         // found a free entry
         debug_printf("Found free entry %u\n", i);
         memcpy_fast(dirBuf + i * sizeof(fat_dir_t), filedir, sizeof(fat_dir_t)); // copy the new entry
         break;
      }
   }
   debug_writestr("Updating directory\n");
   ata_write_exact(true, true, dirAddr, dirBuf, dirSize);

   debug_writestr("Updating FAT table\n");
   ata_write_exact(true, true, fatTableAddr, fatTable, 2 * noClusters);
   free((uint32_t)fatTable, 2 * noClusters);


   return;
}

void fat_write_file(char *path, uint8_t *buffer, uint32_t size) {
   fat_dir_t *dir = fat_parse_path(path, true);
   if(dir == NULL) {
      debug_printf("Error: file not found for path '%s'\n", path);
      return;
   }

   uint32_t clusterNo = dir->firstClusterNo;
   uint32_t oldsize = dir->fileSize;
   uint32_t firstDataSector = rootSector + rootSize;

   debug_printf("Writing file '%s' to cluster %u\n", path, clusterNo);
   debug_printf("Changing size from %u to %u\n", oldsize, size);

   if(size < oldsize) {
      int freed = fat_shrink_cluster_chain(clusterNo, oldsize, size);
      if(freed < 0) {
         debug_printf("Error shrinking cluster chain\n");
         return;
      }
      debug_printf("Freed %u clusters\n", freed);
   } else if(size > oldsize) {
      int allocated = fat_extend_cluster_chain(clusterNo, oldsize, size);
      if(allocated < 0) {
         debug_printf("Error extending cluster chain\n");
         return;
      }
      debug_printf("Allocated %u clusters\n", allocated);
   }

   // read entire fat table again
   uint32_t fatTableAddr = baseAddr + fat_bpb->noReservedSectors*fat_bpb->bytesPerSector;
   uint8_t *fatTable = ata_read_exact(true, true, fatTableAddr, 2*noClusters);

   // get number of clusters after possible extension/shrink
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
   uint32_t bytesWritten = 0;
   uint16_t curCluster = clusterNo;
   while(bytesWritten < size) {
      // Calculate the first sector of this cluster
      uint32_t diskSector = ((curCluster - 2) * fat_bpb->sectorsPerCluster) + firstDataSector;
      uint32_t sectorAddr = baseAddr + diskSector * fat_bpb->bytesPerSector;
      uint32_t toWrite = fat_bpb->sectorsPerCluster * fat_bpb->bytesPerSector;
      ata_write_exact(true, true, sectorAddr, buffer + bytesWritten, toWrite);
      bytesWritten += toWrite;
      // Get next cluster in chain
      uint16_t next = ((uint16_t*)fatTable)[curCluster];
      if(next >= 0xFFF8 || next == 0xFFF7)
         break;
      curCluster = next;
   }

   free((uint32_t)fatTable, 2*noClusters);

   // Update the file size in the directory entry
   dir->fileSize = size;
   char name[9];
   char extension[4];
   strcpy_fixed((char*)name, (char*)dir->filename, 8);
   strcpy_fixed((char*)extension, (char*)dir->filename+8, 3);
   strsplit((char*)name, NULL, (char*)name, ' '); // null terminate at first space
   strsplit((char*)extension, NULL, (char*)extension, ' '); // null terminate at first space
   fat_dir_t *parentDir = fat_parse_path(path, false);
   debug_printf("Updating file '%s' in directory %u\n", path, parentDir->firstClusterNo);
   if(!fat_update_in_dir(parentDir->firstClusterNo, name, extension, dir))
      debug_printf("Error updating directory entry\n");

   free((uint32_t)dir, sizeof(fat_dir_t));   
}

bool fat_new_dir(char *path) {
   char dirname[256];
   char parentpath[256];
   strsplit_last(parentpath, dirname, path, '/');
   if(strcmp(parentpath, ""))
      strcpy(parentpath, "/");
   if(strlen(dirname) > 8) {
      debug_printf("Dir name too long\n");
      return false;
   }
   strtoupper(dirname, dirname);
   debug_printf("Creating dir %s in parent %s\n", dirname, parentpath);
   fat_dir_t *parent = fat_parse_path(parentpath, true);
   if(parent == NULL) {
      debug_printf("Parent not found\n", path);
      return false;
   }
   // create dir
   fat_dir_t *dir = malloc(sizeof(fat_dir_t));
   memset(dir, 0, sizeof(fat_dir_t));

   memset(dir->filename, ' ', 11);
   strcpy_fixed((char*)dir->filename, dirname, 8);
   dir->filename[strlen(dirname)] = ' ';
   dir->attributes = 0x10; // directory
   dir->firstClusterNo = 0; // to be set later
   dir->fileSize = 0;

   // find free cluster
   uint32_t firstDataSector = rootSector + rootSize;
   uint32_t fatTableAddr = baseAddr + fat_bpb->noReservedSectors * fat_bpb->bytesPerSector;
   uint8_t *fatTable = ata_read_exact(true, true, fatTableAddr, 2 * noClusters);
   uint16_t freeCluster = 2;
   while(freeCluster < noClusters && ((uint16_t*)fatTable)[freeCluster] != 0)
      freeCluster++;
   if(freeCluster >= noClusters) {
      debug_printf("Error: no free clusters\n");
      free((uint32_t)fatTable, 2 * noClusters);
      free((uint32_t)dir, sizeof(fat_dir_t));
      return false;
   }
   ((uint16_t*)fatTable)[freeCluster] = 0xFFFF; // mark as end of chain
   dir->firstClusterNo = freeCluster;
   debug_printf("Found free cluster %u\n", freeCluster);
   // clear cluster
   uint32_t newDirSector = ((freeCluster - 2) * fat_bpb->sectorsPerCluster) + firstDataSector;
   uint32_t newDirAddr = baseAddr + newDirSector * fat_bpb->bytesPerSector;
   uint32_t clusterSize = fat_bpb->sectorsPerCluster * fat_bpb->bytesPerSector;
    
    // .
   fat_dir_t dot_entry;
   memset(&dot_entry, 0, sizeof(fat_dir_t));
   memset(dot_entry.filename, ' ', 11);
   dot_entry.filename[0] = '.';
   dot_entry.attributes = 0x12;  // hidden directory attribute
   dot_entry.firstClusterNo = freeCluster;
   dot_entry.fileSize = 0;
    
   // ..
   fat_dir_t dotdot_entry;
   memset(&dotdot_entry, 0, sizeof(fat_dir_t));
   memset(dotdot_entry.filename, ' ', 11);
   dotdot_entry.filename[0] = '.';
   dotdot_entry.filename[1] = '.';
   dotdot_entry.attributes = 0x12;  // hidden directory attribute
   dotdot_entry.firstClusterNo = strcmp(parentpath, "/") ? 0 : parent->firstClusterNo;
   dotdot_entry.fileSize = 0;

    // zero new cluster
   uint8_t *clusterBuf = malloc(clusterSize);
   memset(clusterBuf, 0, clusterSize);
   // add dot entries
   memcpy(clusterBuf, &dot_entry, sizeof(fat_dir_t));
   memcpy(clusterBuf + sizeof(fat_dir_t), &dotdot_entry, sizeof(fat_dir_t));

   ata_write_exact(true, true, newDirAddr, clusterBuf, clusterSize);
   free((uint32_t)clusterBuf, clusterSize);


   // find free entry in parent directory
   uint32_t dirFirstSector = ((parent->firstClusterNo - 2) * fat_bpb->sectorsPerCluster) + firstDataSector;
   uint32_t dirAddr = baseAddr + dirFirstSector * fat_bpb->bytesPerSector;
   uint32_t dirSize = fat_bpb->sectorsPerCluster * fat_bpb->bytesPerSector;
   uint8_t *dirBuf = ata_read_exact(true, true, dirAddr, dirSize);
   int entries = dirSize / sizeof(fat_dir_t);

   for(int i = 0; i < entries; i++) {
      fat_dir_t *fat_dir = (fat_dir_t*)(dirBuf + i * sizeof(fat_dir_t));
      if(memcmp((char*)fat_dir->filename, (char*)dir->filename, 11) == 0) {
         debug_printf("Error: dir already exists\n");
         free((uint32_t)fatTable, 2 * noClusters);
         free((uint32_t)dirBuf, dirSize);
         free((uint32_t)dir, sizeof(fat_dir_t));
         return false;
      }
      if(fat_dir->filename[0] == '\0') {
         // found a free entry
         debug_printf("Found free entry %u\n", i);
         memcpy_fast(dirBuf + i * sizeof(fat_dir_t), dir, sizeof(fat_dir_t)); // copy the new entry
         break;
      }
   }
   debug_writestr("Updating directory\n");
   ata_write_exact(true, true, dirAddr, dirBuf, dirSize);

   debug_writestr("Updating FAT table\n");
   ata_write_exact(true, true, fatTableAddr, fatTable, 2 * noClusters);
   free((uint32_t)fatTable, 2 * noClusters);

   return true;

}

typedef struct {
   uint16_t clusterNo;
   uint32_t size; // read size
   bool readEntireFile;
   uint8_t *buffer;
   uint32_t bufferSize;
   void *callback;
   int currentCluster;
   int readCount; // no clusters read
   uint32_t readBytes; // no bytes read
   uint8_t *fatTable;
   int task;
} fat_read_file_state_t;

void fat_read_file_callback(registers_t *regs, void *msg) {
   fat_read_file_state_t *state = (fat_read_file_state_t*)msg;

   // read all sectors of cluster
   for(int x = 0; x < 8; x++) { // do in batches of 8 clusters
      uint32_t currentClusterSector = ((state->currentCluster - 2) * fat_bpb->sectorsPerCluster) + firstDataSector;
      uint32_t diskAddr = baseAddr + currentClusterSector * fat_bpb->bytesPerSector;
      uint8_t *clusterBuf = ata_read_exact(true, true, diskAddr, fat_bpb->sectorsPerCluster * fat_bpb->bytesPerSector);
      for(int i = 0; i < fat_bpb->sectorsPerCluster; i++) {
         uint8_t *buf = (uint8_t*)(clusterBuf + i * fat_bpb->bytesPerSector); // sector buf
         uint32_t memOffset = fat_bpb->bytesPerSector * (state->readCount*fat_bpb->sectorsPerCluster + i);
         // copy to master buffer
         for(int b = 0; b < fat_bpb->bytesPerSector; b++) {
            if(!state->readEntireFile && state->readBytes >= state->size) {
               free((uint32_t)clusterBuf, fat_bpb->sectorsPerCluster * fat_bpb->bytesPerSector);
               (*(void(*)(void*,int))state->callback)((void*)regs, state->task);
               free((uint32_t)state, sizeof(fat_read_file_state_t));
               return;
            }
            state->buffer[memOffset + b] = buf[b];
            state->readBytes++;

            if(state->readBytes >= state->bufferSize) {
               free((uint32_t)clusterBuf, fat_bpb->sectorsPerCluster * fat_bpb->bytesPerSector);
               (*(void(*)(void*,int))state->callback)((void*)regs, state->task);
               free((uint32_t)state, sizeof(fat_read_file_state_t));
               return;
            }
         }
      }

      free((uint32_t)clusterBuf, fat_bpb->sectorsPerCluster * fat_bpb->bytesPerSector);

      // check if theres more clusters to read
      uint16_t tableVal = ((uint16_t*)state->fatTable)[state->currentCluster];
      if(tableVal >= 0xFFF8) {
         // no more clusters in chain
         
         // call callback
         (*(void(*)(void*,int))state->callback)((void*)regs, state->task);
         return;
      } else if(tableVal == 0xFFF7) {
         // bad cluster
         debug_printf("FAT error: Hit bad cluster\n");
         free((uint32_t)state, sizeof(fat_read_file_state_t));
         return;
      } else { 
         state->currentCluster = tableVal; // table value is the next cluster
         state->readCount++;
      }
   
   }

   events_add(1, &fat_read_file_callback, (void*)state, -1);
   
}

uint8_t *fat_read_file_chunked(uint16_t clusterNo, uint32_t size, void *callback, int task) {
   
   fat_read_file_state_t *state = (fat_read_file_state_t*)malloc(sizeof(fat_read_file_state_t));
   state->clusterNo = clusterNo;
   state->size = size;
   state->callback = callback;
   state->task = task;
   state->readEntireFile = (size == 0);

   // read entire fat table
   uint32_t fatTableAddr = baseAddr + fat_bpb->noReservedSectors*fat_bpb->bytesPerSector;
   state->fatTable = ata_read_exact(true, true, fatTableAddr, 2*noClusters);

   // get no clusters
   uint16_t c = clusterNo;
   uint16_t clusterCount = 1;
   while(true) {
      uint16_t tableVal = ((uint16_t*)state->fatTable)[c];
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

   state->bufferSize = (state->readEntireFile) ? fileSizeDisk : size;
   state->buffer = malloc(state->bufferSize);
   state->currentCluster = state->clusterNo;
   state->readCount = 0;
   state->readBytes = 0;

   debug_printf("Buffer size %u task %u\n", state->bufferSize, state->task);

   // map to task
   if(task > -1) {
      for(uint32_t i = (uint32_t)state->buffer/0x1000; i < ((uint32_t)state->buffer+state->bufferSize+0xFFF)/0x1000; i++)
         map(gettasks()[task].page_dir, i*0x1000, i*0x1000, 1, 1);
   }

   // kick off read event chain
   events_add(1, &fat_read_file_callback, (void*)state, -1);

   return state->buffer;
}


uint8_t *fat_read_file(uint16_t clusterNo, uint32_t size) {

   bool readEntireFile = (size == 0); // read entry entry as stored on disk or the size supplied

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

            if((int)byte >= allocate) {
               free((uint32_t)clusterBuf, fat_bpb->sectorsPerCluster * fat_bpb->bytesPerSector);
               break; // reached the end of the file
            }
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

   debug_printf("Loaded %u clusters into 0x%h\n", clusterCount, (uint32_t)fileContents);

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

fat_dir_t *fat_parse_path(char *path, bool isFile) {
   char *pathRemaining = malloc(strlen(path)+1);
   char *tmp = malloc(strlen(path)+1);
   char *pathElement = malloc(strlen(path)+1);
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
            free((uint32_t)tmp, strlen(path)+1);
            free((uint32_t)pathRemaining, strlen(path)+1);
            free((uint32_t)pathElement, strlen(path)+1);
            return NULL; // file not found
         }
      }

      i++;
   }

   if(!isFile) {
      free((uint32_t)tmp, strlen(path)+1);
      free((uint32_t)pathRemaining, strlen(path)+1);
      free((uint32_t)pathElement, strlen(path)+1);
      return curDir; // don't follow into the file itself
   }
   
   // if(strlen(pathRemaining) == 0) // ends with trailing slash
   fat_dir_t *lastDir = curDir;
   curDir = fat_follow_path_chain(pathRemaining, curDir);

   free((uint32_t)tmp, strlen(path)+1);
   free((uint32_t)pathRemaining, strlen(path)+1);
   free((uint32_t)pathElement, strlen(path)+1);

   if(lastDir != curDir)
      free((uint32_t)lastDir, sizeof(fat_dir_t));

   return curDir; // note that NULL = file not found or root
}

fat_bpb_t fat_get_bpb() {
   return *fat_bpb;
}