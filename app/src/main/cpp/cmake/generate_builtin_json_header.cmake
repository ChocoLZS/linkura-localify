cmake_minimum_required(VERSION 3.22)

# Build-time generator:
# Reads all built-in mock text files in INPUT_DIR and emits a C++ header at OUTPUT_HEADER.
# Supported extensions:
# - *.json     -> <Name>JsonView / <Name>Json
# - *.headers  -> <Name>HeadersView / <Name>Headers
# - *.hdr      -> <Name>HeadersView / <Name>Headers
#
# Naming rules:
#   user_login.json -> UserLoginJsonView / UserLoginJson
#   v1_user_login.json -> V1UserLoginJsonView / V1UserLoginJson
#
# Notes:
# - The std::string_view variant has no dynamic initialization and is preferred.
# - The std::string variant exists only for convenience when callers want std::string.

if(NOT DEFINED INPUT_DIR OR INPUT_DIR STREQUAL "")
  message(FATAL_ERROR "INPUT_DIR is required")
endif()
if(NOT DEFINED OUTPUT_HEADER OR OUTPUT_HEADER STREQUAL "")
  message(FATAL_ERROR "OUTPUT_HEADER is required")
endif()

file(GLOB BUILTIN_FILES "${INPUT_DIR}/*.*")
list(FILTER BUILTIN_FILES INCLUDE REGEX ".*\\.(json|headers|hdr)$")
list(SORT BUILTIN_FILES)

function(_to_pascal_case input_str output_var)
  # Split by non-alnum into tokens.
  string(REGEX REPLACE "[^A-Za-z0-9]+" ";" _tokens "${input_str}")
  set(_out "")
  foreach(_t IN LISTS _tokens)
    if(_t STREQUAL "")
      continue()
    endif()
    string(SUBSTRING "${_t}" 0 1 _first)
    string(LENGTH "${_t}" _len)
    if(_len GREATER 1)
      math(EXPR _rest_len "${_len} - 1")
      string(SUBSTRING "${_t}" 1 ${_rest_len} _rest)
    else()
      set(_rest "")
    endif()
    string(TOUPPER "${_first}" _first_u)
    # Most filenames are lowercase snake_case; normalize rest to lowercase for stable identifiers.
    string(TOLOWER "${_rest}" _rest_l)
    set(_out "${_out}${_first_u}${_rest_l}")
  endforeach()
  if(_out STREQUAL "")
    set(_out "Json")
  endif()
  # If starts with digit, prefix underscore.
  string(SUBSTRING "${_out}" 0 1 _c0)
  if(_c0 MATCHES "^[0-9]$")
    set(_out "_${_out}")
  endif()
  set(${output_var} "${_out}" PARENT_SCOPE)
endfunction()

get_filename_component(_out_dir "${OUTPUT_HEADER}" DIRECTORY)
file(MAKE_DIRECTORY "${_out_dir}")

set(_header "")
string(APPEND _header "#pragma once\n")
string(APPEND _header "#include <string>\n")
string(APPEND _header "#include <string_view>\n\n")
string(APPEND _header "namespace LinkuraLocal::OfflineApiMockBuiltIn {\n")

# Raw string delimiter must be <= 16 chars in C++.
set(_delim "LL_JSON")

foreach(_f IN LISTS BUILTIN_FILES)
  get_filename_component(_stem "${_f}" NAME_WE)
  get_filename_component(_ext "${_f}" EXT)
  _to_pascal_case("${_stem}" _name_base)

  if(_ext STREQUAL ".json")
    set(_name "${_name_base}Json")
  elseif(_ext STREQUAL ".headers" OR _ext STREQUAL ".hdr")
    set(_name "${_name_base}Headers")
  else()
    # Shouldn't happen due to filter, but keep safe.
    continue()
  endif()

  file(READ "${_f}" _content)
  # Normalize line endings just in case.
  string(REPLACE "\r\n" "\n" _content "${_content}")
  string(REPLACE "\r" "\n" _content "${_content}")

  string(APPEND _header "\n")
  string(APPEND _header "inline constexpr std::string_view ${_name}View = R\"${_delim}(\n")
  string(APPEND _header "${_content}\n")
  string(APPEND _header ")${_delim}\";\n")
  string(APPEND _header "inline const std::string ${_name} = std::string(${_name}View);\n")
endforeach()

string(APPEND _header "\n} // namespace LinkuraLocal::OfflineApiMockBuiltIn\n")

file(WRITE "${OUTPUT_HEADER}" "${_header}")

message(STATUS "Generated builtin mock header: ${OUTPUT_HEADER} (${BUILTIN_FILES})")
