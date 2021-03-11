# How to use cmake

```bash
mkdir build; cd build
cmake ..
```

- if `Windows`
It will create `.sln`, just double click it to open.

- if `Mac / Linxu`
when cmake done, run `make`

# Clean 

just remove all files in the folder which named build or your created

# How to build example in tests folder

some change in `CMakeLists.txt` like this:
```makefile
# Change it with a name you like.
project(thp CXX C)
# Change this filepath which the main file of example in.
add_executable(${PROJECT_NAME} ../../test/throughput/src/main.cpp)
```
done! Have fun!