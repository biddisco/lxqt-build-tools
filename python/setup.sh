spack env activate opal

# create venv using sme python as in spack env
uv venv --python $(which python) --prompt grox .venv

# load venv
source .venv/bin/activate

# python utils such as linting
uv pip install ruff black

# install main packages
uv pip install ipykernel jupyter pandas seaborn matplotlib argparse 
uv pip install pip-chill
uv pip install PySide6==$QT_VER

# used in analysis of transactions
uv pip install yfinance yfinance-cache xrpl-py

# Machine learning
uv pip install torch tensorflow

# backtesting and optimization
uv pip install optuna pyzmq h5py

# my dev stuff
uv pip install -e ~/src/plotutils/
uv pip install -e ~/src/xrputils/

# generate requirements after installing things
# uv pip install pkg_resources chill pip-chill
# uv run pip-chill --no-version > requirements.txt

