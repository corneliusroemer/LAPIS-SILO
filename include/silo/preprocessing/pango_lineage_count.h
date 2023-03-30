#ifndef SILO_PANGO_LINEAGE_COUNT_H
#define SILO_PANGO_LINEAGE_COUNT_H

#include <string>
#include <vector>

namespace silo {
struct PangoLineageAliasLookup;
}

namespace silo::preprocessing {

struct PangoLineageCount {
   std::string pango_lineage;
   uint32_t count;
};

struct PangoLineageCounts {
   std::vector<PangoLineageCount> pango_lineage_counts;

   void save(std::ostream& output_file);

   static PangoLineageCounts load(std::istream& input_stream);
};

PangoLineageCounts buildPangoLineageCounts(
   const silo::PangoLineageAliasLookup& alias_key,
   std::istream& meta_in
);

}  // namespace silo::preprocessing

#endif  // SILO_PANGO_LINEAGE_COUNT_H
