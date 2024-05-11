mkdir -p build out
cd build || return
cmake ..
make
cp src/client/client ../out/
cp src/server/server ../out/