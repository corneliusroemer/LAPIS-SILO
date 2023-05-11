#include "silo/query_engine/operators/bitmap_selection.h"

#include "roaring/roaring.hh"
#include "silo/query_engine/operators/operator.h"

namespace silo::query_engine::operators {

BitmapSelection::BitmapSelection(
   const roaring::Roaring* bitmaps,
   Predicate comparator,
   uint32_t value,
   unsigned sequence_count
)
    : bitmaps(bitmaps),
      comparator(comparator),
      value(value),
      sequence_count(sequence_count) {}

BitmapSelection::~BitmapSelection() noexcept = default;

std::string BitmapSelection::toString(const Database& /*database*/) const {
   return "BitmapSelection";
}

Type BitmapSelection::type() const {
   return BITMAP_SELECTION;
}

void BitmapSelection::negate() {
   switch (this->comparator) {
      case CONTAINS:
         this->comparator = NOT_CONTAINS;
         break;
      case NOT_CONTAINS:
         this->comparator = CONTAINS;
         break;
   }
}

OperatorResult BitmapSelection::evaluate() const {
   OperatorResult res = {new roaring::Roaring(), nullptr};
   switch (this->comparator) {
      case CONTAINS:
         for (unsigned i = 0; i < sequence_count; i++) {
            if (bitmaps[i].contains(value)) {
               res.mutable_res->add(i);
            }
         }
         break;
      case NOT_CONTAINS:
         for (unsigned i = 0; i < sequence_count; i++) {
            if (!bitmaps[i].contains(value)) {
               res.mutable_res->add(i);
            }
         }
         break;
   }
   return res;
}

}  // namespace silo::query_engine::operators
