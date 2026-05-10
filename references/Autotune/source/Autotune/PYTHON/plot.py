file_name = "500Hz/fx_real.dat"
data = []

from fp2fx import fx2fp
import matplotlib.pyplot as plt

with open(file_name, "r") as file:
    lines = file.readlines()

# for each line, convert the 2's complement to decimal:
for line in lines:
    data.append(fx2fp(line, 28))

# plot the data:
# line space of 1/2048s
time = [i * (1/2048) for i in range(50)]
plt.plot(time, data[0:50])
plt.xlabel("Time")
plt.ylabel("Amplitude")
plt.title("FFT of Sinusoid")
plt.savefig("500Hz/fft_of_sinusoid.png")
plt.show()
