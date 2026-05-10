
# open file called cal_table.txt
with open('cal_table.txt', 'w') as f:
    for i in range (0, 1024):
        new_bin = min(round(2**(1/12) * i + 0.5), 1023)
        f.write(f"shift_table[{i}] = {new_bin - i};\n")

