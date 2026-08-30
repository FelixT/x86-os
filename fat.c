// fat16 driver
// https://wiki.osdev.org/FAT16
// https://en.wikipedia.org/wiki/Design_of_the_FAT_file_system
// http://www.c-jump.com/CIS24/Slides/FAT/FAT.html

#include "fat.h"
#include "lib/string.h"
#include "windowmgr.h"
#include "events.h"

// 8.3 directory structure

uint32_t baseAddr = 512000;

fat_bpb_t *fat_bpb = NULL;
uint8_t *fat_table = NULL; // cache
bool *dirty_clusters = NULL; // clusterNo->bool map of which fat table entries are dirty
fat_ebr_t *fat_ebr;
uint32_t noSectors;
uint32_t noClusters;
uint32_t rootSize;
uint32_t rootSector;
uint32_t firstDataSector;
uint32_t tableSize;

bool fat_get_info() {
   // get drive formatting info
   uint32_t infoSize = sizeof(fat_bpb_t) + sizeof(fat_ebr_t);

   uint8_t *buf = ata_read_exact(true, true, baseAddr, infoSize);
   if(!buf) {
      debug_printf("fat_get_info: read failed\n");
      return false;
   }

   fat_bpb_t *bpb = (fat_bpb_t*)(&buf[0]);

   // sanity check the bpb before use in division
   if(bpb->bytesPerSector == 0 || bpb->sectorsPerCluster == 0 || bpb->noTables == 0 || bpb->sectorsPerFat == 0) {
      debug_printf("fat_get_info: invalid bpb\n");
      free((uint32_t)buf, infoSize);
      return false;
   }

   // check/clamp values before commiting
   uint32_t sectors = (bpb->noSectors == 0) ? bpb->largeNoSectors : bpb->noSectors;
   uint32_t root = ((bpb->noRootEntries * 32) + (bpb->bytesPerSector - 1)) / bpb->bytesPerSector; // in sectors
   uint32_t rootSect = bpb->noReservedSectors + bpb->noTables * bpb->sectorsPerFat;
   uint32_t firstData = rootSect + root;

   if(sectors <= firstData) {
      debug_printf("fat_get_info: volume too small, %u sectors\n", sectors);
      free((uint32_t)buf, infoSize);
      return false;
   }

   // clamp cluster count
   uint32_t clusters = (sectors - firstData)/bpb->sectorsPerCluster + 2;
   uint32_t fatEntries = (bpb->sectorsPerFat * bpb->bytesPerSector) / sizeof(uint16_t);
   if(clusters > fatEntries) {
      debug_printf("fat_get_info: clamping %u clusters to fat capacity %u\n", clusters, fatEntries);
      clusters = fatEntries;
   }

   // commit
   free((uint32_t)fat_bpb, infoSize);
   fat_bpb = bpb;
   fat_ebr = (fat_ebr_t*)(&buf[sizeof(fat_bpb_t)]); // immediately after bpb
   noSectors = sectors;
   rootSize = root;
   rootSector = rootSect;
   firstDataSector = firstData;
   noClusters = clusters;

   debug_printf("%u clusters %u sectors\n", noClusters, noSectors);
   return true;
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
   char entryName[9];
   char entryExtension[4];
   strcpy_fixed((char*)entryName, (char*)fat_dir->filename, 8);
   strcpy_fixed((char*)entryExtension, (char*)fat_dir->filename+8, 3);
   strsplit((char*)entryName, NULL, (char*)entryName, ' '); // null terminate at first space
   strsplit((char*)entryExtension, NULL, (char*)entryExtension, ' '); // null terminate at first space
   strtoupper((char*)name); // fat ignores file case
   strtoupper((char*)extension);
   strtoupper((char*)entryName); // fat ignores file case
   strtoupper((char*)entryExtension);

   if(!strequ(entryName, name)) return false;
   if(!strequ(entryExtension, extension)) return false;
   
   return true;
}

fat_dir_t *fat_read_root() {
   uint32_t rootDirAddr = rootSector*fat_bpb->bytesPerSector + baseAddr;

   return (fat_dir_t*)ata_read_exact(true, true, rootDirAddr, sizeof(fat_dir_t)*fat_bpb->noRootEntries);
}

bool fat_setup() {
   if(!fat_get_info())
      return false;
   uint32_t newTableSize = fat_bpb->sectorsPerFat * fat_bpb->bytesPerSector;
   uint32_t fatTableAddr = baseAddr + fat_bpb->noReservedSectors * fat_bpb->bytesPerSector;
   uint8_t *fat_table_buffer = ata_read_exact(true, true, fatTableAddr, newTableSize);
   if(!fat_table_buffer)
      return false;
   bool *dirty_clusters_new = malloc(sizeof(bool) * newTableSize/sizeof(uint16_t));
   if(!dirty_clusters_new) {
      free((uint32_t)fat_table_buffer, newTableSize);
      return false;
   }
   for(int i = 0; i < (int)(newTableSize/sizeof(uint16_t)); i++)
      dirty_clusters_new[i] = false;
   if(fat_table)
      free((uint32_t)fat_table, tableSize);
   if(dirty_clusters)
      free((uint32_t)dirty_clusters, sizeof(bool) * tableSize/sizeof(uint16_t));
   fat_table = fat_table_buffer;
   tableSize = newTableSize;
   dirty_clusters = dirty_clusters_new;
   return true;
}

uint32_t fat_table_addr(int table) {
   uint32_t fatTableAddr = baseAddr + fat_bpb->noReservedSectors * fat_bpb->bytesPerSector;
   return fatTableAddr + table * fat_bpb->sectorsPerFat * fat_bpb->bytesPerSector;
}

bool fat_valid_cluster(uint16_t cluster) {
   // EOF, bad cluster, out of bounds
   if(cluster >= 0xFFF8 || cluster == 0xFFF7 || cluster < 2 || cluster >= noClusters)
      return false;
   return true;
}

bool fat_table_update_cluster(uint32_t clusterNo, uint16_t value) {
   if(clusterNo >= noClusters || clusterNo >= tableSize/sizeof(uint16_t)) return false;
   dirty_clusters[clusterNo] = true;
   ((uint16_t*)fat_table)[clusterNo] = value;
   return true;
}

static bool fat_table_flush_write(uint32_t firstSector, uint32_t count) {
   uint32_t byteOffset = firstSector * ATA_SECTOR_SIZE;
   uint32_t size = count * ATA_SECTOR_SIZE;
   bool ok = true;
   for(int i = 0; i < fat_bpb->noTables; i++) {
      if(!ata_write_exact(true, true, fat_table_addr(i) + byteOffset, &fat_table[byteOffset], size)) {
         debug_printf("FAT error: failed writing sectors %u-%u to fat table %i\n", firstSector, firstSector + count - 1, i);
         ok = false;
      }
   }
   return ok;
}

bool fat_table_flush_cache(uint32_t start, uint32_t end) {
   uint32_t entries = tableSize / sizeof(uint16_t);
   if(start >= entries) return true;
   if(end >= entries) end = entries - 1;

   uint32_t perSector = ATA_SECTOR_SIZE / sizeof(uint16_t);
   uint32_t runStart = 0; // coalesce continuous sectors
   uint32_t runLen = 0;
   bool ok = true;

   for(uint32_t sector = start/perSector; sector <= end/perSector; sector++) {
      bool sectorDirty = false;
      uint32_t to = (sector + 1) * perSector;
      if(to > entries) to = entries;
      for(uint32_t c = sector * perSector; c < to; c++) {
         if(dirty_clusters[c]) {
            sectorDirty = true;
            dirty_clusters[c] = false;
         }
      }

      if(sectorDirty) {
         if(runLen == 0) runStart = sector;
         runLen++;
      } else if(runLen > 0) {
         ok &= fat_table_flush_write(runStart, runLen);
         runLen = 0;
      }
   }
   if(runLen > 0) ok &= fat_table_flush_write(runStart, runLen);
   return ok;
}

bool fat_table_flush_cache_all() {
   return fat_table_flush_cache(0, tableSize/sizeof(uint16_t)-1);
}

// number of unallocated clusters in the cached table
uint32_t fat_free_clusters() {
   uint32_t count = 0;
   for(uint32_t i = 2; i < noClusters; i++)
      if(((uint16_t*)fat_table)[i] == 0) count++;
   return count;
}

uint32_t fat_max_file_size() {
   if(noClusters < 3) return 0;
   return (noClusters - 2) * (fat_bpb->sectorsPerCluster * fat_bpb->bytesPerSector);
}

// undo partial chain extension
void fat_unwind_chain(uint16_t lastCluster, uint16_t origTail) {
   if(!fat_valid_cluster(lastCluster)) return;

   uint16_t cur = ((uint16_t*)fat_table)[lastCluster];
   if(cur == origTail)
      return; // nothing linked on yet, chain is untouched

   if(!fat_table_update_cluster(lastCluster, origTail)) // restore end of original chain
      debug_printf("FAT error: unwind failed restoring cluster %u\n", lastCluster);

   // free every cluster linked on after it
   while(fat_valid_cluster(cur)) {
      uint16_t next = ((uint16_t*)fat_table)[cur];
      if(!fat_table_update_cluster(cur, 0))
         debug_printf("FAT error: unwind failed freeing cluster %u\n", cur);
      cur = next;
   }
}

// number of items in
int fat_get_dir_size(uint16_t clusterNo) {
   if(clusterNo == 0) {
      // get root size
      uint32_t rootDirAddr = rootSector*fat_bpb->bytesPerSector + baseAddr;
      uint8_t *rootBuf = ata_read_exact(true, true, rootDirAddr, sizeof(fat_dir_t) * fat_bpb->noRootEntries);
      if(!rootBuf) {
         debug_printf("fat_get_dir_size: failed reading root directory\n");
         return -1;
      }
      int count = 0;
      for(int i = 0; i < fat_bpb->noRootEntries; i++) {
         fat_dir_t *fat_dir = (fat_dir_t*)(rootBuf + i * sizeof(fat_dir_t));
         if(fat_dir->filename[0] == 0) break; // no more files/dirs in directory
         if(fat_dir->filename[0] == 0xE5) continue; // deleted entry
         count++;
      }
      free((uint32_t)rootBuf, sizeof(fat_dir_t) * fat_bpb->noRootEntries);
      return count;
   }

   uint32_t dirFirstSector = ((clusterNo - 2) * fat_bpb->sectorsPerCluster) + firstDataSector;

   uint32_t dirAddr = baseAddr + dirFirstSector * fat_bpb->bytesPerSector;
   uint32_t dirSize = fat_bpb->sectorsPerCluster * fat_bpb->bytesPerSector;

   // Read the whole directory cluster at once
   uint8_t *dirBuf = ata_read_exact(true, true, dirAddr, dirSize);
   if(!dirBuf) {
      debug_printf("fat_get_dir_size: failed reading cluster %u\n", clusterNo);
      return -1;
   }

   int entries = dirSize / sizeof(fat_dir_t);
   int count = 0;
   for(int i = 0; i < entries; i++) {
      fat_dir_t *dir = (fat_dir_t*)(dirBuf + i * sizeof(fat_dir_t));
      if(dir->filename[0] == 0)
         break; // no more files/dirs in directory
      if(dir->filename[0] == 0xE5) continue; // deleted entry
      count++;
   }

   free((uint32_t)dirBuf, dirSize);
   return count;
}

bool fat_read_dir(uint16_t clusterNo, fat_dir_t *items) {
   uint32_t dirFirstSector = ((clusterNo - 2) * fat_bpb->sectorsPerCluster) + firstDataSector;

   uint32_t dirAddr = baseAddr + dirFirstSector * fat_bpb->bytesPerSector;
   uint32_t dirSize = fat_bpb->sectorsPerCluster * fat_bpb->bytesPerSector;

   // Read the whole directory cluster at once
   uint8_t *dirBuf = ata_read_exact(true, true, dirAddr, dirSize);
   if(!dirBuf) {
      debug_printf("fat_read_dir: failed reading cluster %u\n", clusterNo);
      return false;
   }

   int entries = dirSize / sizeof(fat_dir_t);
   int out = 0;
   for(int i = 0; i < entries; i++) {
      fat_dir_t *dir = (fat_dir_t*)(dirBuf + i * sizeof(fat_dir_t));
      if(dir->filename[0] == 0) break;
      if(dir->filename[0] == 0xE5) continue; // deleted entry
      items[out++] = *dir;
   }

   free((uint32_t)dirBuf, dirSize);
   return true;
}

fat_dir_t *fat_find_in_root(char* filename, char* extension) {
   uint32_t rootDirAddr = rootSector*fat_bpb->bytesPerSector + baseAddr;

   // get each file/dir in root

   // read entire root in
   uint8_t *rootBuf = ata_read_exact(true, true, rootDirAddr, sizeof(fat_dir_t) * fat_bpb->noRootEntries);
   if(!rootBuf) {
      debug_printf("fat_find_in_root: failed reading root directory\n");
      return NULL;
   }

   fat_dir_t *return_dir = malloc(sizeof(fat_dir_t));
   for(int i = 0; i < fat_bpb->noRootEntries; i++) {
      fat_dir_t *fat_dir = (fat_dir_t*)(rootBuf + i * sizeof(fat_dir_t));
      if(fat_dir->filename[0] == 0) break; // no more files/dirs in directory
      if(fat_dir->filename[0] == 0xE5) continue; // deleted entry

      if(fat_entry_matches_filename(fat_dir, filename, extension)) {
         memcpy(return_dir, fat_dir, sizeof(fat_dir_t));
         free((uint32_t)rootBuf, sizeof(fat_dir_t) * fat_bpb->noRootEntries);
         return return_dir;
      }
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
   if(!dirBuf) {
      debug_printf("fat_find_in_dir: failed reading cluster %u\n", clusterNo);
      return NULL;
   }

   if(filename[0] == '.' && filename[1] == '.') {
      fat_dir_t *result = malloc(sizeof(fat_dir_t));
      *result = *(fat_dir_t*)(dirBuf + sizeof(fat_dir_t)); // entry 1
      free((uint32_t)dirBuf, dirSize);
      return result;
   }
   int entries = dirSize / sizeof(fat_dir_t);
   for(int i = 0; i < entries; i++) {
      fat_dir_t *fat_dir = (fat_dir_t*)(dirBuf + i * sizeof(fat_dir_t));
      if(fat_dir->filename[0] == 0)
         break; // no more files/dirs in directory
      if(fat_dir->filename[0] == 0xE5)
         continue; // deleted entry

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
   bool inroot = clusterNo == 0;

   int entries;
   uint8_t *dirBuf;
   uint32_t bufSize;
   uint32_t dirAddr;

   if(inroot) {
      entries = fat_bpb->noRootEntries;
      bufSize = sizeof(fat_dir_t) * fat_bpb->noRootEntries;
      dirAddr = rootSector * fat_bpb->bytesPerSector + baseAddr;
      dirBuf = ata_read_exact(true, true, dirAddr, bufSize);
   } else {
      bufSize = fat_bpb->sectorsPerCluster * fat_bpb->bytesPerSector;
      uint32_t dirFirstSector = ((clusterNo - 2) * fat_bpb->sectorsPerCluster) + firstDataSector;
      dirAddr = baseAddr + dirFirstSector * fat_bpb->bytesPerSector;
      dirBuf = ata_read_exact(true, true, dirAddr, bufSize);
      entries = bufSize / sizeof(fat_dir_t);
   }

   if(!dirBuf) {
      debug_printf("fat_update_in_dir: failed reading directory for '%s'\n", filename);
      return false;
   }

   for(int i = 0; i < entries; i++) {
      fat_dir_t *fat_dir = (fat_dir_t*)(dirBuf + i * sizeof(fat_dir_t));
      if(fat_dir->filename[0] == 0)
         break; // no more files/dirs in directory
      if(fat_dir->filename[0] == 0xE5)
         continue; // deleted entry

      if(fat_entry_matches_filename(fat_dir, filename, extension)) {
         memcpy_fast(dirBuf + i * sizeof(fat_dir_t), dir, sizeof(fat_dir_t)); // update the entry
         bool ok = ata_write_exact(true, true, dirAddr, dirBuf, bufSize);
         if(!ok)
            debug_printf("Error writing directory entry for '%s'\n", filename);
         free((uint32_t)dirBuf, bufSize);
         return ok;
      }
   }

   debug_printf("Error: file not found for path '%s'\n", filename);
   free((uint32_t)dirBuf, bufSize);
   return false;
}

int fat_shrink_cluster_chain(uint16_t startCluster, uint32_t oldSize, uint32_t newSize) {
   if(!fat_valid_cluster(startCluster)) {
      debug_printf("FAT error: bad chain head %u\n", startCluster);
      return -1;
   }
   uint32_t bytesPerCluster = fat_bpb->sectorsPerCluster * fat_bpb->bytesPerSector;
   uint32_t oldClusters = (oldSize + bytesPerCluster - 1) / bytesPerCluster;
   uint32_t newClusters = (newSize + bytesPerCluster - 1) / bytesPerCluster;

   if(newClusters >= oldClusters)
      return 0;

   // find last cluster in new chain
   uint16_t cur = startCluster;
   for(uint32_t i = 1; i < newClusters; i++) {
      uint16_t next = ((uint16_t*)fat_table)[cur];
      if(!fat_valid_cluster(next)) break;
      cur = next;
   }

   uint16_t toFree = ((uint16_t*)fat_table)[cur];
   bool ok = fat_table_update_cluster(cur, 0xFFFF); // mark cur as end of chain

   // free remaining clusters in chain
   int freed = 0;
   uint32_t min = cur;
   uint32_t max = cur;
   while(fat_valid_cluster(toFree)) {
      uint16_t next = ((uint16_t*)fat_table)[toFree];
      if(!fat_table_update_cluster(toFree, 0)) // mark as free
         ok = false;
      if(toFree < min) min = toFree;
      if(toFree > max) max = toFree;
      toFree = next;
      freed++;
   }

   if(!fat_table_flush_cache(min, max))
      ok = false;

   if(!ok) {
      debug_printf("FAT error: failed writing shrunk chain from cluster %u\n", startCluster);
      return -1;
   }

   return freed;
}

int fat_extend_cluster_chain(uint16_t startCluster, uint32_t oldSize, uint32_t newSize) {
   uint32_t bytesPerCluster = fat_bpb->sectorsPerCluster * fat_bpb->bytesPerSector;
   uint32_t oldClusters = (oldSize + bytesPerCluster - 1) / bytesPerCluster;
   uint32_t newClusters = (newSize + bytesPerCluster - 1) / bytesPerCluster;

   if(oldClusters == 0) oldClusters = 1; // every file owns >=1 cluster from creation

   if(newSize <= oldSize)
      return 0;
   if(!fat_valid_cluster(startCluster)) {
      debug_printf("FAT error: bad chain head %u\n", startCluster);
      return -1;
   }

   // find last cluster in chain
   uint16_t lastCluster = startCluster;
   for(uint32_t i = 1; i < oldClusters; i++) {
      uint16_t next = ((uint16_t*)fat_table)[lastCluster];
      if(!fat_valid_cluster(next)) break;
      lastCluster = next;
   }

   // zero data from old eof to end of eof's cluster
   if(oldSize % bytesPerCluster != 0 || oldSize == 0) {
      uint32_t from = oldSize % bytesPerCluster;
      uint32_t to = newSize - (oldSize - from);
      if(to > bytesPerCluster)
         to = bytesPerCluster;

      uint32_t gapSector = ((lastCluster - 2) * fat_bpb->sectorsPerCluster) + firstDataSector;
      uint32_t gapAddr = baseAddr + gapSector * fat_bpb->bytesPerSector;
      uint8_t *gapBuf;
      if(from == 0) {
         // fast path - entire cluster is overwritten, skip read
         gapBuf = malloc(bytesPerCluster);
         if(gapBuf)
            memset(gapBuf, 0, bytesPerCluster);
      } else {
         // the bytes before the old eof are unchanged
         gapBuf = ata_read_exact(true, true, gapAddr, bytesPerCluster);
         if(gapBuf)
            memset(gapBuf + from, 0, to - from);
      }
      if(gapBuf == NULL) {
         debug_printf("Error preparing cluster %u to zero\n", lastCluster);
         return -1;
      }
      bool ok = ata_write_exact(true, true, gapAddr, gapBuf, bytesPerCluster);
      free((uint32_t)gapBuf, bytesPerCluster);
      if(!ok) {
         debug_printf("Error zeroing cluster %u\n", lastCluster);
         return -1;
      }
   }

   if(newClusters <= oldClusters)
      return 0;

   // check fat has enough space, return early if not
   uint32_t needed = newClusters - oldClusters;
   if(needed > fat_free_clusters()) {
      debug_printf("FAT: not enough free clusters (need %u)\n", needed);
      return -1;
   }

   // allocate new clusters
   uint16_t origTail = ((uint16_t*)fat_table)[lastCluster];
   uint16_t prev = lastCluster;
   int allocated = 0;
   uint8_t *zeros = malloc(bytesPerCluster);
   if(!zeros) {
      return -1;
   }
   memset(zeros, 0, bytesPerCluster);

   uint16_t searchFrom = 2; // clusters below this are known to be taken
   int min = lastCluster;
   int max = lastCluster;
   for(uint32_t i = oldClusters; i < newClusters; i++) {
      uint16_t freeCluster = searchFrom;
      while(freeCluster < noClusters && ((uint16_t*)fat_table)[freeCluster] != 0)
         freeCluster++;
      if(freeCluster >= noClusters) {
         debug_printf("FAT: out of free clusters\n");
         free((uint32_t)zeros, bytesPerCluster);
         fat_unwind_chain(lastCluster, origTail);
         return -1; // no free clusters
      }
      searchFrom = freeCluster + 1;

      if(freeCluster < min) min = freeCluster;
      if(freeCluster > max) max = freeCluster;
      
      // zero new cluster data
      uint32_t clusterFirstSector = ((freeCluster - 2) * fat_bpb->sectorsPerCluster) + firstDataSector;
      uint32_t clusterAddr = baseAddr + clusterFirstSector * fat_bpb->bytesPerSector;
      if(!ata_write_exact(true, true, clusterAddr, zeros, bytesPerCluster)) {
         debug_printf("Error zeroing new cluster %u\n", freeCluster);
         free((uint32_t)zeros, bytesPerCluster);
         fat_unwind_chain(lastCluster, origTail);
         return -1;
      }

      // mark new cluster as end of chain (0xFFFF) and point prev at it
      if(!fat_table_update_cluster(freeCluster, 0xFFFF) || !fat_table_update_cluster(prev, freeCluster)) {
         debug_printf("Error writing fat table for cluster %u\n", freeCluster);
         free((uint32_t)zeros, bytesPerCluster);
         fat_unwind_chain(lastCluster, origTail);
         return -1;
      }
      prev = freeCluster;
      allocated++;
   }
   if(!fat_table_flush_cache(min, max)) {
      debug_printf("FAT error: failed flushing table for chain from cluster %u\n", startCluster);
      free((uint32_t)zeros, bytesPerCluster);
      fat_unwind_chain(lastCluster, origTail);
      fat_table_flush_cache(min, max); // part of the chain may have reached disk, best effort
      return -1;
   }
   free((uint32_t)zeros, bytesPerCluster);

   return allocated;
}

bool fat_new_file(char *path) {
   // get directory cluster
   char filenamefull[256];
   char parentpath[256];
   strsplit_last(parentpath, filenamefull, path, '/');
   if(strequ(parentpath, ""))
      strcpy(parentpath, "/");
   if(strlen(filenamefull) > 11) {
      debug_printf("Filename '%s' too long\n", filenamefull);
      return false;
   }
   strtoupper(filenamefull);

   fat_dir_t *parent = fat_parse_path(parentpath, true);
   bool inroot = strequ(parentpath, "/");
   if(!inroot && parent == NULL) {
      debug_printf("Error: parent directory '%s' not found\n", parentpath);
      return false;
   }

   fat_dir_t *filedir = malloc(sizeof(fat_dir_t));
   memset(filedir, 0, sizeof(fat_dir_t));

   // extract filename from path
   char filename[9];
   char extension[4];
   strsplit(filename, extension, filenamefull, '.'); // split at first dot
   strtoupper(filename);
   strtoupper(extension);
   memset(filedir->filename, ' ', 11);
   strcpy_fixed((char*)filedir->filename, filename, strlen(filename));
   filedir->filename[strlen(filename)] = ' ';
   strcpy_fixed((char*)filedir->filename+8, extension, strlen(extension));
   filedir->filename[8+strlen(extension)] = ' ';
   filedir->attributes = 0x20; // file
   filedir->firstClusterNo = 0; // to be set later
   filedir->fileSize = 0;

   // find free cluster
   uint16_t freeCluster = 2;
   while(freeCluster < noClusters && ((uint16_t*)fat_table)[freeCluster] != 0)
      freeCluster++;
   if(freeCluster >= noClusters) {
      debug_printf("Error: no free clusters\n");
      free((uint32_t)filedir, sizeof(fat_dir_t));
      if(!inroot)
         free((uint32_t)parent, sizeof(fat_dir_t));
      return false;
   }
   filedir->firstClusterNo = freeCluster;
   debug_printf("Found free cluster %u\n", freeCluster);

   // find free entry in directory

   int entries;
   uint8_t *dirBuf;
   uint32_t bufSize;
   uint32_t dirAddr;

   if(inroot) {
      entries = fat_bpb->noRootEntries;
      bufSize = sizeof(fat_dir_t) * fat_bpb->noRootEntries;
      dirAddr = rootSector * fat_bpb->bytesPerSector + baseAddr;
      dirBuf = ata_read_exact(true, true, dirAddr, bufSize);
    } else {
      bufSize = fat_bpb->sectorsPerCluster * fat_bpb->bytesPerSector;
      uint32_t dirFirstSector = ((parent->firstClusterNo - 2) * fat_bpb->sectorsPerCluster) + firstDataSector;
      dirAddr = baseAddr + dirFirstSector * fat_bpb->bytesPerSector;
      dirBuf = ata_read_exact(true, true, dirAddr, bufSize);
      entries = bufSize / sizeof(fat_dir_t);
   }

   if(!dirBuf) {
      debug_printf("fat_new_file: failed reading directory '%s'\n", parentpath);
      if(!inroot)
         free((uint32_t)parent, sizeof(fat_dir_t));
      free((uint32_t)filedir, sizeof(fat_dir_t));
      return false;
   }

   bool found = false;
   for(int i = 0; i < entries; i++) {
      fat_dir_t *fat_dir = (fat_dir_t*)(dirBuf + i * sizeof(fat_dir_t));
      if(memcmp((char*)fat_dir->filename, (char*)filedir->filename, 11) == 0) {
         debug_printf("Error: file already exists\n");
         free((uint32_t)dirBuf, bufSize);
         free((uint32_t)parent, sizeof(fat_dir_t));
         free((uint32_t)filedir, sizeof(fat_dir_t));
         return false;
      }
      if(fat_dir->filename[0] == '\0') {
         // found a free entry
         debug_printf("Found free entry %u\n", i);
         memcpy_fast(dirBuf + i * sizeof(fat_dir_t), filedir, sizeof(fat_dir_t)); // copy the new entry
         found = true;
         break;
      }
   }

   if(found) {
      debug_writestr("Updating directory\n");
      if(!ata_write_exact(true, true, dirAddr, dirBuf, bufSize)) {
         debug_printf("Error writing directory entry for '%s'\n", path);
         found = false;
      } else {
         debug_writestr("Updating FAT table\n");
         // claim the cluster and get it onto disk before reporting success
         if(!fat_table_update_cluster(freeCluster, 0xFFFF) // mark as end of chain
         || !fat_table_flush_cache(freeCluster, freeCluster))
            found = false;
      }
   } else {
      debug_printf("Error: No free entries found in directory '%s'\n", parentpath);
   }

   if(!found && ((uint16_t*)fat_table)[freeCluster] != 0) {
      // release the reserved cluster, it may already have reached disk
      fat_table_update_cluster(freeCluster, 0);
      fat_table_flush_cache(freeCluster, freeCluster); // best effort
   }

   free((uint32_t)dirBuf, bufSize);
   free((uint32_t)parent, sizeof(fat_dir_t));
   free((uint32_t)filedir, sizeof(fat_dir_t));

   return found;
}

int fat_resize_file(char *path, uint32_t size) {
   fat_dir_t *dir = fat_parse_path(path, true);
   if(dir == NULL) {
      debug_printf("Error: file not found for path '%s'\n", path);
      return -1;
   }

   char parentpath[256];
   strsplit_last(parentpath, NULL, path, '/');
   bool inroot = strequ(parentpath, "/") || strequ(parentpath, "");

   uint32_t clusterNo = dir->firstClusterNo;
   uint32_t oldsize = dir->fileSize;

   debug_printf("Changing file '%s' size from %u to %u\n", path, oldsize, size);

   if(size < oldsize) {
      int freed = fat_shrink_cluster_chain(clusterNo, oldsize, size);
      if(freed < 0) {
         debug_printf("Error shrinking cluster chain\n");
         free((uint32_t)dir, sizeof(fat_dir_t));
         return -2;
      }
      debug_printf("Freed %u clusters\n", freed);
   } else if(size > oldsize) {
      int allocated = fat_extend_cluster_chain(clusterNo, oldsize, size);
      if(allocated < 0) {
         debug_printf("Error extending cluster chain\n");
         free((uint32_t)dir, sizeof(fat_dir_t));
         return -2;
      }
   } else {
      free((uint32_t)dir, sizeof(fat_dir_t));
      return 0;
   }

   // update the file size in the directory entry
   dir->fileSize = size;
   char name[9];
   char extension[4];
   strcpy_fixed((char*)name, (char*)dir->filename, 8);
   strcpy_fixed((char*)extension, (char*)dir->filename+8, 3);
   strsplit((char*)name, NULL, (char*)name, ' '); // null terminate at first space
   strsplit((char*)extension, NULL, (char*)extension, ' '); // null terminate at first space
   fat_dir_t *parentDir = fat_parse_path(path, false);
   int firstCluster = 0;
   if(!inroot && !parentDir) {
      debug_printf("Parent dir '%s' not found\n", parentpath);
      free((uint32_t)dir, sizeof(fat_dir_t));
      return -3;
   }
   if(!inroot)
      firstCluster = parentDir->firstClusterNo;

   if(!fat_update_in_dir(firstCluster, name, extension, dir)) {
      debug_printf("Error updating directory entry\n");
      free((uint32_t)dir, sizeof(fat_dir_t));
      if(!inroot)
         free((uint32_t)parentDir, sizeof(fat_dir_t));
      return -3;
   }

   free((uint32_t)dir, sizeof(fat_dir_t));
   if(!inroot)
      free((uint32_t)parentDir, sizeof(fat_dir_t));
   return 0;
}

int fat_write_file_at(char *path, uint8_t *buffer, uint32_t offset, uint32_t size) {
   fat_dir_t *dir = fat_parse_path(path, true);
   if(dir == NULL) {
      debug_printf("Error: file not found for path '%s'\n", path);
      return -1;
   }

   char parentpath[256];
   strsplit_last(parentpath, NULL, path, '/');
   bool inroot = strequ(parentpath, "/") || strequ(parentpath, "");

   uint32_t clusterNo = dir->firstClusterNo;
   uint32_t clusterSize = fat_bpb->sectorsPerCluster * fat_bpb->bytesPerSector;
   uint32_t oldsize = dir->fileSize;

   if(!fat_valid_cluster(clusterNo)) {
      debug_printf("Error: '%s' has bad first cluster %u\n", path, clusterNo);
      free((uint32_t)dir, sizeof(fat_dir_t));
      return -1;
   }

   uint32_t newsize = offset + size;
   if(newsize > oldsize) {
      int allocated = fat_extend_cluster_chain(clusterNo, oldsize, newsize);
      if(allocated < 0) {
         debug_printf("Error extending cluster chain\n");
         free((uint32_t)dir, sizeof(fat_dir_t));
         return -2;
      }
   }

   // skip to first cluster from offset
   uint32_t offset_remaining = offset;
   uint16_t curCluster = clusterNo;
   while(offset_remaining >= clusterSize) {
      uint16_t next = ((uint16_t*)fat_table)[curCluster];
      if(!fat_valid_cluster(next)) {
         debug_printf("fat_write_file_at: hit eof/bad cluster before offset");
         free((uint32_t)dir, sizeof(fat_dir_t));
         return -3;
      }
      curCluster = next;
      offset_remaining -= clusterSize;
   }

   uint32_t write_offset = offset % clusterSize; // offset into first cluster

   // update each cluster
   uint32_t bytesRemaining = size;
   uint32_t bytesWritten = 0;
   while(bytesRemaining > 0) {
      // calculate the first sector of this cluster
      uint32_t diskSector = ((curCluster - 2) * fat_bpb->sectorsPerCluster) + firstDataSector;
      uint32_t sectorAddr = baseAddr + diskSector * fat_bpb->bytesPerSector;

      uint32_t consumed = clusterSize - write_offset;
      if(consumed > bytesRemaining)
         consumed = bytesRemaining;
      if(write_offset > 0 || bytesRemaining < clusterSize) {
         uint8_t *clusterBuf = ata_read_exact(true, true, sectorAddr, clusterSize);
         if(clusterBuf == NULL) {
            debug_printf("Error reading cluster %u\n", curCluster);
            free((uint32_t)dir, sizeof(fat_dir_t));
            return -4;
         }
         memcpy(clusterBuf + write_offset, buffer + bytesWritten, consumed);
         write_offset = 0;
         bool ok = ata_write_exact(true, true, sectorAddr, clusterBuf, clusterSize);
         free((uint32_t)clusterBuf, clusterSize);
         if(!ok) {
            debug_printf("Error writing cluster %u\n", curCluster);
            break; // report what did make it to disk
         }
      } else {
         if(!ata_write_exact(true, true, sectorAddr, buffer + bytesWritten, clusterSize)) {
            debug_printf("Error writing cluster %u\n", curCluster);
            break;
         }
      }

      bytesRemaining -= consumed;
      bytesWritten += consumed;
      // get next cluster in chain
      uint16_t next = ((uint16_t*)fat_table)[curCluster];
      if(!fat_valid_cluster(next)) {
         // end of chain/bad
         break;
      }
      curCluster = next;
   }

   if(offset + bytesWritten > oldsize) {
      // update the file size in the directory entry
      dir->fileSize = offset + bytesWritten;
      char name[9];
      char extension[4];
      strcpy_fixed((char*)name, (char*)dir->filename, 8);
      strcpy_fixed((char*)extension, (char*)dir->filename+8, 3);
      strsplit((char*)name, NULL, (char*)name, ' '); // null terminate at first space
      strsplit((char*)extension, NULL, (char*)extension, ' '); // null terminate at first space
      fat_dir_t *parentDir = fat_parse_path(path, false);
      int firstCluster = 0;
      if(!inroot && !parentDir) {
         debug_printf("Parent dir '%s' not found\n", parentpath);
         free((uint32_t)dir, sizeof(fat_dir_t));
         return -3;
      }
      if(!inroot)
         firstCluster = parentDir->firstClusterNo;

      if(!fat_update_in_dir(firstCluster, name, extension, dir)) {
         debug_printf("Error updating directory entry\n");
         free((uint32_t)dir, sizeof(fat_dir_t));
         if(!inroot)
            free((uint32_t)parentDir, sizeof(fat_dir_t));
         return -3;
      }
      if(!inroot)
         free((uint32_t)parentDir, sizeof(fat_dir_t));
   }

   free((uint32_t)dir, sizeof(fat_dir_t));
   return bytesWritten;
}

bool fat_new_dir(char *path) {
   char dirname[256];
   char parentpath[256];
   strsplit_last(parentpath, dirname, path, '/');
   if(strequ(parentpath, ""))
      strcpy(parentpath, "/");
   if(strlen(dirname) > 8) {
      debug_printf("Dir name too long\n");
      return false;
   }
   strtoupper(dirname);
   debug_printf("Creating dir %s in parent %s\n", dirname, parentpath);
   bool inroot = strequ(parentpath, "/");
   fat_dir_t *parent = NULL;
   if(!inroot) {
      parent = fat_parse_path(parentpath, true);
      if(parent == NULL) {
         debug_printf("Parent not found\n", path);
         return false;
      }
   }

   // create dir
   fat_dir_t *dir = malloc(sizeof(fat_dir_t));
   memset(dir, 0, sizeof(fat_dir_t));

   memset(dir->filename, ' ', 11);
   strcpy_fixed((char*)dir->filename, dirname, 8);
   dir->filename[8] = ' ';
   dir->attributes = 0x10; // directory
   dir->firstClusterNo = 0; // to be set later
   dir->fileSize = 0;

   // find free cluster
   uint16_t freeCluster = 2;
   while(freeCluster < noClusters && ((uint16_t*)fat_table)[freeCluster] != 0)
      freeCluster++;
   if(freeCluster >= noClusters) {
      debug_printf("Error: no free clusters\n");
      free((uint32_t)dir, sizeof(fat_dir_t));
      if(!inroot)
         free((uint32_t)parent, sizeof(fat_dir_t));
      return false;
   }
   if(!fat_table_update_cluster(freeCluster, 0xFFFF)) { // claim cluster, mark as end of chain
      debug_printf("Error writing fat table for cluster %u\n", freeCluster);
      free((uint32_t)dir, sizeof(fat_dir_t));
      if(!inroot)
         free((uint32_t)parent, sizeof(fat_dir_t));
      return false;
   }
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
   dotdot_entry.firstClusterNo = inroot ? 0 : parent->firstClusterNo;
   dotdot_entry.fileSize = 0;

    // zero new cluster
   uint8_t *clusterBuf = malloc(clusterSize);
   memset(clusterBuf, 0, clusterSize);
   // add dot entries
   memcpy(clusterBuf, &dot_entry, sizeof(fat_dir_t));
   memcpy(clusterBuf + sizeof(fat_dir_t), &dotdot_entry, sizeof(fat_dir_t));

   bool ok = ata_write_exact(true, true, newDirAddr, clusterBuf, clusterSize);
   free((uint32_t)clusterBuf, clusterSize);
   if(!ok) {
      // zero write failed
      debug_printf("Error writing new directory cluster %u\n", freeCluster);
      fat_table_update_cluster(freeCluster, 0); // return cluster
      free((uint32_t)dir, sizeof(fat_dir_t));
      if(!inroot)
         free((uint32_t)parent, sizeof(fat_dir_t));
      return false;
   }

   int entries;
   uint8_t *dirBuf;
   uint32_t bufSize;
   uint32_t dirAddr;

   if(inroot) {
      entries = fat_bpb->noRootEntries;
      bufSize = sizeof(fat_dir_t) * fat_bpb->noRootEntries;
      dirAddr = rootSector*fat_bpb->bytesPerSector + baseAddr;
      dirBuf = ata_read_exact(true, true, dirAddr, bufSize);
   } else {
      bufSize = fat_bpb->sectorsPerCluster * fat_bpb->bytesPerSector;
      uint32_t dirFirstSector = ((parent->firstClusterNo - 2) * fat_bpb->sectorsPerCluster) + firstDataSector;
      dirAddr = baseAddr + dirFirstSector * fat_bpb->bytesPerSector;
      dirBuf = ata_read_exact(true, true, dirAddr, bufSize);
      entries = bufSize / sizeof(fat_dir_t);
   }

   bool success = false;
   bool exists = false;

   if(!dirBuf) {
      debug_printf("fat_new_dir: failed reading directory '%s'\n", parentpath);
   } else {
      // look for free entry in dir
      bool found = false;
      for(int i = 0; i < entries; i++) {
         fat_dir_t *fat_dir = (fat_dir_t*)(dirBuf + i * sizeof(fat_dir_t));
         if(memcmp((char*)fat_dir->filename, (char*)dir->filename, 11) == 0) {
            debug_printf("Error: dir already exists\n");
            exists = true;
            break;
         }

         if(fat_dir->filename[0] == '\0') {
            // found a free entry
            debug_printf("Found free entry %u\n", i);
            // set it
            memcpy_fast(dirBuf + i * sizeof(fat_dir_t), dir, sizeof(fat_dir_t));
            found = true;
            break;
         }
      }

      if(found) {
         debug_writestr("Updating directory\n");
         if(!ata_write_exact(true, true, dirAddr, dirBuf, bufSize)) {
            debug_printf("Error writing directory entry for '%s'\n", path);
         } else {
            debug_writestr("Updating FAT table\n");
            if(fat_table_flush_cache(freeCluster, freeCluster))
               success = true;
         }
      } else if(!exists) {
         debug_printf("No free entry found in '%s'\n", parentpath);
      }
   }

   if(!success) {
      // return cluster - the claim may already have reached disk
      fat_table_update_cluster(freeCluster, 0);
      fat_table_flush_cache(freeCluster, freeCluster); // best effort
   }

   if(dirBuf)
      free((uint32_t)dirBuf, bufSize);
   free((uint32_t)dir, sizeof(fat_dir_t));
   if(!inroot)
      free((uint32_t)parent, sizeof(fat_dir_t));
   return success;
}

typedef struct {
   uint16_t clusterNo;
   uint32_t offset;
   uint32_t size; // read/buffer size
   uint8_t *buffer;
   void (*callback)(void *regs, int task, int bytes, void *data);
   void *data; // opaque "cookie" data used by fs layer
   int currentCluster;
   int readCount; // no clusters read
   uint32_t readBytes; // no bytes read
   uint8_t *clusterBuf;
   uint32_t clusterBufSize;
   uint8_t *fatTable;
   int task;
   uint32_t task_uid;
} fat_read_file_state_t;

static void fat_read_file_finish(void *regs, fat_read_file_state_t *state, int bytes) {
   void (*callback)(void *, int, int, void *) = state->callback;
   int task = state->task;
   void *data = state->data;

   if(state->clusterBuf)
      free((uint32_t)state->clusterBuf, state->clusterBufSize);
   free((uint32_t)state, sizeof(fat_read_file_state_t));

   callback(regs, task, bytes, data);
}

static void fat_read_file_fail(void *regs, fat_read_file_state_t *state) {
   fat_read_file_finish(regs, state, state->readBytes > 0 ? (int)state->readBytes : -1);
}

void fat_read_file_callback(void *regs, void *msg) {
   fat_read_file_state_t *state = (fat_read_file_state_t*)msg;

   task_state_t *task_state = &gettasks()[state->task];
   if(!task_state->enabled || task_state->crashed || task_state->task_uid != state->task_uid) {
      fat_read_file_finish(regs, state, -1);
      return;
   }

   // skip clusters up to offset
   uint32_t clusterSize = fat_bpb->sectorsPerCluster * fat_bpb->bytesPerSector;
   while(state->offset >= clusterSize) {
      uint16_t tableVal = ((uint16_t*)state->fatTable)[state->currentCluster];
      if(tableVal >= 0xFFF8) {
         // no more clusters in chain
         fat_read_file_finish(regs, state, state->readBytes);
         return;
      } else if(!fat_valid_cluster(tableVal)) {
         debug_printf("FAT error: Hit bad cluster %u\n", tableVal);
         fat_read_file_fail(regs, state);
         return;
      } else {
         state->offset -= clusterSize;
         state->currentCluster = tableVal; // table value is the next cluster
         state->readCount++;
      }
   }
   
   for(int x = 0; x < 8; x++) { // do in batches of 8 clusters
      uint32_t currentClusterSector = ((state->currentCluster - 2) * fat_bpb->sectorsPerCluster) + firstDataSector;
      uint32_t diskAddr = baseAddr + currentClusterSector * fat_bpb->bytesPerSector + state->offset;
      uint32_t clusterReadSize = clusterSize - state->offset;
      if(state->readBytes + clusterReadSize > state->size)
         clusterReadSize = state->size - state->readBytes;
      
      if(!ata_read_exact_into(true, true, diskAddr, clusterReadSize, state->clusterBuf)) {
         debug_printf("Error reading cluster %u from disk\n", state->currentCluster);
         fat_read_file_fail(regs, state);
         return;
      }
      int written = copy_to_task(state->task, state->buffer + state->readBytes, state->clusterBuf, clusterReadSize);
      if(written < 0) {
        debug_printf("Error writing to task memory 0x%h size %u\n", state->buffer, clusterReadSize);
        fat_read_file_fail(regs, state);
        return;
      }
      state->readBytes += written;
      state->offset = 0;

      if(state->readBytes >= state->size || (uint32_t)written < clusterReadSize) { // finished read or hit end of buffer map
         fat_read_file_finish(regs, state, state->readBytes);
         return;
      }

      // check if theres more clusters to read
      uint16_t tableVal = ((uint16_t*)state->fatTable)[state->currentCluster];
      if(tableVal >= 0xFFF8) {
         // no more clusters in chain
         fat_read_file_finish(regs, state, state->readBytes);
         return;
      } else if(!fat_valid_cluster(tableVal)) {
         // bad/freed cluster
         debug_printf("FAT error: Hit bad cluster %u\n", tableVal);
         fat_read_file_fail(regs, state);
         return;
      } else {
         state->currentCluster = tableVal; // table value is the next cluster
         state->readCount++;
      }
   }

   events_add(1, &fat_read_file_callback, (void*)state, -1);
   
}

bool fat_read_file_chunked(uint16_t clusterNo, uint8_t *buffer, uint32_t offset, uint32_t size, void(*callback)(void *, int, int, void *), int task, void *data) {
   uint32_t clusterSize = fat_bpb->sectorsPerCluster * fat_bpb->bytesPerSector;

   fat_read_file_state_t *state = (fat_read_file_state_t*)malloc(sizeof(fat_read_file_state_t));
   if(!state) return false;

   state->clusterBuf = (uint8_t*)malloc(clusterSize);
   if(!state->clusterBuf) {
      free((uint32_t)state, sizeof(fat_read_file_state_t));
      return false;
   }
   state->clusterBufSize = clusterSize;

   state->clusterNo = clusterNo;
   state->offset = offset;
   state->size = size;
   state->callback = callback;
   state->data = data;
   state->task = task;
   state->task_uid = gettasks()[task].task_uid;
   state->fatTable = fat_table;
   state->buffer = buffer;
   state->currentCluster = clusterNo;
   state->readCount = 0;
   state->readBytes = 0;

   // kick off read event chain
   events_add(1, &fat_read_file_callback, (void*)state, -1);
   return true;
}

// reads file contents synchronously, used by kernel
// usermode uses chunker version above
uint8_t *fat_read_file(uint16_t clusterNo, uint32_t size) {
   if(!fat_valid_cluster(clusterNo)) {
      debug_printf("fat_read_file: bad chain head %u\n", clusterNo);
      return NULL;
   }

   bool readEntireFile = (size == 0); // read entry entry as stored on disk or the size supplied

   // read entire fat table

   // get no clusters
   uint16_t c = clusterNo;
   uint16_t clusterCount = 1;
   while(clusterCount <= noClusters) {
      uint16_t tableVal = ((uint16_t*)fat_table)[c];
      if(!fat_valid_cluster(tableVal)) {
         break;
      } else {
         c = tableVal;
         clusterCount++;
      }
   }

   if(clusterCount >= noClusters) {
      debug_printf("fat_read_file: cycle detected in chain starting %u\n", clusterNo);
      return NULL;
   }

   uint32_t fileSizeDisk = clusterCount*fat_bpb->sectorsPerCluster*fat_bpb->bytesPerSector; // size on disk

   uint32_t allocate = (readEntireFile) ? fileSizeDisk : size;
   uint8_t *fileContents = malloc(allocate);

   uint32_t byte = 0;
   c = clusterNo;
   while(true) { // until we reach the end of the cluster chain or byte >= size
      // read all sectors of cluster
      uint32_t currentClusterSector = ((c - 2) * fat_bpb->sectorsPerCluster) + firstDataSector;
      uint32_t diskAddr = baseAddr + currentClusterSector * fat_bpb->bytesPerSector;
      uint8_t *clusterBuf = ata_read_exact(true, true, diskAddr, fat_bpb->sectorsPerCluster * fat_bpb->bytesPerSector);
      if(!clusterBuf) {
         debug_printf("fat_read_file: failed reading cluster %u\n", c);
         free((uint32_t)fileContents, allocate);
         return NULL;
      }
      bool done = false;
      for(int i = 0; i < fat_bpb->sectorsPerCluster && !done; i++) {
         uint8_t *buf = clusterBuf + i * fat_bpb->bytesPerSector;
         for(int b = 0; b < fat_bpb->bytesPerSector; b++) {
            if(byte >= (uint32_t)allocate) {
               done = true;
               break;
            }
            fileContents[byte++] = buf[b];
         }
      }
      free((uint32_t)clusterBuf, fat_bpb->sectorsPerCluster * fat_bpb->bytesPerSector);
      if(done) break;

      // check if theres more clusters to read
      uint16_t tableVal = ((uint16_t*)fat_table)[c];
      if(tableVal >= 0xFFF8) {
         // no more clusters in chain
         break;
      } else if(tableVal == 0xFFF7) {
         // bad cluster
         break;
      } else { 
         c = tableVal; // table value is the next cluster
      }
   }

   return fileContents;

}

bool fat_rename(char *path, char *filename) {
   // get parent
   char parentpath[512];
   char oldname[16];
   char name[9];
   char extension[4];

   strsplit_last(parentpath, oldname, path, '/');
   strsplit(name, extension, oldname, '.');

   if(parentpath[strlen(parentpath)-1] != '/') {
      // dir, append /
      strcat(parentpath, "/");
   }

   uint32_t dirAddr;
   uint32_t dirSize;

   if(strequ(parentpath, "/")) {
      // rename in root
      dirAddr = rootSector*fat_bpb->bytesPerSector + baseAddr;
      dirSize = sizeof(fat_dir_t)*fat_bpb->noRootEntries;
   } else {
      fat_dir_t *dir = fat_parse_path(parentpath, false);
      if(!dir) {
         debug_printf("fat_rename: Parent '%s' not found\n", parentpath);
         return false;
      }
      uint32_t clusterNo = dir->firstClusterNo;
      uint32_t dirFirstSector = ((clusterNo - 2) * fat_bpb->sectorsPerCluster) + firstDataSector;
      dirAddr = baseAddr + dirFirstSector * fat_bpb->bytesPerSector;
      dirSize = fat_bpb->sectorsPerCluster * fat_bpb->bytesPerSector;
      free((uint32_t)dir, sizeof(fat_dir_t));
   }

   uint8_t *dirBuf = ata_read_exact(true, true, dirAddr, dirSize);
   if(!dirBuf) {
      debug_printf("fat_rename: failed reading directory\n");
      return false;
   }

   int entries = dirSize / sizeof(fat_dir_t);
   bool match = false;
   for(int i = 0; i < entries; i++) {
      fat_dir_t *fat_dir = (fat_dir_t*)(dirBuf + i * sizeof(fat_dir_t));
      if(fat_entry_matches_filename(fat_dir, name, extension)) {
         match = true;
         char newfilename[12];
         newfilename[11] = '\0';
         memset(newfilename, ' ', 11);
         char newfile[9];
         char newext[4];
         strsplit(newfile, newext, filename, '.');
         strncpy(newfilename, newfile, strlen(newfile));
         newfilename[strlen(newfile)] = ' ';
         strncpy(newfilename+8, newext, strlen(newext));
         newfilename[8+strlen(newext)] = ' ';
         strncpy((char*)fat_dir->filename, newfilename, 11);
         debug_printf("fat_rename: Renaming to '%s'\n", newfilename);
      }
   }
   if(match) {
      bool success = ata_write_exact(true, true, dirAddr, dirBuf, dirSize);
      if(!success)
         debug_printf("fat_rename: Error writing directory\n");
      free((uint32_t)dirBuf, dirSize);
      return success;
   } else {
      debug_printf("fat_rename: File not found\n");
      free((uint32_t)dirBuf, dirSize);
      return false;
   }
}

fat_dir_t *fat_follow_path_chain(char *pathElement, fat_dir_t *dir) {
   if(strlen(pathElement) == 0)
      return dir;

   if(strlen(pathElement) > 12)
      return NULL;

   char name[9];
   char ext[4];
   if(strequ(pathElement, "..")) {
      strcpy(name, pathElement);
      ext[0] = '\0';
   } else if(strequ(pathElement, ".")) {
      return dir;
   } else if(!strsplit(name, ext, pathElement, '.')) {
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

bool fat_delete_file(char *path) {
   fat_dir_t *file = fat_parse_path(path, true);
   if(!file) {
      debug_printf("fat_delete_file: '%s' not found\n", path);
      return false;
   }

   // read name/extension directly from stored 8.3 entry
   char name[9], extension[4];
   strcpy_fixed(name, (char*)file->filename, 8);
   strcpy_fixed(extension, (char*)file->filename+8, 3);
   strsplit(name, NULL, name, ' ');
   strsplit(extension, NULL, extension, ' ');

   fat_dir_t deleted = *file;
   deleted.filename[0] = 0xE5;

   char parentpath[256];
   strsplit_last(parentpath, NULL, path, '/');
   if(strequ(parentpath, "")) strcpy(parentpath, "/");
   bool inroot = strequ(parentpath, "/");

   uint16_t parentCluster = 0;
   if(!inroot) {
      fat_dir_t *parent = fat_parse_path(parentpath, true);
      if(!parent) {
         debug_printf("fat_delete_file: parent '%s' not found\n", parentpath);
         free((uint32_t)file, sizeof(fat_dir_t));
         return false;
      }
      parentCluster = parent->firstClusterNo;
      free((uint32_t)parent, sizeof(fat_dir_t));
   }

   bool ok = fat_update_in_dir(parentCluster, name, extension, &deleted);
   if(ok) {
      // free cluster chain
      uint16_t head = file->firstClusterNo;
      uint16_t cur = head;
      uint32_t min = cur;
      uint32_t max = cur;
      while(fat_valid_cluster(cur)) {
         uint16_t next = ((uint16_t*)fat_table)[cur];
         if(!fat_table_update_cluster(cur, 0))
            ok = false;
         if(cur < min) min = cur;
         if(cur > max) max = cur;
         cur = next;
      }
      if(!fat_table_flush_cache(min, max))
         ok = false;
      if(!ok)
         debug_printf("FAT error: failed freeing chain from cluster %u\n", head);
   }
   free((uint32_t)file, sizeof(fat_dir_t));
   return ok;
}

bool fat_delete_dir(char *path) {
   fat_dir_t *dir = fat_parse_path(path, true);
   if(!dir) {
      debug_printf("fat_delete_dir: '%s' not found\n", path);
      return false;
   }
   if(!(dir->attributes & 0x10)) {
      debug_printf("fat_delete_dir: '%s' is not a directory\n", path);
      free((uint32_t)dir, sizeof(fat_dir_t));
      return false;
   }

   // check directory is empty (only . and .. entries allowed)
   uint32_t clusterSize = fat_bpb->sectorsPerCluster * fat_bpb->bytesPerSector;
   uint32_t dirFirstSector = ((dir->firstClusterNo - 2) * fat_bpb->sectorsPerCluster) + firstDataSector;
   uint8_t *dirBuf = ata_read_exact(true, true, baseAddr + dirFirstSector * fat_bpb->bytesPerSector, clusterSize);
   if(!dirBuf) {
      debug_printf("fat_delete_dir: failed reading '%s'\n", path);
      free((uint32_t)dir, sizeof(fat_dir_t));
      return false;
   }
   int entries = clusterSize / sizeof(fat_dir_t);
   bool empty = true;
   for(int i = 0; i < entries; i++) {
      fat_dir_t *e = (fat_dir_t*)(dirBuf + i * sizeof(fat_dir_t));
      if(e->filename[0] == 0) break;
      if(e->filename[0] == 0xE5) continue; // deleted
      if(e->filename[0] == '.' && (e->filename[1] == ' ' || e->filename[1] == '.')) continue; // . or ..
      empty = false;
      break;
   }
   free((uint32_t)dirBuf, clusterSize);

   if(!empty) {
      debug_printf("fat_delete_dir: '%s' is not empty\n", path);
      free((uint32_t)dir, sizeof(fat_dir_t));
      return false;
   }

   char name[9];
   strcpy_fixed(name, (char*)dir->filename, 8);
   strsplit(name, NULL, name, ' ');

   fat_dir_t deleted = *dir;
   deleted.filename[0] = 0xE5;

   char dirname[256], parentpath[256];
   strsplit_last(parentpath, dirname, path, '/');
   if(strequ(parentpath, "")) strcpy(parentpath, "/");
   bool inroot = strequ(parentpath, "/");

   uint16_t parentCluster = 0;
   if(!inroot) {
      fat_dir_t *parent = fat_parse_path(parentpath, true);
      if(!parent) {
         debug_printf("fat_delete_dir: parent '%s' not found\n", parentpath);
         free((uint32_t)dir, sizeof(fat_dir_t));
         return false;
      }
      parentCluster = parent->firstClusterNo;
      free((uint32_t)parent, sizeof(fat_dir_t));
   }

   bool ok = fat_update_in_dir(parentCluster, name, "", &deleted);
   if(ok) {
      // free cluster chain
      uint16_t head = dir->firstClusterNo;
      uint16_t cur = head;
      uint32_t min = cur;
      uint32_t max = cur;
      while(fat_valid_cluster(cur)) {
         uint16_t next = ((uint16_t*)fat_table)[cur];
         if(!fat_table_update_cluster(cur, 0))
            ok = false;
         if(cur < min) min = cur;
         if(cur > max) max = cur;
         cur = next;
      }
      if(!fat_table_flush_cache(min, max))
         ok = false;
      if(!ok)
         debug_printf("FAT error: failed freeing chain from cluster %u\n", head);
   }
   free((uint32_t)dir, sizeof(fat_dir_t));
   return ok;
}

fat_bpb_t fat_get_bpb() {
   return *fat_bpb;
}