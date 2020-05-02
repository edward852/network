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
./epoll

# detailed log
./epoll -v
```

# Test
```shell script
echo "hello" | nc -q 1 localhost 1080
```
