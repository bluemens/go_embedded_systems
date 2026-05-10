import struct

def fp2fx(fp, size=16):
    '''
    Given floating point number, return 16-bit 2's complement fixed point
    representation. Where the most significant bit is the sign bit, the next
    15 bits are the fractional part.
    '''
    # Convert floating point to binary string
    fx = fp * 2**15
    # truncate the fractional part
    fx = int(fx)
    # convert fx into binary string of 16-bit 2's complement fixed point
    fx = bin(fx & (2**size - 1))[2:].zfill(size)
    return fx

def fx2fp(fx, size=16):
    '''
    Given a string of 16-bit 2's complement fixed point representation, return the
    floating point number with 6 decimal places.
    '''
    # convert fx[1:15] into integer, 16-bit 2's complement fixed point
    fxx = int(fx[1:], 2)
    # convert to floating point
    if fx[0] == '1':
        fxx = fxx - 2**(size - 1)    
    fp = fxx / 2**15
    return round(fp, 6)   

if __name__ == '__main__':
    print(fp2fx(0.99999971))
    print(fx2fp(fp2fx(0.99999971)))
    print(fp2fx(0.5))   
    print(fx2fp(fp2fx(0.5)))
    print(fp2fx(0.0))
    print(fx2fp(fp2fx(0.0)))
    print(fp2fx(-1.0))
    print(fx2fp(fp2fx(-1.0)))
    print(fp2fx(-0.75))
    print(fx2fp(fp2fx(-0.75)))
    print(fp2fx(-0.0))
    print(fx2fp(fp2fx(-0.0)))
    
    
