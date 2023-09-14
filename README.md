# ee542_lab4
By default, the uploaded file will have the name prefix `uploaded_`.
## Build
```
mkdir build & cd build
cmake .. & make -j
```

For server, simply run 
```
./server
```

For client, run 
```
./client server_address /path/to/file
```

To check md5sum of two files, run
```
./md5check.sh /path/to/original/file /path/to/uploaded/file
```
