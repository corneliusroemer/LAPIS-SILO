#include "silo/query_engine/filter_expressions/true.h"

#include "silo/query_engine/filter_expressions/expression.h"
#include "silo/query_engine/operators/full.h"

#include "silo/storage/database_partition.h"

namespace silo::query_engine::filter_expressions {

True::True() = default;

std::string True::toString(const silo::Database& /*database*/) {
   return "True";
}

std::unique_ptr<silo::query_engine::operators::Operator> True::compile(
   const silo::Database& /*database*/,
   const silo::DatabasePartition& database_partition
) const {
   return std::make_unique<operators::Full>(database_partition.sequenceCount);
}

}  // namespace silo::query_engine::filter_expressions