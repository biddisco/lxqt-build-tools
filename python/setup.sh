spack env activate opal

# create venv using sme python as in spack env
uv venv --python $(which python) --prompt grox .venv

# load venv
source .venv/bin/activate

# install main packages
uv pip install ipykernel jupyter pandas seaborn matplotlib argparse 
uv pip install pyqt6 pip-chill

# used in analysis of transactions
uv pip install yfinance yfinance-cache xrpl-py

# my dev stuff
uv pip install -e ~/src/plotutils/
uv pip install -e ~/src/xrputils/

# generate requirements after installing things
# uv pip install pkg_resources chill pip-chill
# uv run pip-chill --no-version > requirements.txt

