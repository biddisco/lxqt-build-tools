# for use when debugging scripts for parsing json etc
# removes all files that are generated or downloaded, 
# but be sure to have a master copy somewhere of known csv/json transactions 

rm ./transactions/grox-data/*.csv
rm ./transactions/grox-data/*.pkl
rm ./transactions/grox-data/*.h5
rm ~/.local/share/grox/*.csv
rm ~/.local/share/grox/*.pkl
rm ~/.local/share/grox/*.h5
rm ~/.local/share/grox/*.bak
rm ~/.local/share/grox/*.json
rm ~/.local/share/grox/latest_datetime.ini

