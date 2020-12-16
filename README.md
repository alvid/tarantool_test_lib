# tarantool_test_lib
$ git clone https://github.com/alvid/tarantool_test_lib.git
$ cd tarantool_test_lib
$ mkdir build
$ cd build && cmake .. 
$ make
$ cd ../Test
$ ln -sf ../build/libluartsib.so rtsib.so
$ export LUA_CPATH="?.so"
$ tarantool rtsib_test.lua
