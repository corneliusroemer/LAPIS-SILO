#pragma once

#include "silo/config/database_config.h"
#include "silo/preprocessing/preprocessing_config.h"
#include "silo/preprocessing/preprocessing_database.h"

namespace silo {
class Database;
class PangoLineageAliasLookup;

namespace preprocessing {

class Preprocessor {
   PreprocessingConfig preprocessing_config;
   config::DatabaseConfig database_config;
   PreprocessingDatabase preprocessing_db;

  public:
   Preprocessor(
      const preprocessing::PreprocessingConfig preprocessing_config,
      const config::DatabaseConfig database_config
   );

   Database preprocess();

  private:
   void buildTablesFromNdjsonInput(
      const std::filesystem::path& file_name,
      const ReferenceGenomes& reference_genomes
   );
   void buildMetadataTableFromFile(const std::filesystem::path& metadata_filename);

   void buildPartitioningTable();
   void buildPartitioningTableByColumn(const std::string& partition_by_field);
   void buildEmptyPartitioning();

   void createSequenceViews(const ReferenceGenomes& reference_genomes);
   void createPartitionedSequenceTables(const ReferenceGenomes& reference_genomes);
   void createPartitionedTableForSequence(
      const std::string& sequence_name,
      const std::string& reference_sequence,
      const std::filesystem::path& filename,
      const std::string& table_prefix
   );

   Database buildDatabase(
      const preprocessing::Partitions& partition_descriptor,
      const ReferenceGenomes& reference_genomes,
      const std::string& order_by_clause,
      const silo::PangoLineageAliasLookup& alias_key
   );
};
}  // namespace preprocessing
}  // namespace silo
