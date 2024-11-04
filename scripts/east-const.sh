#!/bin/bash

# Convert const West to East const format
# 
# sed -i -E 's/const ([a-zA-Z0-9_<>,:]+)&/\1 const\&/g'
# 
# NB. the character set includes alphanumeric as well as "_<>,:"
# to allow const std::vector<std::vector, allocator> expressions
# to be correctly detected. 
# Others might be needed for more complex parameter lists.
#
# Corner case: convert this
# pika::start(int argc, const char *const *argv, init_params const &params)
# to this
# pika::start(int argc, char const* const* argv, init_params const &params)


ALL_FILES=$(git ls-files "*.cpp" "*.cxx" "*.c" "*.h" "*.hpp" "*.h.in" "*.tpp" "*.cu" "*.rst") 
#ALL_FILES=$(git ls-files "*.rst") 

for file in ${ALL_FILES} ; do
  # symlinks have a filemode 120000 in git and should be ignored
  test=$( git ls-files -s $file | grep 120000 -o )
  if [[ "$test" ]]; then
    printf "ignore symlink: $file\n"
  else 
	  sed -i -E 's/const ([a-zA-Z0-9_<>,:]+)(&| |\*)/\1 const&/g' $file
  fi
done

