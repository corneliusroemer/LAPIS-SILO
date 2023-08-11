#ifndef SILO_INCLUDE_SILO_COMMON_DATAVERSION_H_
#define SILO_INCLUDE_SILO_COMMON_DATAVERSION_H_

#include <optional>
#include <string>

namespace silo {
class Database;

class DataVersion {
   friend class Database;

  public:
   [[nodiscard]] std::string toString() const;

   bool operator==(const DataVersion& other) const;
   bool operator!=(const DataVersion& other) const;
   bool operator<(const DataVersion& other) const;
   bool operator>(const DataVersion& other) const;
   bool operator<=(const DataVersion& other) const;
   bool operator>=(const DataVersion& other) const;

   static DataVersion mineDataVersion();

   static std::optional<DataVersion> fromString(const std::string& string);

  private:
   explicit DataVersion(std::string data_version);
   std::string data_version;
};

}  // namespace silo

#endif  // SILO_INCLUDE_SILO_COMMON_DATAVERSION_H_
