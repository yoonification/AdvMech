# read data from the imu and plot

# sudo apt-get install python3-pip
# python -m pip install pyserial
# sudo apt-get install python-matplotlib

import serial
ser = serial.Serial('COM7',115200)
print('Opening port: ')
print(ser.name)

read_samples = 10 # anything bigger than 1 to start out
pitch = []
roll = []
print('Requesting data collection...')
ser.write(b'\n')
while read_samples > 1:
    data_read = ser.read_until(b'\n',200) # get the data as bytes
    data_text = str(data_read,'utf-8') # turn the bytes to a string
    data = [float(i) for i in data_text.split()] # turn the string into a list of floats

    if(len(data)==3):
        read_samples = int(data[0]) # keep reading until this becomes 1
        pitch.append(data[1])
        roll.append(data[2])
print('Data collection complete')
# plot it
import matplotlib.pyplot as plt 
t = range(len(pitch)) # time array
plt.plot(t,pitch,'r*-')
plt.ylabel('Pitch')
plt.xlabel('sample')
plt.show()

t = range(len(roll)) # time array
plt.plot(t,roll,'r*-')
plt.ylabel('Roll')
plt.xlabel('sample')
plt.show()


# be sure to close the port
ser.close()