from ulab import numpy as np # to get access to ulab numpy functions
import time
# Declare an array with some made up data like
a = np.empty([1024])
# Test some stats functions, like
for i in range(1024):
    sin_1 = 50*np.sin(10*i)
    sin_2 = 50*np.sin(15*i)
    sin_3 = 50*np.sin(20*i)
    a[i] = sin_1+sin_2+sin_3

fft = np.fft.fft(a)

j = 0
while 1:
    print("("+str(a[j])+",)")
    j += 1
    if j == 1023:
       j = 0
    time.sleep(0.05)

# Want to know all the functions available in numpy? In REPL type np. and press Tab.
