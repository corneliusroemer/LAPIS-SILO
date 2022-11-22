//
// Created by Alexander Taepper on 16.11.22.
//

#ifndef SILO_DATABASE_H
#define SILO_DATABASE_H

#include <silo/meta_store.h>
#include <silo/sequence_store.h>
#include <silo/silo.h>

namespace silo {

struct partition_t {
   std::string name;
   uint32_t count;
   std::vector<chunk_t> chunks;
};

struct partitioning_descriptor_t {
   std::vector<partition_t> partitions;
};

struct pango_descriptor_t {
   std::vector<pango_t> pangos;
};

class DatabasePartition {
   public:
   MetaStore meta_store;
   SequenceStore seq_store;

   std::vector<silo::chunk_t> chunks;

   unsigned sequenceCount;
};

class Database {
   private:
   std::unordered_map<std::string, std::string> alias_key;

   std::unordered_map<std::string, uint32_t> dict_lookup;
   std::vector<std::string> dict;

   public:
   std::vector<DatabasePartition> partitions;
   std::unique_ptr<pango_descriptor_t> pango_def;
   std::unique_ptr<partitioning_descriptor_t> part_def;

   const std::unordered_map<std::string, std::string> get_alias_key() {
      return alias_key;
   }

   Database() {
      std::ifstream alias_key_file("../Data/pango_alias.txt");
      if (!alias_key_file) {
         std::cerr << "Expected file Data/pango_alias.txt." << std::endl;
      }
      while (true) {
         std::string alias, val;
         if (!getline(alias_key_file, alias, '\t')) break;
         if (!getline(alias_key_file, val, '\n')) break;
         alias_key[alias] = val;
      }
   }

   void build(const std::string& part_prefix, const std::string& meta_suffix, const std::string& seq_suffix);
};

unsigned processSeq(SequenceStore& seq_store, std::istream& in);

unsigned processMeta(MetaStore& meta_store, std::istream& in, const std::unordered_map<std::string, std::string>& alias_key);

} // namespace silo

#endif //SILO_DATABASE_H
