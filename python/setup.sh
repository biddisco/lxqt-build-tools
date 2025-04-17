# create venv
uv venv --prompt grox .venv

# load venv
source .venv/bin/activate

# install requirements
uv pip install -r requirements.txt

# my dev stuff
uv pip install -e ~/src/plotutils

# generate requirements after installing things
pip-chill --no-version > requirements.txt

