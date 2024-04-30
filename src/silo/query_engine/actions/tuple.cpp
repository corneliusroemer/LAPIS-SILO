//! Runtime-defined compact tuple for one row of the database, for the
//! columns requested by the user.

//! Used for:

//! - index in hash tables for aggregation (hashing and comparison (via memcmp))
//! - currently sorting for order by queries in Details (via TupleFactory)

#include "silo/query_engine/actions/tuple.h"

#include <cassert>
#include <cmath>
#include <compare>
#include <cstring>
#include <stdexcept>
#include <string_view>
#include <utility>

#include <boost/container_hash/hash.hpp>

#include "silo/common/date.h"
#include "silo/common/optional_bool.h"
#include "silo/common/string.h"
#include "silo/common/types.h"
#include "silo/config/database_config.h"
#include "silo/query_engine/actions/action.h"
#include "silo/storage/column/float_column.h"
#include "silo/storage/column_group.h"

using silo::query_engine::actions::Tuple;
using silo::query_engine::actions::TupleFactory;

namespace {
using silo::common::Date;
using silo::common::OptionalBool;
using silo::common::String;
using silo::common::STRING_SIZE;
using silo::config::ColumnType;

void assignTupleField(
   std::byte** data_pointer,
   uint32_t sequence_id,
   const silo::storage::ColumnMetadata& metadata,
   const silo::storage::ColumnPartitionGroup& columns
) {
   if (metadata.type == ColumnType::DATE) {
      const Date value = columns.date_columns.at(metadata.name).getValues()[sequence_id];
      *reinterpret_cast<Date*>(*data_pointer) = value;
      *data_pointer += sizeof(decltype(value));
   } else if (metadata.type == ColumnType::BOOL) {
      const OptionalBool value = columns.bool_columns.at(metadata.name).getValues()[sequence_id];
      *reinterpret_cast<OptionalBool*>(*data_pointer) = value;
      *data_pointer += sizeof(decltype(value));
   } else if (metadata.type == ColumnType::INT) {
      const int32_t value = columns.int_columns.at(metadata.name).getValues()[sequence_id];
      *reinterpret_cast<int32_t*>(*data_pointer) = value;
      *data_pointer += sizeof(decltype(value));
   } else if (metadata.type == ColumnType::FLOAT) {
      const double value = columns.float_columns.at(metadata.name).getValues()[sequence_id];
      *reinterpret_cast<double*>(*data_pointer) = value;
      *data_pointer += sizeof(decltype(value));
   } else if (metadata.type == ColumnType::STRING) {
      const String<STRING_SIZE> value =
         columns.string_columns.at(metadata.name).getValues()[sequence_id];
      *reinterpret_cast<String<STRING_SIZE>*>(*data_pointer) = value;
      *data_pointer += sizeof(decltype(value));
   } else if (metadata.type == ColumnType::INDEXED_PANGOLINEAGE) {
      const silo::Idx value =
         columns.pango_lineage_columns.at(metadata.name).getValues()[sequence_id];
      *reinterpret_cast<silo::Idx*>(*data_pointer) = value;
      *data_pointer += sizeof(decltype(value));
   } else if (metadata.type == ColumnType::INDEXED_STRING) {
      const silo::Idx value =
         columns.indexed_string_columns.at(metadata.name).getValues()[sequence_id];
      *reinterpret_cast<silo::Idx*>(*data_pointer) = value;
      *data_pointer += sizeof(decltype(value));
   } else if (metadata.type == ColumnType::NUC_INSERTION) {
      const silo::Idx value =
         columns.nuc_insertion_columns.at(metadata.name).getValues()[sequence_id];
      *reinterpret_cast<silo::Idx*>(*data_pointer) = value;
      *data_pointer += sizeof(decltype(value));
   } else if (metadata.type == ColumnType::AA_INSERTION) {
      const silo::Idx value =
         columns.aa_insertion_columns.at(metadata.name).getValues()[sequence_id];
      *reinterpret_cast<silo::Idx*>(*data_pointer) = value;
      *data_pointer += sizeof(decltype(value));
   } else {
      throw std::runtime_error("Unchecked column type of column " + metadata.name);
   }
}

silo::common::JsonValueType tupleFieldToValueType(
   const std::byte** data_pointer,
   const silo::storage::ColumnMetadata& metadata,
   const silo::storage::ColumnPartitionGroup& columns
) {
   if (metadata.type == ColumnType::DATE) {
      const Date value = *reinterpret_cast<const Date*>(*data_pointer);
      *data_pointer += sizeof(decltype(value));
      return silo::common::dateToString(value);
   }
   if (metadata.type == ColumnType::BOOL) {
      const OptionalBool value = *reinterpret_cast<const OptionalBool*>(*data_pointer);
      *data_pointer += sizeof(decltype(value));
      if (value.isNull()) {
         return std::nullopt;
      }
      return value.value();
   }
   if (metadata.type == ColumnType::INT) {
      const int32_t value = *reinterpret_cast<const int32_t*>(*data_pointer);
      *data_pointer += sizeof(decltype(value));
      if (value == INT32_MIN) {
         return std::nullopt;
      }
      return value;
   }
   if (metadata.type == ColumnType::FLOAT) {
      const double value = *reinterpret_cast<const double*>(*data_pointer);
      *data_pointer += sizeof(decltype(value));
      if (std::isnan(value)) {
         return std::nullopt;
      }
      return value;
   }
   if (metadata.type == ColumnType::STRING) {
      const String<STRING_SIZE> value =
         *reinterpret_cast<const String<STRING_SIZE>*>(*data_pointer);
      *data_pointer += sizeof(decltype(value));
      std::string string_value = columns.string_columns.at(metadata.name).lookupValue(value);
      if (string_value.empty()) {
         return std::nullopt;
      }
      return std::move(string_value);
   }
   if (metadata.type == ColumnType::INDEXED_PANGOLINEAGE) {
      const silo::Idx value = *reinterpret_cast<const silo::Idx*>(*data_pointer);
      *data_pointer += sizeof(decltype(value));
      std::string string_value =
         columns.pango_lineage_columns.at(metadata.name).lookupAliasedValue(value).value;
      if (string_value.empty()) {
         return std::nullopt;
      }
      return std::move(string_value);
   }
   if (metadata.type == ColumnType::INDEXED_STRING) {
      const silo::Idx value = *reinterpret_cast<const silo::Idx*>(*data_pointer);
      *data_pointer += sizeof(decltype(value));
      std::string string_value =
         columns.indexed_string_columns.at(metadata.name).lookupValue(value);
      if (string_value.empty()) {
         return std::nullopt;
      }
      return std::move(string_value);
   }
   if (metadata.type == ColumnType::NUC_INSERTION) {
      const silo::Idx value = *reinterpret_cast<const silo::Idx*>(*data_pointer);
      *data_pointer += sizeof(decltype(value));
      std::string string_value = columns.nuc_insertion_columns.at(metadata.name).lookupValue(value);
      if (string_value.empty()) {
         return std::nullopt;
      }
      return std::move(string_value);
   }
   if (metadata.type == ColumnType::AA_INSERTION) {
      const silo::Idx value = *reinterpret_cast<const silo::Idx*>(*data_pointer);
      *data_pointer += sizeof(decltype(value));
      std::string string_value = columns.aa_insertion_columns.at(metadata.name).lookupValue(value);
      if (string_value.empty()) {
         return std::nullopt;
      }
      return std::move(string_value);
   }
   throw std::runtime_error("Unchecked column type of column " + metadata.name);
}

std::strong_ordering compareDouble(double value1, double value2) {
   const std::partial_ordering compare = value1 <=> value2;
   if (compare == std::partial_ordering::less) {
      return std::strong_ordering::less;
   }
   if (compare == std::partial_ordering::equivalent) {
      return std::strong_ordering::equal;
   }
   if (compare == std::partial_ordering::greater) {
      return std::strong_ordering::greater;
   }
   if (std::isnan(value2)) {
      if (std::isnan(value1)) {
         return std::strong_ordering::equal;
      }
      return std::strong_ordering::less;
   }
   return std::strong_ordering::greater;
}

std::strong_ordering compareString(const std::string& value1, const std::string& value2) {
   const int compare_value = value1.compare(value2);
   if (compare_value < 0) {
      return std::strong_ordering::less;
   }
   if (compare_value > 0) {
      return std::strong_ordering::greater;
   }
   return std::strong_ordering::equal;
}

std::strong_ordering compareTupleFields(
   const std::byte** data_pointer1,
   const std::byte** data_pointer2,
   const silo::storage::ColumnMetadata& metadata,
   const silo::storage::ColumnPartitionGroup& columns
) {
   if (metadata.type == ColumnType::DATE) {
      const Date value1 = *reinterpret_cast<const Date*>(*data_pointer1);
      *data_pointer1 += sizeof(decltype(value1));
      const Date value2 = *reinterpret_cast<const Date*>(*data_pointer2);
      *data_pointer2 += sizeof(decltype(value2));
      return value1 <=> value2;
   }
   if (metadata.type == ColumnType::BOOL) {
      const OptionalBool value1 = *reinterpret_cast<const OptionalBool*>(*data_pointer1);
      *data_pointer1 += sizeof(decltype(value1));
      const OptionalBool value2 = *reinterpret_cast<const OptionalBool*>(*data_pointer2);
      *data_pointer2 += sizeof(decltype(value2));
      return value1 <=> value2;
   }
   if (metadata.type == ColumnType::INT) {
      const int32_t value1 = *reinterpret_cast<const int32_t*>(*data_pointer1);
      *data_pointer1 += sizeof(decltype(value1));
      const int32_t value2 = *reinterpret_cast<const int32_t*>(*data_pointer2);
      *data_pointer2 += sizeof(decltype(value2));
      return value1 <=> value2;
   }
   if (metadata.type == ColumnType::FLOAT) {
      const double value1 = *reinterpret_cast<const double*>(*data_pointer1);
      *data_pointer1 += sizeof(decltype(value1));
      const double value2 = *reinterpret_cast<const double*>(*data_pointer2);
      *data_pointer2 += sizeof(decltype(value2));
      return compareDouble(value1, value2);
   }
   if (metadata.type == ColumnType::STRING) {
      const String<STRING_SIZE> value1 =
         *reinterpret_cast<const String<STRING_SIZE>*>(*data_pointer1);
      *data_pointer1 += sizeof(decltype(value1));
      const String<STRING_SIZE> value2 =
         *reinterpret_cast<const String<STRING_SIZE>*>(*data_pointer2);
      *data_pointer2 += sizeof(decltype(value2));

      auto fast_compare = value1.fastCompare(value2);
      if (fast_compare) {
         return fast_compare.value();
      }
      const std::string string_value1 =
         columns.string_columns.at(metadata.name).lookupValue(value1);
      const std::string string_value2 =
         columns.string_columns.at(metadata.name).lookupValue(value2);
      return compareString(string_value1, string_value2);
   }
   if (metadata.type == ColumnType::INDEXED_PANGOLINEAGE) {
      const silo::Idx value1 = *reinterpret_cast<const silo::Idx*>(*data_pointer1);
      *data_pointer1 += sizeof(decltype(value1));
      const std::string string_value1 =
         columns.pango_lineage_columns.at(metadata.name).lookupAliasedValue(value1).value;
      const silo::Idx value2 = *reinterpret_cast<const silo::Idx*>(*data_pointer2);
      *data_pointer2 += sizeof(decltype(value2));
      const std::string string_value2 =
         columns.pango_lineage_columns.at(metadata.name).lookupAliasedValue(value2).value;
      return compareString(string_value1, string_value2);
   }
   if (metadata.type == ColumnType::INDEXED_STRING) {
      const silo::Idx value1 = *reinterpret_cast<const silo::Idx*>(*data_pointer1);
      *data_pointer1 += sizeof(decltype(value1));
      const std::string string_value1 =
         columns.indexed_string_columns.at(metadata.name).lookupValue(value1);
      const silo::Idx value2 = *reinterpret_cast<const silo::Idx*>(*data_pointer2);
      *data_pointer2 += sizeof(decltype(value2));
      const std::string string_value2 =
         columns.indexed_string_columns.at(metadata.name).lookupValue(value2);
      return compareString(string_value1, string_value2);
   }
   if (metadata.type == ColumnType::NUC_INSERTION) {
      const silo::Idx value1 = *reinterpret_cast<const silo::Idx*>(*data_pointer1);
      *data_pointer1 += sizeof(decltype(value1));
      const std::string string_value1 =
         columns.nuc_insertion_columns.at(metadata.name).lookupValue(value1);
      const silo::Idx value2 = *reinterpret_cast<const silo::Idx*>(*data_pointer2);
      *data_pointer2 += sizeof(decltype(value2));
      const std::string string_value2 =
         columns.nuc_insertion_columns.at(metadata.name).lookupValue(value2);
      return compareString(string_value1, string_value2);
   }
   if (metadata.type == ColumnType::AA_INSERTION) {
      const silo::Idx value1 = *reinterpret_cast<const silo::Idx*>(*data_pointer1);
      *data_pointer1 += sizeof(decltype(value1));
      const std::string string_value1 =
         columns.aa_insertion_columns.at(metadata.name).lookupValue(value1);
      const silo::Idx value2 = *reinterpret_cast<const silo::Idx*>(*data_pointer2);
      *data_pointer2 += sizeof(decltype(value2));
      const std::string string_value2 =
         columns.aa_insertion_columns.at(metadata.name).lookupValue(value2);
      return compareString(string_value1, string_value2);
   }
   throw std::runtime_error("Unchecked column type of column " + metadata.name);
}

size_t getColumnSize(const silo::storage::ColumnMetadata& metadata) {
   if (metadata.type == silo::config::ColumnType::STRING) {
      return sizeof(silo::common::String<silo::common::STRING_SIZE>);
   }
   if (metadata.type == silo::config::ColumnType::FLOAT) {
      return sizeof(double);
   }
   if (metadata.type == silo::config::ColumnType::BOOL) {
      return sizeof(OptionalBool);
   }
   if (metadata.type == silo::config::ColumnType::INT) {
      return sizeof(int32_t);
   }
   if (metadata.type == silo::config::ColumnType::DATE) {
      return sizeof(silo::common::Date);
   }
   return sizeof(silo::Idx);
}

}  // namespace

size_t silo::query_engine::actions::getTupleSize(
   const std::vector<silo::storage::ColumnMetadata>& metadata_list
) {
   size_t size = 0;
   for (const auto& metadata : metadata_list) {
      size += getColumnSize(metadata);
   }
   return size;
}

Tuple::Tuple(const silo::storage::ColumnPartitionGroup* columns, std::byte* data, size_t data_size)
    : columns(columns),
      data(data),
      data_size(data_size) {}

Tuple::Tuple(Tuple&& other) noexcept
    : columns(other.columns),
      data(std::exchange(other.data, nullptr)),
      data_size(other.data_size) {}

Tuple& Tuple::operator=(const Tuple& other) {
   if (this == &other) {
      return *this;
   }
   assert(this->data_size == other.data_size);
   columns = other.columns;
   std::memcpy(this->data, other.data, data_size);
   return *this;
}

Tuple& Tuple::operator=(Tuple&& other) noexcept {
   this->columns = other.columns;
   this->data_size = other.data_size;
   std::swap(this->data, other.data);
   return *this;
}

std::map<std::string, silo::common::JsonValueType> Tuple::getFields() const {
   std::map<std::string, common::JsonValueType> fields;
   const std::byte* data_pointer = data;
   for (const auto& metadata : columns->metadata) {
      fields[metadata.name] = tupleFieldToValueType(&data_pointer, metadata, *columns);
   }
   return fields;
}

std::vector<Tuple::ComparatorField> Tuple::getCompareFields(
   const std::vector<silo::storage::ColumnMetadata>& columns_metadata,
   const std::vector<OrderByField>& order_by_fields
) {
   std::vector<ComparatorField> tuple_field_comparators;
   tuple_field_comparators.resize(order_by_fields.size());
   size_t offset = 0;
   for (const auto& metadata : columns_metadata) {
      auto element = std::find_if(
         order_by_fields.begin(),
         order_by_fields.end(),
         [&](const auto& order_by_field) { return metadata.name == order_by_field.name; }
      );
      if (element != order_by_fields.end()) {
         const size_t index = std::distance(order_by_fields.begin(), element);
         tuple_field_comparators[index] =
            ComparatorField{.offset = offset, .type = metadata, .ascending = element->ascending};
      }
      offset += getColumnSize(metadata);
   }
   return tuple_field_comparators;
}

Tuple::Comparator Tuple::getComparator(
   const std::vector<silo::storage::ColumnMetadata>& columns_metadata,
   const std::vector<OrderByField>& order_by_fields,
   const std::optional<uint32_t>& randomize_seed
) {
   auto tuple_field_comparators =
      actions::Tuple::getCompareFields(columns_metadata, order_by_fields);
   if (randomize_seed) {
      const size_t seed = *randomize_seed;
      return [tuple_field_comparators, seed](const Tuple& tuple1, const Tuple& tuple2) {
         if (tuple1.compareLess(tuple2, tuple_field_comparators)) {
            return true;
         }
         if (tuple2.compareLess(tuple1, tuple_field_comparators)) {
            return false;
         }
         size_t random_number1 = seed;
         size_t random_number2 = seed;
         boost::hash_combine(
            random_number1, std::hash<silo::query_engine::actions::Tuple>()(tuple1)
         );
         boost::hash_combine(
            random_number2, std::hash<silo::query_engine::actions::Tuple>()(tuple2)
         );
         return random_number1 < random_number2;
      };
   }
   return [tuple_field_comparators](const Tuple& tuple1, const Tuple& tuple2) {
      return tuple1.compareLess(tuple2, tuple_field_comparators);
   };
}

/// Compare tuples according to user-provided fields
bool Tuple::compareLess(const Tuple& other, const std::vector<ComparatorField>& fields) const {
   for (const auto& field : fields) {
      const std::byte* data_pointer1 = (this->data + field.offset);
      const std::byte* data_pointer2 = (other.data + field.offset);
      const std::strong_ordering compare =
         compareTupleFields(&data_pointer1, &data_pointer2, field.type, *columns);
      if (compare == std::strong_ordering::less) {
         return field.ascending;
      }
      if (compare == std::strong_ordering::greater) {
         return !field.ascending;
      }
   }
   return false;
}

bool Tuple::operator==(const Tuple& other) const {
   return std::memcmp(data, other.data, data_size) == 0;
}
bool Tuple::operator!=(const Tuple& other) const {
   return !(*this == other);
}

/// Compare according to native column order.
bool Tuple::operator<(const Tuple& other) const {
   const std::byte* data_pointer1 = data;
   const std::byte* data_pointer2 = other.data;
   for (const auto& metadata : columns->metadata) {
      const std::strong_ordering compare =
         compareTupleFields(&data_pointer1, &data_pointer2, metadata, *columns);
      if (compare != std::strong_ordering::equal) {
         return compare == std::strong_ordering::less;
      }
   }
   return false;
}

bool Tuple::operator>(const Tuple& other) const {
   return (other < *this);
}
bool Tuple::operator<=(const Tuple& other) const {
   return !(other < *this);
}
bool Tuple::operator>=(const Tuple& other) const {
   return !(*this < other);
}

std::size_t std::hash<Tuple>::operator()(const silo::query_engine::actions::Tuple& tuple) const {
   const std::string_view str_view(reinterpret_cast<char*>(tuple.data), tuple.data_size);
   return std::hash<std::string_view>{}(str_view);
}

TupleFactory::TupleFactory(
   const silo::storage::ColumnPartitionGroup& all_columns,
   const std::vector<silo::storage::ColumnMetadata>& fields
) {
   columns = all_columns.getSubgroup(fields);
   tuple_size = getTupleSize(columns.metadata);
}

Tuple& TupleFactory::overwrite(Tuple& tuple, uint32_t sequence_id) {
   std::byte* data_pointer = tuple.data;
   for (const auto& metadata : columns.metadata) {
      assignTupleField(&data_pointer, sequence_id, metadata, columns);
   }
   return tuple;
}

Tuple TupleFactory::allocateOne(uint32_t sequence_id) {
   all_tuple_data.emplace_back(tuple_size);
   auto& data = all_tuple_data.back();
   std::byte* data_pointer = data.data();
   for (const auto& metadata : columns.metadata) {
      assignTupleField(&data_pointer, sequence_id, metadata, columns);
   }
   return {&columns, data.data(), data.size()};
}

Tuple TupleFactory::copyTuple(const Tuple& tuple) {
   all_tuple_data.emplace_back(tuple_size);
   auto& data = all_tuple_data.back();
   std::memcpy(data.data(), tuple.data, tuple_size);
   return {tuple.columns, data.data(), data.size()};
}

std::vector<Tuple> TupleFactory::allocateMany(size_t count) {
   std::vector<Tuple> tuples;
   tuples.reserve(count);
   const size_t allocation_size = tuple_size * count;
   std::vector<std::byte>& data = all_tuple_data.emplace_back(allocation_size);
   for (unsigned i = 0; i < count; i++) {
      tuples.emplace_back(&columns, data.data() + i * tuple_size, tuple_size);
   }
   return tuples;
}
