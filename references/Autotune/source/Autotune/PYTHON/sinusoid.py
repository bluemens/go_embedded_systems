# generate a sinusoid with a frequency of 440Hz with a sample rate of 8kHz
# Generate 2048 points
# Plot the sinusoid

import numpy as np
import matplotlib.pyplot as plt
import sys
import os
from fp2fx import fp2fx, fx2fp

def generate_sinusoid(f, fs, dir_path='./'):
    t = np.linspace(0, 8192/fs, 8192)

    # Generate the sinusoid
    x = np.sin(2 * np.pi * f * t)

    # Plot the sinusoid and save to file
    plt.figure(figsize=(10, 6))
    plt.plot(t, x)
    plt.title(f'{f} Hz Sinusoid')
    plt.xlabel('Time (seconds)')
    plt.ylabel('Amplitude')
    plt.grid(True)
    plt.savefig(f'{dir_path}plot.png')
    plt.close()

    # save the sinusoid to a file

    real_file_path = f'{dir_path}real.dat'
    fx_real_file_path = f'{dir_path}fx_real.dat'
    imag_file_path = f'{dir_path}imag.dat' 
    fx_imag_file_path = f'{dir_path}fx_imag.dat'
    file_path = f'{dir_path}input.dat'
    fx_file_path = f'{dir_path}fx_input.dat'

    fx_x = np.array([fp2fx(x[i]) for i in range(len(x))])
    with open(real_file_path, 'w') as f:
        for i in range(len(x)):
            f.write(f"{x[i]:.6f}\n")
    with open(fx_real_file_path, 'w') as f:
        for i in range(len(fx_x)):
            f.write(f"{fx_x[i]}\n")
    with open(imag_file_path, 'w') as f:
        for i in range(len(x)):
            f.write(f"{0:.6f}\n")
    with open(fx_imag_file_path, 'w') as f:
        for i in range(len(fx_x)):
            f.write(f"0000000000000000\n")
    with open(file_path, 'w') as f:
        for i in range(len(x)):
            f.write(f"{x[i]:.6f} {0:.6f}\n")
    with open(fx_file_path, 'w') as f:
        for i in range(len(fx_x)):
            f.write(f"{fx_x[i]} 0000000000000000\n")

if __name__ == "__main__":
    # first argument is the frequency
    f = int(sys.argv[1])
    # second argument is the sample rate
    fs = int(sys.argv[2])
    dir_path = f"{sys.argv[1]}Hz"
    # create the directory if it doesn't exist
    os.makedirs(dir_path, exist_ok=True)
    dir_path = f"{dir_path}/"
    generate_sinusoid(f, fs, dir_path=dir_path)
    

    
    