#include "Hasher.hh"

#include <xxhash.h>

namespace lr {
HasherXXH64::HasherXXH64() {
    ZoneScoped;

    this->handle = XXH3_createState();
    reset();
}

HasherXXH64::~HasherXXH64() {
    ZoneScoped;

    XXH3_freeState(ls::bit_cast<XXH3_state_t *>(this->handle));
}

bool HasherXXH64::hash(const void *data, usize data_size) {
    ZoneScoped;

    return XXH3_64bits_update(ls::bit_cast<XXH3_state_t *>(this->handle), data, data_size) == XXH_OK;
}

u64 HasherXXH64::value() {
    ZoneScoped;

    return XXH3_64bits_digest(ls::bit_cast<XXH3_state_t *>(this->handle));
}

void HasherXXH64::reset() {
    ZoneScoped;

    XXH3_64bits_reset(ls::bit_cast<XXH3_state_t *>(this->handle));
}

}  // namespace lr
