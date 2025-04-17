#!/usr/bin/env python
# coding: utf-8

# In[21]:


# activate .venv in grox/python dir : source ~/src/grox/python/.venv/bin/activate
import pandas as pd
import os
import sys
import argparse
import json
import glob
import shutil
#
from IPython.display import display, HTML
#
os.environ["QT_QPA_PLATFORM_PLUGIN_PATH"] = os.environ["QT_PLUGIN_PATH"]
from PyQt6.QtCore import QStandardPaths
from PyQt6.QtCore import QSettings
from PyQt6.QtCore import QLibraryInfo
#
print("LD_LIBRARY_PATH    :", os.environ["LD_LIBRARY_PATH"])
print("Qt library path    :", QLibraryInfo.path(QLibraryInfo.LibraryPath.LibrariesPath))
print("Qt version         :", QLibraryInfo.version().toString())
#
import plotutils as pu

print(f"Python version     : {sys.version}")
print(f"Python binary path : {sys.executable}")
delete_airdrops = False
delete_files = False
decimals = 8
debug = True


# ---
# ## setup directories
# grox_data_dir : where grox stores data it uses for decisions
# grox_json_dir : where this script looks for new json files (normally when not debugging grox_json_dir==grox_data_dir)
# 
# ---

# In[22]:


# The directory where grox will store new json files downloaded from exchanges
grox_data_dir = os.path.join(QStandardPaths.standardLocations(QStandardPaths.StandardLocation.AppDataLocation)[0], 'grox/')
print(f"grox_data_dir: {grox_data_dir}")
# when debugging, set this to another location for json files
grox_json_dir = '/home/biddisco/src/grox/python/transactions/grox-data'
print(f"grox_json_dir: {grox_json_dir}")
# use the main grox ini file to store timestamps
grox_ini_file = os.path.join(QStandardPaths.standardLocations(QStandardPaths.StandardLocation.AppConfigLocation)[0], 'grox.ini')
print(f"grox_ini_file: {grox_ini_file}")


# ---
# ## setup dictionaries and categories
# grox_data_dir : setup vars we need for account/transaction types/file categorization
# 
# ---

# In[23]:


# these are officil transaction types as defined by the bitstamp API
# https://www.bitstamp.net/api/#tag/Transactions-private/operation/GetUserTransactions
transaction_types_dict = {
    0: 'deposit',
    1: 'withdrawal',
    2: 'market trade',
    14: 'sub account transfer',
    25: 'credited with staked assets',
    26: 'sent assets to staking',
    27: 'staking reward',
    32: 'referral reward',
    35: 'inter account transfer',
    33: 'settlement transfer',
    58: 'derivatives periodic settlement',
    59: 'insurance fund claim',
    60: 'insurance fund premium',
    61: 'collateral liquidation'
}

# account names that we want to handle
account_names = ['Main', 'Currency', 'Test']


# In[24]:


if 'ipykernel' in sys.modules:
    print(f"Running in a Jupyter environment: using default grox_data_dir {grox_data_dir}")
    # this makes the notebook wider on a larger screen using %x of the display
    display(HTML("<style>.container { width:100% !important; }</style>"))
    # save this notebook as a raw python file as well please
    notebook = globals().get('__notebook_name__', 'transactions-grox-json.ipynb')
    get_ipython().system(f'jupyter nbconvert --to script {notebook}')

else:
    parser = argparse.ArgumentParser(description='Collate json')
    parser.add_argument('--grox_data_dir', default=grox_data_dir, type=str, help='grox data dir')
    parser.add_argument('--grox_json_dir', default=grox_json_dir, type=str, help='grox json dir')
    parser.add_argument('--accounts', default=["Main"], type=str, nargs='+', help='Account names to use in file/json lookups')
    parser.add_argument('--delete_files', action='store_true', help='Delete JSON files after processing')
    args = parser.parse_args()
    grox_data_dir = args.grox_data_dir
    grox_json_dir = args.grox_json_dir
    account_names = args.accounts
    delete_files = args.delete_files
    print(f"Using account names: {account_names}")
    print(f"Using grox_data_dir: {grox_data_dir}")
    print(f"Using grox_json_dir: {grox_json_dir}")

pd.options.display.float_format = '{:.8f}'.format


# In[25]:


# we must handle "2021-02-23 08:59:14.652000" and "2021-02-23 08:59:14" and unix timestamps
def date_formatter(date):
    try:
        if isinstance(date, pd.Timestamp):
            return date.tz_convert('UTC').tz_localize(None) if date.tzinfo else date
        elif isinstance(date, int):
            return pd.to_datetime(date, unit='s')
        elif '.' in date:
            return pd.to_datetime(date, format='%Y-%m-%d %H:%M:%S.%f')
        elif '-' in date:
            return pd.to_datetime(date, format='%Y-%m-%d %H:%M:%S')
        else:
            raise ValueError("Unknown date format")
    except Exception as e:
        print(f"Error converting date: {date} (type: {type(date)}) - {e}")
        sys.exit(1)
        raise e

def convert_to_datetime(df, column_in, column_out):
    # Try converting assuming the column is a Unix timestamp
    df[column_out] = df[column_in].apply(date_formatter)
    df[column_out] = df[column_out].dt.floor('s')
    return df

def cleanup_dataframe(df, message, debug=False):
    print(f"{message} : Before Dataframe shape: {df.shape}, num columns {len(df.columns)}")
    # pu.title_print(f"Before cleaning \n{df.dtypes}", df)

    for col in df.columns:
        if col=='datetime':
            df = convert_to_datetime(df, 'datetime', 'datetime')
            # df.set_index('datetime', inplace=True)
        elif col in ['id', 'order_id', 'type']:
            df[col] = df[col].astype('Int64')
        elif col in ['amount', 'fee', 'btc', 'eur', 'usd', 'xrp', 'xrp_usd', 'xrp_eur', 'xrp_gbp', 'btc_usd', 'eth', 'sgb', 'flr']:
            df[col] = pd.to_numeric(df[col], errors='coerce').astype('Float64')
        elif col in ['currency', 'destinationAddress', 'network', 'txid', 'Type']:
            df[col] = df[col].astype('string')
        elif df[col].dtype == 'object':
            pu.debug_print(f"Converting column {col} to numeric", debug)
            try:
                df[col] = pd.to_numeric(df[col], errors='coerce').astype('Float64')
            except Exception as e:
                print(f"Error converting column {col}: {e}")

    # Round float columns
    # for col in df.select_dtypes(include=['float64']).columns:
    #     df[col] = df[col].round(decimals).to_numpy()
    # Remove airdrops
    if delete_airdrops:
        for token in ["sgb", "flr", "eth"]:
            if token in df.columns:
                df = df[df[token].isna()]
                df.drop(columns=[token], inplace=True)

    df.sort_values(by='datetime', inplace=True)
    df = df.drop_duplicates(ignore_index=True, keep='first')

    # Remove columns that are all zero and are of float type
    df = df.loc[:, ~((df == 0).all() & (df.dtypes == 'Float64'))]

    pu.title_print(f"After cleaning shape: {df.shape}, num columns {len(df.columns)} \n{df.dtypes}", df, debug)
    return df


# ---
# ## JSON: Check if there are new json files in the grox_json_dir
# If there are no new files, exit now
# 
# ---

# In[16]:


account_files = {}
json_file_count = 0
for account in account_names:
    account_files[account] = sorted(glob.glob(os.path.join(grox_json_dir, f'transactions-user-{account}*.json')))
    json_file_count += len(account_files[account])

if json_file_count == 0:
    print(f"No JSON files in {grox_json_dir}")
    # no need to process anything, just leave things as they are
    sys.exit(0)
else:
    print(f"Number of JSON files in {grox_json_dir}: {json_file_count}")
    for account, files in account_files.items():
        for file in files:
            print(f"  {os.path.basename(file)}")


# ---
# ## CSV: Load previously generated csv files from grox_data_dir into pandas dataframes
# These are the files we have processed on previous runs and represent the current known 'state'
# 
# ---

# In[17]:


# load previously generated csv files into pandas dataframes
account_data = {}
for account in account_names:
    account_data[account] = None
    csv_file = os.path.join(grox_data_dir, f'transactions-user-{account}.csv')
    if os.path.exists(csv_file):
        backup_file = csv_file + ".bak"
        shutil.copy(csv_file, backup_file)
        account_data[account] = cleanup_dataframe(pd.read_csv(csv_file, parse_dates=['datetime']), f"Loaded {csv_file}", debug=debug)
        print(f"Read {len(account_data[account])} rows from {csv_file}, backing-up -> {backup_file}")
        pu.title_print(f"Loaded {csv_file}", account_data[account], debug=debug)


# ---
# ## Load all json files in the grox_json_dir (or grox_data_dir in production)
# These are the files most recently generated when grox is run
# 
# The old transaction handling code used crypto, market and user APIs to get 3 datasets, just 'user' now handled
# 
# ---

# In[18]:


json_files_read = []
success = True
# ---------------------------------------------------------------------
# Load all new JSON files and merge them with the existing dataframes
# ---------------------------------------------------------------------
#
new_account_data = {}
try:
    for account in account_names:
        new_account_data[account] = None
        # Load all JSON files with the right suffix and add them to a list of datasets
        account_files = glob.glob(os.path.join(grox_json_dir, f'transactions-user-{account}*.json'))
        for file in account_files:
            with open(file, 'r') as f:
                json_data = json.load(f)
                if len(json_data) > 0:
                    df = pd.DataFrame(json_data)
                    if new_account_data[account] is not None:
                        new_account_data[account] = pd.concat([new_account_data[account], df], ignore_index=True).drop_duplicates(ignore_index=True, keep='first')
                    else:
                        new_account_data[account] = df
                    new_account_data[account] = new_account_data[account].sort_values(by='id', ascending=True).reset_index(drop=True)
                if debug: print(f"Processed file: {file}, total cumulative rows: {len(new_account_data[account])}")
                json_files_read.append(file)
except Exception as e:
    new_account_data = None
    print(f"Error processing file {file}: {e}")
    raise e

try:
    for account in account_names:
        # now merge the new dataframe into the existing dataframe
        if new_account_data[account] is not None:
            new_account_data[account] = cleanup_dataframe(new_account_data[account], f"New JSON data for {account}", debug=debug)
            if account_data[account] is not None:
                account_data[account] = pd.concat([account_data[account], new_account_data[account]], ignore_index=True).drop_duplicates(ignore_index=True, keep='first')
            else:
                account_data[account] = new_account_data[account]
            account_data[account] = account_data[account].sort_values(by='id', ascending=True).reset_index(drop=True)

except Exception as e:
    print(f"Error merging dataframes: {e}")
    raise e

success = True
if all(data is None for data in account_data.values()):
    print("No valid data found in account_data. Exiting.")
    success = False
    sys.exit(0)


# In[19]:


if success:
    # Open ini file using QSettings
    settings = QSettings(grox_ini_file, QSettings.Format.IniFormat)
    # Write the latest datetime to the ini file
    settings.beginGroup("TransactionLogs")
    settings.beginGroup("Bitstamp")
    for account in account_names:
        if account_data[account] is None:
            continue

        last_datetime = None
        last_orderId = None
        # Save the transactions to csv files
        csv_name = os.path.join(grox_data_dir, f'transactions-user-{account}.csv')
        print(f"Saving user {account} to CSV file {csv_name}")
        # if 'datetime' in account_data[account].columns:
        #     account_data[account]['datetime'] = account_data[account]['datetime'].dt.strftime('%Y-%m-%d %H:%M:%S.%f')
        account_data[account].to_csv(csv_name, index=False)
        #
        last_datetime = account_data[account]['datetime'].iloc[-1]

        # Get the last valid id if it exists
        if 'id' in account_data[account].columns:
            last_Id = account_data[account]['id'].iloc[-1]
            # Scan backwards to find a valid id if the last one is invalid
            if pd.isna(last_Id):
                for id in reversed(account_data[account]['id']):
                    if not pd.isna(id):
                        last_Id = id
                        break

        print(f"Last datetime for user {account}: {last_datetime} -> {grox_ini_file}")
        print(f"Last valid id for user {account}: {last_Id} -> {grox_ini_file}")
        # settings.setValue(f"Datetime_{suffix}_{key}", latest_datetime.strftime('%y-%m-%d %H:%M:%S'))
        settings.setValue(f"Datetime_user_{account}", last_datetime.strftime('%y-%m-%d %H:%M:%S'))
        if pd.notna(last_Id):
            settings.setValue(f"ID_user_{account}", int(last_Id))
    settings.endGroup()
    settings.endGroup()

    if delete_files:
        for file in json_files_read:
            try:
                os.remove(file)
                print(f"Deleted file: {file}")
            except Exception as e:
                print(f"Error deleting file {file}: {e}")
else:
    raise Exception("Error processing files")


# ---
# ## Find which transactions in the "official" bitstamp export are not present in our API exports 
# ---

# In[ ]:


bitstamp_csv_file = '/home/biddisco/src/grox/transactions/transactions-2025-03-09.csv'
print(f"Reading Bitstamp CSV files from {bitstamp_csv_file}")
bitstamp_df = pd.read_csv(bitstamp_csv_file, parse_dates=['Datetime'])
convert_to_datetime(bitstamp_df, 'Datetime', 'Datetime')
pu.title_print(f'Bitstamp DataFrame {len(bitstamp_df)}', bitstamp_df)
orig_size = len(bitstamp_df)

remove_ids = False
for account in account_names:
    if account_data[account] is not None:
        pu.title_print(f'Account {account} DataFrame {len(account_data[account])}', account_data[account])
        # if 'datetime' in df.columns:
        #     convert_to_datetime(df, 'datetime', 'datetime')
        if 'id' in account_data[account].columns:
            ids = account_data[account]['id'].dropna().unique()
            print(f"Found {len(ids)} unique IDs in user {account} dataframe")
            if remove_ids:
                # Find the ids in the current dataframe
                print(f"Found {len(ids)} unique IDs in user {account} dataframe")
                # Remove rows from bitstamp_df where id matches
                bitstamp_df = bitstamp_df[~bitstamp_df['ID'].isin(ids)]
            else:
                # Check if the column 'ID_Found' already exists
                if 'ID_Found' in bitstamp_df.columns:
                    # Update the 'ID_Found' column with the new values
                    bitstamp_df['ID_Found'] = bitstamp_df['ID_Found'] | bitstamp_df['ID'].isin(ids).astype(bool)
                else:
                    # Add a new column 'ID_Found' indicating whether the ID is present in the current dataframe
                    bitstamp_df.insert(0, 'ID_Found', bitstamp_df['ID'].isin(ids).astype(bool))
        else:
            print(f"No 'id' column in user {account} dataframe")
        # if 'amount' in df.columns and 'datetime' in df.columns:
        #     pu.title_print(f'df {df.dtypes}', df)
        #     # Find the rows in bitstamp_df that have the same amount and datetime as entries in df
        #     matching_rows = bitstamp_df.merge(df[['amount', 'datetime']], how='inner', left_on=['Amount', 'Datetime'], right_on=['amount', 'datetime'])
        #     pu.title_print('Matching Rows', matching_rows)
        #     # Remove matching rows from bitstamp_df
        #     bitstamp_df = bitstamp_df[~bitstamp_df.index.isin(matching_rows.index)]
        #     pu.title_print(f'bitstamp_df {bitstamp_df.dtypes}', bitstamp_df)

# Display the remaining unmatched rows in bitstamp_df
# print(f"rows removed from bitstamp_df: {orig_size - len(bitstamp_df)}")
found_count = bitstamp_df['ID_Found'].sum()
print(f"Number of rows with ID_Found set to True: {found_count}")
pu.title_print(f'Found IDs : {len(bitstamp_df)}', bitstamp_df[["ID_Found", "ID", "Account", "Type", "Subtype", "Datetime"]], debug=debug)


# In[ ]:




