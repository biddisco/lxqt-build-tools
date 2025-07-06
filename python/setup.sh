# create venv
uv venv --prompt grox .venv

# load venv
source .venv/bin/activate

# install main packages
uv pip install ipykernel pandas seaborn matplotlib argparse 
uv pip install pyqt6 pip-chill

# used in analysis of transactions
uv pip install yfinance yfinance-cache xrpl-py

# my dev stuff
uv pip install -e ~/src/plotutils/
uv pip install -e ~/src/xrputils/

# install requirements
uv pip install -r requirements.txt

# generate requirements after installing things
pip-chill --no-version > requirements.txt

