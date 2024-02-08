#include "silo/query_engine/filter_expressions/aa_symbol_equals.h"

#include <optional>
#include <utility>

#include <nlohmann/json.hpp>

#include "silo/common/aa_symbols.h"
#include "silo/query_engine/filter_expressions/and.h"
#include "silo/query_engine/filter_expressions/expression.h"
#include "silo/query_engine/filter_expressions/negation.h"
#include "silo/query_engine/operators/bitmap_selection.h"
#include "silo/query_engine/operators/complement.h"
#include "silo/query_engine/operators/index_scan.h"
#include "silo/query_engine/operators/operator.h"
#include "silo/query_engine/query_parse_exception.h"
#include "silo/storage/database_partition.h"
#include "silo/storage/position.h"
#include "silo/storage/sequence_store.h"

namespace silo {
class Database;
}  // namespace silo

namespace silo::query_engine::filter_expressions {

AASymbolEquals::AASymbolEquals(
   std::string aa_sequence_name,
   uint32_t position_idx,
   std::optional<AminoAcid::Symbol> value
)
    : aa_sequence_name(std::move(aa_sequence_name)),
      position_idx(position_idx),
      value(value) {}

std::string AASymbolEquals::toString(const silo::Database& /*database*/) const {
   const char symbol_char = value.has_value() ? AminoAcid::symbolToChar(*value) : '.';
   return aa_sequence_name + ":" + std::to_string(position_idx + 1) + std::to_string(symbol_char);
}

std::unique_ptr<silo::query_engine::operators::Operator> AASymbolEquals::compile(
   const silo::Database& database,
   const silo::DatabasePartition& database_partition,
   Expression::AmbiguityMode /*mode*/
) const {
   const auto& aa_store_partition = database_partition.aa_sequences.at(aa_sequence_name);
   if (position_idx >= aa_store_partition.reference_sequence.size()) {
      throw QueryParseException(
         "AminoAcidEquals position is out of bounds '" + std::to_string(position_idx + 1) +
         "' > '" + std::to_string(aa_store_partition.reference_sequence.size()) + "'"
      );
   }
   const AminoAcid::Symbol aa_symbol =
      value.value_or(aa_store_partition.reference_sequence.at(position_idx));
   if (aa_symbol == AminoAcid::SYMBOL_MISSING) {
      return std::make_unique<operators::BitmapSelection>(
         aa_store_partition.missing_symbol_bitmaps.data(),
         aa_store_partition.missing_symbol_bitmaps.size(),
         operators::BitmapSelection::CONTAINS,
         position_idx
      );
   }
   if (aa_store_partition.positions[position_idx].isSymbolFlipped(aa_symbol)) {
      return std::make_unique<operators::Complement>(
         std::make_unique<operators::IndexScan>(
            aa_store_partition.getBitmap(position_idx, aa_symbol), database_partition.sequence_count
         ),
         database_partition.sequence_count
      );
   }
   if (aa_store_partition.positions[position_idx].isSymbolDeleted(aa_symbol)) {
      std::vector<AminoAcid::Symbol> symbols =
         std::vector<AminoAcid::Symbol>(AminoAcid::SYMBOLS.begin(), AminoAcid::SYMBOLS.end());
      // NOLINTNEXTLINE(bugprone-unused-return-value)
      (void)std::remove(symbols.begin(), symbols.end(), aa_symbol);
      std::vector<std::unique_ptr<filter_expressions::Expression>> symbol_filters;
      std::transform(
         symbols.begin(),
         symbols.end(),
         std::back_inserter(symbol_filters),
         [&](AminoAcid::Symbol symbol) {
            return std::make_unique<Negation>(
               std::make_unique<AASymbolEquals>(aa_sequence_name, position_idx, symbol)
            );
         }
      );
      return And(std::move(symbol_filters)).compile(database, database_partition, NONE);
   }
   return std::make_unique<operators::IndexScan>(
      aa_store_partition.getBitmap(position_idx, aa_symbol), database_partition.sequence_count
   );
}

// NOLINTNEXTLINE(readability-identifier-naming)
void from_json(const nlohmann::json& json, std::unique_ptr<AASymbolEquals>& filter) {
   CHECK_SILO_QUERY(
      json.contains("sequenceName") && json["sequenceName"].is_string(),
      "AminoAcidEquals expression requires the string field sequenceName"
   )
   CHECK_SILO_QUERY(
      json.is_object() && json.contains("position"),
      "The field 'position' is required in a AminoAcidEquals expression"
   )
   CHECK_SILO_QUERY(
      json["position"].is_number_unsigned() && json["position"].get<uint32_t>() > 0,
      "The field 'position' in a AminoAcidEquals expression needs to be an unsigned "
      "integer greater than 0"
   )
   CHECK_SILO_QUERY(
      json.contains("symbol") && json["symbol"].is_string(),
      "The string field 'symbol' is required in a AminoAcidEquals expression"
   )
   const std::string aa_sequence_name = json["sequenceName"].get<std::string>();
   const uint32_t position_idx = json["position"].get<uint32_t>() - 1;
   const std::string aa_char = json["symbol"].get<std::string>();

   CHECK_SILO_QUERY(
      aa_char.size() == 1, "The string field 'symbol' must be exactly one character long"
   )
   const std::optional<AminoAcid::Symbol> aa_value = AminoAcid::charToSymbol(aa_char.at(0));
   CHECK_SILO_QUERY(
      aa_value.has_value() || aa_char.at(0) == '.',
      "The string field 'symbol' must be either a valid amino acid or the '.' symbol."
   )
   filter = std::make_unique<AASymbolEquals>(aa_sequence_name, position_idx, aa_value);
}

}  // namespace silo::query_engine::filter_expressions
