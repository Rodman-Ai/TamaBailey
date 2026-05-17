#include "tama/friends.h"

namespace tama {

const char* friend_name(Friend f) {
  switch (f) {
    case Friend::Ollie:    return "Ollie";
    case Friend::Mitchell: return "Mitchell";
    case Friend::Enzo:     return "Enzo";
    case Friend::Lincoln:  return "Lincoln";
    case Friend::Ruben:    return "Ruben";
    case Friend::Francie:  return "Francie";
    case Friend::Bomi:     return "Bomi";
    case Friend::Noshy:    return "Noshy";
    default:               return "?";
  }
}

const char* friend_breed(Friend f) {
  switch (f) {
    case Friend::Ollie:    return "brindle mix";
    case Friend::Mitchell: return "Havanese";
    case Friend::Enzo:     return "rottweiler";
    case Friend::Lincoln:  return "golden retriever";
    case Friend::Ruben:    return "Portuguese water dog";
    case Friend::Francie:  return "Boston Terrier";
    case Friend::Bomi:     return "Korean Jindo";
    case Friend::Noshy:    return "Black Cockapoo";
    default:               return "?";
  }
}

float friend_size_scale(Friend f) {
  switch (f) {
    case Friend::Ollie:    return 1.00f;
    case Friend::Mitchell: return 0.56f;   // was 0.70, user asked -20%
    case Friend::Enzo:     return 1.00f;
    case Friend::Lincoln:  return 1.20f;
    case Friend::Ruben:    return 1.10f;
    case Friend::Francie:  return 0.50f;
    case Friend::Bomi:     return 0.85f;   // was 1.00, Jindo smaller than Bailey
    case Friend::Noshy:    return 0.80f;   // a touch smaller than Bomi
    default:               return 1.0f;
  }
}

}  // namespace tama
