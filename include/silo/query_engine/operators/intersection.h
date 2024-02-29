#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "silo/query_engine/operator_result.h"
#include "silo/query_engine/operators/operator.h"

namespace silo::query_engine::filter_expressions {
class And;
class NOf;
}  // namespace silo::query_engine::filter_expressions

namespace silo::query_engine::operators {

class Intersection : public Operator {
   friend class silo::query_engine::filter_expressions::And;
   friend class silo::query_engine::filter_expressions::NOf;

   std::vector<std::unique_ptr<Operator>> children;
   std::vector<std::unique_ptr<Operator>> negated_children;
   uint32_t row_count;

  public:
   explicit Intersection(
      std::vector<std::unique_ptr<Operator>>&& children,
      std::vector<std::unique_ptr<Operator>>&& negated_children,
      uint32_t row_count
   );

   ~Intersection() noexcept override;

   virtual std::string toString() const override;

   [[nodiscard]] Type type() const override;

   bool isNegatedDisjointUnion() const;

   virtual OperatorResult evaluate() const override;

   static std::unique_ptr<Operator> negate(std::unique_ptr<Intersection>&& intersection);
};

}  // namespace silo::query_engine::operators
