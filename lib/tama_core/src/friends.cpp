#include "tama/friends.h"

namespace tama {

const char* friend_name(Friend f) {
  switch (f) {
    case Friend::Ollie:    return "Ollie";
    case Friend::Mitchell: return "Mitchell";
    case Friend::Enzo:     return "Enzo";
    case Friend::Lincoln:  return "Lincoln";
    default:               return "?";
  }
}

const char* friend_breed(Friend f) {
  switch (f) {
    case Friend::Ollie:    return "brindle mix";
    case Friend::Mitchell: return "Havanese";
    case Friend::Enzo:     return "rottweiler";
    case Friend::Lincoln:  return "golden retriever";
    default:               return "?";
  }
}

float friend_size_scale(Friend f) {
  switch (f) {
    case Friend::Ollie:    return 1.00f;
    case Friend::Mitchell: return 0.70f;
    case Friend::Enzo:     return 1.00f;
    case Friend::Lincoln:  return 1.20f;
    default:               return 1.0f;
  }
}

}  // namespace tama
