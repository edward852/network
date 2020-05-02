# Note
Build under Linux environment.  

# Build
```shell script
mkdir build; cd build
cmake ..
make
```

# Run
```shell script
# without log
./select

# detailed log
./select -v
```

# Test
```shell script
echo "hello" | nc -q 1 localhost 1080
```
