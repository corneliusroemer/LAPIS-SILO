#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include <fmt/format.h>

namespace duckdb {
class Connection;
}

namespace silo {

class ReferenceGenomes;

namespace preprocessing {

class PreprocessingDatabase;

class SequenceInfo {
   std::vector<std::string> nuc_sequence_names;
   std::vector<std::string> aa_sequence_names;

  public:
   SequenceInfo(const silo::ReferenceGenomes& reference_genomes);

   std::vector<std::string> getSequenceSelects(
      const silo::preprocessing::PreprocessingDatabase& preprocessingDatabase
   );

   void validate(duckdb::Connection& connection, const std::filesystem::path& input_filename) const;
};
}  // namespace preprocessing
}  // namespace silo