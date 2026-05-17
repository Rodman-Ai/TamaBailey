#pragma once

#include <cstdint>

namespace tama {

// Bailey's four friends. Order is stable for save-format compatibility:
// add new friends at the end only, never reorder.
enum class Friend : uint8_t {
  Ollie    = 0,   // brindle mix, same size as Bailey
  Mitchell = 1,   // Havanese, smaller than Bailey
  Enzo     = 2,   // rottweiler, same size as Bailey
  Lincoln  = 3,   // golden retriever, bigger than Bailey
  Ruben    = 4,   // Portuguese water dog, slightly bigger than Bailey
  COUNT    = 5,
};

const char* friend_name(Friend f);
const char* friend_breed(Friend f);
float       friend_size_scale(Friend f);

}  // namespace tama
