
#ifndef SILO_SEQUENCE_STORE_H
#define SILO_SEQUENCE_STORE_H

#include <array>
#include <optional>

#include <silo/common/fasta_reader.h>
#include <spdlog/spdlog.h>
#include <boost/serialization/array.hpp>
#include <roaring/roaring.hh>

#include "silo/common/fasta_reader.h"
#include "silo/common/nucleotide_symbols.h"
#include "silo/roaring/roaring_serialize.h"
#include "silo/storage/serialize_optional.h"

namespace silo {

struct Position {
   friend class boost::serialization::access;

   template <class Archive>
   void serialize(Archive& archive, [[maybe_unused]] const unsigned int version) {
      // clang-format off
      archive& symbol_whose_bitmap_is_flipped;
      archive& bitmaps;
      archive& nucleotide_symbol_n_indexed;
      // clang-format on
   }

   std::array<roaring::Roaring, SYMBOL_COUNT> bitmaps;
   std::optional<NUCLEOTIDE_SYMBOL> symbol_whose_bitmap_is_flipped = std::nullopt;
   bool nucleotide_symbol_n_indexed = false;
};

struct SequenceStoreInfo {
   uint32_t sequence_count;
   uint64_t size;
   size_t n_bitmaps_size;
};

class SequenceStore {
  private:
   unsigned sequence_count{};

  public:
   friend class boost::serialization::access;

   template <class Archive>
   void serialize(Archive& archive, [[maybe_unused]] const unsigned int version) {
      // clang-format off
      archive& sequence_count;
      archive& positions;
      archive& nucleotide_symbol_n_bitmaps;
      // clang-format on
   }

   std::array<Position, GENOME_LENGTH> positions;
   std::vector<roaring::Roaring> nucleotide_symbol_n_bitmaps;

   SequenceStore();

   [[nodiscard]] size_t computeSize() const;

   [[nodiscard]] const roaring::Roaring* getBitmap(size_t position, NUCLEOTIDE_SYMBOL symbol) const;

   [[nodiscard]] roaring::Roaring* getBitmapFromAmbiguousSymbol(
      size_t position,
      NUCLEOTIDE_SYMBOL ambiguous_symbol
   ) const;

   [[nodiscard]] roaring::Roaring* getFlippedBitmapFromAmbiguousSymbol(
      size_t position,
      NUCLEOTIDE_SYMBOL ambiguous_symbol
   ) const;

   void interpret(const std::vector<std::string>& genomes);

   void indexAllNucleotideSymbolsN();

   void naiveIndexAllNucleotideSymbolN();

   SequenceStoreInfo getInfo() const;

   unsigned fill(silo::FastaReader& input_file);
};

[[maybe_unused]] unsigned runOptimize(SequenceStore& sequence_store);

[[maybe_unused]] unsigned shrinkToFit(SequenceStore& sequence_store);

}  // namespace silo

template <>
struct [[maybe_unused]] fmt::formatter<silo::SequenceStoreInfo> : fmt::formatter<std::string> {
   [[maybe_unused]] static auto format(
      silo::SequenceStoreInfo sequence_store_info,
      format_context& ctx
   ) -> decltype(ctx.out());
};

#endif  // SILO_SEQUENCE_STORE_H
