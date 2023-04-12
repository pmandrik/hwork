
from sqlalchemy import create_engine, select, MetaData, Table, and_
import matplotlib
import pandas as pd

from collections import defaultdict

engine = create_engine("sqlite:///ftrace_test.db")
df = pd.read_sql('select * from ftrace',engine)


# 1. overall function informations
fnames = df.fname.unique()

units = df.duration_units.unique()
print(units)

unit_map = { "us" : 1, '' : 0 }
def calc_time(row):
  return unit_map[row["duration_units"]] * row["duration"]
  
df["dtime"] = df.apply(calc_time, axis = 1)

class Fdata:
  def __init__(self):
    self.name = ""

fdic = defaultdict( Fdata )
flist = []
for name in list(fnames):
  fd = Fdata()
  fd.name = name
  
  def update_func(row):
    #fd.total_time += row[]
    pass
  
  rslt_df = df[df['fname'] == name]
  rslt_df_dtime = rslt_df['dtime']
  fd.total_time = rslt_df_dtime.sum()
  fd.aver_time = rslt_df_dtime.mean()
  fd.max_time = rslt_df_dtime.max()
  fd.std_time = rslt_df_dtime.std()
  fd.n_entries = rslt_df_dtime.count()
  
  #print( fd.__dict__ )
  flist += [ fd ]
  
flist = sorted( flist , key=lambda x: x.total_time )
flist_top = flist[-100:]

for f in flist_top:
  print( f.__dict__ )
  
  
  
  
  
  
  
  
  
  
  
