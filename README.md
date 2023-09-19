# ee542_lab4
By default, the uploaded file will have the name prefix `uploaded_`.
## Build
```
mkdir build & cd build
cmake .. & make -j
```

For server, simply run 
```
./tcp_server port_number file_path thread_count
```

For client, run 
```
 ./tcp_client server_addr port_number thread_count
```

To check md5sum of two files, run
```
./md5check.sh /path/to/original/file /path/to/uploaded/file
```
