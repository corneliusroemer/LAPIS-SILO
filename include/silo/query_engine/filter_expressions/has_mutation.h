#ifndef SILO_HAS_MUTATION_H
#define SILO_HAS_MUTATION_H

#include "silo/query_engine/filter_expressions/expression.h"

namespace silo::query_engine::filter_expressions {

struct HasMutation : public Expression {
  private:
   unsigned position;

  public:
   explicit HasMutation(unsigned position);

   std::string toString(const Database& database) override;

   [[nodiscard]] std::unique_ptr<silo::query_engine::operators::Operator> compile(
      const Database& database,
      const DatabasePartition& database_partition
   ) const override;
};

}  // namespace silo::query_engine::filter_expressions

#endif  // SILO_HAS_MUTATION_H
