#ifndef SILO_INT_COLUMN_H
#define SILO_INT_COLUMN_H

#include <deque>
#include <string>
#include <vector>

namespace boost::serialization {
struct access;
}

namespace silo {
struct Database;
}

namespace silo::storage::column {

class IntColumnPartition {
   friend class boost::serialization::access;

  private:
   template <class Archive>
   [[maybe_unused]] void serialize(Archive& archive, const unsigned int /* version */) {
      // clang-format off
      archive& values;
      // clang-format on
   }

   std::vector<int64_t> values;

  public:
   IntColumnPartition();

   [[nodiscard]] const std::vector<int64_t>& getValues() const;

   void insert(int64_t value);
};

class IntColumn {
   friend class boost::serialization::access;

  private:
   template <class Archive>
   [[maybe_unused]] void serialize(Archive& archive, const unsigned int /* version */) {
      // clang-format off
      // clang-format on
   }

   std::deque<IntColumnPartition> partitions;

  public:
   IntColumn();

   IntColumnPartition& createPartition();
};

}  // namespace silo::storage::column

#endif  // SILO_INT_COLUMN_H
