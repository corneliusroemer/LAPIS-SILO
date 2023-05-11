#ifndef SILO_PANGO_LINEAGE_H
#define SILO_PANGO_LINEAGE_H

#include <memory>
#include <string>

#include "silo/query_engine/filter_expressions/expression.h"

namespace silo::query_engine::filter_expressions {

struct PangoLineage : public Expression {
   uint32_t lineage_key;
   bool include_sublineages;

   explicit PangoLineage(uint32_t lineage_key, bool include_sublineages);

   std::string toString(const Database& database) override;

   [[nodiscard]] std::unique_ptr<silo::query_engine::operators::Operator> compile(
      const Database& database,
      const DatabasePartition& database_partition
   ) const override;
};

}  // namespace silo::query_engine::filter_expressions

#endif  // SILO_PANGO_LINEAGE_H
