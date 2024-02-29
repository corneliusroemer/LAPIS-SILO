#include "silo/query_engine/filter_expressions/and.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include <fmt/format.h>
#include <spdlog/spdlog.h>
#include <boost/algorithm/string/join.hpp>
#include <nlohmann/json.hpp>

#include "silo/query_engine/filter_expressions/expression.h"
#include "silo/query_engine/operators/complement.h"
#include "silo/query_engine/operators/empty.h"
#include "silo/query_engine/operators/full.h"
#include "silo/query_engine/operators/intersection.h"
#include "silo/query_engine/operators/operator.h"
#include "silo/query_engine/operators/selection.h"
#include "silo/query_engine/operators/union.h"
#include "silo/query_engine/query_parse_exception.h"
#include "silo/storage/database_partition.h"

namespace silo {
struct Database;
}  // namespace silo

namespace silo::query_engine::filter_expressions {

using OperatorVector = std::vector<std::unique_ptr<operators::Operator>>;
using operators::Operator;

And::And(std::vector<std::unique_ptr<Expression>>&& children)
    : children(std::move(children)) {}

std::string And::toString() const {
   std::vector<std::string> child_strings;
   std::transform(
      children.begin(),
      children.end(),
      std::back_inserter(child_strings),
      [&](const std::unique_ptr<Expression>& child) { return child->toString(); }
   );
   return "And(" + boost::algorithm::join(child_strings, " & ") + ")";
}

namespace {

template <typename T>
void inline appendVectorToVector(
   std::vector<std::unique_ptr<T>>& vec_1,
   std::vector<std::unique_ptr<T>>& vec_2
) {
   std::transform(
      vec_1.begin(),
      vec_1.end(),
      std::back_inserter(vec_2),
      [&](std::unique_ptr<T>& ele) { return std::move(ele); }
   );
}
}  // namespace

void logCompiledChildren(
   OperatorVector& non_negated_child_operators,
   OperatorVector& negated_child_operators,
   std::vector<std::unique_ptr<operators::Predicate>>& predicates
) {
   std::vector<std::string> child_operator_strings;
   std::transform(
      non_negated_child_operators.begin(),
      non_negated_child_operators.end(),
      std::back_inserter(child_operator_strings),
      [&](const std::unique_ptr<operators::Operator>& operator_) { return operator_->toString(); }
   );
   std::transform(
      negated_child_operators.begin(),
      negated_child_operators.end(),
      std::back_inserter(child_operator_strings),
      [&](const std::unique_ptr<operators::Operator>& operator_) {
         return "!" + operator_->toString();
      }
   );
   std::vector<std::string> predicate_strings;
   std::transform(
      predicates.begin(),
      predicates.end(),
      std::back_inserter(predicate_strings),
      [&](const std::unique_ptr<operators::Predicate>& predicate) { return predicate->toString(); }
   );
   SPDLOG_TRACE(
      "Compiled and processed child operators: {}, predicates {}, children: {}, negated children: "
      "{}, predicates: {}",
      fmt::join(child_operator_strings, ","),
      fmt::join(predicate_strings, ","),
      non_negated_child_operators.size(),
      negated_child_operators.size(),
      predicates.size()
   );
}

std::tuple<OperatorVector, OperatorVector, std::vector<std::unique_ptr<operators::Predicate>>> And::
   compileChildren(
      const Database& database,
      const DatabasePartition& database_partition,
      AmbiguityMode mode
   ) const {
   OperatorVector all_child_operators;
   std::transform(
      children.begin(),
      children.end(),
      std::back_inserter(all_child_operators),
      [&](const std::unique_ptr<Expression>& expression) {
         return expression->compile(database, database_partition, mode);
      }
   );
   OperatorVector non_negated_child_operators;
   OperatorVector negated_child_operators;
   std::vector<std::unique_ptr<operators::Predicate>> predicates;
   for (auto& child : all_child_operators) {
      if (child->type() == operators::FULL) {
         SPDLOG_TRACE("Skipping full child");
         continue;
      }
      if (child->type() == operators::EMPTY) {
         SPDLOG_TRACE("Shortcutting because found empty child");
         OperatorVector empty;
         empty.emplace_back(std::make_unique<operators::Empty>(database_partition.sequence_count));
         return {
            std::move(empty), OperatorVector(), std::vector<std::unique_ptr<operators::Predicate>>{}
         };
      }
      if (child->type() == operators::INTERSECTION) {
         auto* intersection_child = dynamic_cast<operators::Intersection*>(child.get());
         appendVectorToVector(intersection_child->children, non_negated_child_operators);
         appendVectorToVector(intersection_child->negated_children, negated_child_operators);
      } else if (child->type() == operators::COMPLEMENT) {
         negated_child_operators.emplace_back(operators::Operator::negate(std::move(child)));
      } else if (child->type() == operators::SELECTION) {
         auto* selection_child = dynamic_cast<operators::Selection*>(child.get());
         appendVectorToVector<operators::Predicate>(selection_child->predicates, predicates);
         SPDLOG_TRACE(
            "Found selection, appended {} predicates", selection_child->predicates.size()
         );
         if (selection_child->child_operator.has_value()) {
            SPDLOG_TRACE(
               "Appending child of selection {}", selection_child->child_operator->get()->toString()
            );
            all_child_operators.emplace_back(std::move(*selection_child->child_operator));
         }
      } else {
         non_negated_child_operators.push_back(std::move(child));
      }
   }

#if SPDLOG_ACTIVE_LEVEL <= SPDLOG_LEVEL_TRACE
   logCompiledChildren(non_negated_child_operators, negated_child_operators, predicates);
#endif

   return {
      std::move(non_negated_child_operators),
      std::move(negated_child_operators),
      std::move(predicates)
   };
}

std::unique_ptr<Operator> And::compile(
   const Database& database,
   const DatabasePartition& database_partition,
   AmbiguityMode mode
) const {
   auto [non_negated_child_operators, negated_child_operators, predicates] =
      compileChildren(database, database_partition, mode);

   if (non_negated_child_operators.empty() && negated_child_operators.empty()) {
      if (predicates.empty()) {
         SPDLOG_TRACE(
            "Compiled And filter expression to Full, since no predicates and no child operators"
         );
         return std::make_unique<operators::Full>(database_partition.sequence_count);
      }
      auto result = std::make_unique<operators::Selection>(
         std::move(predicates), database_partition.sequence_count
      );
      SPDLOG_TRACE(
         "Compiled And filter expression to {} - found only predicates", result->toString()
      );

      return result;
   }

   std::unique_ptr<Operator> index_arithmetic_operator;
   if (non_negated_child_operators.size() == 1 && negated_child_operators.empty()) {
      index_arithmetic_operator = std::move(non_negated_child_operators[0]);
   } else if (negated_child_operators.size() == 1 && non_negated_child_operators.empty()) {
      index_arithmetic_operator = std::make_unique<operators::Complement>(
         std::move(negated_child_operators[0]), database_partition.sequence_count
      );
   } else if (non_negated_child_operators.empty()) {
      std::unique_ptr<operators::Union> union_ret = std::make_unique<operators::Union>(
         std::move(negated_child_operators), database_partition.sequence_count
      );
      index_arithmetic_operator = std::make_unique<operators::Complement>(
         std::move(union_ret), database_partition.sequence_count
      );
   } else {
      index_arithmetic_operator = std::make_unique<operators::Intersection>(
         std::move(non_negated_child_operators),
         std::move(negated_child_operators),
         database_partition.sequence_count
      );
   }
   if (predicates.empty()) {
      SPDLOG_TRACE(
         "Compiled And filter expression to {} - found no predicates",
         index_arithmetic_operator->toString()
      );

      return index_arithmetic_operator;
   }
   auto result = std::make_unique<operators::Selection>(
      std::move(index_arithmetic_operator), std::move(predicates), database_partition.sequence_count
   );

   SPDLOG_TRACE("Compiled And filter expression to {}", result->toString());

   return result;
}

// NOLINTNEXTLINE(readability-identifier-naming)
void from_json(const nlohmann::json& json, std::unique_ptr<And>& filter) {
   CHECK_SILO_QUERY(
      json.contains("children"), "The field 'children' is required in an And expression"
   )
   CHECK_SILO_QUERY(
      json["children"].is_array(), "The field 'children' in an And expression needs to be an array"
   )
   auto children = json.at("children").get<std::vector<std::unique_ptr<Expression>>>();
   filter = std::make_unique<And>(std::move(children));
}

}  // namespace silo::query_engine::filter_expressions
