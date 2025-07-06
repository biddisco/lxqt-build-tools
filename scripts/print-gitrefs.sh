find . -maxdepth 2 -type d -name .git -exec bash -c 'DIR=$1; SHA=$(git --git-dir $DIR log --format=format:"%H" HEAD -1); printf "%-40s %s\n" "- $(basename $(dirname $DIR))" "@git.$SHA=main "' _ {} \; | sort

