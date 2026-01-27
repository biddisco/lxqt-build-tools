#!/usr/bin/env bash

# iterate over all files in the repo and reformat those that must be checked

CLANG_FORMAT_VERSION=clang-format-18

for file in $(git ls-files | grep -E "\.(cpp|hpp|h|cu)(\.in)?$"); do
    # to allow for per-directory clang format files, we cd into the dir first
    DIR=$(dirname "$file")
    pushd ${DIR} >/dev/null
    ${CLANG_FORMAT_VERSION} -i $(basename -- ${file})
    popd >/dev/null
done
