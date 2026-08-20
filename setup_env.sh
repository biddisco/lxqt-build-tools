# ----------------------------------------
# In build dir, symlink this file and load it in .envrc for direnv
# ln -s ~/src/grox/environment_setup.sh ./.spack_env.sh
# ----------------------------------------

# ----------------------------------------
# Setup sanitizer options if needed for debugging
# ----------------------------------------
export ASAN_OPTIONS=fast_unwind_on_malloc=0:strict_string_checks=1:detect_leaks=1:detect_stack_use_after_return=0:check_initialization_order=1:strict_init_order=1
export ASAN_OPTIONS=fast_unwind_on_malloc=0:detect_leaks=1:detect_stack_use_after_return=0:check_initialization_order=1:strict_init_order=1
#unset ASAN_OPTIONS
export LSAN_OPTIONS=suppressions=/home/biddisco/src/grox/scripts/asan.suppressions

# ----------------------------------------
# load the spack env we need for our build
# ----------------------------------------
spack env activate home

# ----------------------------------------
# Load python venv
# Let direnv manage the python venv instead of sourcing activate.
# This avoids the "PS1 cannot be exported" warning.

# NOTE: to see the python venv prompt, add this to your .bashrc/.zshrc:
#   PS1='${VIRTUAL_ENV_PROMPT:+$VIRTUAL_ENV_PROMPT}'$PS1
# (direnv cannot export PS1, so the prompt prefix must come from the shell.)
# ----------------------------------------
export VIRTUAL_ENV=$HOME/.venv-home
export VIRTUAL_ENV_PROMPT="(py-home) "
PATH_add $VIRTUAL_ENV/bin
