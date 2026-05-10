# read data from ../final-hw/testbench/fft_test/magnitude_output.dat

# for each line, square root the value and divided by 2^17
import math
import matplotlib.pyplot as plt

def process_magnitude_data():
    # Read the magnitude output data
    with open('../final-hw/testbench/fft_test/magnitude_output.dat', 'r') as f:
        # Read all lines and convert to float
        values = [float(line.strip()) for line in f if line.strip()]
    
    # Process each value: square and divide by 2^17
    processed_values = [math.sqrt(value) / (2**17) for value in values]
    
    return processed_values

if __name__ == "__main__":
    processed_data = process_magnitude_data()
    
    # Print first few values for verification
    print("First 10 processed values:")
    for i, value in enumerate(processed_data[:10]):
        print(f"Value {i}: {value}")
    
    # write it to a file
    with open('magnitude_output.dat', 'w') as f:
        for value in processed_data:
            f.write(f"{value}\n")   
    
    # plot the frequency magnitude response
    plt.plot(processed_data)
    plt.savefig('magnitude_response.png')
    
