import numpy as np
import sys
import os
import matplotlib.pyplot as plt
from fp2fx import fx2fp, fp2fx
# dir_path = sys.argv[1]

def fft_2048_pts(pts):
    '''
    given 2048 data points in pts in dtype=np.complex128, return the 2048 data points in fft_pts
    '''
    fft_pts = np.fft.fft(pts)
    # break down the fft_pts into real and imag
    fft_real_pts = np.real(fft_pts)
    fft_imag_pts = np.imag(fft_pts)
    return fft_real_pts, fft_imag_pts

def fft_magitude_pts(fft_real_pts, fft_imag_pts):
    fft_magitude_pts = np.sqrt(fft_real_pts**2 + fft_imag_pts**2)
    return fft_magitude_pts

def fft_phase_pts(fft_real_pts, fft_imag_pts):
    fft_phase_pts = np.arctan2(fft_imag_pts, fft_real_pts)
    return fft_phase_pts

def write_to_file(file_path, fft_real_pts, fft_imag_pts, fft_magitude_pts, fft_phase_pts):
    fft_real_pts_file_path = f"{dir_path}/fft_real.dat"
    fx_fft_real_pts_file_path = f"{dir_path}/fx_fft_real.dat"   
    fft_imag_pts_file_path = f"{dir_path}/fft_imag.dat"
    fx_fft_imag_pts_file_path = f"{dir_path}/fx_fft_imag.dat"
    fft_magitude_pts_file_path = f"{dir_path}/fft_mag.dat"
    fx_fft_magitude_pts_file_path = f"{dir_path}/fx_fft_mag.dat"
    fft_phase_pts_file_path = f"{dir_path}/fft_phase.dat"
    fx_fft_phase_pts_file_path = f"{dir_path}/fx_fft_phase.dat"
    fft_pts_file_path = f"{dir_path}/fft_output.dat"
    fx_fft_pts_file_path = f"{dir_path}/fx_fft_output.dat"

    fx_fft_real_pts = np.array([fp2fx(fft_real_pts[i], 28) for i in range(len(fft_real_pts))])
    fx_fft_imag_pts = np.array([fp2fx(fft_imag_pts[i], 28) for i in range(len(fft_imag_pts))])
    fx_fft_magitude_pts = np.array([fp2fx(fft_magitude_pts[i], 28) for i in range(len(fft_magitude_pts))])
    fx_fft_phase_pts = np.array([fp2fx(fft_phase_pts[i], 28) for i in range(len(fft_phase_pts))])

    with open(fft_real_pts_file_path, "w") as f:
        for i in range(len(fft_real_pts)):
            f.write(f"{fft_real_pts[i]:.6f}\n")
    with open(fx_fft_real_pts_file_path, "w") as f:
        for i in range(len(fx_fft_real_pts)):
            f.write(f"{fx_fft_real_pts[i]}\n")
    with open(fft_imag_pts_file_path, "w") as f:
        for i in range(len(fft_imag_pts)):
            f.write(f"{fft_imag_pts[i]:.6f}\n")
    with open(fx_fft_imag_pts_file_path, "w") as f:
        for i in range(len(fx_fft_imag_pts)):
            f.write(f"{fx_fft_imag_pts[i]}\n")
    with open(fft_magitude_pts_file_path, "w") as f:
        for i in range(len(fft_magitude_pts)):
            f.write(f"{fft_magitude_pts[i]:.6f}\n") 
    with open(fx_fft_magitude_pts_file_path, "w") as f:
        for i in range(len(fx_fft_magitude_pts)):
            f.write(f"{fx_fft_magitude_pts[i]}\n")
    with open(fft_phase_pts_file_path, "w") as f:
        for i in range(len(fft_phase_pts)):
            f.write(f"{fft_phase_pts[i]:.6f}\n")
    with open(fx_fft_phase_pts_file_path, "w") as f:
        for i in range(len(fx_fft_phase_pts)):
            f.write(f"{fx_fft_phase_pts[i]}\n")
    with open(fft_pts_file_path, "w") as f:
        for i in range(len(fft_real_pts)):
            f.write(f"{fft_real_pts[i]:.6f} {fft_imag_pts[i]:.6f} {fft_magitude_pts[i]:.6f} {fft_phase_pts[i]:.6f}\n")
    with open(fx_fft_pts_file_path, "w") as f:
        for i in range(len(fx_fft_real_pts)):
            f.write(f"{fx_fft_real_pts[i]} {fx_fft_imag_pts[i]} {fx_fft_magitude_pts[i]} {fx_fft_phase_pts[i]}\n")

    # save magnitude into a png, map 0 to 2047 index from 0 to 8kHz
    # plot the magnitude and phase in one plot
    x_axis = np.linspace(0, 8000, 2048)
    plt.plot(x_axis, fft_magitude_pts, label="Magnitude")
    plt.plot(x_axis, fft_phase_pts, label="Phase")
    plt.legend()
    plt.savefig(f"{dir_path}/fft_mag_phase.png")
    plt.close()


if __name__ == "__main__":
    dir_path = sys.argv[1]
    # check if the directory exists
    if not os.path.exists(dir_path):
        print(f"Error: The directory {dir_path} does not exist.")
        sys.exit(1)

    # each line is a point data real and imag
    file_path = f"{dir_path}/fx_input.dat"
    with open(file_path, "r") as f:
        lines = f.readlines()

    real_pts = []
    imag_pts = []
    for line in lines:  
        real_pts.append(fx2fp(line.split()[0]))
    for line in lines:
        imag_pts.append(fx2fp(line.split()[1]))

    # convert to numpy array, using .8f format
    real_pts = np.array(real_pts, dtype=np.float64)
    imag_pts = np.array(imag_pts, dtype=np.float64)
    # set up np.array with complex number data type
    pts = np.array(real_pts + imag_pts * 1j, dtype=np.complex128)   

    # compute 2048 points fft     
    fft_real_pts, fft_imag_pts = fft_2048_pts(pts)
    fft_magitude_pts = fft_magitude_pts(fft_real_pts, fft_imag_pts)
    fft_phase_pts = fft_phase_pts(fft_real_pts, fft_imag_pts)
    write_to_file(file_path, fft_real_pts, fft_imag_pts, fft_magitude_pts, fft_phase_pts)
