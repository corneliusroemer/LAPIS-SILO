#include "silo/query_engine/actions/aggregated.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#include <oneapi/tbb/blocked_range.h>
#include <oneapi/tbb/concurrent_vector.h>
#include <oneapi/tbb/enumerable_thread_specific.h>
#include <oneapi/tbb/parallel_for.h>
#include <nlohmann/json.hpp>
#include <roaring/roaring.hh>

#include "silo/common/date.h"
#include "silo/common/pango_lineage.h"
#include "silo/common/string.h"
#include "silo/common/types.h"
#include "silo/config/database_config.h"
#include "silo/database.h"
#include "silo/query_engine/actions/action.h"
#include "silo/query_engine/actions/tuple.h"
#include "silo/query_engine/operator_result.h"
#include "silo/query_engine/query_parse_exception.h"
#include "silo/query_engine/query_result.h"
#include "silo/storage/column/date_column.h"
#include "silo/storage/column/float_column.h"
#include "silo/storage/column/indexed_string_column.h"
#include "silo/storage/column/int_column.h"
#include "silo/storage/column/pango_lineage_column.h"
#include "silo/storage/column/string_column.h"
#include "silo/storage/column_group.h"
#include "silo/storage/database_partition.h"

namespace {

std::vector<silo::storage::ColumnMetadata> parseGroupByFields(
   const silo::Database& database,
   const std::vector<std::string>& group_by_fields
) {
   std::vector<silo::storage::ColumnMetadata> group_by_metadata;
   for (const std::string& group_by_field : group_by_fields) {
      const auto& metadata = database.database_config.getMetadata(group_by_field);
      CHECK_SILO_QUERY(
         metadata.has_value(), "Metadata field '" + group_by_field + "' to group by not found"
      )
      group_by_metadata.push_back({metadata->name, metadata->getColumnType()});
   }
   return group_by_metadata;
}

}  // namespace

namespace silo::query_engine::actions {

const std::string COUNT_FIELD = "count";

std::vector<QueryResultEntry> generateResult(std::unordered_map<Tuple, uint32_t>& tuple_counts) {
   std::vector<QueryResultEntry> result;
   result.reserve(tuple_counts.size());
   for (auto& [tuple, count] : tuple_counts) {
      std::map<std::string, std::optional<std::variant<std::string, int32_t, double>>> fields =
         tuple.getFields();
      fields[COUNT_FIELD] = static_cast<int32_t>(count);
      result.push_back({fields});
   }
   return result;
}

QueryResult aggregateWithoutGrouping(const std::vector<OperatorResult>& bitmap_filters) {
   uint32_t count = 0;
   for (const auto& filter : bitmap_filters) {
      count += filter->cardinality();
   };
   std::map<std::string, std::optional<std::variant<std::string, int32_t, double>>> tuple_fields;
   tuple_fields[COUNT_FIELD] = static_cast<int32_t>(count);
   return QueryResult{std::vector<QueryResultEntry>{{tuple_fields}}};
}

Aggregated::Aggregated(std::vector<std::string> group_by_fields)
    : group_by_fields(std::move(group_by_fields)) {}

void Aggregated::validateOrderByFields(const Database& database) const {
   const std::vector<silo::storage::ColumnMetadata> field_metadata =
      parseGroupByFields(database, group_by_fields);

   for (const OrderByField& field : order_by_fields) {
      CHECK_SILO_QUERY(
         field.name == COUNT_FIELD || std::any_of(
                                         field_metadata.begin(),
                                         field_metadata.end(),
                                         [&](const silo::storage::ColumnMetadata& metadata) {
                                            return metadata.name == field.name;
                                         }
                                      ),
         "The orderByField '" + field.name +
            "' cannot be ordered by, as it does not appear in the groupByFields."
      )
   }
}

QueryResult Aggregated::execute(
   const Database& database,
   std::vector<OperatorResult> bitmap_filters
) const {
   if (group_by_fields.empty()) {
      return aggregateWithoutGrouping(bitmap_filters);
   }
   // TODO(#133) optimize when equal to partition_by field
   // TODO(#133) optimize single field groupby

   const std::vector<silo::storage::ColumnMetadata> group_by_metadata =
      parseGroupByFields(database, group_by_fields);

   std::vector<storage::ColumnPartitionGroup> group_by_column_groups;
   group_by_column_groups.reserve(database.partitions.size());
   for (const auto& partition : database.partitions) {
      group_by_column_groups.emplace_back(partition.columns.getSubgroup(group_by_metadata));
   }

   const size_t tuple_size = getTupleSize(group_by_metadata);

   tbb::enumerable_thread_specific<std::unordered_map<Tuple, uint32_t>> maps;

   tbb::parallel_for(
      tbb::blocked_range<uint32_t>(0, database.partitions.size()),
      [&](tbb::blocked_range<uint32_t> range) {
         std::unordered_map<Tuple, uint32_t>& map = maps.local();
         for (uint32_t partition_id = range.begin(); partition_id != range.end(); ++partition_id) {
            for (const uint32_t sequence_id : *bitmap_filters[partition_id]) {
               ++map[Tuple(sequence_id, &group_by_column_groups[partition_id], tuple_size)];
            }
         }
      }
   );
   std::unordered_map<Tuple, uint32_t> final_map;
   for (auto& map : maps) {
      for (auto& [key, value] : map) {
         final_map[key] += value;
      }
   }
   QueryResult result = {generateResult(final_map)};
   return result;
}

// NOLINTNEXTLINE(readability-identifier-naming)
void from_json(const nlohmann::json& json, std::unique_ptr<Aggregated>& action) {
   const std::vector<std::string> group_by_fields =
      json.value("groupByFields", std::vector<std::string>());
   action = std::make_unique<Aggregated>(group_by_fields);
}

}  // namespace silo::query_engine::actions
