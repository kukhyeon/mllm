from gguf import GGUFReader
import pandas as pd
import re, sys

def extract_op_group(name):
    # Pattern: blk.N.subtype -> extract subtype 
    match = re.search(r'\.(\w+_\w+)\.', name)
    if match:
        return match.group(1)
    
    # Pattern: blk.N.type -> extract type
    match = re.search(r'\.(\w+)\.', name)
    if match:
        return match.group(1)
    
    # When op is a Global Tensor
    return name.split('.')[0]


# File Input
argv = sys.argv
file_name = argv[1]

# GGUF File Load
reader = GGUFReader(file_name)

# List to Collect Tensor Info 
tensor_stats = []

for tensor in reader.tensors:
    name = tensor.name
    op_group = extract_op_group(name)
    dtype = tensor.tensor_type.name # Datatype: Q6_K
    size_kb = tensor.n_bytes / 1024 # unit: KB
    tensor_stats.append((name, op_group, dtype, size_kb))

# Create and Store Dataframe
df = pd.DataFrame(tensor_stats, columns=["tensor_name", "op_group", "dtype", "size_kb"])

# Accumulate Tensor Size after Grouping according to the tensor_name
# df_grouped = df.groupby("tensor_name").agg(
#     total_kb = ("size_kb", "sum"),
#     dtype = ("dtype", "first"),
#     count = ("size_kb", "count")
# ).reset_index()

# Sort by Tensor Size
# df_sorted = df_grouped.sort_values(by="total_kb", ascending=False)

# Print Result
pd.set_option("display.max_rows", None)
pd.set_option("display.max_colwidth", None)
print(df)