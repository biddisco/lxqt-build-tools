#!/usr/bin/env python
# coding: utf-8

# In[20]:


# activate .venv in .venv/bin/activate
import pandas as pd
import os
import sys
import argparse
import json
import glob
import shutil
#
from PyQt6.QtCore import QStandardPaths
from PyQt6.QtCore import QSettings
#
from IPython.display import display, HTML
#
sys.path.append('/home/biddisco/src/plotutils')
import plotutils as pu

delete_airdrops = False
decimals = 8


# In[21]:


# The directory where grox will store new json files downloaded from exchanges
grox_data_dir = os.path.join(QStandardPaths.standardLocations(QStandardPaths.StandardLocation.AppDataLocation)[0], 'grox/')
print(f"grox_data_dir: {grox_data_dir}")
# when debugging, set this to another location for json files
grox_json_dir = '/home/biddisco/src/grox/transactions/grox-data'
print(f"grox_json_dir: {grox_json_dir}")
# use the main grox ini file to store timestamps
grox_ini_file = os.path.join(QStandardPaths.standardLocations(QStandardPaths.StandardLocation.AppConfigLocation)[0], 'grox.ini')
print(f"grox_ini_file: {grox_ini_file}")


# In[19]:


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
    args = parser.parse_args()
    grox_data_dir = args.grox_data_dir

pd.options.display.float_format = '{:.8f}'.format


# In[4]:


# we must handle "2021-02-23 08:59:14.652000" and "2021-02-23 08:59:14" and unix timestamps
def date_formatter(date):
    try:
        if isinstance(date, pd.Timestamp):
            return date
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

def cleanup_dataframe(df):
    # print(f"Before Dataframe shape: {df.shape}")
    # pu.title_print(f"Before cleaning \n{df.dtypes}", df)

    for col in df.columns:
        if col=='datetime':
            df = convert_to_datetime(df, 'datetime', 'datetime')
            # df.set_index('datetime', inplace=True)
        elif col in ['id', 'order_id', 'type']:
            df[col] = df[col].astype('Int64')
        elif col in ['amount', 'fee', 'btc', 'eur', 'usd', 'xrp', 'xrp_usd', 'xrp_eur', 'xrp_gbp', 'btc_usd', 'eth', 'sgb', 'flr']:
            df[col] = pd.to_numeric(df[col], errors='coerce').fillna(0.0).astype('Float64')
        elif col in ['currency', 'destinationAddress', 'network', 'txid']:
            df[col] = df[col].astype('string')
        elif df[col].dtype == 'object':
            print(f"Converting column {col} to numeric")
            try:
                df[col] = pd.to_numeric(df[col], errors='coerce').fillna(0.0).astype('Float64')
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
    # print(f"After Dataframe shape: {df.shape}")
    # pu.title_print(f"After cleaning \n{df.dtypes}", df)
    return df


# In[5]:


# load previously generated pkl files into pandas dataframes
category_dict = {"crypto": None, "user": None, "market": None}
for suffix in category_dict.keys():
    transaction_type_dict = {'deposits': None, 'withdrawals': None, 'others': None}
    for suffix2 in transaction_type_dict.keys():
        csv_file = os.path.join(grox_data_dir, f'transactions-{suffix}-{suffix2}.csv')
        if os.path.exists(csv_file):
            backup_file = csv_file + ".bak"
            shutil.copy(csv_file, backup_file)
            transaction_type_dict[suffix2] = cleanup_dataframe(pd.read_csv(csv_file, parse_dates=['datetime']))
            print(f"Read {len(transaction_type_dict[suffix2])} rows from {csv_file}, backing-up -> {backup_file}")
            # pu.title_print(f"Loaded {pkl_file}", transaction_type_dict[suffix2])
    category_dict[suffix] = transaction_type_dict


# In[ ]:


json_files_read = []
success = True
# ---------------------------------------------------------------------
# Load all new JSON files and merge them with the existing dataframes
# ---------------------------------------------------------------------
for suffix in category_dict.keys():
    transaction_type_dict = {'deposits': [], 'withdrawals': [], 'others': []}
    # Load all JSON files with the right suffix and add them to a list of datasets
    all_files = glob.glob(os.path.join(grox_json_dir, f'transactions-{suffix}-*.json'))
    try:
        for file in all_files:
            with open(file, 'r') as f:
                json_data = json.load(f)
                # Convert deposits and withdrawals to DataFrames
                if 'deposits' in json_data:
                    # print(f"appending dataset to {suffix} deposits")
                    transaction_type_dict['deposits'].append(pd.DataFrame(json_data['deposits']))
                if 'withdrawals' in json_data:
                    # print(f"appending dataset to {suffix} withdrawals")
                    transaction_type_dict['withdrawals'].append(pd.DataFrame(json_data['withdrawals']))
                if not 'deposits' in json_data and not 'withdrawals' in json_data:
                    # print(f"appending dataset to {suffix} others")
                    transaction_type_dict['others'].append(pd.DataFrame(json_data))
                print(f"Processed file: {file}")
                json_files_read.append(file)
    except Exception as e:
        transaction_type_dict = None
        success = False
        print(f"Error processing file {file}: {e}")
        raise e

    try:
        # merge all (json) datasets in each group into a single dataframe
        for key in transaction_type_dict.keys():
            all_transactions = None
            for df in transaction_type_dict[key]:
                # Merge all dataframes under this key into a single dataframe
                if all_transactions is not None:
                    all_transactions = pd.concat([all_transactions, cleanup_dataframe(df)], ignore_index=True).drop_duplicates(ignore_index=True, keep='first')
                else:
                    all_transactions = cleanup_dataframe(df)

            # now merge the new dataframe into the existing dataframe
            if all_transactions is not None:
                if category_dict[suffix][key] is not None:
                    combined_df = pd.concat([category_dict[suffix][key], all_transactions], ignore_index=True).drop_duplicates(ignore_index=True, keep='first')
                    category_dict[suffix][key] = cleanup_dataframe(combined_df)
                else:
                    category_dict[suffix][key] = all_transactions

    except Exception as e:
        success = False
        print(f"Error merging dataframes: {e}")
        raise e
        continue

if success:
    # Open ini file using QSettings
    settings = QSettings(grox_ini_file, QSettings.Format.IniFormat)
    # Write the latest datetime to the ini file
    settings.beginGroup("TransactionLogs")
    for suffix in category_dict.keys():
        latest_datetime = None
        last_orderId = None
        # Save the transactions to pickle files
        for key in category_dict[suffix].keys():
            if category_dict[suffix] is not None and category_dict[suffix][key] is not None:
                # pu.title_print(f"{suffix} {key}", category_dict[suffix][key])
                csv_name = os.path.join(grox_data_dir, f'transactions-{suffix}-{key}.csv')
                print(f"Saving {suffix} {key} to CSV file {csv_name}")
                category_dict[suffix][key].to_csv(csv_name, index=False)
                if 'datetime' in category_dict[suffix][key].columns:
                    if latest_datetime is not None:
                        latest_datetime = max(latest_datetime, category_dict[suffix][key]['datetime'].iloc[-1])
                    else:
                        latest_datetime = category_dict[suffix][key]['datetime'].iloc[-1]
                # Get the last valid order_id if it exists
                if 'order_id' in category_dict[suffix][key].columns:
                    if last_orderId is not None:
                        last_orderId = max(last_orderId, category_dict[suffix][key]['order_id'].iloc[-1])
                    else:
                        last_orderId = category_dict[suffix][key]['order_id'].iloc[-1]
                    # Scan backwards to find a valid order_id if the last one is invalid
                    if pd.isna(last_orderId):
                        for order_id in reversed(category_dict[suffix][key]['order_id']):
                            if not pd.isna(order_id):
                                last_orderId = order_id
                                break

        print(f"Latest datetime for {suffix} {key}: {latest_datetime} -> {grox_ini_file}")
        print(f"Last valid order_id for {suffix} {key}: {last_orderId} -> {grox_ini_file}")
        settings.setValue(f"Datetime_{suffix}_{key}", latest_datetime.strftime('%y-%m-%d %H:%M:%S'))
        if pd.notna(last_orderId):
            settings.setValue(f"Order_ID_{suffix}_{key}", int(last_orderId))
    settings.endGroup()

    for file in json_files_read:
        try:
            # os.remove(file)
            print(f"NOT Deleted file: {file}")
        except Exception as e:
            print(f"Error deleting file {file}: {e}")
else:
    raise Exception("Error processing files")


# In[ ]:





# In[7]:


# transaction_types = { 0: 'Deposit',
#                       1: 'Withdrawal',
#                       2: 'Code2',
#                      14: "SubAccountTransfer",
#                      55: "Airdrop"}
# # extract all trades with code buy/sell from the others
# if len(transaction_type_dict['others']) > 0:
#     other_transactions = transaction_type_dict['others']
#     type_dict = {}
#     for t in other_transactions['type'].unique():
#         type_dict[transaction_types[int(t)]] = other_transactions[other_transactions['type'] == t]

# for key in type_dict.keys():
#     pu.title_print_all(f'{key} Transactions', type_dict[key])


# In[ ]:




