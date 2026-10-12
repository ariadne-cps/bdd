# Try to find the standalone Ariadne BDD library.
#
# Once done this will define:
#
#  AriadneBdd_FOUND - system has Ariadne BDD
#  ARIADNE_BDD_INCLUDE_DIRS - the Ariadne BDD include directories
#  ARIADNE_BDD_LIBRARIES - libraries required to use Ariadne BDD
#
# and, when found:
#
#  AriadneBdd::ariadne-bdd - imported library target
#
# Copyright  2026  Luca Geretti

find_library(ARIADNE_BDD_LIBRARY NAMES ariadne-bdd)
find_path(ARIADNE_BDD_INCLUDE_DIR ariadne-bdd.hpp PATH_SUFFIXES ariadne-bdd)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(AriadneBdd DEFAULT_MSG
  ARIADNE_BDD_LIBRARY
  ARIADNE_BDD_INCLUDE_DIR
)

set(ARIADNE_BDD_FOUND ${AriadneBdd_FOUND})

if(AriadneBdd_FOUND)
  set(ARIADNE_BDD_INCLUDE_DIRS ${ARIADNE_BDD_INCLUDE_DIR})
  set(ARIADNE_BDD_LIBRARIES ${ARIADNE_BDD_LIBRARY})

  if(NOT TARGET AriadneBdd::ariadne-bdd)
    add_library(AriadneBdd::ariadne-bdd UNKNOWN IMPORTED)
    set_target_properties(AriadneBdd::ariadne-bdd PROPERTIES
      IMPORTED_LOCATION "${ARIADNE_BDD_LIBRARY}"
      INTERFACE_INCLUDE_DIRECTORIES "${ARIADNE_BDD_INCLUDE_DIR}"
    )
  endif()
endif()

mark_as_advanced(
  ARIADNE_BDD_INCLUDE_DIR
  ARIADNE_BDD_LIBRARY
)
