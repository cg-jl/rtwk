#pragma once

#include <span>
#include <type_traits>

#include "collection.h"

template <typename Derived, typename Base>
concept is_derived_of = std::is_base_of_v<Base, Derived>;
