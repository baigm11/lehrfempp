set(sources 
  base.h
  dereference_lambda_random_access_iterator.h
  forward_iterator.h
  forward_range.h
  invalid_type_exception.h
  lf_assert.cc
  lf_assert.h
  lf_exception.h
  predicate_true.h
  random_access_iterator.h
  random_access_range.h
  ref_el.cc
  ref_el.h
  static_vars.h
  static_vars.cc
)

lf_add_library(lf.base ${sources})
target_link_libraries(lf.base PUBLIC Eigen3::Eigen Boost::boost)

if(MSVC)
  if(${MSVC_VERSION} GREATER_EQUAL 1915) 
    # You must acknowledge that you understand MSVC resolved a byte alignment issue in this compiler
    # We get this due to using Eigen objects and allocating those objects with make_shared
    target_compile_definitions( lf.base PUBLIC _ENABLE_EXTENDED_ALIGNED_STORAGE )
  endif()
endif()

if(LF_ENABLE_TESTING)
  add_subdirectory(test)
endif()
