# Given the file convert it into a wav file
# input are given as 2's complement 28-bit fixed point
# output is a wav file

import numpy as np
import wave
import sys
from fp2fx import fx2fp

if __name__ == "__main__":
    filename = sys.argv[1]
    with open(filename, "r") as f:
        data = f.readlines()
    data = [fx2fp(line, 28) for line in data]
    data = [int(x * 2**15) for x in data]
    data = np.array(data, dtype=np.int16)
    wav_file = wave.open("output.wav", "w")
    wav_file.setparams((1, 2, 8000, 0, "NONE", "not compressed"))
    wav_file.writeframes(data)
    wav_file.close()
